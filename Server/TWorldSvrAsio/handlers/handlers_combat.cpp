#include "handlers.h"
#include "../senders/senders.h"
#include "../wire_codec.h"

#include <spdlog/spdlog.h>

#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>

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

// Route key + main-server for a char (0 msi if absent).
struct Route { bool found = false; std::uint32_t key = 0; std::uint8_t msi = 0; };
Route RouteOf(const HandlerContext& ctx, std::uint32_t char_id)
{
    Route r;
    if (!ctx.chars) return r;
    auto c = ctx.chars->Find(char_id);
    if (!c) return r;
    std::lock_guard g(c->lock);
    r.found = true; r.key = c->key; r.msi = c->main_server_id;
    return r;
}

} // namespace

boost::asio::awaitable<void>
OnMagicMirrorAck(std::shared_ptr<PeerSession> peer,
                 std::vector<std::byte>       body,
                 const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers)
    {
        spdlog::warn("OnMagicMirrorAck[{}]: registries not wired", ip);
        co_return;
    }

    // Body: host_id, attack_id, target_id, attack_type, target_type.
    // Routed to the attacker's map; forwarded verbatim.
    wire::Reader r(body.data(), body.size());
    std::uint32_t host_id = 0, attack_id = 0;
    if (!r.Read(host_id) || !r.Read(attack_id))
    {
        spdlog::warn("OnMagicMirrorAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    auto rt = RouteOf(ctx, attack_id);
    if (!rt.found) co_return;
    if (auto p = FindMapPeer(ctx, rt.msi))
        co_await senders::SendMwMagicMirrorReq(p, body);
    co_return;
}

boost::asio::awaitable<void>
OnMonTemptAck(std::shared_ptr<PeerSession> peer,
              std::vector<std::byte>       body,
              const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers)
    {
        spdlog::warn("OnMonTemptAck[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t atk_id = 0;
    std::uint16_t mon_id = 0;
    if (!r.Read(atk_id) || !r.Read(mon_id))
    {
        spdlog::warn("OnMonTemptAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    auto rt = RouteOf(ctx, atk_id);
    if (!rt.found) co_return;
    if (auto p = FindMapPeer(ctx, rt.msi))
        co_await senders::SendMwMonTemptReq(p, atk_id, rt.key, mon_id);
    co_return;
}

boost::asio::awaitable<void>
OnMonTemptEvoAck(std::shared_ptr<PeerSession> peer,
                 std::vector<std::byte>       body,
                 const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers)
    {
        spdlog::warn("OnMonTemptEvoAck[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t atk_id = 0, host_id = 0;
    std::uint8_t  host_type = 0;
    if (!r.Read(atk_id) || !r.Read(host_id) || !r.Read(host_type))
    {
        spdlog::warn("OnMonTemptEvoAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    auto rt = RouteOf(ctx, atk_id);
    if (!rt.found) co_return;
    if (auto p = FindMapPeer(ctx, rt.msi))
        co_await senders::SendMwMonTemptEvoReq(p, atk_id, rt.key, host_id,
            host_type);
    co_return;
}

// MONSTERDIE / TAKEMONMONEY — the map asked world about a monster
// result; route it back to the char's main map (found by id+key),
// forwarding the body verbatim.
namespace {
boost::asio::awaitable<void>
RouteMonResult(std::string_view tag, std::shared_ptr<PeerSession> peer,
               std::vector<std::byte> body, const HandlerContext& ctx,
               bool die)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers)
    {
        spdlog::warn("{}[{}]: registries not wired", tag, ip);
        co_return;
    }
    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    if (!r.Read(char_id) || !r.Read(key))
    {
        spdlog::warn("{}[{}]: short body ({} bytes)", tag, ip, body.size());
        co_return;
    }
    auto rt = RouteOf(ctx, char_id);
    if (!rt.found || rt.key != key) co_return;
    if (auto p = FindMapPeer(ctx, rt.msi))
    {
        if (die) co_await senders::SendMwMonsterDieReq(p, body);
        else     co_await senders::SendMwTakeMonMoneyReq(p, body);
    }
}
} // namespace

boost::asio::awaitable<void>
OnMonsterDieAck(std::shared_ptr<PeerSession> peer,
                std::vector<std::byte>       body,
                const HandlerContext&        ctx)
{
    co_await RouteMonResult("OnMonsterDieAck", std::move(peer), std::move(body),
                            ctx, /*die=*/true);
}

boost::asio::awaitable<void>
OnTakeMonMoneyAck(std::shared_ptr<PeerSession> peer,
                  std::vector<std::byte>       body,
                  const HandlerContext&        ctx)
{
    co_await RouteMonResult("OnTakeMonMoneyAck", std::move(peer),
                            std::move(body), ctx, /*die=*/false);
}

} // namespace tworldsvr::handlers
