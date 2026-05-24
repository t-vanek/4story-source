#include "handlers.h"
#include "../senders/senders.h"
#include "../wire_codec.h"

#include <spdlog/spdlog.h>

#include <cstdint>
#include <mutex>
#include <string>

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
OnAddItemResultAck(std::shared_ptr<PeerSession> peer,
                   std::vector<std::byte>       body,
                   const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.peers)
    {
        spdlog::warn("OnAddItemResultAck[{}]: peers not wired", ip);
        co_return;
    }

    // The DB / another map computed an item-add result; relay it to
    // the requesting map server (bMapSvrID).
    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0, mon_id = 0;
    std::uint8_t  map_svr_id = 0, channel = 0, item_id = 0, result = 0;
    std::uint16_t map_id = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(map_svr_id) ||
        !r.Read(channel) || !r.Read(map_id) || !r.Read(mon_id) ||
        !r.Read(item_id) || !r.Read(result))
    {
        spdlog::warn("OnAddItemResultAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    if (auto p = FindMapPeer(ctx, map_svr_id))
        co_await senders::SendMwAddItemResultReq(p, char_id, key, channel,
            map_id, mon_id, item_id, result);
    co_return;
}

boost::asio::awaitable<void>
OnDealItemErrorAck(std::shared_ptr<PeerSession> peer,
                   std::vector<std::byte>       body,
                   const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers)
    {
        spdlog::warn("OnDealItemErrorAck[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::string target, error_char;
    std::uint8_t error = 0;
    if (!r.ReadString(target) || !r.ReadString(error_char) || !r.Read(error))
    {
        spdlog::warn("OnDealItemErrorAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    auto tgt = ctx.chars->FindByName(target);
    if (!tgt) co_return;
    std::uint8_t msi = 0;
    { std::lock_guard g(tgt->lock); msi = tgt->main_server_id; }
    if (auto p = FindMapPeer(ctx, msi))
        co_await senders::SendMwDealItemErrorReq(p, target, error_char, error);
    co_return;
}

} // namespace tworldsvr::handlers
