#include "handlers.h"
#include "../senders/senders.h"
#include "../wire_codec.h"

#include "fourstory/db/co_offload.h"

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

    // W6-37: arm the per-peer confirmation barrier before the
    // broadcast (legacy SSHandler.cpp:402 clears m_bCashSale in the
    // same loop that sends). The DB persist fires only after every
    // armed peer answers MW_CASHITEMSALE_ACK.
    for (auto& p : ctx.peers->Snapshot())
    {
        p->SetCashSaleConfirmed(false);
        co_await senders::SendMwCashItemSaleReq(p, broadcast.dw_index,
            broadcast.value, broadcast.items);
    }
    co_return;
}

boost::asio::awaitable<void>
OnMwCashItemSaleAck(std::shared_ptr<PeerSession> peer,
                    std::vector<std::byte>       body,
                    const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();

    wire::Reader r(body.data(), body.size());
    std::uint32_t dw_index = 0;
    std::uint16_t value    = 0;
    std::uint8_t  ret      = 0;
    if (!r.Read(dw_index) || !r.Read(value) || !r.Read(ret))
    {
        spdlog::warn("OnMwCashItemSaleAck[{}]: short body ({} bytes)",
            ip, body.size());
        co_return;
    }

    // Legacy reads bRet but never branches on it (SSHandler.cpp:
    // 10565-10573) — the per-map result is informational only.
    peer->SetCashSaleConfirmed(true);

    if (!ctx.peers || !ctx.cash_sales)
    {
        spdlog::warn("OnMwCashItemSaleAck[{}]: peers/cash_sales not "
                     "wired", ip);
        co_return;
    }

    // Barrier: every registered map must have confirmed. Recomputed
    // on every ACK (legacy parity — a late-joining peer that ACKs
    // its replay re-runs the persist; the SP is an idempotent
    // UPDATE so the repeat is harmless).
    for (auto& p : ctx.peers->Snapshot())
        if (!p->CashSaleConfirmed())
            co_return;

    TCashItemSaleEvent ev{};
    if (!ctx.cash_sales->Get(dw_index, ev))
    {
        // Unknown campaign — legacy finds nothing in
        // m_mapTCashItemSale and never fires the DM chain.
        spdlog::info("OnMwCashItemSaleAck[{}]: dw_index={} not in "
                     "registry — persist skipped", ip, dw_index);
        co_return;
    }

    if (!ctx.cash_sale_repo)
    {
        spdlog::warn("OnMwCashItemSaleAck[{}]: cash_sale_repo not "
                     "wired — dropping persist (no DB configured)", ip);
        co_return;
    }

    // Persist each (id, sale_value) via the TCashItemSale SP; first
    // failure aborts the batch (legacy OnDM_CASHITEMSALE_REQ breaks
    // and replies bRet=FALSE, which OnDM_CASHITEMSALE_ACK drops).
    const bool ok = co_await fourstory::db::CoOffloadIf(
        ctx.db_pool,
        [repo = ctx.cash_sale_repo, items = ev.items]() -> bool
        {
            for (const auto& it : items)
                if (!repo->PersistSaleValue(it.id, it.sale_value))
                    return false;
            return true;
        });
    if (!ok)
    {
        spdlog::warn("OnMwCashItemSaleAck[{}]: dw_index={} persist "
                     "failed — no stop broadcast (legacy bRet=FALSE "
                     "drop)", ip, dw_index);
        co_return;
    }

    // value==0 → the deactivated campaign now actually leaves the
    // registry (the W6-33 zero-in-place kept it visible until the
    // cluster + DB confirmed — legacy OnDM_CASHITEMSALE_ACK erase).
    if (value == 0)
        ctx.cash_sales->Erase(dw_index);

    // Cash-shop refresh signal to every map: legacy sends
    // SendMW_CASHSHOPSTOP_REQ(FALSE, FALSE).
    for (auto& p : ctx.peers->Snapshot())
        co_await senders::SendMwCashShopStopReq(p, /*type=*/0,
            /*send_player=*/0);

    // Operator echo only on the deactivate path (legacy
    // `if(wValue == 0 && m_pCtrlSvr)`).
    if (value == 0 && ctx.ctrl_svr)
    {
        if (auto cs = ctx.ctrl_svr->Get())
            co_await senders::SendCtCashItemSaleAck(cs, dw_index, value);
        else
            spdlog::info("OnMwCashItemSaleAck[{}]: ctrl-svr offline — "
                         "CT_CASHITEMSALE_ACK dropped", ip);
    }
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

    // Admin path (tool=1): reply CT_CMGIFT_ACK on the ctrl-svr peer
    // (legacy SSHandler.cpp:13760-13765, 13999-14005). The slot may
    // be empty (ctrl-svr never identified) or stale (the recorded
    // peer disconnected); both surface as `Get()` returning nullptr,
    // which we treat as a silent drop — matches legacy
    // `if(m_pCtrlSvr) ...` short-circuit.
    if (tool != 0)
    {
        if (!ctx.ctrl_svr)
        {
            spdlog::info("OnCmGiftResultAck[{}]: tool=1 admin path "
                         "(result={}, gm_id={}) — ctrl_svr slot not "
                         "wired", ip, result, gm_id);
            co_return;
        }
        if (auto cs = ctx.ctrl_svr->Get())
            co_await senders::SendCtCmGiftAck(cs, result, gm_id);
        else
            spdlog::info("OnCmGiftResultAck[{}]: tool=1 admin path "
                         "(result={}, gm_id={}) — ctrl-svr offline",
                ip, result, gm_id);
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
