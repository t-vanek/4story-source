#include "handlers.h"
#include "../senders/senders.h"
#include "../wire_codec.h"

#include <spdlog/spdlog.h>

#include <cstdint>
#include <mutex>
#include <string>
#include <utility>

namespace tworldsvr::handlers {

namespace {

// Walk the peer registry for the map server with LOBYTE(wid) == msi.
// Same shape as the helper in handlers_chat.cpp / handlers_char.cpp;
// kept file-local rather than pulled up to a shared header until
// enough callers justify the move.
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
OnCtCashItemSaleReq(std::shared_ptr<PeerSession> peer,
                    std::vector<std::byte>       body,
                    const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.peers || !ctx.cash_sales)
    {
        spdlog::warn("OnCtCashItemSaleReq[{}]: peers/cash_sales not wired",
            ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t dw_index = 0;
    std::uint16_t value    = 0;
    std::uint16_t count    = 0;
    if (!r.Read(dw_index) || !r.Read(value) || !r.Read(count))
    {
        spdlog::warn("OnCtCashItemSaleReq[{}]: short header ({} bytes)",
            ip, body.size());
        co_return;
    }

    TCashItemSaleEvent event{};
    event.dw_index = dw_index;
    event.value    = value;
    event.items.reserve(count);
    for (std::uint16_t i = 0; i < count; ++i)
    {
        TCashItemSale row{};
        if (!r.Read(row.id) || !r.Read(row.sale_value))
        {
            spdlog::warn("OnCtCashItemSaleReq[{}]: truncated row {}/{}",
                ip, i, count);
            co_return;
        }
        event.items.push_back(row);
    }

    // Pick the broadcast payload by branch:
    // - value!=0 → store the new row, broadcast it.
    // - value==0 → look up + deactivate the existing row (clears
    //   sale_value on every item, keeps the entry — legacy parity
    //   SSHandler.cpp:372-385). If the row doesn't exist, log + skip
    //   the broadcast (legacy SSHandler.cpp:393-397).
    TCashItemSaleEvent broadcast{};
    if (value != 0)
    {
        broadcast = event;
        ctx.cash_sales->Set(std::move(event));
    }
    else
    {
        if (!ctx.cash_sales->Deactivate(dw_index, broadcast))
        {
            spdlog::warn("OnCtCashItemSaleReq[{}]: deactivate dw_index={} "
                         "miss — dropping broadcast (legacy parity)",
                ip, dw_index);
            co_return;
        }
    }

    for (auto& p : ctx.peers->Snapshot())
        co_await senders::SendMwCashItemSaleReq(p, broadcast.dw_index,
            broadcast.value, broadcast.items);
    co_return;
}

boost::asio::awaitable<void>
OnCtCashShopStopReq(std::shared_ptr<PeerSession> peer,
                    std::vector<std::byte>       body,
                    const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.peers)
    {
        spdlog::warn("OnCtCashShopStopReq[{}]: peers not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint8_t type = 0;
    if (!r.Read(type))
    {
        spdlog::warn("OnCtCashShopStopReq[{}]: short body ({} bytes)",
            ip, body.size());
        co_return;
    }

    // Legacy SSSender.cpp:3294 always emits send_player=1 from this
    // path (the 2nd parameter has a default of TRUE).
    for (auto& p : ctx.peers->Snapshot())
        co_await senders::SendMwCashShopStopReq(p, type,
            /*send_player=*/1);
    co_return;
}

boost::asio::awaitable<void>
OnCmGiftResultAck(std::shared_ptr<PeerSession> peer,
                  std::vector<std::byte>       body,
                  const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers)
    {
        spdlog::warn("OnCmGiftResultAck[{}]: chars/peers not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint8_t  result = 0;
    std::uint8_t  tool   = 0;
    std::uint32_t gm_id  = 0;
    if (!r.Read(result) || !r.Read(tool) || !r.Read(gm_id))
    {
        spdlog::warn("OnCmGiftResultAck[{}]: short body ({} bytes)",
            ip, body.size());
        co_return;
    }

    // Admin path (tool=1) routes back to control-server, but
    // ctrl-svr identification isn't ported yet (no OnCT_CTRLSVR_REQ).
    // Log + drop — this is the same deferral note as the W7 CMGift
    // umbrella.
    if (tool != 0)
    {
        spdlog::info("OnCmGiftResultAck[{}]: tool=1 admin path "
                     "(result={}, gm_id={}) — deferred (ctrl-svr "
                     "identification not ported)", ip, result, gm_id);
        co_return;
    }

    // In-game GM path: find the issuing GM char by id, send the
    // result to their main map. Missing char / main_server_id=0 /
    // peer offline are silent drops, matching legacy SSHandler.cpp:
    // 13769-13783 (no failure reply).
    auto c = ctx.chars->Find(gm_id);
    if (!c) co_return;
    std::uint8_t main_server_id = 0;
    {
        std::lock_guard g(c->lock);
        main_server_id = c->main_server_id;
    }
    if (auto p = FindMapPeer(ctx, main_server_id))
        co_await senders::SendMwCmGiftResultReq(p, result, gm_id);
    co_return;
}

} // namespace tworldsvr::handlers
