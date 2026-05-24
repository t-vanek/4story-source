#include "handlers.h"
#include "../senders/senders.h"
#include "../wire_codec.h"

#include <spdlog/spdlog.h>

#include <cstdint>
#include <mutex>
#include <string>

namespace tworldsvr::handlers {

namespace {

// TCONTRY_TYPE (NetCode.h:1091) — only the two the LOCALOCCUPY flip
// needs. D=0, C=1, B=2, N=3, PEACE=4.
constexpr std::uint8_t kCountryB = 2;
constexpr std::uint8_t kCountryN = 3;

// Resolve a guild's display name (empty when absent / not wired).
std::string GuildName(const HandlerContext& ctx, std::uint32_t guild_id)
{
    if (!ctx.guilds || guild_id == 0) return {};
    auto g = ctx.guilds->Find(guild_id);
    if (!g) return {};
    std::lock_guard lock(g->lock);
    return g->name;
}

} // namespace

boost::asio::awaitable<void>
OnCastleOccupyAck(std::shared_ptr<PeerSession> peer,
                  std::vector<std::byte>       body,
                  const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.peers)
    {
        spdlog::warn("OnCastleOccupyAck[{}]: peers not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint8_t  type = 0, country = 0;
    std::uint16_t castle_id = 0;
    std::uint32_t guild_id = 0, lose_guild = 0;
    if (!r.Read(type) || !r.Read(castle_id) || !r.Read(guild_id) ||
        !r.Read(country) || !r.Read(lose_guild))
    {
        spdlog::warn("OnCastleOccupyAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    // Deferred (blocked on the absent CALCULATE_NEXTGEXP / *_STATEXP
    // constants + the castle-apply model): the winning guild's
    // ResetCastleApply + StatExp award, and the loser's ResetCastleApply.
    const std::string name = GuildName(ctx, guild_id);

    for (auto& p : ctx.peers->Snapshot())
        co_await senders::SendMwCastleOccupyReq(p, type, castle_id, guild_id,
            country, name);
    co_return;
}

boost::asio::awaitable<void>
OnLocalOccupyAck(std::shared_ptr<PeerSession> peer,
                 std::vector<std::byte>       body,
                 const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.peers)
    {
        spdlog::warn("OnLocalOccupyAck[{}]: peers not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint8_t  type = 0, country = 0, cur_country = 0;
    std::uint16_t local_id = 0;
    std::uint32_t guild_id = 0;
    if (!r.Read(type) || !r.Read(local_id) || !r.Read(country) ||
        !r.Read(guild_id) || !r.Read(cur_country))
    {
        spdlog::warn("OnLocalOccupyAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    // StatExp award deferred (missing constants). Resolve the guild
    // for the name + the B-country display flip.
    std::string  name;
    std::uint8_t guild_country = 0;
    bool         have_guild = false;
    if (ctx.guilds && guild_id != 0)
        if (auto g = ctx.guilds->Find(guild_id))
        {
            std::lock_guard lock(g->lock);
            name = g->name;
            guild_country = g->country;
            have_guild = true;
        }

    // A B-country guild's capture shows as the opposing flag and is
    // reported guild-less (legacy OnMW_LOCALOCCUPY_ACK).
    if (have_guild && guild_country == kCountryB)
    {
        if (cur_country != kCountryN)
            country = country ? 0 : 1;
        guild_id = 0;
    }

    for (auto& p : ctx.peers->Snapshot())
        co_await senders::SendMwLocalOccupyReq(p, type, local_id, country,
            guild_id, name);
    co_return;
}

boost::asio::awaitable<void>
OnMissionOccupyAck(std::shared_ptr<PeerSession> peer,
                   std::vector<std::byte>       body,
                   const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.peers)
    {
        spdlog::warn("OnMissionOccupyAck[{}]: peers not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint8_t  type = 0, country = 0;
    std::uint16_t local_id = 0;
    if (!r.Read(type) || !r.Read(local_id) || !r.Read(country))
    {
        spdlog::warn("OnMissionOccupyAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    for (auto& p : ctx.peers->Snapshot())
        co_await senders::SendMwMissionOccupyReq(p, type, local_id, country);
    co_return;
}

} // namespace tworldsvr::handlers
