#include "handlers.h"
#include "../senders/senders.h"
#include "../wire_codec.h"

#include <spdlog/spdlog.h>

#include <cstdint>
#include <string>
#include <utility>

namespace tworldsvr::handlers {

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

} // namespace tworldsvr::handlers
