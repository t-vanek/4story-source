#include "handlers.h"
#include "../senders/senders.h"
#include "../services/battle_constants.h"
#include "../services/castle_constants.h"
#include "../wire_codec.h"

#include "fourstory/db/co_offload.h"

#include <spdlog/spdlog.h>

#include <cstdint>
#include <ctime>
#include <mutex>
#include <string>

namespace tworldsvr::handlers {

namespace {

// TCONTRY_TYPE (NetCode.h:1091) — only the two the LOCALOCCUPY flip
// needs. D=0, C=1, B=2, N=3, PEACE=4.
constexpr std::uint8_t kCountryB = 2;
constexpr std::uint8_t kCountryN = 3;

std::shared_ptr<PeerSession>
FindMapPeer(const HandlerContext& ctx, std::uint8_t msi)
{
    if (msi == 0 || !ctx.peers) return nullptr;
    for (auto& p : ctx.peers->Snapshot())
        if (static_cast<std::uint8_t>(p->Wid() & 0xFF) == msi)
            return p;
    return nullptr;
}

// Resolve a guild's display name (empty when absent / not wired).
std::string GuildName(const HandlerContext& ctx, std::uint32_t guild_id)
{
    if (!ctx.guilds || guild_id == 0) return {};
    auto g = ctx.guilds->Find(guild_id);
    if (!g) return {};
    std::lock_guard lock(g->lock);
    return g->name;
}

// Clear every member + tactics application to `castle_id` in `guild`
// and tell each affected char's map (legacy CTWorldSvrModule::
// ResetCastleApply). Snapshot the affected char_ids under the guild
// lock, then route the resets via separate char locks.
boost::asio::awaitable<void>
ResetCastleApply(const HandlerContext& ctx, std::uint32_t guild_id,
                 std::uint16_t castle_id)
{
    if (guild_id == 0 || !ctx.guilds || !ctx.chars) co_return;
    auto guild = ctx.guilds->Find(guild_id);
    if (!guild) co_return;

    std::vector<std::uint32_t> affected;
    {
        std::lock_guard g(guild->lock);
        for (auto& m : guild->members)
            if (m.castle == castle_id)
            { m.castle = 0; m.camp = 0; affected.push_back(m.char_id); }
        for (auto& t : guild->tactics_members)
            if (t.castle == castle_id)
            { t.castle = 0; t.camp = 0; affected.push_back(t.id); }
    }

    for (auto cid : affected)
    {
        auto c = ctx.chars->Find(cid);
        if (!c) continue;
        std::uint32_t key = 0; std::uint8_t msi = 0;
        { std::lock_guard g(c->lock); key = c->key; msi = c->main_server_id; }
        if (auto p = FindMapPeer(ctx, msi))
            co_await senders::SendMwCastleApplyReq(p, cid, key,
                castle::kSuccess, /*castle=*/0, /*target=*/cid, /*camp=*/0);
    }

    // Legacy ResetCastleApply persists the cleared rows too
    // (TWorldSvr.cpp:5425 SendDM_CASTLEAPPLY_REQ(0, member, 0)) —
    // repository-collapsed (W6-43).
    if (ctx.war_ops && !affected.empty())
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [repo = ctx.war_ops, affected]
            {
                for (auto cid : affected)
                    repo->SaveCastleApplicant(0, cid, 0);
            });
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

    const std::string name = GuildName(ctx, guild_id);

