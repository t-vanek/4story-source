#include "handlers.h"
#include "../senders/senders.h"
#include "../wire_codec.h"

#include "MessageId.h"

#include "fourstory/db/co_offload.h"

#include <spdlog/spdlog.h>

#include <cstdint>
#include <ctime>
#include <vector>

namespace tworldsvr::handlers {

namespace {

std::shared_ptr<PeerSession>
FindMapPeer(const HandlerContext& ctx, std::uint8_t msi)
{
    if (msi == 0 || !ctx.peers) return nullptr;
    for (auto& p : ctx.peers->Snapshot())
        if (static_cast<std::uint8_t>(p->Wid() & 0xFF) == msi)
            return p;
    return nullptr;
}

} // namespace

boost::asio::awaitable<void>
OnRpsGameAck(std::shared_ptr<PeerSession> peer,
             std::vector<std::byte>       body,
             const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers || !ctx.rps)
    {
        spdlog::warn("OnRpsGameAck[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    std::uint8_t  type = 0, win_count = 0, player_rps = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(type) ||
        !r.Read(win_count) || !r.Read(player_rps))
    {
        spdlog::warn("OnRpsGameAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    auto ch = ctx.chars->Find(char_id);
    if (!ch) co_return;

    std::uint8_t  main_id = 0;
    bool          key_ok = true;
    {
        std::lock_guard g(ch->lock);
        if (ch->key != key) key_ok = false;
        else                main_id = ch->main_server_id;
    }
    if (!key_ok) co_return;

    const std::int64_t now = static_cast<std::int64_t>(std::time(nullptr));
    std::vector<RpsRegistry::PersistOp> ops;
    const auto outcome = ctx.rps->RecordWin(type, win_count, char_id, now,
        ops);

    // W6-49: persist the ledger mutations (legacy
    // SendDM_RPSGAMERECORD_REQ → TRPSGameRecord). One offload task
    // for the batch; failures log inside the repo.
    if (!ops.empty())
    {
        if (ctx.rps_repo)
        {
            co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
                [repo = ctx.rps_repo, ops]
                {
                    for (const auto& op : ops)
                        repo->Record(op.insert, op.char_id, op.type,
                            op.win_count, op.date_unix);
                });
        }
        else
            spdlog::info("OnRpsGameAck[{}]: {} persist op(s) dropped "
                         "(no DB configured)", ip, ops.size());
    }

    std::uint8_t result = 1;            // legacy default TRUE
    switch (outcome)
    {
    case RpsRegistry::Outcome::kAllowed:     result = 1; break;
    case RpsRegistry::Outcome::kCapReached:  result = 0; break;
    case RpsRegistry::Outcome::kNotFound:    result = 0; break;
    }

    auto main_peer = FindMapPeer(ctx, main_id);
    if (!main_peer) co_return;

    spdlog::info("OnRpsGameAck[{}]: char_id={} type={} win_count={} → "
                 "result={}", ip, char_id, type, win_count, result);
    co_await senders::SendMwRpsGameReq(main_peer, char_id, key, result,
        player_rps);
}

boost::asio::awaitable<void>
OnRpsGameRecordReq(std::shared_ptr<PeerSession> peer,
                   std::vector<std::byte>       body,
                   const HandlerContext&        ctx)
{
    // W6-49: hybrid fan-in — a legacy DB worker (or another shard)
    // pushing the record packet now really persists through
    // IRpsRepository (legacy OnDM_RPSGAMERECORD_REQ,
    // SSHandler.cpp:13232).
    const std::string& ip = peer->Wire()->RemoteIPv4();
    wire::Reader r(body.data(), body.size());
    std::uint8_t  record = 0, type = 0, win_count = 0;
    std::uint32_t char_id = 0;
    std::int64_t  date = 0;
    if (!r.Read(record) || !r.Read(char_id) || !r.Read(type) ||
        !r.Read(win_count) || !r.Read(date))
    {
        spdlog::warn("OnRpsGameRecordReq[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }
    if (!ctx.rps_repo)
    {
        spdlog::info("OnRpsGameRecordReq[{}]: record={} char_id={} "
                     "type={} win_count={} — dropped (no DB)", ip,
            record, char_id, type, win_count);
        co_return;
    }
    co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
        [repo = ctx.rps_repo, record, char_id, type, win_count, date]
        {
            repo->Record(record != 0, char_id, type, win_count, date);
        });
    co_return;
}

boost::asio::awaitable<void>
OnRpsGameDataReq(std::shared_ptr<PeerSession> peer,
                 std::vector<std::byte>       body,
                 const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.rps)
    {
        spdlog::warn("OnRpsGameDataReq[{}]: rps registry not wired", ip);
        co_return;
    }
    wire::Reader r(body.data(), body.size());
    std::uint8_t group = 0;
    if (!r.Read(group))
    {
        spdlog::warn("OnRpsGameDataReq[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }
    auto snapshot = ctx.rps->Snapshot();
    spdlog::info("OnRpsGameDataReq[{}]: group={} → {} row(s)", ip, group,
        snapshot.size());
    co_await senders::SendCtRpsGameDataAck(peer, /*change=*/0, group,
        snapshot);
}

boost::asio::awaitable<void>
OnRpsGameChangeReq(std::shared_ptr<PeerSession> peer,
                   std::vector<std::byte>       body,
                   const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.rps || !ctx.peers)
    {
        spdlog::warn("OnRpsGameChangeReq[{}]: registries not wired", ip);
        co_return;
    }
    wire::Reader r(body.data(), body.size());
    std::uint8_t  group = 0;
    std::uint16_t count = 0;
    if (!r.Read(group) || !r.Read(count))
    {
        spdlog::warn("OnRpsGameChangeReq[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    std::uint16_t applied = 0;
    for (std::uint16_t i = 0; i < count; ++i)
    {
        std::uint8_t  type = 0, win_count = 0;
        std::uint8_t  win_prob = 0, draw_prob = 0, lose_prob = 0;
        std::uint16_t win_keep = 0, win_period = 0;
        if (!r.Read(type) || !r.Read(win_count) || !r.Read(win_prob) ||
            !r.Read(draw_prob) || !r.Read(lose_prob) ||
            !r.Read(win_keep) || !r.Read(win_period))
        {
            spdlog::warn("OnRpsGameChangeReq[{}]: truncated row {}/{}", ip,
                i, count);
            co_return;
        }
        if (ctx.rps->Set(type, win_count, win_prob, draw_prob, lose_prob,
                         win_keep, win_period))
            ++applied;
    }

    spdlog::info("OnRpsGameChangeReq[{}]: group={} applied {} / {} rows",
        ip, group, applied, count);

    if (count == 0) co_return;       // legacy parity (`if(wCount)` gate)

    // Reply the change snapshot to the requester (control peer)
    // and broadcast the verbatim wire body to every map peer so
    // they refresh their config caches.
    co_await senders::SendCtRpsGameDataAck(peer, /*change=*/1, group,
        ctx.rps->Snapshot());
    for (auto& mp : ctx.peers->Snapshot())
        co_await senders::SendMwRpsGameChangeReq(mp, body);
}

} // namespace tworldsvr::handlers
