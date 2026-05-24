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
OnUserPositionAck(std::shared_ptr<PeerSession> peer,
                  std::vector<std::byte>       body,
                  const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers)
    {
        spdlog::warn("OnUserPositionAck[{}]: registries not wired", ip);
        co_return;
    }

    // A GM asks "where is <target>"; relay the request to the target's
    // map (legacy requires both the target and the GM to be online).
    wire::Reader r(body.data(), body.size());
    std::string target_name, gm_name;
    if (!r.ReadString(target_name) || !r.ReadString(gm_name))
    {
        spdlog::warn("OnUserPositionAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    auto target = ctx.chars->FindByName(target_name);
    if (!target) co_return;
    if (!ctx.chars->FindByName(gm_name)) co_return;   // GM must be online
    std::uint32_t t_id = 0, t_key = 0; std::uint8_t t_msi = 0;
    {
        std::lock_guard g(target->lock);
        t_id = target->char_id; t_key = target->key;
        t_msi = target->main_server_id;
    }
    if (auto p = FindMapPeer(ctx, t_msi))
        co_await senders::SendMwUserPositionReq(p, t_id, t_key, gm_name);
    co_return;
}

boost::asio::awaitable<void>
OnUserMoveAck(std::shared_ptr<PeerSession> peer,
              std::vector<std::byte>       body,
              const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers)
    {
        spdlog::warn("OnUserMoveAck[{}]: registries not wired", ip);
        co_return;
    }

    // A GM force-moves a user; relay the destination to the user's map.
    wire::Reader r(body.data(), body.size());
    std::string   user;
    std::uint8_t  channel = 0;
    std::uint16_t map_id = 0, party_id = 0;
    float         pos_x = 0, pos_y = 0, pos_z = 0;
    if (!r.ReadString(user) || !r.Read(channel) || !r.Read(map_id) ||
        !r.Read(pos_x) || !r.Read(pos_y) || !r.Read(pos_z) ||
        !r.Read(party_id))
    {
        spdlog::warn("OnUserMoveAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    auto c = ctx.chars->FindByName(user);
    if (!c) co_return;
    std::uint32_t cid = 0, key = 0; std::uint8_t msi = 0;
    {
        std::lock_guard g(c->lock);
        cid = c->char_id; key = c->key; msi = c->main_server_id;
    }
    if (auto p = FindMapPeer(ctx, msi))
        co_await senders::SendCtUserMoveAck(p, cid, key, channel, map_id,
            pos_x, pos_y, pos_z, party_id);
    co_return;
}

} // namespace tworldsvr::handlers