    // W5-3: the siege is over — clear both guilds' applications to
    // this castle (legacy ResetCastleApply). The StatExp award stays
    // deferred (absent CALCULATE_NEXTGEXP / *_STATEXP constants).
    co_await ResetCastleApply(ctx, guild_id, castle_id);
    if (lose_guild != 0 && lose_guild != guild_id)
        co_await ResetCastleApply(ctx, lose_guild, castle_id);

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

boost::asio::awaitable<void>
OnCastleApplyAck(std::shared_ptr<PeerSession> peer,
                 std::vector<std::byte>       body,
                 const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.guilds || !ctx.peers)
    {
        spdlog::warn("OnCastleApplyAck[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0, target = 0;
    std::uint16_t castle = 0;
    std::uint8_t  camp = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(castle) ||
        !r.Read(target) || !r.Read(camp))
    {
        spdlog::warn("OnCastleApplyAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    auto self = ctx.chars->Find(char_id);
    if (!self) co_return;
    std::uint32_t guild_id = 0;
    {
        std::lock_guard g(self->lock);
        if (self->key != key) co_return;
        guild_id = self->guild_id;
    }
    if (guild_id == 0) co_return;
    auto guild = ctx.guilds->Find(guild_id);
    if (!guild) co_return;

    // Resolve + apply under the guild lock, then route the replies
    // (char locks are taken separately — never held with the guild
    // lock, per the README §5 ordering).
    std::uint16_t eff_castle = castle, prev_castle = 0;
    std::uint8_t  eff_camp = camp;
    std::uint16_t prev_count = 0, new_count = 0;
    bool full = false, changed = false;
    {
        std::lock_guard g(guild->lock);
        if (guild->chief_char_id != char_id) co_return;   // chief-only

        TGuildMember*   m = guild->FindMember(target);
        TTacticsMember* t = nullptr;
        if (!m)
        {
            t = guild->FindTactics(target);
            if (!t) co_return;                 // target not in the guild
        }
        else if (m->tactics != 0)
            co_return;                         // member is a merc elsewhere

        prev_castle = m ? m->castle : t->castle;
        // Re-applying to the same castle cancels the application.
        if (prev_castle == eff_castle) { eff_castle = 0; eff_camp = 0; }

        if (eff_castle != 0 && !guild->CanApplyWar(eff_castle))
            full = true;                       // 49-applicant cap hit
        else if (prev_castle != 0 || eff_castle != 0)
        {
            if (m) { m->castle = eff_castle; m->camp = eff_camp; }
            else   { t->castle = eff_castle; t->camp = eff_camp; }
            changed = true;
            if (prev_castle) prev_count = guild->CastleApplicantCount(prev_castle);
            if (eff_castle)  new_count  = guild->CastleApplicantCount(eff_castle);
        }
    }

    if (full)
    {
        co_await senders::SendMwCastleApplyReq(peer, char_id, key,
            castle::kFull, eff_castle, target, eff_camp);
        co_return;
    }
    if (!changed) co_return;     // no state change → legacy sends nothing

    co_await senders::SendMwCastleApplyReq(peer, char_id, key, castle::kSuccess,
        eff_castle, target, eff_camp);

    if (target != char_id)
        if (auto tc = ctx.chars->Find(target))
        {
            std::uint32_t tkey = 0; std::uint8_t tmsi = 0;
            { std::lock_guard g(tc->lock);
              tkey = tc->key; tmsi = tc->main_server_id; }
            if (auto tp = FindMapPeer(ctx, tmsi))
                co_await senders::SendMwCastleApplyReq(tp, target, tkey,
                    castle::kSuccess, eff_castle, target, eff_camp);
        }

    // Persist the (possibly toggled-to-zero) application — legacy
    // SendDM_CASTLEAPPLY_REQ(wCastle, dwTarget, bCamp) on the success
    // path (SSHandler.cpp:7996), repository-collapsed (W6-43).
    if (ctx.war_ops)
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [repo = ctx.war_ops, eff_castle, target, eff_camp]
            { repo->SaveCastleApplicant(eff_castle, target, eff_camp); });

    // Re-broadcast the applicant count for the vacated + joined castle
    // (legacy NotifyCastleApply).
    auto snapshot = ctx.peers->Snapshot();
    if (prev_castle)
        for (auto& p : snapshot)
            co_await senders::SendMwCastleApplicantCountReq(p, prev_castle,
                guild_id, prev_count);
    if (eff_castle)
        for (auto& p : snapshot)
            co_await senders::SendMwCastleApplicantCountReq(p, eff_castle,
                guild_id, new_count);
    co_return;
}

boost::asio::awaitable<void>
OnBattleStatusReq(std::shared_ptr<PeerSession> peer,
                  std::vector<std::byte>       body,
                  const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.peers)
    {
        spdlog::warn("OnBattleStatusReq[{}]: peers not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint8_t  type = 0, status = 0;
    std::uint32_t start = 0, second = 0;
    if (!r.Read(type) || !r.Read(status) || !r.Read(start) || !r.Read(second))
    {
        spdlog::warn("OnBattleStatusReq[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    // Fan the matching war-window enable to every map peer. The
    // BS_PEACE peace-time bookkeeping (record-date reset +
    // CalcWeekRecord + castle-war-info clear) is deferred — it needs
    // the PvP-record-date + castle-war-info systems. SKYGARDEN
    // (#ifdef) is also deferred.
    for (auto& p : ctx.peers->Snapshot())
    {
        switch (type)
        {
        case battle::kTypeLocal:
            co_await senders::SendMwLocalEnableReq(p, status, second, 0, 0, 0);
            break;
        case battle::kTypeCastle:
            co_await senders::SendMwCastleEnableReq(p, status, second);
            break;
        case battle::kTypeMission:
            co_await senders::SendMwMissionEnableReq(p, status, start, second);
            break;
        default:
            break;
        }
    }
    co_return;
}

boost::asio::awaitable<void>
OnMwEndWarAck(std::shared_ptr<PeerSession> peer,
              std::vector<std::byte>       body,
              const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.peers)
    {
        spdlog::warn("OnMwEndWarAck[{}]: peers not wired", ip);
        co_return;
    }
    wire::Reader r(body.data(), body.size());
    std::uint16_t castle = 0;
    if (!r.Read(castle))
    {
        spdlog::warn("OnMwEndWarAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }
    for (auto& p : ctx.peers->Snapshot())
        co_await senders::SendMwEndWarReq(p, castle);
    co_return;
}

boost::asio::awaitable<void>
OnMwSkyGardenOccupyAck(std::shared_ptr<PeerSession> peer,
                       std::vector<std::byte>       body,
                       const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.peers)
    {
        spdlog::warn("OnMwSkyGardenOccupyAck[{}]: peers not wired", ip);
        co_return;
    }
    wire::Reader r(body.data(), body.size());
    std::uint8_t  type = 0, country = 0;
    std::uint16_t id = 0;
    if (!r.Read(type) || !r.Read(id) || !r.Read(country))
    {
        spdlog::warn("OnMwSkyGardenOccupyAck[{}]: short body ({} bytes)",
            ip, body.size());
        co_return;
    }
    for (auto& p : ctx.peers->Snapshot())
        co_await senders::SendMwSkyGardenOccupyReq(p, type, id, country);
    co_return;
}

boost::asio::awaitable<void>
OnMwWarCountryBalanceAck(std::shared_ptr<PeerSession> peer,
                         std::vector<std::byte>       body,
                         const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.war_index)
    {
        spdlog::warn("OnMwWarCountryBalanceAck[{}]: chars/war_index "
                     "not wired", ip);
        co_return;
    }
    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    if (!r.Read(char_id) || !r.Read(key))
    {
        spdlog::warn("OnMwWarCountryBalanceAck[{}]: short body "
                     "({} bytes)", ip, body.size());
        co_return;
    }

    auto c = ctx.chars->Find(char_id);
    if (!c) co_return;
    std::uint8_t level = 0;
    {
        std::lock_guard g(c->lock);
        if (c->key != key) co_return;
        level = c->level;
    }
    const std::uint8_t gap = WarCountryGapOf(level);
    if (gap >= kWarCountryMaxGap)
        co_return;                    // legacy: no reply out of range

    co_await senders::SendMwWarCountryBalanceReq(peer, char_id, key,
        static_cast<std::uint32_t>(ctx.war_index->Count(0, gap)),
        static_cast<std::uint32_t>(ctx.war_index->Count(1, gap)),
        gap);
    co_return;
}

boost::asio::awaitable<void>
OnCtCastleGuildChgReq(std::shared_ptr<PeerSession> peer,
                      std::vector<std::byte>       body,
                      const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.guilds || !ctx.peers)
    {
        spdlog::warn("OnCtCastleGuildChgReq[{}]: registries not wired",
            ip);
        co_return;
    }
    wire::Reader r(body.data(), body.size());
    std::uint16_t castle = 0;
    std::uint32_t def_id = 0, atk_id = 0, manager = 0;
    std::int64_t  when = 0;
    if (!r.Read(castle) || !r.Read(def_id) || !r.Read(atk_id) ||
        !r.Read(manager) || !r.Read(when))
    {
        spdlog::warn("OnCtCastleGuildChgReq[{}]: short body ({} bytes)",
            ip, body.size());
        co_return;
    }

    std::string def_name, atk_name;
    if (auto g = ctx.guilds->Find(def_id))
    {
        std::lock_guard lk(g->lock);
        def_name = g->name;
    }
    if (auto g = ctx.guilds->Find(atk_id))
    {
        std::lock_guard lk(g->lock);
        atk_name = g->name;
    }

    // Legacy short-fails when either guild is unknown - the ACK still
    // carries the full field layout with defaults (SSSender.cpp:3101
    // has default args; the wire always has 8 fields).
    if (def_name.empty() || atk_name.empty())
    {
        co_await senders::SendCtCastleGuildChgAck(peer, manager,
            /*ret=*/0, 0, 0, "", 0, "", 0);
        co_return;
    }

    for (auto& p : ctx.peers->Snapshot())
        co_await senders::SendMwCastleGuildChgReq(p, castle, def_id,
            def_name, atk_id, atk_name, when);

    co_await senders::SendCtCastleGuildChgAck(peer, manager, /*ret=*/1,
        castle, def_id, def_name, atk_id, atk_name, when);
    co_return;
}

boost::asio::awaitable<void>
RefreshWarCountryIndex(const HandlerContext& ctx)
{
    if (!ctx.war_index || !ctx.war_ops)
        co_return;
    // Legacy prunes entries older than WEEK_ONE before the reload.
    const std::int64_t week = 7 * 24 * 60 * 60;
    const std::int64_t cutoff =
        static_cast<std::int64_t>(std::time(nullptr)) - week;
    auto rows = co_await fourstory::db::CoOffloadIf(ctx.db_pool,
        [repo = ctx.war_ops, cutoff]
        { return repo->LoadActiveChars(cutoff); });
    ctx.war_index->Rebuild(rows);
    spdlog::info("RefreshWarCountryIndex: {} active char(s) indexed",
        ctx.war_index->TotalSize());
    co_return;
}

} // namespace tworldsvr::handlers
