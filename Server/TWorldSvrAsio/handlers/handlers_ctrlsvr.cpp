#include "handlers.h"
#include "../senders/senders.h"
#include "../wire_codec.h"

#include "fourstory/db/co_offload.h"

#include <spdlog/spdlog.h>

#include <cstdint>
#include <utility>
#include <vector>

namespace tworldsvr::handlers {

boost::asio::awaitable<void>
OnCtCtrlsvrReq(std::shared_ptr<PeerSession> peer,
               std::vector<std::byte>       body,
               const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.ctrl_svr)
    {
        spdlog::warn("OnCtCtrlsvrReq[{}]: ctrl_svr slot not wired",
            ip);
        co_return;
    }

    if (!body.empty())
        spdlog::debug("OnCtCtrlsvrReq[{}]: ignored unexpected body "
                      "({} bytes)", ip, body.size());

    ctx.ctrl_svr->Set(peer);
    spdlog::info("OnCtCtrlsvrReq[{}]: registered as cluster ctrl-svr "
                 "(wID={})", ip, peer->Wid());
    co_return;
}

boost::asio::awaitable<void>
OnCtItemStateReq(std::shared_ptr<PeerSession> peer,
                 std::vector<std::byte>       body,
                 const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.peers)
    {
        spdlog::warn("OnCtItemStateReq[{}]: peers not wired", ip);
        co_return;
    }
    if (!ctx.item_state_repo)
    {
        spdlog::warn("OnCtItemStateReq[{}]: item_state_repo not wired "
                     "— dropping (no DB configured)", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t id    = 0;
    std::uint16_t count = 0;
    if (!r.Read(id) || !r.Read(count))
    {
        spdlog::warn("OnCtItemStateReq[{}]: short header ({} bytes)",
            ip, body.size());
        co_return;
    }

    std::vector<ItemStateChange> rows;
    rows.reserve(count);
    for (std::uint16_t i = 0; i < count; ++i)
    {
        ItemStateChange row{};
        if (!r.Read(row.item_id) || !r.Read(row.init_state))
        {
            spdlog::warn("OnCtItemStateReq[{}]: truncated row {}/{}",
                ip, i, count);
            co_return;
        }
        rows.push_back(row);
    }

    // Apply sequentially, stopping at the first failing item — the
    // legacy DB thread breaks out of the per-item SP loop
    // (SSHandler.cpp:10056) and ACKs only the prefix that succeeded.
    // One offload task for the whole batch, mirroring the single
    // DM round-trip.
    const std::size_t ok = co_await fourstory::db::CoOffloadIf(
        ctx.db_pool,
        [repo = ctx.item_state_repo, &rows]() -> std::size_t
        {
            std::size_t n = 0;
            for (const auto& row : rows)
            {
                if (!repo->ChangeState(row.item_id, row.init_state))
                    break;
                ++n;
            }
            return n;
        });
    rows.resize(ok);

    if (ok < count)
        spdlog::warn("OnCtItemStateReq[{}]: id={} applied {}/{} rows "
                     "(stopped at first failure — legacy parity)",
            ip, id, ok, count);
    else
        spdlog::info("OnCtItemStateReq[{}]: id={} applied {} row(s)",
            ip, id, ok);

    // Legacy OnDM_ITEMSTATE_ACK fans the (possibly empty) result to
    // every map server and echoes it to the ctrl-svr when known.
    for (auto& p : ctx.peers->Snapshot())
        co_await senders::SendMwItemStateReq(p, id, rows);

    if (ctx.ctrl_svr)
    {
        if (auto cs = ctx.ctrl_svr->Get())
            co_await senders::SendCtItemStateAck(cs, id, rows);
        else
            spdlog::info("OnCtItemStateReq[{}]: ctrl-svr offline — "
                         "CT_ITEMSTATE_ACK dropped", ip);
    }
    co_return;
}

} // namespace tworldsvr::handlers
