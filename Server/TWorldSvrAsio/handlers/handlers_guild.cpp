#include "handlers.h"
#include "../senders/senders.h"
#include "../services/guild_broadcast.h"
#include "../services/guild_cabinet_codec.h"
#include "../services/guild_constants.h"
#include "../services/guild_peerage.h"
#include "../services/pvp_aggregate.h"
#include "../wire_codec.h"

#include "MessageId.h"

#include "fourstory/db/co_offload.h"

#include <spdlog/spdlog.h>

#include <cstdint>
#include <ctime>
#include <mutex>
#include <vector>

namespace tworldsvr::handlers {

// --- W6-11: day-change guild ranking -------------------------------
//
// SM_CHANGEDAY_REQ — the scheduler's daily rollover; world recomputes
// every guild's PvP rank (total + month) from the in-memory points
// (legacy CalcGuildRanking, TWorldSvr.cpp:7743). A guild's rank is
// (number of guilds with strictly more points) + 1, counting only
// guilds that have points; pointless guilds rank 0. Ranks are read
// back by OnGuildInfoAck — no reply, no persistence.
boost::asio::awaitable<void>
OnChangeDayReq(std::shared_ptr<PeerSession> peer,
               std::vector<std::byte>       /*body*/,
               const HandlerContext&        ctx)
{
    if (!ctx.guilds) co_return;

    struct G { std::uint32_t id, total, month, rt, rm; };
    std::vector<G> gs;
    for (auto id : ctx.guilds->SnapshotIds())
        if (auto g = ctx.guilds->Find(id))
        {
            std::lock_guard lk(g->lock);
            gs.push_back({id, g->pvp_total_point, g->pvp_month_point, 0, 0});
        }

    for (auto& g : gs)
    {
        if (g.total == 0 && g.month == 0) continue;   // unranked
        for (const auto& c : gs)
        {
            if (c.total == 0 && c.month == 0) continue;
            if (g.total && g.total < c.total) ++g.rt;
            if (g.month && g.month < c.month) ++g.rm;
        }
        if (g.total) ++g.rt;
        if (g.month) ++g.rm;
    }

    for (const auto& g : gs)
        if (auto gd = ctx.guilds->Find(g.id))
        {
            std::lock_guard lk(gd->lock);
            gd->rank_total = g.rt;
            gd->rank_month = g.rm;
        }

    spdlog::info("OnChangeDayReq[{}]: recomputed ranking for {} guild(s)",
        peer->Wire()->RemoteIPv4(), gs.size());
    co_return;
}

namespace {

bool SkipCabinet(wire::Reader&, std::uint16_t count, const std::string& ip)
{
    if (count == 0) return true;
    spdlog::info("OnGuildLoadAck[{}]: cabinet wCount={} — skipping items "
                 "(W3a-4b will parse them)", ip, count);
    return true;
}

// File-scope helper struct used by multiple W3a-* handler blocks
// (W3a-8 articles + W3a-12 volunteer reply). Locking the
// requester's guild + validating gates is the same pattern
// across blocks; keeping the type definition in the top
// anonymous namespace lets each block's forward-declared
// ResolveRequesterGuild use it without redefinition.
struct GuildHandle
{
    std::shared_ptr<TGuild> guild;
    std::uint32_t           guild_id = 0;
};

} // namespace

boost::asio::awaitable<void>
OnGuildLoadAck(std::shared_ptr<PeerSession>  peer,
               std::vector<std::byte>        body,
               const HandlerContext&         ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();

    if (!ctx.chars || !ctx.guilds)
    {
        spdlog::warn("OnGuildLoadAck[{}]: char/guild registry not wired "
                     "— dropped", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id  = 0;
    std::uint32_t key      = 0;
    std::uint32_t guild_id = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(guild_id))
    {
        spdlog::warn("OnGuildLoadAck[{}]: short body ({} bytes) — dropped",
            ip, body.size());
        co_return;
    }

    auto tchar = ctx.chars->Find(char_id);
    if (!tchar)
    {
        spdlog::warn("OnGuildLoadAck[{}]: char_id={} not in registry — "
                     "dropped (legacy FindTChar miss)", ip, char_id);
        co_return;
    }
    std::uint8_t char_country = 0;
    std::string  char_name;
    {
        std::lock_guard g(tchar->lock);
        if (tchar->key != key)
        {
            spdlog::warn("OnGuildLoadAck[{}]: char_id={} key mismatch "
                         "(registry=0x{:08X} incoming=0x{:08X}) — dropped",
                ip, char_id, tchar->key, key);
            co_return;
        }
        // TODO W3a-3: tchar->country, tchar->name (need CHANGECHARBASE).
        char_country = 0;
        char_name    = std::string{};
    }

    if (ctx.guilds->Find(guild_id))
    {
        spdlog::debug("OnGuildLoadAck[{}]: guild_id={} already loaded — "
                      "ignored", ip, guild_id);
        co_return;
    }

    auto guild = std::make_shared<TGuild>();
    guild->id            = guild_id;
    guild->chief_char_id = char_id;
    guild->chief_name    = char_name;
    guild->country       = char_country;

    std::uint16_t cabinet_count = 0;
    std::int64_t  establish_t64 = 0;
    if (!r.ReadString(guild->name) ||
        !r.Read(guild->fame) ||
        !r.Read(guild->fame_color) ||
        !r.Read(guild->max_cabinet) ||
        !r.Read(guild->guild_points) ||
        !r.Read(guild->level) ||
        !r.Read(guild->chief_char_id) ||
        !r.Read(guild->exp) ||
        !r.Read(guild->gi) ||
        !r.Read(guild->status) ||
        !r.Read(guild->gold) ||
        !r.Read(guild->silver) ||
        !r.Read(guild->cooper) ||
        !r.Read(guild->disorg) ||
        !r.Read(guild->disorg_time) ||
        !r.Read(establish_t64) ||
        !r.Read(guild->pvp_total_point) ||
        !r.Read(guild->pvp_useable_point) ||
        !r.Read(cabinet_count))
    {
        spdlog::warn("OnGuildLoadAck[{}]: truncated guild row body — "
                     "dropped", ip);
        co_return;
    }
    guild->establish_time = establish_t64;

    if (!SkipCabinet(r, cabinet_count, ip))
    {
        spdlog::warn("OnGuildLoadAck[{}]: malformed cabinet body — "
                     "dropping guild", ip);
        co_return;
    }

    TGuildMember chief;
    chief.char_id  = char_id;
    chief.guild_id = guild_id;
    chief.duty     = guild::kDutyChief;
    chief.peer     = 0;                       // GUILD_PEER_NONE
    chief.name     = char_name;

    // Capture the values we need for the reply before the move into
    // the registry empties our local copy.
    const std::string reply_name = guild->name;

    guild->members.push_back(std::move(chief));

    if (!ctx.guilds->Insert(guild))
    {
        spdlog::info("OnGuildLoadAck[{}]: lost insert race for guild_id={}",
            ip, guild_id);
        co_return;
    }

    // W3a-4: link the founding char to its guild via the new
    // TChar.guild_id back-pointer. Legacy sets pTCHAR->m_pGuild;
    // we keep just the id and let the GuildRegistry hold the
    // strong ref to TGuild. Mirrors SSHandler.cpp:9013.
    {
        std::lock_guard g(tchar->lock);
        tchar->guild_id = guild_id;
    }

    spdlog::info("OnGuildLoadAck[{}]: guild_id={} name='{}' level={} "
                 "members={} fame={} — registered (total={}), "
                 "char_id={} linked",
        ip, guild_id, reply_name, guild->level, guild->members.size(),
        guild->fame, ctx.guilds->Size(), char_id);

    // W3a-2: complete the legacy round-trip by ACKing the map server
    // that the guild is now world-side registered. Result code
    // kSuccess + bEstablish=0 (this is a load, not a create).
    // Legacy: SSHandler.cpp:9019.
    co_await senders::SendMwGuildEstablishReq(
        peer, char_id, key, guild::kSuccess, guild_id,
        reply_name, /*establish=*/0);
}

boost::asio::awaitable<void>
OnGuildLeaveAck(std::shared_ptr<PeerSession>  peer,
                std::vector<std::byte>        body,
                const HandlerContext&         ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();

    if (!ctx.chars || !ctx.guilds)
    {
        spdlog::warn("OnGuildLeaveAck[{}]: char/guild registry not wired",
            ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0;
    std::uint32_t key     = 0;
    if (!r.Read(char_id) || !r.Read(key))
    {
        spdlog::warn("OnGuildLeaveAck[{}]: short body ({} bytes) — dropped",
            ip, body.size());
        co_return;
    }

    // Legacy gate (SSHandler.cpp:3581-3589):
    //   FindTChar(char_id, key) → drop on miss
    //   pTCHAR->m_pGuild == nullptr → drop (char isn't in a guild)
    //   FindMember(char_id) miss → drop (member-list desync)
    auto tchar = ctx.chars->Find(char_id);
    if (!tchar)
    {
        spdlog::info("OnGuildLeaveAck[{}]: char_id={} not in registry — "
                     "dropped", ip, char_id);
        co_return;
    }

    // Snapshot the per-char fields we need under the lock, then
    // release before touching the guild lock. Locking order:
    // char lock first → guild lock second (we never hold both at
    // once, avoiding deadlock with handlers that lock the guild
    // first and then a member's char).
    std::uint32_t guild_id   = 0;
    std::uint32_t actual_key = 0;
    std::string   char_name;
    {
        std::lock_guard g(tchar->lock);
        actual_key = tchar->key;
        guild_id   = tchar->guild_id;
        char_name  = tchar->name;
    }
    if (actual_key != key)
    {
        spdlog::warn("OnGuildLeaveAck[{}]: char_id={} key mismatch — "
                     "dropped", ip, char_id);
        co_return;
    }
    if (guild_id == 0)
    {
        spdlog::info("OnGuildLeaveAck[{}]: char_id={} has no guild — "
                     "dropped", ip, char_id);
        co_return;
    }

    auto guild = ctx.guilds->Find(guild_id);
    if (!guild)
    {
        // Char carries a guild_id but the guild was unloaded —
        // self-heal by clearing the back-pointer + replying so
        // the client at least observes the leave.
        spdlog::warn("OnGuildLeaveAck[{}]: char_id={} carried stale "
                     "guild_id={} (guild not in registry) — clearing "
                     "back-pointer", ip, char_id, guild_id);
        std::lock_guard g(tchar->lock);
        tchar->guild_id = 0;
    }
    else
    {
        // Remove the member under the guild's lock. RemoveMember
        // returns false if the member is already gone (e.g. a
        // double-leave from a flapping client); legacy treats
        // that as benign too.
        bool removed = false;
        std::size_t remaining = 0;
        {
            std::lock_guard g(guild->lock);
            removed = guild->RemoveMember(char_id);
            remaining = guild->members.size();
        }
        if (!removed)
        {
            spdlog::info("OnGuildLeaveAck[{}]: char_id={} not in "
                         "guild_id={} member list (legacy benign drop)",
                ip, char_id, guild_id);
        }
        else
        {
            spdlog::info("OnGuildLeaveAck[{}]: char_id={} left "
                         "guild_id={} (members remaining: {})",
                ip, char_id, guild_id, remaining);
        }

        // Always clear the back-pointer so a follow-up leave from
        // a stale broker is a no-op (and OnEnterCharReq sees no
        // guild).
        std::lock_guard g(tchar->lock);
        tchar->guild_id = 0;
    }

    // Forward the leave back to the requesting peer. Legacy
    // m_timeCurrent is a Unix epoch second the WorkThread refreshes
    // each tick; we just sample std::time(nullptr) here — it's
    // close enough for the 1-second-granularity guild log.
    const std::uint32_t now = static_cast<std::uint32_t>(std::time(nullptr));
    co_await senders::SendMwGuildLeaveReq(peer, char_id, key, char_name,
        guild::kLeaveSelf, now);

    // W3a-4d: persist the leave to TGUILDMEMBERTABLE off the
    // io_context thread. Closes the remaining W-1 work from
    // PATCH_README §6 — when ctx.db_pool is non-null the SOCI
    // DELETE runs on a worker; when null (dev / test runs) the
    // lambda runs in-line on the current coroutine thread.
    if (ctx.guild_repo && guild_id != 0)
    {
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [repo = ctx.guild_repo, char_id, guild_id] {
                repo->RemoveMember(char_id, guild_id);
            });
    }

    // TODO W3a-4c (broadcast): notify other peers so the chat
    // window updates on every map server, not just the one this
    // request came in on.
    co_return;
}

// --- W3a-4b mutating guild handlers ---------------------------------

boost::asio::awaitable<void>
OnGuildDisorganizationReq(std::shared_ptr<PeerSession> peer,
                          std::vector<std::byte>       body,
                          const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.guilds)
    {
        spdlog::warn("OnGuildDisorganizationReq[{}]: registries not wired",
            ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0, guild_id = 0;
    std::uint8_t  disorg = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(guild_id) ||
        !r.Read(disorg))
    {
        spdlog::warn("OnGuildDisorganizationReq[{}]: short body ({} bytes)",
            ip, body.size());
        co_return;
    }

    // Legacy SSHandler.cpp:3216 — dwTime is m_timeCurrent on
    // start-disband, 0 on cancel. The cancel branch zeroes the
    // disorg countdown so the periodic GUILDEXTINCTION sweep
    // (W3a-5+) won't reap the guild.
    const std::uint32_t now =
        static_cast<std::uint32_t>(std::time(nullptr));
    const std::uint32_t time_unix = disorg ? now : 0;

    // Persist first (legacy fires the CSP synchronously from the
    // DM_ handler then forwards the ACK); fall through to the
    // in-memory mutation either way so the cluster state stays
    // consistent with the legacy WorkThread → BatchThread fan-out
    // semantics — the registry is the operator-visible truth and
    // the DB write is best-effort. W3a-4d offloads to db_pool.
    if (ctx.guild_repo)
    {
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [repo = ctx.guild_repo, guild_id, disorg, time_unix] {
                repo->SetDisorg(guild_id, disorg, time_unix);
            });
    }

    auto guild = ctx.guilds->Find(guild_id);
    if (guild)
    {
        std::lock_guard g(guild->lock);
        guild->disorg      = disorg;
        guild->disorg_time = time_unix;
    }
    else
    {
        spdlog::warn("OnGuildDisorganizationReq[{}]: guild_id={} not in "
                     "registry (DB write happened but no in-memory flip)",
            ip, guild_id);
    }

    spdlog::info("OnGuildDisorganizationReq[{}]: char_id={} guild_id={} "
                 "disorg={} time={}", ip, char_id, guild_id, disorg,
        time_unix);

    co_await senders::SendMwGuildDisorganizationReq(peer, char_id, key,
        disorg);

    // TODO W3a-5+: SM_GUILDDISORGANIZATION_REQ → cluster-wide
    // disorg-countdown sweeper that fires GUILDEXTINCTION when
    // the countdown elapses. Lives in the timer-thread family.
}

boost::asio::awaitable<void>
OnGuildDutyAck(std::shared_ptr<PeerSession> peer,
               std::vector<std::byte>       body,
               const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.guilds)
    {
        spdlog::warn("OnGuildDutyAck[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    std::string   target_name;
    std::uint8_t  new_duty = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.ReadString(target_name) ||
        !r.Read(new_duty))
    {
        spdlog::warn("OnGuildDutyAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    // Validate requesting char + their guild (legacy gates at
    // SSHandler.cpp:3423-3431).
    auto requester = ctx.chars->Find(char_id);
    if (!requester)
    {
        spdlog::info("OnGuildDutyAck[{}]: char_id={} not in registry",
            ip, char_id);
        co_return;
    }
    std::uint32_t guild_id = 0;
    std::uint32_t actual_key = 0;
    {
        std::lock_guard g(requester->lock);
        actual_key = requester->key;
        guild_id   = requester->guild_id;
    }
    if (actual_key != key || guild_id == 0) co_return;

    auto guild = ctx.guilds->Find(guild_id);
    if (!guild) co_return;

    // Locate the target by name. The legacy code uses TGuild::
    // FindMember(name); we don't index members by name within
    // TGuild (members vector is small — linear scan under the
    // guild lock is fine).
    std::uint32_t target_char_id = 0;
    std::uint8_t  old_duty       = 0;
    bool          gate_failed    = false;
    {
        std::lock_guard g(guild->lock);
        if (guild->disorg) { co_return; }
        TGuildMember* tgt = nullptr;
        for (auto& m : guild->members)
        {
            if (m.name == target_name) { tgt = &m; break; }
        }
        if (!tgt) co_return;
        if (tgt->duty == new_duty ||
            tgt->char_id == char_id)
        {
            // Same duty / promoting self → silent drop (legacy
            // parity SSHandler.cpp:3435-3438).
            co_return;
        }

        // Vice-chief cap: legacy refuses if there'd be 2 vice-chiefs.
        if (new_duty == guild::kDutyViceChief)
        {
            std::size_t vice = 0;
            for (const auto& m : guild->members)
                if (m.duty == guild::kDutyViceChief) ++vice;
            if (vice >= 2) { gate_failed = true; co_return; }
        }

        old_duty = tgt->duty;
        // Chief promotion: the current chief drops to kDutyNone
        // before the target takes over (legacy Designate at line
        // 3459). The target's duty assignment happens just below.
        if (new_duty == guild::kDutyChief)
        {
            if (auto* current_chief = guild->FindMember(char_id))
                current_chief->duty = guild::kDutyNone;
        }
        tgt->duty      = new_duty;
        target_char_id = tgt->char_id;
    }

    if (gate_failed) co_return;

    // Persist (best-effort, legacy WorkThread → BatchThread).
    // W3a-4d: offloaded to db_pool so the SOCI UPDATE doesn't
    // hold the io_context. For chief promotion we do both writes
    // back-to-back on the worker — two roundtrips are still
    // cheaper than blocking the reactor.
    if (ctx.guild_repo)
    {
        const bool is_chief = (new_duty == guild::kDutyChief);
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [repo = ctx.guild_repo, target_char_id, guild_id, new_duty,
             char_id, is_chief] {
                repo->UpdateMemberDuty(target_char_id, guild_id, new_duty);
                if (is_chief)
                    repo->UpdateMemberDuty(char_id, guild_id,
                        guild::kDutyNone);
            });
    }

    // Notify the requesting peer.
    co_await senders::SendMwGuildDutyReq(peer, char_id, key, target_name,
        new_duty);

    spdlog::info("OnGuildDutyAck[{}]: char_id={} promoted target='{}' "
                 "(char_id={}) {} → {} in guild_id={}",
        ip, char_id, target_name, target_char_id, old_duty, new_duty,
        guild_id);

    // TODO W3a-4c: route MW_GUILDDUTY_REQ to the target's own main
    // map peer too so a target who's online elsewhere sees the
    // change immediately. Same pattern as the W3a-4b fame
    // broadcast below.
    co_return;
}

boost::asio::awaitable<void>
OnGuildFameAck(std::shared_ptr<PeerSession> peer,
               std::vector<std::byte>       body,
               const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.guilds || !ctx.peers)
    {
        spdlog::warn("OnGuildFameAck[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0, fame = 0, fame_color = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(fame) ||
        !r.Read(fame_color))
    {
        spdlog::warn("OnGuildFameAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    auto requester = ctx.chars->Find(char_id);
    if (!requester) co_return;
    std::uint32_t guild_id = 0, actual_key = 0;
    {
        std::lock_guard g(requester->lock);
        actual_key = requester->key;
        guild_id   = requester->guild_id;
    }
    if (actual_key != key || guild_id == 0) co_return;

    auto guild = ctx.guilds->Find(guild_id);
    if (!guild) co_return;

    // Snapshot the values we need (members + PvP point budget +
    // current fame) under the guild lock. Releasing before the
    // SendPacket broadcast keeps the lock-hold window tight.
    std::vector<std::uint32_t> member_char_ids;
    std::uint32_t old_fame = 0, old_fame_color = 0;
    bool          insufficient_points = false;
    bool          unchanged           = false;
    {
        std::lock_guard g(guild->lock);
        if (guild->disorg) co_return;
        if (guild->pvp_useable_point < guild::kPvPointCostFameChange)
        {
            insufficient_points = true;
        }
        else if (guild->fame == fame && guild->fame_color == fame_color)
        {
            unchanged = true;
        }
        else
        {
            old_fame       = guild->fame;
            old_fame_color = guild->fame_color;
            guild->pvp_useable_point -= guild::kPvPointCostFameChange;
            guild->fame       = fame;
            guild->fame_color = fame_color;
            member_char_ids.reserve(guild->members.size());
            for (const auto& m : guild->members)
                member_char_ids.push_back(m.char_id);
        }
    }

    if (insufficient_points)
    {
        co_await senders::SendMwGuildFameReq(peer, char_id, key,
            guild::kNoPoint, char_id, fame, fame_color);
        spdlog::info("OnGuildFameAck[{}]: char_id={} guild_id={} "
                     "kNoPoint (need={} pts)", ip, char_id, guild_id,
            guild::kPvPointCostFameChange);
        co_return;
    }
    if (unchanged)
    {
        spdlog::debug("OnGuildFameAck[{}]: char_id={} guild_id={} fame "
                      "unchanged (drop)", ip, char_id, guild_id);
        co_return;
    }

    // Persist new fame + the PvP-point deduction. (A dedicated
    // UpdatePvPoint repo call lands with the W3a-5+ PvP-record
    // handlers; for now we rely on the fame UPDATE landing and
    // accept that the PvP budget delta is in-memory only until
    // then.) W3a-4d offloads the UPDATE.
    if (ctx.guild_repo)
    {
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [repo = ctx.guild_repo, guild_id, fame, fame_color] {
                repo->UpdateFame(guild_id, fame, fame_color);
            });
    }

    // Broadcast to every online member via the W3a-4c helper.
    // Mirrors legacy SSHandler.cpp:4391-4405.
    const std::size_t broadcasted = co_await BroadcastToGuildMembers(
        *ctx.chars, *ctx.peers, member_char_ids,
        [originator = char_id, fame, fame_color](
            std::shared_ptr<PeerSession> target,
            std::uint32_t                recipient_char_id,
            std::uint32_t                recipient_key)
            -> boost::asio::awaitable<void>
        {
            co_await senders::SendMwGuildFameReq(target, recipient_char_id,
                recipient_key, guild::kSuccess, originator, fame, fame_color);
        });

    spdlog::info("OnGuildFameAck[{}]: char_id={} guild_id={} fame "
                 "{}→{} broadcast to {}/{} members",
        ip, char_id, guild_id, old_fame, fame, broadcasted,
        member_char_ids.size());
    (void)old_fame_color;
}

// --- W3a-4c additions ----------------------------------------------

boost::asio::awaitable<void>
OnGuildKickoutAck(std::shared_ptr<PeerSession> peer,
                  std::vector<std::byte>       body,
                  const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.guilds)
    {
        spdlog::warn("OnGuildKickoutAck[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    std::string   target_name;
    if (!r.Read(char_id) || !r.Read(key) || !r.ReadString(target_name))
    {
        spdlog::warn("OnGuildKickoutAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    auto requester = ctx.chars->Find(char_id);
    if (!requester) co_return;
    std::uint32_t guild_id = 0, actual_key = 0;
    {
        std::lock_guard g(requester->lock);
        actual_key = requester->key;
        guild_id   = requester->guild_id;
    }
    if (actual_key != key || guild_id == 0) co_return;

    auto guild = ctx.guilds->Find(guild_id);
    if (!guild) co_return;

    // Find the target by name + apply the legacy "officer kick"
    // gate (SSHandler.cpp:3364-3367): a member with any duty
    // (vice-chief, officer) can only be kicked by the chief.
    std::uint32_t target_char_id = 0;
    std::uint8_t  target_duty    = 0;
    std::uint32_t guild_chief    = 0;
    {
        std::lock_guard g(guild->lock);
        guild_chief = guild->chief_char_id;
        for (const auto& m : guild->members)
        {
            if (m.name == target_name)
            {
                target_char_id = m.char_id;
                target_duty    = m.duty;
                break;
            }
        }
    }
    if (target_char_id == 0)
    {
        spdlog::info("OnGuildKickoutAck[{}]: target='{}' not in guild_id={}",
            ip, target_name, guild_id);
        co_return;
    }
    if (target_duty != guild::kDutyNone && guild_chief != char_id)
    {
        spdlog::info("OnGuildKickoutAck[{}]: char_id={} cannot kick "
                     "officer target='{}' (not chief)", ip, char_id,
            target_name);
        co_return;
    }

    // Remove from registry + clear the target's back-pointer if
    // they're online elsewhere. The chief can't be kicked (legacy
    // path drops with no further action).
    {
        std::lock_guard g(guild->lock);
        guild->RemoveMember(target_char_id);
    }
    if (auto target_char = ctx.chars->Find(target_char_id))
    {
        std::lock_guard g(target_char->lock);
        target_char->guild_id = 0;
    }

    // Persist the kickout. Legacy fires CSPGuildKickout from the
    // BatchThread; W3a-4d wraps in CoOffloadVoidIf so the SOCI
    // DELETE runs on a worker thread when db_pool is configured.
    if (ctx.guild_repo)
    {
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [repo = ctx.guild_repo, target_char_id, guild_id] {
                repo->RemoveMember(target_char_id, guild_id);
            });
    }

    // Two MW replies — one to the requesting peer (chief got
    // confirmation), one to the kicked char's main map peer if
    // they're online (so their client sees the leave). Legacy
    // fans both at SSHandler.cpp:3378-3387. The kicked-target
    // broadcast goes through BroadcastToGuildMembers with a
    // single-element list — keeps the routing logic centralised.
    co_await senders::SendMwGuildLeaveReq(peer, char_id, key, target_name,
        guild::kLeaveKick, 0);

    if (ctx.peers)
    {
        std::vector<std::uint32_t> target_only{target_char_id};
        co_await BroadcastToGuildMembers(
            *ctx.chars, *ctx.peers, target_only,
            [target_name](std::shared_ptr<PeerSession> target_peer,
                           std::uint32_t recipient_char_id,
                           std::uint32_t recipient_key)
                -> boost::asio::awaitable<void>
            {
                co_await senders::SendMwGuildLeaveReq(target_peer,
                    recipient_char_id, recipient_key, target_name,
                    guild::kLeaveKick, 0);
            });
    }

    spdlog::info("OnGuildKickoutAck[{}]: char_id={} kicked target='{}' "
                 "(char_id={}) from guild_id={}",
        ip, char_id, target_name, target_char_id, guild_id);
}

boost::asio::awaitable<void>
OnGuildContributionAck(std::shared_ptr<PeerSession> peer,
                       std::vector<std::byte>       body,
                       const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.guilds)
    {
        spdlog::warn("OnGuildContributionAck[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    std::uint32_t exp = 0, gold = 0, silver = 0, cooper = 0, pvp_point = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(exp) || !r.Read(gold) ||
        !r.Read(silver) || !r.Read(cooper) || !r.Read(pvp_point))
    {
        spdlog::warn("OnGuildContributionAck[{}]: short body ({} bytes)",
            ip, body.size());
        co_return;
    }

    auto tchar = ctx.chars->Find(char_id);
    if (!tchar) co_return;

    std::uint32_t guild_id = 0, actual_key = 0;
    {
        std::lock_guard g(tchar->lock);
        actual_key = tchar->key;
        guild_id   = tchar->guild_id;
    }
    if (actual_key != key || guild_id == 0) co_return;

    auto guild = ctx.guilds->Find(guild_id);
    if (!guild) co_return;

    // Max-level guild rejects exp deposits (legacy SSHandler.cpp:
    // 4052-4058) with kFail+0s body. The non-exp portion (money +
    // pvp) is still rejected by the same branch in legacy — it
    // returns NOERROR after the reply.
    std::uint8_t guild_level = 0;
    bool         is_disorg   = false;
    {
        std::lock_guard g(guild->lock);
        guild_level = guild->level;
        is_disorg   = (guild->disorg != 0);
    }
    if (is_disorg) co_return;
    constexpr std::uint8_t kMaxGuildLevel = 10;   // NetCode.h MAX_GUILD_LEVEL
    if (guild_level == kMaxGuildLevel && exp != 0)
    {
        co_await senders::SendMwGuildContributionReq(peer, char_id, key,
            guild::kFail, 0, 0, 0, 0, 0);
        co_return;
    }

    // Apply the delta in-memory. The FakeGuildRepository's
    // IncrementContribution also bumps the in-memory totals (it
    // shares storage with the GuildRegistry in tests), so we
    // gate on the SOCI vs Fake split by checking whether the
    // repo write also mirrored the change.
    //
    // For W3a-4c the simpler shape is: write to the registry
    // first (under lock), then call the repo for persistence.
    // Legacy's CTGuild::Contribution caps via MAX_GUILD_CONTRIBUTION
    // / MIN_GUILD_CONTRIBUTION; W3a-4c skips those caps (TODO
    // when guild-level cache lands — the per-level caps come
    // from m_pTLEVEL).
    {
        std::lock_guard g(guild->lock);
        guild->exp    += exp;
        guild->gold   += gold;
        guild->silver += silver;
        guild->cooper += cooper;
        guild->pvp_total_point   += pvp_point;
        guild->pvp_useable_point += pvp_point;
        if (auto* m = guild->FindMember(char_id))
            m->service += exp;
    }

    if (ctx.guild_repo)
    {
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [repo = ctx.guild_repo, char_id, guild_id, exp, gold, silver,
             cooper, pvp_point] {
                repo->IncrementContribution(char_id, guild_id, exp, gold,
                    silver, cooper, pvp_point);
            });
    }

    co_await senders::SendMwGuildContributionReq(peer, char_id, key,
        guild::kSuccess, exp, gold, silver, cooper, pvp_point);

    spdlog::info("OnGuildContributionAck[{}]: char_id={} guild_id={} "
                 "exp+={} gold+={} silver+={} cooper+={} pvp+={}",
        ip, char_id, guild_id, exp, gold, silver, cooper, pvp_point);
}

boost::asio::awaitable<void>
OnGuildMemberAddReq(std::shared_ptr<PeerSession> peer,
                    std::vector<std::byte>       body,
                    const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();

    wire::Reader r(body.data(), body.size());
    std::uint32_t guild_id = 0, char_id = 0;
    std::uint8_t  level = 0, duty = 0;
    if (!r.Read(guild_id) || !r.Read(char_id) ||
        !r.Read(level) || !r.Read(duty))
    {
        spdlog::warn("OnGuildMemberAddReq[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    // Pure DB persistence — the in-memory member-add happens on
    // the OnMW_GUILDINVITEANSWER_ACK side (W3a-5+). Legacy fires
    // CSPGuildMemberAdd from the BatchThread and never replies;
    // W3a-4d wraps the call in CoOffloadVoidIf so the INSERT
    // runs on the worker pool.
    if (ctx.guild_repo)
    {
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [repo = ctx.guild_repo, char_id, guild_id, level, duty] {
                repo->AddMember(char_id, guild_id, level, duty);
            });
    }

    spdlog::info("OnGuildMemberAddReq[{}]: persisted guild_id={} "
                 "char_id={} level={} duty={}", ip, guild_id, char_id,
        level, duty);
    co_return;
}

// --- W3a-5 additions ----------------------------------------------

boost::asio::awaitable<void>
OnGuildPeerAck(std::shared_ptr<PeerSession> peer,
               std::vector<std::byte>       body,
               const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.guilds)
    {
        spdlog::warn("OnGuildPeerAck[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    std::string   target_name;
    std::uint8_t  new_peer = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.ReadString(target_name) ||
        !r.Read(new_peer))
    {
        spdlog::warn("OnGuildPeerAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    auto requester = ctx.chars->Find(char_id);
    if (!requester) co_return;
    std::uint32_t guild_id = 0, actual_key = 0;
    {
        std::lock_guard g(requester->lock);
        actual_key = requester->key;
        guild_id   = requester->guild_id;
    }
    if (actual_key != key || guild_id == 0) co_return;

    auto guild = ctx.guilds->Find(guild_id);
    if (!guild) co_return;

    // Resolve target + check the CheckPeerage gate under the
    // guild lock so the slot-cap count is consistent with the
    // mutation. Snapshot the values we need outside the lock so
    // CoOffloadVoidIf + SendMwGuildPeerReq don't run with the
    // lock held.
    std::uint32_t target_char_id = 0;
    std::uint8_t  old_peer       = 0;
    std::uint8_t  requester_duty = 0;
    bool          target_missing = false;
    bool          unchanged      = false;
    bool          gate_failed    = false;
    {
        std::lock_guard g(guild->lock);
        if (guild->disorg) co_return;
        const TGuildMember* req_member = guild->FindMember(char_id);
        if (req_member) requester_duty = req_member->duty;

        TGuildMember* tgt = nullptr;
        for (auto& m : guild->members)
            if (m.name == target_name) { tgt = &m; break; }
        if (!tgt) { target_missing = true; }
        else if (tgt->peer == new_peer) { unchanged = true; }
        else
        {
            const TGuildLevelRow* lvl = nullptr;
            if (ctx.guild_levels)
                lvl = ctx.guild_levels->Find(guild->level);
            if (!guild::CheckPeerage(lvl, requester_duty, new_peer, *guild))
            {
                gate_failed    = true;
                old_peer       = tgt->peer;
                target_char_id = tgt->char_id;
            }
            else
            {
                old_peer       = tgt->peer;
                target_char_id = tgt->char_id;
                tgt->peer      = new_peer;
            }
        }
    }

    if (target_missing)
    {
        spdlog::info("OnGuildPeerAck[{}]: target='{}' not in guild_id={}",
            ip, target_name, guild_id);
        co_return;
    }
    if (unchanged)
    {
        spdlog::debug("OnGuildPeerAck[{}]: target='{}' already at peer={} "
                      "— drop", ip, target_name, new_peer);
        co_return;
    }
    if (gate_failed)
    {
        co_await senders::SendMwGuildPeerReq(peer, char_id, key,
            guild::kFail, target_name, new_peer, old_peer);
        spdlog::info("OnGuildPeerAck[{}]: char_id={} CheckPeerage "
                     "refused target='{}' new_peer={} (guild_level={})",
            ip, char_id, target_name, new_peer,
            ctx.guild_levels && ctx.guild_levels->Find(
                guild->level) ? int(guild->level) : -1);
        co_return;
    }

    // Persist + reply to requester. Target's own map gets the
    // notification via BroadcastToGuildMembers (1-element list)
    // when they're online elsewhere.
    if (ctx.guild_repo)
    {
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [repo = ctx.guild_repo, target_char_id, guild_id, new_peer] {
                repo->UpdateMemberPeer(target_char_id, guild_id, new_peer);
            });
    }

    co_await senders::SendMwGuildPeerReq(peer, char_id, key,
        guild::kSuccess, target_name, new_peer, old_peer);

    if (ctx.peers)
    {
        std::vector<std::uint32_t> target_only{target_char_id};
        co_await BroadcastToGuildMembers(
            *ctx.chars, *ctx.peers, target_only,
            [target_name, new_peer, old_peer](
                std::shared_ptr<PeerSession> target_peer,
                std::uint32_t                recipient_char_id,
                std::uint32_t                recipient_key)
                -> boost::asio::awaitable<void>
            {
                co_await senders::SendMwGuildPeerReq(target_peer,
                    recipient_char_id, recipient_key, guild::kSuccess,
                    target_name, new_peer, old_peer);
            });
    }

    spdlog::info("OnGuildPeerAck[{}]: char_id={} target='{}' (char_id={}) "
                 "peer {}→{} in guild_id={}",
        ip, char_id, target_name, target_char_id, old_peer, new_peer,
        guild_id);
}

boost::asio::awaitable<void>
OnGuildCabinetMaxReq(std::shared_ptr<PeerSession> peer,
                     std::vector<std::byte>       body,
                     const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();

    wire::Reader r(body.data(), body.size());
    std::uint32_t guild_id = 0;
    std::uint8_t  max_cabinet = 0;
    if (!r.Read(guild_id) || !r.Read(max_cabinet))
    {
        spdlog::warn("OnGuildCabinetMaxReq[{}]: short body ({} bytes)",
            ip, body.size());
        co_return;
    }

    // Sanity-clamp against the per-level cabinet cap from the
    // guild-level cache. The legacy module trusts the inbound
    // value; we add a soft check because nothing on the wire
    // says the map server validated it. Out-of-range values get
    // clamped to the cap (operator-visible via the log line).
    if (ctx.guild_levels && ctx.guilds)
    {
        if (auto g = ctx.guilds->Find(guild_id))
        {
            std::uint8_t gl_level = 0;
            {
                std::lock_guard glk(g->lock);
                gl_level = g->level;
            }
            if (const auto* lvl = ctx.guild_levels->Find(gl_level))
            {
                if (max_cabinet > lvl->cabinet_count)
                {
                    spdlog::warn("OnGuildCabinetMaxReq[{}]: guild_id={} "
                                 "max_cabinet={} > level cap {} — "
                                 "clamping", ip, guild_id, max_cabinet,
                        lvl->cabinet_count);
                    max_cabinet = lvl->cabinet_count;
                }
            }
        }
    }

    // Update in-memory + persist. The handler is on the DM_ side
    // (pure DB origin in legacy); we still mirror the registry
    // so OnGuildInfoAck reads the right cap on the next refresh.
    if (ctx.guilds)
    {
        if (auto g = ctx.guilds->Find(guild_id))
        {
            std::lock_guard gl(g->lock);
            g->max_cabinet = max_cabinet;
        }
    }
    if (ctx.guild_repo)
    {
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [repo = ctx.guild_repo, guild_id, max_cabinet] {
                repo->UpdateMaxCabinet(guild_id, max_cabinet);
            });
    }

    spdlog::info("OnGuildCabinetMaxReq[{}]: guild_id={} max_cabinet={} "
                 "persisted", ip, guild_id, max_cabinet);
}

// --- W3a-13 PvP point persistence + promote-into-guild helper -----

namespace {

// Shared promotion path used by the W3a-6 InviteAnswer YES branch
// and the W3a-12 VolunteerReply accept branch. Both did the same
// thing: validate target isn't in a guild, validate guild not
// disorg and not full, atomically add member + set
// TChar.guild_id under guild.lock, persist via CoOffloadVoidIf.
// The dual JOIN_REQ broadcast stays at the call site because
// the chief peer differs between paths.
struct PromotionResult
{
    std::uint8_t  result      = 0; // kSuccess / kNotFound / kHaveGuild /
                                   //   kMemberFull
    std::string   guild_name;
    std::uint32_t fame        = 0;
    std::uint32_t fame_color  = 0;
    std::uint8_t  max_member  = 0; // populated on kMemberFull
};

boost::asio::awaitable<PromotionResult>
TryPromoteIntoGuild(const HandlerContext&     ctx,
                    std::shared_ptr<TGuild>   guild,
                    std::uint32_t             guild_id,
                    std::shared_ptr<TChar>    target_char,
                    std::uint32_t             target_char_id,
                    const std::string&        target_name,
                    std::uint8_t              target_level)
{
    PromotionResult out;

    // Snapshot target's current guild state outside the guild
    // lock to keep the actor-model invariant (char.lock first,
    // guild.lock second never both held simultaneously).
    std::uint32_t target_guild = 0;
    {
        std::lock_guard g(target_char->lock);
        target_guild = target_char->guild_id;
    }

    bool collide_disorg = false, collide_have = false,
         collide_full   = false;
    {
        std::lock_guard gl(guild->lock);
        collide_disorg = (guild->disorg != 0);
        if (!collide_disorg)
        {
            out.guild_name = guild->name;
            out.fame       = guild->fame;
            out.fame_color = guild->fame_color;
            if (ctx.guild_levels)
                if (const auto* lvl = ctx.guild_levels->Find(guild->level))
                    out.max_member = lvl->max_count;
            if (target_guild != 0) collide_have = true;
            else if (out.max_member > 0 &&
                     guild->members.size() >= out.max_member)
                collide_full = true;
            else
            {
                TGuildMember m;
                m.char_id  = target_char_id;
                m.guild_id = guild_id;
                m.duty     = guild::kDutyNone;
                m.name     = target_name;
                m.level    = target_level;
                guild->members.push_back(std::move(m));
            }
        }
    }

    if (collide_disorg) { out.result = guild::kNotFound;   co_return out; }
    if (collide_have)   { out.result = guild::kHaveGuild;  co_return out; }
    if (collide_full)   { out.result = guild::kMemberFull; co_return out; }

    // Atomic mutation succeeded — link the back-pointer + persist.
    {
        std::lock_guard g(target_char->lock);
        target_char->guild_id = guild_id;
    }
    if (ctx.guild_repo)
    {
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [repo = ctx.guild_repo, target_char_id, guild_id, target_level] {
                repo->AddMember(target_char_id, guild_id, target_level,
                    guild::kDutyNone);
            });
    }
    out.result = guild::kSuccess;
    co_return out;
}

} // namespace

boost::asio::awaitable<void>
OnGuildPvPointReq(std::shared_ptr<PeerSession> peer,
                  std::vector<std::byte>       body,
                  const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.guilds)
    {
        spdlog::warn("OnGuildPvPointReq[{}]: guild registry not wired",
            ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t guild_id = 0;
    std::uint32_t total_point = 0, useable_point = 0, month_point = 0;
    if (!r.Read(guild_id)      || !r.Read(total_point) ||
        !r.Read(useable_point) || !r.Read(month_point))
    {
        spdlog::warn("OnGuildPvPointReq[{}]: short body ({} bytes)",
            ip, body.size());
        co_return;
    }

    if (auto guild = ctx.guilds->Find(guild_id))
    {
        std::lock_guard gl(guild->lock);
        guild->pvp_total_point   = total_point;
        guild->pvp_useable_point = useable_point;
        guild->pvp_month_point   = month_point;
    }
    else
    {
        spdlog::info("OnGuildPvPointReq[{}]: guild_id={} not in registry "
                     "(DB write only)", ip, guild_id);
    }

    if (ctx.guild_repo)
    {
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [repo = ctx.guild_repo, guild_id, total_point, useable_point,
             month_point] {
                repo->UpdatePvPoints(guild_id, total_point, useable_point,
                    month_point);
            });
    }

    spdlog::info("OnGuildPvPointReq[{}]: guild_id={} total={} useable={} "
                 "month={}", ip, guild_id, total_point, useable_point,
        month_point);
}

// --- W3a-14 DB-side fan-in handlers -------------------------------
//
// Five thin wrappers around existing IGuildRepository methods +
// two new ones (UpdateLevel / LogPointReward). Each receives a
// DB-pushed state change and persists via CoOffloadVoidIf. None
// of them reply on the wire; the DB is authoritative for the
// fields they touch. OnGuildLevelReq defensively updates
// TGuild.level in the registry too — keeping that stale would
// silently break the peerage-gate cap arithmetic.

boost::asio::awaitable<void>
OnGuildDutyReq(std::shared_ptr<PeerSession> peer,
               std::vector<std::byte>       body,
               const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, guild_id = 0;
    std::uint8_t  duty    = 0;
    if (!r.Read(char_id) || !r.Read(guild_id) || !r.Read(duty))
    {
        spdlog::warn("OnGuildDutyReq[{}]: short body ({} bytes)",
            ip, body.size());
        co_return;
    }
    if (ctx.guild_repo)
    {
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [repo = ctx.guild_repo, char_id, guild_id, duty] {
                repo->UpdateMemberDuty(char_id, guild_id, duty);
            });
    }
    spdlog::info("OnGuildDutyReq[{}]: char_id={} guild_id={} duty={}",
        ip, char_id, guild_id, duty);
}

boost::asio::awaitable<void>
OnGuildPeerReq(std::shared_ptr<PeerSession> peer,
               std::vector<std::byte>       body,
               const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, guild_id = 0;
    std::uint8_t  peerage = 0;
    if (!r.Read(char_id) || !r.Read(guild_id) || !r.Read(peerage))
    {
        spdlog::warn("OnGuildPeerReq[{}]: short body ({} bytes)",
            ip, body.size());
        co_return;
    }
    if (ctx.guild_repo)
    {
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [repo = ctx.guild_repo, char_id, guild_id, peerage] {
                repo->UpdateMemberPeer(char_id, guild_id, peerage);
            });
    }
    spdlog::info("OnGuildPeerReq[{}]: char_id={} guild_id={} peer={}",
        ip, char_id, guild_id, peerage);
}

boost::asio::awaitable<void>
OnGuildContributionReq(std::shared_ptr<PeerSession> peer,
                       std::vector<std::byte>       body,
                       const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    wire::Reader r(body.data(), body.size());
    std::uint32_t guild_id = 0, char_id = 0;
    std::uint32_t exp = 0, gold = 0, silver = 0, cooper = 0;
    if (!r.Read(guild_id) || !r.Read(char_id) || !r.Read(exp) ||
        !r.Read(gold) || !r.Read(silver) || !r.Read(cooper))
    {
        spdlog::warn("OnGuildContributionReq[{}]: short body ({} bytes)",
            ip, body.size());
        co_return;
    }
    // Legacy wire CSPGuildContribution carries 6 fields (no
    // pvp_point). The repo signature gained a pvp_point param
    // for forward parity with W3a-13's castle-war flow, so we
    // pass 0 here — the DB SP ignores it for this path.
    if (ctx.guild_repo)
    {
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [repo = ctx.guild_repo, char_id, guild_id, exp, gold, silver,
             cooper] {
                repo->IncrementContribution(char_id, guild_id, exp, gold,
                    silver, cooper, /*pvp_point=*/0);
            });
    }
    spdlog::info("OnGuildContributionReq[{}]: guild_id={} char_id={} "
                 "exp={} gold={} silver={} cooper={}",
        ip, guild_id, char_id, exp, gold, silver, cooper);
}

boost::asio::awaitable<void>
OnGuildLevelReq(std::shared_ptr<PeerSession> peer,
                std::vector<std::byte>       body,
                const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    wire::Reader r(body.data(), body.size());
    std::uint32_t guild_id = 0;
    std::uint8_t  level    = 0;
    if (!r.Read(guild_id) || !r.Read(level))
    {
        spdlog::warn("OnGuildLevelReq[{}]: short body ({} bytes)",
            ip, body.size());
        co_return;
    }
    // The level field affects the peerage gate's member cap
    // (GuildLevelCache::Find(level)->max_count). A stale cache
    // would silently mis-gate kickout/peerage decisions, so
    // refresh in-memory too — same pattern as OnGuildPvPointReq.
    if (ctx.guilds)
    {
        if (auto guild = ctx.guilds->Find(guild_id))
        {
            std::lock_guard gl(guild->lock);
            guild->level = level;
        }
    }
    if (ctx.guild_repo)
    {
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [repo = ctx.guild_repo, guild_id, level] {
                repo->UpdateLevel(guild_id, level);
            });
    }
    spdlog::info("OnGuildLevelReq[{}]: guild_id={} level={}",
        ip, guild_id, level);
}

boost::asio::awaitable<void>
OnGuildPointRewardReq(std::shared_ptr<PeerSession> peer,
                      std::vector<std::byte>       body,
                      const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    wire::Reader r(body.data(), body.size());
    std::uint32_t guild_id = 0, point = 0, total = 0, useable = 0;
    std::string   recipient;
    if (!r.Read(guild_id) || !r.Read(point) || !r.ReadString(recipient) ||
        !r.Read(total) || !r.Read(useable))
    {
        spdlog::warn("OnGuildPointRewardReq[{}]: short body ({} bytes)",
            ip, body.size());
        co_return;
    }
    // Mirror W3a-13's defensive in-memory update for the
    // total/useable fields — they shadow the canonical DB row
    // and OnGuildInfoAck reads them straight from the registry.
    // W3a-27: also append the reward to TGuild.point_log so the
    // matching OnGuildPointLogAck reader returns the entry on
    // the next call (legacy parity — pGuild->PointLog at
    // SSHandler.cpp:10357 appends to m_vPointReward right after
    // the SP fan-out).
    if (ctx.guilds)
    {
        if (auto guild = ctx.guilds->Find(guild_id))
        {
            std::lock_guard gl(guild->lock);
            guild->pvp_total_point   = total;
            guild->pvp_useable_point = useable;
            TPointRewardEntry entry;
            entry.date_unix      =
                static_cast<std::int64_t>(std::time(nullptr));
            entry.recipient_name = recipient;
            entry.point          = point;
            // W3a-29: legacy CTGuild::PointLog inserts newest-
            // first and pop_back()s once size exceeds 50
            // (matching CTBLGuildPvPointReward SELECT TOP 50).
            guild->point_log.insert(guild->point_log.begin(),
                std::move(entry));
            if (guild->point_log.size() > guild::kPointLogMaxEntries)
                guild->point_log.pop_back();
        }
    }
    if (ctx.guild_repo)
    {
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [repo = ctx.guild_repo, guild_id, point, recipient, total,
             useable] {
                repo->LogPointReward(guild_id, point, recipient, total,
                    useable);
            });
    }
    spdlog::info("OnGuildPointRewardReq[{}]: guild_id={} +{} pts to '{}' "
                 "(total={}, useable={})",
        ip, guild_id, point, recipient, total, useable);
}

// --- W3a-15 fame + article DB fan-in ------------------------------
//
// FAME mirrors the new values into the registry because they're
// read by GuildInfoAck / Establish broadcasts. The article
// handlers skip the in-memory mirror: TGuild.articles is owned
// by the article_index counter (incremented on
// OnGuildArticleAddAck), and DB-pushed rows arrive with an
// article_id chosen DB-side that could collide. We defer to
// the next OnGuildArticleListAck refresh — same behavior the
// legacy SSHandler.cpp:4201/4264/4323 exhibits.

boost::asio::awaitable<void>
OnGuildFameReq(std::shared_ptr<PeerSession> peer,
               std::vector<std::byte>       body,
               const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    wire::Reader r(body.data(), body.size());
    std::uint32_t guild_id = 0, fame = 0, fame_color = 0;
    if (!r.Read(guild_id) || !r.Read(fame) || !r.Read(fame_color))
    {
        spdlog::warn("OnGuildFameReq[{}]: short body ({} bytes)",
            ip, body.size());
        co_return;
    }
    if (ctx.guilds)
    {
        if (auto guild = ctx.guilds->Find(guild_id))
        {
            std::lock_guard gl(guild->lock);
            guild->fame       = fame;
            guild->fame_color = fame_color;
        }
    }
    if (ctx.guild_repo)
    {
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [repo = ctx.guild_repo, guild_id, fame, fame_color] {
                repo->UpdateFame(guild_id, fame, fame_color);
            });
    }
    spdlog::info("OnGuildFameReq[{}]: guild_id={} fame={} color=0x{:06X}",
        ip, guild_id, fame, fame_color);
}

boost::asio::awaitable<void>
OnGuildArticleAddReq(std::shared_ptr<PeerSession> peer,
                     std::vector<std::byte>       body,
                     const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    wire::Reader r(body.data(), body.size());
    std::uint32_t guild_id = 0, article_id = 0, time_unix = 0;
    std::uint8_t  duty     = 0;
    std::string   writer, title, body_text;
    if (!r.Read(guild_id) || !r.Read(article_id) || !r.Read(duty) ||
        !r.ReadString(writer) || !r.ReadString(title) ||
        !r.ReadString(body_text) || !r.Read(time_unix))
    {
        spdlog::warn("OnGuildArticleAddReq[{}]: short body ({} bytes)",
            ip, body.size());
        co_return;
    }
    if (ctx.guild_repo)
    {
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [repo = ctx.guild_repo, guild_id, article_id, duty, writer,
             title, body_text, time_unix] {
                repo->AddArticle(guild_id, article_id, duty, writer, title,
                    body_text, time_unix);
            });
    }
    spdlog::info("OnGuildArticleAddReq[{}]: guild_id={} article_id={} "
                 "writer='{}' title_len={} body_len={}",
        ip, guild_id, article_id, writer, title.size(), body_text.size());
}

boost::asio::awaitable<void>
OnGuildArticleDelReq(std::shared_ptr<PeerSession> peer,
                     std::vector<std::byte>       body,
                     const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    wire::Reader r(body.data(), body.size());
    std::uint32_t guild_id = 0, article_id = 0;
    if (!r.Read(guild_id) || !r.Read(article_id))
    {
        spdlog::warn("OnGuildArticleDelReq[{}]: short body ({} bytes)",
            ip, body.size());
        co_return;
    }
    if (ctx.guild_repo)
    {
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [repo = ctx.guild_repo, guild_id, article_id] {
                repo->DelArticle(guild_id, article_id);
            });
    }
    spdlog::info("OnGuildArticleDelReq[{}]: guild_id={} article_id={}",
        ip, guild_id, article_id);
}

boost::asio::awaitable<void>
OnGuildArticleUpdateReq(std::shared_ptr<PeerSession> peer,
                        std::vector<std::byte>       body,
                        const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    wire::Reader r(body.data(), body.size());
    std::uint32_t guild_id = 0, article_id = 0;
    std::string   title, body_text;
    if (!r.Read(guild_id) || !r.Read(article_id) ||
        !r.ReadString(title) || !r.ReadString(body_text))
    {
        spdlog::warn("OnGuildArticleUpdateReq[{}]: short body ({} bytes)",
            ip, body.size());
        co_return;
    }
    if (ctx.guild_repo)
    {
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [repo = ctx.guild_repo, guild_id, article_id, title,
             body_text] {
                repo->UpdateArticle(guild_id, article_id, title, body_text);
            });
    }
    spdlog::info("OnGuildArticleUpdateReq[{}]: guild_id={} article_id={} "
                 "title_len={} body_len={}",
        ip, guild_id, article_id, title.size(), body_text.size());
}

// --- W3a-16 wanted/volunteering DB fan-in -------------------------
//
// Mirrors W3a-14/15 plumbing into the recruitment subsystem
// (GuildWantedRegistry). All 4 do a defensive in-memory mirror —
// the registry feeds the next LIST handler + the "already
// applied" indicator. The VOLUNTEERING pair filters on bType:
// kMember flows through, kTactics gets dropped with a
// deferred-log (tactics subsystem ships later).

boost::asio::awaitable<void>
OnGuildWantedAddReq(std::shared_ptr<PeerSession> peer,
                    std::vector<std::byte>       body,
                    const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    wire::Reader r(body.data(), body.size());
    std::uint32_t guild_id  = 0;
    std::uint8_t  min_level = 0, max_level = 0;
    std::string   title, text;
    if (!r.Read(guild_id) || !r.Read(min_level) || !r.Read(max_level) ||
        !r.ReadString(title) || !r.ReadString(text))
    {
        spdlog::warn("OnGuildWantedAddReq[{}]: short body ({} bytes)",
            ip, body.size());
        co_return;
    }

    // Match the player path's end_time computation
    // (OnGuildWantedAddAck) so cross-server entries expire on
    // the same clock the local board uses.
    const std::int64_t end_time =
        static_cast<std::int64_t>(std::time(nullptr)) +
        guild::kGuildWantedPeriodSec;

    if (ctx.guild_wanted && ctx.guilds)
    {
        if (auto guild = ctx.guilds->Find(guild_id))
        {
            std::string guild_name;
            std::uint8_t country = 0;
            {
                std::lock_guard gl(guild->lock);
                guild_name = guild->name;
                country    = guild->country;
            }
            TGuildWanted entry;
            entry.guild_id  = guild_id;
            entry.country   = country;
            entry.min_level = min_level;
            entry.max_level = max_level;
            entry.end_time  = end_time;
            entry.name      = guild_name;
            entry.title     = title;
            entry.text      = text;
            ctx.guild_wanted->AddOrUpdate(entry);
        }
        else
        {
            spdlog::info("OnGuildWantedAddReq[{}]: guild_id={} not in "
                         "registry (DB write only)", ip, guild_id);
        }
    }

    if (ctx.guild_repo)
    {
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [repo = ctx.guild_repo, guild_id, min_level, max_level, title,
             text, end_time] {
                repo->AddWanted(guild_id, min_level, max_level, title, text,
                    end_time);
            });
    }

    spdlog::info("OnGuildWantedAddReq[{}]: guild_id={} levels {}-{} "
                 "end_time={}", ip, guild_id, min_level, max_level,
        end_time);
}

boost::asio::awaitable<void>
OnGuildWantedDelReq(std::shared_ptr<PeerSession> peer,
                    std::vector<std::byte>       body,
                    const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    wire::Reader r(body.data(), body.size());
    std::uint32_t guild_id = 0;
    if (!r.Read(guild_id))
    {
        spdlog::warn("OnGuildWantedDelReq[{}]: short body ({} bytes)",
            ip, body.size());
        co_return;
    }

    if (ctx.guild_wanted)
        ctx.guild_wanted->Remove(guild_id);

    if (ctx.guild_repo)
    {
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [repo = ctx.guild_repo, guild_id] {
                repo->DeleteWanted(guild_id);
            });
    }

    spdlog::info("OnGuildWantedDelReq[{}]: guild_id={}", ip, guild_id);
}

boost::asio::awaitable<void>
OnGuildVolunteeringReq(std::shared_ptr<PeerSession> peer,
                       std::vector<std::byte>       body,
                       const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    wire::Reader r(body.data(), body.size());
    std::uint8_t  type      = 0;
    std::uint32_t char_id   = 0;
    std::uint32_t wanted_id = 0;
    if (!r.Read(type) || !r.Read(char_id) || !r.Read(wanted_id))
    {
        spdlog::warn("OnGuildVolunteeringReq[{}]: short body ({} bytes)",
            ip, body.size());
        co_return;
    }

    if (type != guild::kVolunteerKindMember)
    {
        spdlog::info("OnGuildVolunteeringReq[{}]: type={} (tactics) "
                     "deferred — drop", ip, type);
        co_return;
    }

    // Defensive in-memory mirror. Bypass AddApp's validation
    // gates by snapshotting the applicant's char fields + always
    // upserting. If the registry rejects the row (e.g.,
    // wanted-entry expired locally), we still persist via repo.
    if (ctx.guild_wanted && ctx.chars)
    {
        if (auto tchar = ctx.chars->Find(char_id))
        {
            std::uint8_t country = 0, level = 0, klass = 0;
            std::string  name;
            {
                std::lock_guard g(tchar->lock);
                country = tchar->country;
                level   = tchar->level;
                klass   = tchar->klass;
                name    = tchar->name;
            }
            TGuildWantedApp app;
            app.char_id   = char_id;
            app.wanted_id = wanted_id;
            app.level     = level;
            app.klass     = klass;
            app.name      = name;
            const auto result = ctx.guild_wanted->AddApp(app, country);
            if (result != guild::kSuccess)
            {
                spdlog::info("OnGuildVolunteeringReq[{}]: registry AddApp "
                             "returned result={} for char_id={} wanted_id={} "
                             "— DB-authoritative path persists anyway",
                    ip, result, char_id, wanted_id);
            }
        }
        else
        {
            spdlog::info("OnGuildVolunteeringReq[{}]: char_id={} not in "
                         "registry (DB write only)", ip, char_id);
        }
    }

    if (ctx.guild_repo)
    {
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [repo = ctx.guild_repo, char_id, wanted_id] {
                repo->AddVolunteerApp(char_id, wanted_id);
            });
    }

    spdlog::info("OnGuildVolunteeringReq[{}]: char_id={} wanted_id={}",
        ip, char_id, wanted_id);
}

boost::asio::awaitable<void>
OnGuildVolunteeringDelReq(std::shared_ptr<PeerSession> peer,
                          std::vector<std::byte>       body,
                          const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    wire::Reader r(body.data(), body.size());
    std::uint8_t  type    = 0;
    std::uint32_t char_id = 0;
    if (!r.Read(type) || !r.Read(char_id))
    {
        spdlog::warn("OnGuildVolunteeringDelReq[{}]: short body ({} bytes)",
            ip, body.size());
        co_return;
    }

    if (type != guild::kVolunteerKindMember)
    {
        spdlog::info("OnGuildVolunteeringDelReq[{}]: type={} (tactics) "
                     "deferred — drop", ip, type);
        co_return;
    }

    if (ctx.guild_wanted)
        ctx.guild_wanted->DelApp(char_id);

    if (ctx.guild_repo)
    {
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [repo = ctx.guild_repo, char_id] {
                repo->DelVolunteerApp(char_id);
            });
    }

    spdlog::info("OnGuildVolunteeringDelReq[{}]: char_id={}",
        ip, char_id);
}

// --- W3a-17 leave/kickout DB fan-in --------------------------------
//
// Both handlers do the same in-memory cleanup: find the guild,
// drop the member from guild->members, clear TChar.guild_id.
// They differ only in wire layout (LEAVE carries bLeave + dwTime
// as extra audit fields) and in semantics (the bLeave code lets
// us log "self-leave vs. forced removal" but we don't persist it
// separately — the modern repo has no leave-log table yet).

namespace {

// Shared in-memory cleanup for the W3a-17 LEAVE + KICKOUT fan-in.
// Returns true if anything was actually removed (caller's log
// distinguishes "did real work" from "drop from stale broker").
bool ScrubMembershipInMemory(const HandlerContext& ctx,
                             std::uint32_t         guild_id,
                             std::uint32_t         char_id)
{
    bool did_work = false;
    if (ctx.guilds)
    {
        if (auto guild = ctx.guilds->Find(guild_id))
        {
            std::lock_guard gl(guild->lock);
            if (guild->RemoveMember(char_id)) did_work = true;
        }
    }
    if (ctx.chars)
    {
        if (auto tchar = ctx.chars->Find(char_id))
        {
            std::lock_guard g(tchar->lock);
            // Clear the back-pointer unconditionally — if it was
            // already 0 that's fine (idempotent), and if it
            // pointed at a different guild_id we still want to
            // clear it (DB is authoritative and just told us this
            // char isn't in any guild any more).
            if (tchar->guild_id != 0) did_work = true;
            tchar->guild_id = 0;
        }
    }
    return did_work;
}

} // namespace

boost::asio::awaitable<void>
OnGuildLeaveReq(std::shared_ptr<PeerSession> peer,
                std::vector<std::byte>       body,
                const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    wire::Reader r(body.data(), body.size());
    std::uint32_t guild_id = 0, char_id = 0, leave_time = 0;
    std::uint8_t  leave_kind = 0;
    if (!r.Read(guild_id) || !r.Read(char_id) || !r.Read(leave_kind) ||
        !r.Read(leave_time))
    {
        spdlog::warn("OnGuildLeaveReq[{}]: short body ({} bytes)",
            ip, body.size());
        co_return;
    }

    const bool did_work = ScrubMembershipInMemory(ctx, guild_id, char_id);

    if (ctx.guild_repo)
    {
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [repo = ctx.guild_repo, char_id, guild_id] {
                repo->RemoveMember(char_id, guild_id);
            });
    }

    spdlog::info("OnGuildLeaveReq[{}]: guild_id={} char_id={} "
                 "leave_kind={} leave_time={} (in_mem_work={})",
        ip, guild_id, char_id, leave_kind, leave_time, did_work);
}

boost::asio::awaitable<void>
OnGuildKickoutReq(std::shared_ptr<PeerSession> peer,
                  std::vector<std::byte>       body,
                  const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    wire::Reader r(body.data(), body.size());
    std::uint32_t guild_id = 0, char_id = 0;
    if (!r.Read(guild_id) || !r.Read(char_id))
    {
        spdlog::warn("OnGuildKickoutReq[{}]: short body ({} bytes)",
            ip, body.size());
        co_return;
    }

    const bool did_work = ScrubMembershipInMemory(ctx, guild_id, char_id);

    if (ctx.guild_repo)
    {
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [repo = ctx.guild_repo, char_id, guild_id] {
                repo->RemoveMember(char_id, guild_id);
            });
    }

    spdlog::info("OnGuildKickoutReq[{}]: guild_id={} char_id={} "
                 "(in_mem_work={})",
        ip, guild_id, char_id, did_work);
}

// --- W3a-18 guild establishment ------------------------------------
//
// "Create new guild" — the gameplay handler player clients fire
// from the guild-create UI. Legacy splits this across 4 packets
// because the DB lives in a separate process; our SOCI-direct
// arch collapses it into a single coroutine.

boost::asio::awaitable<void>
OnGuildEstablishAck(std::shared_ptr<PeerSession> peer,
                    std::vector<std::byte>       body,
                    const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.guilds)
    {
        spdlog::warn("OnGuildEstablishAck[{}]: char/guild registry not "
                     "wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    std::string   guild_name;
    if (!r.Read(char_id) || !r.Read(key) || !r.ReadString(guild_name))
    {
        spdlog::warn("OnGuildEstablishAck[{}]: short body ({} bytes)",
            ip, body.size());
        co_return;
    }

    auto tchar = ctx.chars->Find(char_id);
    if (!tchar)
    {
        spdlog::info("OnGuildEstablishAck[{}]: char_id={} not registered "
                     "— dropped", ip, char_id);
        co_return;
    }

    std::uint32_t actual_key  = 0, current_guild = 0;
    std::string   char_name;
    std::uint8_t  country = 0, level = 0, klass = 0;
    {
        std::lock_guard g(tchar->lock);
        actual_key    = tchar->key;
        current_guild = tchar->guild_id;
        char_name     = tchar->name;
        country       = tchar->country;
        level         = tchar->level;
        klass         = tchar->klass;
    }
    if (actual_key != key)
    {
        spdlog::warn("OnGuildEstablishAck[{}]: char_id={} key mismatch — "
                     "dropped", ip, char_id);
        co_return;
    }

    // Gate 1: char must not already belong to a guild. Legacy
    // SSHandler.cpp:3043 fires kHaveGuild with bEstablish=1 (so
    // the client knows this came from the create path, not a
    // load reconfirm).
    if (current_guild != 0)
    {
        co_await senders::SendMwGuildEstablishReq(peer, char_id, key,
            guild::kHaveGuild, /*guild_id=*/0, guild_name,
            /*establish=*/1);
        spdlog::info("OnGuildEstablishAck[{}]: char_id={} already in "
                     "guild_id={} — replied kHaveGuild",
            ip, char_id, current_guild);
        co_return;
    }

    // Gate 2: name length cap. Legacy at SSHandler.cpp:3056 just
    // drops silently; we surface kFail so the client UI can show
    // a generic "name too long" message instead of hanging.
    if (guild_name.empty() ||
        guild_name.size() > guild::kGuildMaxNameLen)
    {
        co_await senders::SendMwGuildEstablishReq(peer, char_id, key,
            guild::kFail, /*guild_id=*/0, guild_name,
            /*establish=*/1);
        spdlog::info("OnGuildEstablishAck[{}]: char_id={} name length={} "
                     "out of bounds — replied kFail",
            ip, char_id, guild_name.size());
        co_return;
    }

    // Persist + assign id. Note CoOffloadIf (non-void) returns
    // the optional<uint32_t> back through the coroutine.
    const std::int64_t establish_time =
        static_cast<std::int64_t>(std::time(nullptr));
    std::optional<std::uint32_t> new_id;
    if (ctx.guild_repo)
    {
        new_id = co_await fourstory::db::CoOffloadIf(ctx.db_pool,
            [repo = ctx.guild_repo, guild_name, char_id, country,
             establish_time] {
                return repo->CreateGuild(guild_name, char_id, country,
                    establish_time);
            });
    }
    if (!new_id)
    {
        // DB rejected (duplicate name or other failure). Legacy
        // CSPGuildEstablish returns bRet=2 for dup-name; we can't
        // distinguish without a richer return type so we send the
        // safer kAlreadyGuildName (more informative than kFail).
        co_await senders::SendMwGuildEstablishReq(peer, char_id, key,
            guild::kAlreadyGuildName, /*guild_id=*/0, guild_name,
            /*establish=*/1);
        spdlog::info("OnGuildEstablishAck[{}]: char_id={} name='{}' "
                     "create rejected (likely dup or no repo) — replied "
                     "kAlreadyGuildName", ip, char_id, guild_name);
        co_return;
    }

    // Build in-memory state. Insert into the registry then add the
    // chief as the first member under the new guild's lock.
    auto guild = std::make_shared<TGuild>();
    guild->id             = *new_id;
    guild->name           = guild_name;
    guild->chief_char_id  = char_id;
    guild->chief_name     = char_name;
    guild->country        = country;
    guild->level          = 1;
    guild->establish_time = establish_time;
    {
        TGuildMember m;
        m.char_id  = char_id;
        m.guild_id = *new_id;
        m.duty     = guild::kDutyChief;
        m.name     = char_name;
        m.level    = level;
        m.klass    = klass;
        guild->members.push_back(std::move(m));
    }
    if (!ctx.guilds->Insert(guild))
    {
        // Lost an insert race against another coroutine that
        // beat us to the same id. Extremely unlikely given the
        // DB just gave us a fresh id, but treat as kEstablishErr.
        spdlog::warn("OnGuildEstablishAck[{}]: lost registry insert "
                     "race for new guild_id={}", ip, *new_id);
        co_await senders::SendMwGuildEstablishReq(peer, char_id, key,
            guild::kEstablishErr, *new_id, guild_name, /*establish=*/1);
        co_return;
    }

    // Link the chief's back-pointer + persist the chief
    // membership row. The TGUILDTABLE row already exists from
    // CreateGuild; AddMember adds the corresponding
    // TGUILDMEMBERTABLE row for the chief.
    {
        std::lock_guard g(tchar->lock);
        tchar->guild_id = *new_id;
    }
    if (ctx.guild_repo)
    {
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [repo = ctx.guild_repo, char_id, new_id, level] {
                repo->AddMember(char_id, *new_id, level,
                    guild::kDutyChief);
            });
    }

    co_await senders::SendMwGuildEstablishReq(peer, char_id, key,
        guild::kSuccess, *new_id, guild_name, /*establish=*/1);
    spdlog::info("OnGuildEstablishAck[{}]: char_id={} created guild_id={} "
                 "name='{}' country={}",
        ip, char_id, *new_id, guild_name, country);
}

// --- W3a-20 vestigial DB-server confirmation echoes ----------------
//
// All three handlers below accept legacy BATCH-server broadcast
// echoes and drop them at info-level. The synchronous REQ-side
// handlers (W3a-4b / W3a-10 / W3a-18) already did all the work
// these ACKs would have triggered in a 3-process cluster. The
// stubs exist purely so a hybrid deployment doesn't pollute the
// log with "unknown wID" warnings on every guild mutation.

boost::asio::awaitable<void>
OnGuildEstablishAckEcho(std::shared_ptr<PeerSession> peer,
                        std::vector<std::byte>       body,
                        const HandlerContext&        /*ctx*/)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    wire::Reader r(body.data(), body.size());
    std::uint8_t  result = 0;
    std::uint32_t guild_id = 0;
    std::string   name;
    std::int64_t  time_es = 0;
    std::uint32_t char_id = 0, key = 0;
    if (!r.Read(result) || !r.Read(guild_id) || !r.ReadString(name) ||
        !r.Read(time_es) || !r.Read(char_id) || !r.Read(key))
    {
        spdlog::warn("OnGuildEstablishAckEcho[{}]: short body ({} bytes)",
            ip, body.size());
        co_return;
    }
    spdlog::info("OnGuildEstablishAckEcho[{}]: vestigial echo for "
                 "guild_id={} name='{}' result={} (W3a-18 already "
                 "handled the create synchronously)",
        ip, guild_id, name, result);
    co_return;
}

boost::asio::awaitable<void>
OnGuildDisorganizationAckEcho(std::shared_ptr<PeerSession> peer,
                              std::vector<std::byte>       body,
                              const HandlerContext&        /*ctx*/)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0, guild_id = 0, disorg_time = 0;
    std::uint8_t  disorg = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(guild_id) ||
        !r.Read(disorg) || !r.Read(disorg_time))
    {
        spdlog::warn("OnGuildDisorganizationAckEcho[{}]: short body "
                     "({} bytes)", ip, body.size());
        co_return;
    }
    spdlog::info("OnGuildDisorganizationAckEcho[{}]: vestigial echo for "
                 "guild_id={} disorg={} time={} (W3a-4b already "
                 "handled the flag flip synchronously)",
        ip, guild_id, disorg, disorg_time);
    co_return;
}

boost::asio::awaitable<void>
OnGuildExtinctionAckEcho(std::shared_ptr<PeerSession> peer,
                         std::vector<std::byte>       body,
                         const HandlerContext&        /*ctx*/)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    wire::Reader r(body.data(), body.size());
    std::uint32_t guild_id = 0;
    std::uint8_t  result = 0;
    if (!r.Read(guild_id) || !r.Read(result))
    {
        spdlog::warn("OnGuildExtinctionAckEcho[{}]: short body "
                     "({} bytes)", ip, body.size());
        co_return;
    }
    spdlog::info("OnGuildExtinctionAckEcho[{}]: vestigial echo for "
                 "guild_id={} result={} (W3a-10 already handled the "
                 "delete synchronously)",
        ip, guild_id, result);
    co_return;
}

// --- W3a-21 PvP record audit log -----------------------------------
//
// Pure DB-write fan-in: batched PvP record persistence. Wire
// carries N rows in one packet; we loop and queue one repo
// write per row via CoOffloadVoidIf (mirrors legacy parity —
// CSPSaveGuildPvPRecord is also per-row). No in-memory mirror:
// the weekrecord aggregate that the matching MW_GUILDPVPRECORD
// reader would consume lives in TGuildMember state we haven't
// modelled yet — deferred. Until then this handler is the audit-
// trail-only sink.

boost::asio::awaitable<void>
OnPvPRecordReq(std::shared_ptr<PeerSession> peer,
               std::vector<std::byte>       body,
               const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    wire::Reader r(body.data(), body.size());
    std::uint32_t guild_id = 0, member_id = 0;
    std::uint16_t count    = 0;
    if (!r.Read(guild_id) || !r.Read(member_id) || !r.Read(count))
    {
        spdlog::warn("OnPvPRecordReq[{}]: short header ({} bytes)",
            ip, body.size());
        co_return;
    }

    std::size_t persisted = 0;
    for (std::uint16_t i = 0; i < count; ++i)
    {
        std::uint32_t date = 0;
        std::uint16_t kill_count = 0, die_count = 0;
        std::array<std::uint32_t, guild::kPvPEventCount> points{};
        if (!r.Read(date) || !r.Read(kill_count) || !r.Read(die_count))
        {
            spdlog::warn("OnPvPRecordReq[{}]: short row {} of {} "
                         "(guild_id={} member_id={})",
                ip, i, count, guild_id, member_id);
            co_return;
        }
        bool point_short = false;
        for (std::size_t p = 0; p < guild::kPvPEventCount; ++p)
        {
            if (!r.Read(points[p])) { point_short = true; break; }
        }
        if (point_short)
        {
            spdlog::warn("OnPvPRecordReq[{}]: short point array on row "
                         "{} (guild_id={} member_id={})",
                ip, i, guild_id, member_id);
            co_return;
        }

        if (ctx.guild_repo)
        {
            co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
                [repo = ctx.guild_repo, guild_id, member_id, date,
                 kill_count, die_count, points] {
                    repo->LogPvPRecord(guild_id, member_id, date,
                        kill_count, die_count, points);
                });
        }
        ++persisted;
    }

    spdlog::info("OnPvPRecordReq[{}]: guild_id={} member_id={} "
                 "persisted={} of {} row(s)",
        ip, guild_id, member_id, persisted, count);
}

// --- W3a-22 full-row guild update fan-in ---------------------------
//
// Admin / bulk-load path. Overwrites the 8 scalar columns of
// TGUILDTABLE for one guild in a single shot, mirroring legacy
// CSPGuildUpdate. Wire packet also carries variable-length
// alliance + enemy DWORD ID lists; our TGuild doesn't model
// those yet so we parse + log + skip. Defensive in-memory
// mirror updates the registry entry so the next GuildInfoAck
// returns the new values without waiting for a reload.

boost::asio::awaitable<void>
OnGuildUpdateReq(std::shared_ptr<PeerSession> peer,
                 std::vector<std::byte>       body,
                 const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    wire::Reader r(body.data(), body.size());

    std::uint32_t guild_id = 0;
    std::uint8_t  fame = 0, gpoint = 0, level = 0, status = 0;
    std::uint32_t chief = 0, exp = 0, gi = 0, time_unix = 0;
    if (!r.Read(guild_id) || !r.Read(fame) || !r.Read(gpoint) ||
        !r.Read(level) || !r.Read(status) || !r.Read(chief) ||
        !r.Read(exp) || !r.Read(gi) || !r.Read(time_unix))
    {
        spdlog::warn("OnGuildUpdateReq[{}]: short scalar block "
                     "({} bytes)", ip, body.size());
        co_return;
    }

    // Parse the alliance + enemy DWORD lists. W3a-25 keeps them
    // in TGuild.alliance_ids / .enemy_ids (vectors) — SOCI
    // persistence is still deferred (legacy uses comma-separated
    // szAllience / szEnemy columns that our portable schema
    // doesn't model yet) but in-memory consumers (W5+ war
    // system) can read them as soon as they land here.
    std::uint8_t ally_count = 0;
    if (!r.Read(ally_count))
    {
        spdlog::warn("OnGuildUpdateReq[{}]: short ally count "
                     "(guild_id={})", ip, guild_id);
        co_return;
    }
    std::vector<std::uint32_t> alliance_ids;
    alliance_ids.reserve(ally_count);
    for (std::uint8_t i = 0; i < ally_count; ++i)
    {
        std::uint32_t id = 0;
        if (!r.Read(id))
        {
            spdlog::warn("OnGuildUpdateReq[{}]: short ally list at "
                         "row {} of {} (guild_id={})",
                ip, i, ally_count, guild_id);
            co_return;
        }
        alliance_ids.push_back(id);
    }
    std::uint8_t enemy_count = 0;
    if (!r.Read(enemy_count))
    {
        spdlog::warn("OnGuildUpdateReq[{}]: short enemy count "
                     "(guild_id={})", ip, guild_id);
        co_return;
    }
    std::vector<std::uint32_t> enemy_ids;
    enemy_ids.reserve(enemy_count);
    for (std::uint8_t i = 0; i < enemy_count; ++i)
    {
        std::uint32_t id = 0;
        if (!r.Read(id))
        {
            spdlog::warn("OnGuildUpdateReq[{}]: short enemy list at "
                         "row {} of {} (guild_id={})",
                ip, i, enemy_count, guild_id);
            co_return;
        }
        enemy_ids.push_back(id);
    }

    // Defensive in-memory mirror. Field set matches
    // FakeGuildRepository::UpdateGuildFull below.
    if (ctx.guilds)
    {
        if (auto guild = ctx.guilds->Find(guild_id))
        {
            std::lock_guard gl(guild->lock);
            guild->fame          = fame;
            guild->guild_points  = gpoint;
            guild->level         = level;
            guild->status        = status;
            guild->chief_char_id = chief;
            guild->gi            = gi;
            guild->exp           = exp;
            guild->disorg_time   = time_unix;
            guild->alliance_ids  = alliance_ids;
            guild->enemy_ids     = enemy_ids;
        }
        else
        {
            spdlog::info("OnGuildUpdateReq[{}]: guild_id={} not in "
                         "registry (DB write only)", ip, guild_id);
        }
    }

    if (ctx.guild_repo)
    {
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [repo = ctx.guild_repo, guild_id, fame, gpoint, level, status,
             chief, gi, exp, time_unix, alliance_ids, enemy_ids] {
                repo->UpdateGuildFull(guild_id, fame, gpoint, level,
                    status, chief, gi, exp, time_unix, alliance_ids,
                    enemy_ids);
            });
    }

    spdlog::info("OnGuildUpdateReq[{}]: guild_id={} fame={} gp={} "
                 "level={} status={} chief={} gi={} exp={} time={} "
                 "alliance={} enemy={}",
        ip, guild_id, fame, gpoint, level, status, chief, gi, exp,
        time_unix, alliance_ids.size(), enemy_ids.size());
}

// --- W3a-23 PvP record list reader ---------------------------------
//
// Read-side counterpart to W3a-21's OnPvPRecordReq. Returns
// every member's rolling weekly PvP outcome aggregate.
// Currently weekrecord stays zero-initialized at member load
// time (the per-day vRecord fan-in path lives in legacy
// SSHandler.cpp:10155 and hasn't ported yet), so the reply just
// carries zeros. That's wire-compat — the client UI shows an
// empty record table.

boost::asio::awaitable<void>
OnGuildPvPRecordAck(std::shared_ptr<PeerSession> peer,
                    std::vector<std::byte>       body,
                    const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.guilds)
    {
        spdlog::warn("OnGuildPvPRecordAck[{}]: char/guild registry not "
                     "wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    if (!r.Read(char_id) || !r.Read(key))
    {
        spdlog::warn("OnGuildPvPRecordAck[{}]: short body ({} bytes)",
            ip, body.size());
        co_return;
    }

    auto tchar = ctx.chars->Find(char_id);
    if (!tchar) co_return;
    std::uint32_t actual_key = 0, guild_id = 0;
    {
        std::lock_guard g(tchar->lock);
        actual_key = tchar->key;
        guild_id   = tchar->guild_id;
    }
    if (actual_key != key) co_return;
    // Legacy SSHandler.cpp:10391 also short-circuits when the
    // char has no guild AND no tactics. We haven't wired the
    // tactics half yet — skip the tactics branch and just drop
    // when guild_id == 0.
    if (guild_id == 0)
    {
        spdlog::info("OnGuildPvPRecordAck[{}]: char_id={} has no guild "
                     "— dropped", ip, char_id);
        co_return;
    }

    auto guild = ctx.guilds->Find(guild_id);
    if (!guild)
    {
        spdlog::info("OnGuildPvPRecordAck[{}]: char_id={} stale "
                     "guild_id={} — dropped", ip, char_id, guild_id);
        co_return;
    }

    // Snapshot the member list under the guild lock. Wire emits
    // only 6 of the 8 storage point buckets — slice on copy.
    std::vector<senders::GuildPvPRecordRow> rows;
    {
        std::lock_guard gl(guild->lock);
        rows.reserve(guild->members.size());
        for (const auto& m : guild->members)
        {
            senders::GuildPvPRecordRow row;
            row.char_id    = m.char_id;
            row.kill_count = m.weekrecord.kill_count;
            row.die_count  = m.weekrecord.die_count;
            for (std::size_t i = 0; i < row.points.size(); ++i)
                row.points[i] = m.weekrecord.points[i];
            rows.push_back(std::move(row));
        }
    }

    co_await senders::SendMwGuildPvPRecordReq(peer, char_id, key, rows);

    spdlog::info("OnGuildPvPRecordAck[{}]: char_id={} guild_id={} "
                 "members={} (weekrecord zero-init until per-day "
                 "fan-in ports)",
        ip, char_id, guild_id, rows.size());
}

// --- W3a-24 per-period war-result fan-in ---------------------------
//
// Accumulates the deltas from MW_LOCALRECORD_ACK into
// TGuildMember.weekrecord. Pairs with W3a-21 (audit log writes)
// and W3a-23 (reader). After this lands, the W3a-23 reader
// returns live (non-zero) data once the map server has fanned
// at least one record batch through.

boost::asio::awaitable<void>
OnLocalRecordAck(std::shared_ptr<PeerSession> peer,
                 std::vector<std::byte>       body,
                 const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.guilds)
    {
        spdlog::warn("OnLocalRecordAck[{}]: guild registry not wired",
            ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t win_guild_id = 0, guild_point = 0;
    std::uint16_t guild_count = 0;
    if (!r.Read(win_guild_id) || !r.Read(guild_point) ||
        !r.Read(guild_count))
    {
        spdlog::warn("OnLocalRecordAck[{}]: short header ({} bytes)",
            ip, body.size());
        co_return;
    }

    std::size_t applied = 0, dropped = 0;
    for (std::uint16_t g = 0; g < guild_count; ++g)
    {
        std::uint32_t guild_id = 0;
        std::uint16_t record_count = 0;
        if (!r.Read(guild_id) || !r.Read(record_count))
        {
            spdlog::warn("OnLocalRecordAck[{}]: short guild header at "
                         "guild row {} of {}", ip, g, guild_count);
            co_return;
        }

        auto guild = ctx.guilds->Find(guild_id);

        for (std::uint16_t i = 0; i < record_count; ++i)
        {
            std::uint32_t char_id = 0;
            std::uint16_t kill_count = 0, die_count = 0;
            if (!r.Read(char_id) || !r.Read(kill_count) ||
                !r.Read(die_count))
            {
                spdlog::warn("OnLocalRecordAck[{}]: short record header "
                             "at guild={} row {} of {}",
                    ip, guild_id, i, record_count);
                co_return;
            }
            std::array<std::uint32_t, guild::kPvPEventCount> points{};
            bool point_short = false;
            for (std::size_t p = 0; p < guild::kPvPEventCount; ++p)
            {
                if (!r.Read(points[p])) { point_short = true; break; }
            }
            if (point_short)
            {
                spdlog::warn("OnLocalRecordAck[{}]: short point array at "
                             "guild={} record={}", ip, guild_id, i);
                co_return;
            }

            if (!guild) { ++dropped; continue; }

            // W3a-28: append/merge the deltas into today's
            // vRecord row, then run CalcWeekRecord to refresh
            // weekrecord AND trim stale rows. Tactics-only
            // members get skipped here (deferred along with the
            // rest of the tactics subsystem).
            const std::int64_t today =
                static_cast<std::int64_t>(std::time(nullptr)) /
                guild::kDaySec;
            bool matched = false;
            {
                std::lock_guard gl(guild->lock);
                for (auto& m : guild->members)
                {
                    if (m.char_id != char_id) continue;
                    TPvPDayRecord* day = nullptr;
                    if (!m.vRecord.empty() &&
                        m.vRecord.back().day_index == today)
                    {
                        day = &m.vRecord.back();
                    }
                    else
                    {
                        TPvPDayRecord fresh;
                        fresh.day_index = today;
                        m.vRecord.push_back(fresh);
                        day = &m.vRecord.back();
                    }
                    day->kill_count += kill_count;
                    day->die_count  += die_count;
                    for (std::size_t p = 0;
                         p < guild::kPvPEventCount; ++p)
                        day->points[p] += points[p];
                    CalcWeekRecord(m, today);
                    matched = true;
                    break;
                }
            }
            if (matched) ++applied;
            else         ++dropped;
        }
    }

    spdlog::info("OnLocalRecordAck[{}]: win_guild={} guild_pt={} "
                 "guilds={} applied={} dropped={}",
        ip, win_guild_id, guild_point, guild_count, applied, dropped);
}

// --- W3a-26 cabinet LIST stub --------------------------------------
//
// Wire-compat stub for the guild-storage UI open path. Always
// emits an empty cabinet list because the TItem state model +
// item codec haven't ported yet. Clients see a "no items" view
// — semantically truthful since nothing else populates the
// cabinet in our port either. PUTIN / TAKEOUT + DM cabinet
// fan-in land together with the item codec in a future W3a-*
// session.

boost::asio::awaitable<void>
OnGuildCabinetListAck(std::shared_ptr<PeerSession> peer,
                      std::vector<std::byte>       body,
                      const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.guilds)
    {
        spdlog::warn("OnGuildCabinetListAck[{}]: char/guild registry "
                     "not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    if (!r.Read(char_id) || !r.Read(key))
    {
        spdlog::warn("OnGuildCabinetListAck[{}]: short body ({} bytes)",
            ip, body.size());
        co_return;
    }

    auto tchar = ctx.chars->Find(char_id);
    if (!tchar) co_return;
    std::uint32_t actual_key = 0, guild_id = 0;
    {
        std::lock_guard g(tchar->lock);
        actual_key = tchar->key;
        guild_id   = tchar->guild_id;
    }
    if (actual_key != key) co_return;
    // Legacy SSHandler.cpp:3953 also short-circuits when the
    // char is on the tactics path (pTactics != null). Skipped
    // here along with the rest of the tactics subsystem.
    if (guild_id == 0)
    {
        spdlog::info("OnGuildCabinetListAck[{}]: char_id={} has no "
                     "guild — dropped", ip, char_id);
        co_return;
    }

    auto guild = ctx.guilds->Find(guild_id);
    if (!guild)
    {
        spdlog::info("OnGuildCabinetListAck[{}]: char_id={} stale "
                     "guild_id={} — dropped", ip, char_id, guild_id);
        co_return;
    }

    std::uint8_t max_cabinet = 0;
    std::vector<TGuildCabinetItem> items;
    {
        std::lock_guard gl(guild->lock);
        max_cabinet = guild->max_cabinet;
        items = guild->cabinet_items;   // value copy under lock
    }

    co_await senders::SendMwGuildCabinetListReq(peer, char_id, key,
        max_cabinet, items);

    spdlog::info("OnGuildCabinetListAck[{}]: char_id={} guild_id={} "
                 "max_cabinet={} items={}",
        ip, char_id, guild_id, max_cabinet, items.size());
}

// --- W3a-37 cabinet put-in / take-out ------------------------------

boost::asio::awaitable<void>
OnGuildCabinetPutinAck(std::shared_ptr<PeerSession> peer,
                       std::vector<std::byte>       body,
                       const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.guilds)
    {
        spdlog::warn("OnGuildCabinetPutinAck[{}]: registries not wired",
            ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0, slot_id = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(slot_id))
    {
        spdlog::warn("OnGuildCabinetPutinAck[{}]: short header ({} bytes)",
            ip, body.size());
        co_return;
    }
    TGuildCabinetItem item;
    if (!ReadCabinetItem(r, item))
    {
        spdlog::warn("OnGuildCabinetPutinAck[{}]: malformed item codec "
                     "(slot_id={})", ip, slot_id);
        co_return;
    }
    item.slot_id = slot_id;

    auto tchar = ctx.chars->Find(char_id);
    if (!tchar) co_return;
    std::uint32_t actual_key = 0, guild_id = 0;
    {
        std::lock_guard g(tchar->lock);
        actual_key = tchar->key;
        guild_id   = tchar->guild_id;
    }
    if (actual_key != key) co_return;
    if (guild_id == 0) co_return;

    auto guild = ctx.guilds->Find(guild_id);
    if (!guild) co_return;

    std::uint8_t max_cabinet = 0;
    std::vector<TGuildCabinetItem> items;
    {
        std::lock_guard gl(guild->lock);
        guild->PutInCabinet(item);
        max_cabinet = guild->max_cabinet;
        items = guild->cabinet_items;
    }

    // Refresh the cabinet view (legacy chases PUTIN with a
    // CABINETLIST_REQ).
    co_await senders::SendMwGuildCabinetListReq(peer, char_id, key,
        max_cabinet, items);

    spdlog::info("OnGuildCabinetPutinAck[{}]: char_id={} guild_id={} "
                 "slot_id={} count={} (cabinet size={})",
        ip, char_id, guild_id, slot_id, item.count, items.size());
}

boost::asio::awaitable<void>
OnGuildCabinetTakeoutAck(std::shared_ptr<PeerSession> peer,
                         std::vector<std::byte>       body,
                         const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.guilds)
    {
        spdlog::warn("OnGuildCabinetTakeoutAck[{}]: registries not wired",
            ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0, slot_id = 0;
    std::uint8_t  count = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(slot_id) ||
        !r.Read(count))
    {
        spdlog::warn("OnGuildCabinetTakeoutAck[{}]: short body ({} bytes)",
            ip, body.size());
        co_return;
    }

    auto tchar = ctx.chars->Find(char_id);
    if (!tchar) co_return;
    std::uint32_t actual_key = 0, guild_id = 0;
    {
        std::lock_guard g(tchar->lock);
        actual_key = tchar->key;
        guild_id   = tchar->guild_id;
    }
    if (actual_key != key) co_return;
    if (guild_id == 0) co_return;

    auto guild = ctx.guilds->Find(guild_id);
    if (!guild) co_return;

    bool removed = false;
    std::uint8_t max_cabinet = 0;
    std::vector<TGuildCabinetItem> items;
    {
        std::lock_guard gl(guild->lock);
        removed = guild->TakeOutCabinet(slot_id, count);
        max_cabinet = guild->max_cabinet;
        items = guild->cabinet_items;
    }
    if (!removed)
    {
        spdlog::info("OnGuildCabinetTakeoutAck[{}]: slot_id={} not in "
                     "guild_id={} cabinet — drop", ip, slot_id, guild_id);
        co_return;
    }

    co_await senders::SendMwGuildCabinetListReq(peer, char_id, key,
        max_cabinet, items);

    spdlog::info("OnGuildCabinetTakeoutAck[{}]: char_id={} guild_id={} "
                 "slot_id={} count={} (cabinet size={})",
        ip, char_id, guild_id, slot_id, count, items.size());
}

// --- W3a-38 disband + point-reward player actions ------------------

boost::asio::awaitable<void>
OnGuildDisorganizationAck(std::shared_ptr<PeerSession> peer,
                          std::vector<std::byte>       body,
                          const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.guilds)
    {
        spdlog::warn("OnGuildDisorganizationAck[{}]: registries not "
                     "wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    std::uint8_t  disorg = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(disorg))
    {
        spdlog::warn("OnGuildDisorganizationAck[{}]: short body "
                     "({} bytes)", ip, body.size());
        co_return;
    }

    auto tchar = ctx.chars->Find(char_id);
    if (!tchar) co_return;
    std::uint32_t actual_key = 0, guild_id = 0;
    {
        std::lock_guard g(tchar->lock);
        actual_key = tchar->key;
        guild_id   = tchar->guild_id;
    }
    if (actual_key != key) co_return;
    if (guild_id == 0) co_return;

    auto guild = ctx.guilds->Find(guild_id);
    if (!guild) co_return;

    // Legacy gate (SSHandler.cpp:3190): no-op when the flag is
    // already at the requested value.
    bool changed = false;
    std::uint32_t time_unix = 0;
    {
        std::lock_guard gl(guild->lock);
        if (guild->disorg != disorg)
        {
            const std::uint32_t now =
                static_cast<std::uint32_t>(std::time(nullptr));
            time_unix = disorg ? now : 0;
            guild->disorg      = disorg;
            guild->disorg_time = time_unix;
            changed = true;
        }
    }
    if (!changed)
    {
        spdlog::info("OnGuildDisorganizationAck[{}]: char_id={} "
                     "guild_id={} disorg already {} — no-op",
            ip, char_id, guild_id, disorg);
        co_return;
    }

    if (ctx.guild_repo)
    {
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [repo = ctx.guild_repo, guild_id, disorg, time_unix] {
                repo->SetDisorg(guild_id, disorg, time_unix);
            });
    }

    co_await senders::SendMwGuildDisorganizationReq(peer, char_id, key,
        disorg);

    spdlog::info("OnGuildDisorganizationAck[{}]: char_id={} guild_id={} "
                 "disorg={} time={}", ip, char_id, guild_id, disorg,
        time_unix);
}

boost::asio::awaitable<void>
OnGuildPointRewardAck(std::shared_ptr<PeerSession> peer,
                      std::vector<std::byte>       body,
                      const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.guilds || !ctx.peers)
    {
        spdlog::warn("OnGuildPointRewardAck[{}]: registries not wired",
            ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0, point = 0;
    std::string   target_name, message;
    if (!r.Read(char_id) || !r.Read(key) || !r.ReadString(target_name) ||
        !r.Read(point) || !r.ReadString(message))
    {
        spdlog::warn("OnGuildPointRewardAck[{}]: short body ({} bytes)",
            ip, body.size());
        co_return;
    }

    auto chief = ctx.chars->Find(char_id);
    if (!chief) co_return;
    std::uint32_t actual_key = 0, guild_id = 0;
    {
        std::lock_guard g(chief->lock);
        actual_key = chief->key;
        guild_id   = chief->guild_id;
    }
    if (actual_key != key) co_return;
    if (guild_id == 0) co_return;

    auto guild = ctx.guilds->Find(guild_id);
    if (!guild) co_return;

    // Resolve the target member by name + their map peer for the
    // gain-toast relay.
    std::uint32_t target_id = 0;
    std::uint8_t  target_msi = 0;
    std::uint32_t target_key = 0;
    if (auto target = ctx.chars->FindByName(target_name))
    {
        std::lock_guard g(target->lock);
        target_id  = target->char_id;
        target_msi = target->main_server_id;
        target_key = target->key;
    }

    std::uint8_t  result      = guild::kGprSuccess;
    std::uint32_t remain      = 0, new_total = 0, new_useable = 0,
                  new_month   = 0;
    {
        std::lock_guard gl(guild->lock);
        // Gate: only the chief may grant rewards.
        if (guild->chief_char_id != char_id)
        {
            result = guild::kGprNoMember;   // legacy drops; closest code
        }
        else if (guild->pvp_useable_point < point)
        {
            result = guild::kGprNeedPoint;
        }
        else if (target_id == 0 || guild->FindMember(target_id) == nullptr)
        {
            result = guild::kGprNoMember;
        }
        else
        {
            guild->pvp_useable_point -= point;
            TPointRewardEntry entry;
            entry.date_unix      =
                static_cast<std::int64_t>(std::time(nullptr));
            entry.recipient_name = target_name;
            entry.point          = point;
            guild->point_log.insert(guild->point_log.begin(),
                std::move(entry));
            if (guild->point_log.size() > guild::kPointLogMaxEntries)
                guild->point_log.pop_back();
        }
        remain      = guild->pvp_useable_point;
        new_total   = guild->pvp_total_point;
        new_useable = guild->pvp_useable_point;
        new_month   = guild->pvp_month_point;
    }

    if (result == guild::kGprSuccess && ctx.guild_repo)
    {
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [repo = ctx.guild_repo, guild_id, point, target_name,
             new_total, new_useable] {
                repo->LogPointReward(guild_id, point, target_name,
                    new_total, new_useable);
            });
    }

    co_await senders::SendMwGuildPointRewardReq(peer, result, char_id,
        key, remain, point, target_id, target_name, message);

    // On success, relay the PvP-point gain toast to the recipient's
    // map peer (legacy SendMW_GAINPVPPOINT_REQ).
    if (result == guild::kGprSuccess && target_msi != 0)
    {
        for (auto& p : ctx.peers->Snapshot())
        {
            if (static_cast<std::uint8_t>(p->Wid() & 0xFF) == target_msi)
            {
                co_await senders::SendMwGainPvPointReq(p, target_id,
                    point, /*event=*/0, guild::kPvPMaskUseable,
                    /*gain=*/1, std::string{}, 0, 0);
                break;
            }
        }
    }

    (void)new_month;
    spdlog::info("OnGuildPointRewardAck[{}]: char_id={} guild_id={} "
                 "target='{}' point={} result={} (remain={})",
        ip, char_id, guild_id, target_name, point, result, remain);
}

// --- W3a-27 PvP point reward log reader ----------------------------
//
// Read-side counterpart to W3a-14's OnGuildPointRewardReq. The
// W3a-27 patch added the in-memory mirror onto TGuild.point_log
// so this reader returns live data after at least one reward
// fan-in.

boost::asio::awaitable<void>
OnGuildPointLogAck(std::shared_ptr<PeerSession> peer,
                   std::vector<std::byte>       body,
                   const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.guilds)
    {
        spdlog::warn("OnGuildPointLogAck[{}]: char/guild registry not "
                     "wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    if (!r.Read(char_id) || !r.Read(key))
    {
        spdlog::warn("OnGuildPointLogAck[{}]: short body ({} bytes)",
            ip, body.size());
        co_return;
    }

    auto tchar = ctx.chars->Find(char_id);
    if (!tchar) co_return;
    std::uint32_t actual_key = 0, guild_id = 0;
    {
        std::lock_guard g(tchar->lock);
        actual_key = tchar->key;
        guild_id   = tchar->guild_id;
    }
    if (actual_key != key) co_return;
    // Legacy SSHandler.cpp:10298 also short-circuits on tactics
    // path (pTactics != null). Skipped along with the rest of
    // the tactics subsystem.
    if (guild_id == 0)
    {
        spdlog::info("OnGuildPointLogAck[{}]: char_id={} has no guild "
                     "— dropped", ip, char_id);
        co_return;
    }

    auto guild = ctx.guilds->Find(guild_id);
    if (!guild)
    {
        spdlog::info("OnGuildPointLogAck[{}]: char_id={} stale "
                     "guild_id={} — dropped", ip, char_id, guild_id);
        co_return;
    }

    // Snapshot the log under the guild lock. Each TGuild stores
    // its full per-process log; the wire format carries every
    // entry (legacy CTBLGuildPvPointReward trims to TOP 50 on
    // read, but the in-memory pGuild->m_vPointReward grows
    // unbounded until process restart — same behavior here).
    std::vector<senders::GuildPointLogEntry> entries;
    {
        std::lock_guard gl(guild->lock);
        entries.reserve(guild->point_log.size());
        for (const auto& e : guild->point_log)
        {
            senders::GuildPointLogEntry row;
            row.date_unix      = e.date_unix;
            row.recipient_name = e.recipient_name;
            row.point          = e.point;
            entries.push_back(std::move(row));
        }
    }

    co_await senders::SendMwGuildPointLogReq(peer, char_id, key, entries);

    spdlog::info("OnGuildPointLogAck[{}]: char_id={} guild_id={} "
                 "entries={}", ip, char_id, guild_id, entries.size());
}

// --- W3a-29 PvP-point gain/use fan-in ------------------------------
//
// Per-event PvP-point delta from the map server. Owner is a
// char (relay to map peer) or a guild (apply to the bank +
// persist). Mirrors legacy GainPvPoint / UsePvPoint.

boost::asio::awaitable<void>
OnGainPvPointAck(std::shared_ptr<PeerSession> peer,
                 std::vector<std::byte>       body,
                 const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    wire::Reader r(body.data(), body.size());
    std::uint8_t  owner_type = 0;
    std::uint32_t owner_id = 0, point = 0;
    std::uint8_t  event = 0, type = 0, gain = 0;
    std::string   name;
    std::uint8_t  klass = 0, level = 0;
    if (!r.Read(owner_type) || !r.Read(owner_id) || !r.Read(point) ||
        !r.Read(event) || !r.Read(type) || !r.Read(gain) ||
        !r.ReadString(name) || !r.Read(klass) || !r.Read(level))
    {
        spdlog::warn("OnGainPvPointAck[{}]: short body ({} bytes)",
            ip, body.size());
        co_return;
    }

    if (owner_type == guild::kPvPOwnerChar)
    {
        // Relay the toast to the char's main map peer. Look up
        // the char's main_server_id, find the matching peer,
        // forward the packet verbatim.
        if (!ctx.chars || !ctx.peers)
        {
            spdlog::warn("OnGainPvPointAck[{}]: char/peer registry not "
                         "wired for TOWNER_CHAR relay", ip);
            co_return;
        }
        auto tchar = ctx.chars->Find(owner_id);
        if (!tchar)
        {
            spdlog::info("OnGainPvPointAck[{}]: char owner_id={} not in "
                         "registry — drop", ip, owner_id);
            co_return;
        }
        std::uint8_t msi = 0;
        {
            std::lock_guard g(tchar->lock);
            msi = tchar->main_server_id;
        }
        std::shared_ptr<PeerSession> map_peer;
        if (msi != 0)
        {
            for (auto& p : ctx.peers->Snapshot())
            {
                if (static_cast<std::uint8_t>(p->Wid() & 0xFF) == msi)
                {
                    map_peer = std::move(p);
                    break;
                }
            }
        }
        if (!map_peer)
        {
            spdlog::info("OnGainPvPointAck[{}]: char owner_id={} main "
                         "map (msi={}) offline — drop", ip, owner_id, msi);
            co_return;
        }
        co_await senders::SendMwGainPvPointReq(map_peer, owner_id, point,
            event, type, gain, name, klass, level);
        spdlog::info("OnGainPvPointAck[{}]: relayed char owner_id={} "
                     "point={} gain={} to msi={}",
            ip, owner_id, point, gain, msi);
        co_return;
    }

    // TOWNER_GUILD — apply the delta to the guild banks.
    if (!ctx.guilds)
    {
        spdlog::warn("OnGainPvPointAck[{}]: guild registry not wired",
            ip);
        co_return;
    }
    auto guild = ctx.guilds->Find(owner_id);
    if (!guild)
    {
        spdlog::info("OnGainPvPointAck[{}]: guild owner_id={} not in "
                     "registry — drop", ip, owner_id);
        co_return;
    }

    std::uint32_t new_total = 0, new_useable = 0, new_month = 0;
    if (point != 0)
    {
        std::lock_guard gl(guild->lock);
        if (gain)
        {
            // GainPvPoint: TOTAL bumps total + month; USEABLE
            // bumps useable.
            if (type & guild::kPvPMaskTotal)
            {
                guild->pvp_total_point += point;
                guild->pvp_month_point += point;
            }
            if (type & guild::kPvPMaskUseable)
                guild->pvp_useable_point += point;
        }
        else
        {
            // UsePvPoint: TOTAL + USEABLE saturate at 0; month
            // is never decremented (legacy parity).
            if (type & guild::kPvPMaskTotal)
                guild->pvp_total_point =
                    guild->pvp_total_point > point
                        ? guild->pvp_total_point - point : 0;
            if (type & guild::kPvPMaskUseable)
                guild->pvp_useable_point =
                    guild->pvp_useable_point > point
                        ? guild->pvp_useable_point - point : 0;
        }
        new_total   = guild->pvp_total_point;
        new_useable = guild->pvp_useable_point;
        new_month   = guild->pvp_month_point;
    }
    else
    {
        std::lock_guard gl(guild->lock);
        new_total   = guild->pvp_total_point;
        new_useable = guild->pvp_useable_point;
        new_month   = guild->pvp_month_point;
    }

    // Persist the new bank totals (legacy fires
    // SendDM_GUILDPVPOINT_REQ from inside Gain/UsePvPoint).
    if (point != 0 && ctx.guild_repo)
    {
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [repo = ctx.guild_repo, owner_id, new_total, new_useable,
             new_month] {
                repo->UpdatePvPoints(owner_id, new_total, new_useable,
                    new_month);
            });
    }

    spdlog::info("OnGainPvPointAck[{}]: guild owner_id={} {}{} type={} "
                 "→ total={} useable={} month={}",
        ip, owner_id, gain ? "+" : "-", point, type, new_total,
        new_useable, new_month);
}

// --- W3a-31 tactics wanted board -----------------------------------
//
// Tactics-recruitment counterpart to the W3a-11 guild wanted
// board. In-memory only (DB persistence deferred). The three
// handlers parallel OnGuildWantedAdd/Del/ListAck closely.

namespace {

// Build the country-filtered tactics-wanted row list for the
// LIST reply. Shared by the explicit LIST handler + the
// add/del refresh tails. `applied_guild_id` is the wanted-guild
// the requester has a pending application to (0 if none); the
// matching row's already_applied flag is set so the client UI
// can grey out the apply button (W3a-32).
std::vector<senders::GuildTacticsWantedRow>
BuildTacticsWantedRows(const GuildTacticsWantedRegistry& reg,
                       std::uint8_t                      country,
                       std::uint32_t                     applied_guild_id)
{
    auto entries = reg.SnapshotByCountry(country);
    std::vector<senders::GuildTacticsWantedRow> rows;
    rows.reserve(entries.size());
    for (const auto& w : entries)
    {
        senders::GuildTacticsWantedRow r;
        r.id              = w.id;
        r.guild_id        = w.guild_id;
        r.name            = w.name;
        r.title           = w.title;
        r.text            = w.text;
        r.day             = w.day;
        r.min_level       = w.min_level;
        r.max_level       = w.max_level;
        r.point           = w.point;
        r.gold            = w.gold;
        r.silver          = w.silver;
        r.cooper          = w.cooper;
        r.end_time_unix   = w.end_time;
        r.already_applied =
            (applied_guild_id != 0 && w.guild_id == applied_guild_id)
                ? 1 : 0;
        rows.push_back(std::move(r));
    }
    return rows;
}

} // namespace

boost::asio::awaitable<void>
OnGuildTacticsWantedAddAck(std::shared_ptr<PeerSession> peer,
                           std::vector<std::byte>       body,
                           const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.guilds || !ctx.guild_tactics_wanted)
    {
        spdlog::warn("OnGuildTacticsWantedAddAck[{}]: registries not "
                     "wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0, id = 0;
    std::string   title, text;
    std::uint8_t  day = 0, min_level = 0, max_level = 0;
    std::uint32_t point = 0, gold = 0, silver = 0, cooper = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(id) ||
        !r.ReadString(title) || !r.ReadString(text) || !r.Read(day) ||
        !r.Read(min_level) || !r.Read(max_level) || !r.Read(point) ||
        !r.Read(gold) || !r.Read(silver) || !r.Read(cooper))
    {
        spdlog::warn("OnGuildTacticsWantedAddAck[{}]: short body "
                     "({} bytes)", ip, body.size());
        co_return;
    }

    auto tchar = ctx.chars->Find(char_id);
    if (!tchar) co_return;
    std::uint32_t actual_key = 0, guild_id = 0;
    std::uint8_t  char_country = 0;
    {
        std::lock_guard g(tchar->lock);
        actual_key   = tchar->key;
        guild_id     = tchar->guild_id;
        char_country = tchar->country;
    }
    if (actual_key != key) co_return;
    if (guild_id == 0)
    {
        co_await senders::SendMwGuildTacticsWantedAddReq(peer, char_id,
            key, guild::kFail);
        co_return;
    }

    auto guild = ctx.guilds->Find(guild_id);
    if (!guild)
    {
        co_await senders::SendMwGuildTacticsWantedAddReq(peer, char_id,
            key, guild::kFail);
        co_return;
    }
    std::string  guild_name;
    bool         is_disorg = false;
    {
        std::lock_guard gl(guild->lock);
        is_disorg  = (guild->disorg != 0);
        guild_name = guild->name;
    }
    if (is_disorg)
    {
        co_await senders::SendMwGuildTacticsWantedAddReq(peer, char_id,
            key, guild::kFail);
        co_return;
    }

    // A non-zero `id` updates an existing posting; zero allocates
    // a fresh global id (legacy `dwID > 0 ? dwID : ++m_dwTacticsIndex`).
    const std::uint32_t entry_id =
        (id > 0) ? id : ctx.guild_tactics_wanted->NextId();

    TGuildTacticsWanted entry;
    entry.id        = entry_id;
    entry.guild_id  = guild_id;
    entry.country   = char_country;
    entry.name      = guild_name;
    entry.title     = title;
    entry.text      = text;
    entry.day       = day;
    entry.min_level = min_level;
    entry.max_level = max_level;
    entry.point     = point;
    entry.gold      = gold;
    entry.silver    = silver;
    entry.cooper    = cooper;
    entry.end_time  =
        static_cast<std::int64_t>(std::time(nullptr)) +
        guild::kGuildWantedPeriodSec;

    const std::uint8_t result =
        ctx.guild_tactics_wanted->AddOrUpdate(entry);

    co_await senders::SendMwGuildTacticsWantedAddReq(peer, char_id, key,
        result);

    if (result == guild::kSuccess)
    {
        co_await senders::SendMwGuildTacticsWantedListReq(peer, char_id,
            key, BuildTacticsWantedRows(*ctx.guild_tactics_wanted,
                char_country,
                ctx.guild_tactics_wanted->FindAppGuildByChar(char_id)));
    }

    spdlog::info("OnGuildTacticsWantedAddAck[{}]: char_id={} guild_id={} "
                 "id={} levels {}-{} point={} result={}",
        ip, char_id, guild_id, entry_id, min_level, max_level, point,
        result);
}

boost::asio::awaitable<void>
OnGuildTacticsWantedDelAck(std::shared_ptr<PeerSession> peer,
                           std::vector<std::byte>       body,
                           const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.guild_tactics_wanted)
    {
        spdlog::warn("OnGuildTacticsWantedDelAck[{}]: registries not "
                     "wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0, id = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(id))
    {
        spdlog::warn("OnGuildTacticsWantedDelAck[{}]: short body "
                     "({} bytes)", ip, body.size());
        co_return;
    }

    auto tchar = ctx.chars->Find(char_id);
    if (!tchar) co_return;
    std::uint32_t actual_key = 0, guild_id = 0;
    std::uint8_t  char_country = 0;
    {
        std::lock_guard g(tchar->lock);
        actual_key   = tchar->key;
        guild_id     = tchar->guild_id;
        char_country = tchar->country;
    }
    if (actual_key != key) co_return;
    if (guild_id == 0)
    {
        co_await senders::SendMwGuildTacticsWantedDelReq(peer, char_id,
            key, guild::kFail);
        co_return;
    }

    const std::uint8_t result =
        ctx.guild_tactics_wanted->Remove(guild_id, id);

    co_await senders::SendMwGuildTacticsWantedDelReq(peer, char_id, key,
        result);

    if (result == guild::kSuccess)
    {
        co_await senders::SendMwGuildTacticsWantedListReq(peer, char_id,
            key, BuildTacticsWantedRows(*ctx.guild_tactics_wanted,
                char_country,
                ctx.guild_tactics_wanted->FindAppGuildByChar(char_id)));
    }

    spdlog::info("OnGuildTacticsWantedDelAck[{}]: char_id={} guild_id={} "
                 "id={} result={}", ip, char_id, guild_id, id, result);
}

boost::asio::awaitable<void>
OnGuildTacticsWantedListAck(std::shared_ptr<PeerSession> peer,
                            std::vector<std::byte>       body,
                            const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.guild_tactics_wanted)
    {
        spdlog::warn("OnGuildTacticsWantedListAck[{}]: registries not "
                     "wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    if (!r.Read(char_id) || !r.Read(key))
    {
        spdlog::warn("OnGuildTacticsWantedListAck[{}]: short body "
                     "({} bytes)", ip, body.size());
        co_return;
    }

    auto tchar = ctx.chars->Find(char_id);
    if (!tchar) co_return;
    std::uint32_t actual_key = 0;
    std::uint8_t  char_country = 0;
    {
        std::lock_guard g(tchar->lock);
        actual_key   = tchar->key;
        char_country = tchar->country;
    }
    if (actual_key != key) co_return;

    co_await senders::SendMwGuildTacticsWantedListReq(peer, char_id, key,
        BuildTacticsWantedRows(*ctx.guild_tactics_wanted, char_country,
            ctx.guild_tactics_wanted->FindAppGuildByChar(char_id)));

    spdlog::info("OnGuildTacticsWantedListAck[{}]: char_id={} country={} "
                 "list", ip, char_id, char_country);
}

// --- W3a-32 tactics volunteer (applicant) flow ---------------------
//
// Apply / cancel / chief-browse, parallel to the W3a-12 guild
// volunteer flow. REPLY (accept/reject) deferred to W3a-33.

boost::asio::awaitable<void>
OnGuildTacticsVolunteeringAck(std::shared_ptr<PeerSession> peer,
                              std::vector<std::byte>       body,
                              const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.guild_tactics_wanted)
    {
        spdlog::warn("OnGuildTacticsVolunteeringAck[{}]: registries not "
                     "wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0, guild_id = 0, wanted_id = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(guild_id) ||
        !r.Read(wanted_id))
    {
        spdlog::warn("OnGuildTacticsVolunteeringAck[{}]: short body "
                     "({} bytes)", ip, body.size());
        co_return;
    }

    auto tchar = ctx.chars->Find(char_id);
    if (!tchar) co_return;
    std::uint32_t actual_key = 0, current_guild = 0;
    std::uint8_t  country = 0, level = 0, klass = 0;
    std::string   name;
    {
        std::lock_guard g(tchar->lock);
        actual_key    = tchar->key;
        current_guild = tchar->guild_id;
        country       = tchar->country;
        level         = tchar->level;
        klass         = tchar->klass;
        name          = tchar->name;
    }
    if (actual_key != key) co_return;

    TGuildTacticsWantedApp app;
    app.char_id         = char_id;
    app.wanted_id       = wanted_id;
    app.wanted_guild_id = guild_id;
    app.level           = level;
    app.klass           = klass;
    app.name            = name;
    const std::uint8_t result = ctx.guild_tactics_wanted->AddApp(
        app, country, current_guild);

    co_await senders::SendMwGuildTacticsVolunteeringReq(peer, char_id,
        key, result);

    if (result == guild::kSuccess)
    {
        // Refresh the wanted board so the requester's
        // already_applied flag flips on the posting they just
        // applied to (legacy NotifyGuildTacticsWantedList tail).
        co_await senders::SendMwGuildTacticsWantedListReq(peer, char_id,
            key, BuildTacticsWantedRows(*ctx.guild_tactics_wanted,
                country,
                ctx.guild_tactics_wanted->FindAppGuildByChar(char_id)));
    }

    spdlog::info("OnGuildTacticsVolunteeringAck[{}]: char_id={} "
                 "wanted_id={} guild_id={} result={}",
        ip, char_id, wanted_id, guild_id, result);
}

boost::asio::awaitable<void>
OnGuildTacticsVolunteeringDelAck(std::shared_ptr<PeerSession> peer,
                                 std::vector<std::byte>       body,
                                 const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.guild_tactics_wanted)
    {
        spdlog::warn("OnGuildTacticsVolunteeringDelAck[{}]: registries "
                     "not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    if (!r.Read(char_id) || !r.Read(key))
    {
        spdlog::warn("OnGuildTacticsVolunteeringDelAck[{}]: short body "
                     "({} bytes)", ip, body.size());
        co_return;
    }

    auto tchar = ctx.chars->Find(char_id);
    if (!tchar) co_return;
    std::uint32_t actual_key = 0;
    std::uint8_t  country = 0;
    {
        std::lock_guard g(tchar->lock);
        actual_key = tchar->key;
        country    = tchar->country;
    }
    if (actual_key != key) co_return;

    const std::uint8_t result =
        ctx.guild_tactics_wanted->DelApp(char_id) ? guild::kSuccess
                                                  : guild::kFail;

    co_await senders::SendMwGuildTacticsVolunteeringDelReq(peer, char_id,
        key, result);

    if (result == guild::kSuccess)
    {
        co_await senders::SendMwGuildTacticsWantedListReq(peer, char_id,
            key, BuildTacticsWantedRows(*ctx.guild_tactics_wanted,
                country,
                ctx.guild_tactics_wanted->FindAppGuildByChar(char_id)));
    }

    spdlog::info("OnGuildTacticsVolunteeringDelAck[{}]: char_id={} "
                 "result={}", ip, char_id, result);
}

boost::asio::awaitable<void>
OnGuildTacticsVolunteerListAck(std::shared_ptr<PeerSession> peer,
                               std::vector<std::byte>       body,
                               const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.guild_tactics_wanted)
    {
        spdlog::warn("OnGuildTacticsVolunteerListAck[{}]: registries not "
                     "wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    if (!r.Read(char_id) || !r.Read(key))
    {
        spdlog::warn("OnGuildTacticsVolunteerListAck[{}]: short body "
                     "({} bytes)", ip, body.size());
        co_return;
    }

    auto tchar = ctx.chars->Find(char_id);
    if (!tchar) co_return;
    std::uint32_t actual_key = 0, guild_id = 0;
    {
        std::lock_guard g(tchar->lock);
        actual_key = tchar->key;
        guild_id   = tchar->guild_id;
    }
    if (actual_key != key) co_return;
    if (guild_id == 0)
    {
        // No guild → empty applicant list (legacy NotifyGuild...
        // returns early; we send an empty list so the client UI
        // clears cleanly).
        co_await senders::SendMwGuildTacticsVolunteerListReq(peer,
            char_id, key, {});
        co_return;
    }

    // Snapshot applicants across all of the chief's guild's
    // postings, refreshing each applicant's region from the
    // CharRegistry (legacy NotifyGuildTacticsVolunteerList).
    auto apps = ctx.guild_tactics_wanted->SnapshotAppsFor(guild_id);
    std::vector<senders::GuildTacticsVolunteerRow> rows;
    rows.reserve(apps.size());
    for (const auto& a : apps)
    {
        senders::GuildTacticsVolunteerRow row;
        row.char_id = a.char_id;
        row.name    = a.name;
        row.level   = a.level;
        row.klass   = a.klass;
        // Legacy refreshes m_dwRegion from the live TChar here;
        // our TChar doesn't model region yet (it's a map-zone id
        // not needed by any ported handler), so leave it 0.
        row.region  = 0;
        row.day    = a.day;
        row.point  = a.point;
        row.gold   = a.gold;
        row.silver = a.silver;
        row.cooper = a.cooper;
        rows.push_back(std::move(row));
    }

    co_await senders::SendMwGuildTacticsVolunteerListReq(peer, char_id,
        key, rows);

    spdlog::info("OnGuildTacticsVolunteerListAck[{}]: char_id={} "
                 "guild_id={} applicants={}",
        ip, char_id, guild_id, rows.size());
}

// --- W3a-33 tactics reply (accept/reject hire) ---------------------
//
// Chief accepts or rejects a tactics-wanted applicant. Accept
// promotes the applicant to a hired tactics member, charging the
// guild's PvP-useable points + money up front. Mirrors legacy
// OnMW_GUILDTACTICSREPLY_ACK + ApplyGuildTacticsApp.

namespace {

// Send the chief's volunteer-list refresh after a reply (legacy
// NotifyGuildTacticsVolunteerList tail). Mirrors the body of
// OnGuildTacticsVolunteerListAck without the wire-read.
boost::asio::awaitable<void>
RefreshTacticsVolunteerList(std::shared_ptr<PeerSession> peer,
                            const HandlerContext&        ctx,
                            std::uint32_t                char_id,
                            std::uint32_t                key,
                            std::uint32_t                guild_id)
{
    auto apps = ctx.guild_tactics_wanted->SnapshotAppsFor(guild_id);
    std::vector<senders::GuildTacticsVolunteerRow> rows;
    rows.reserve(apps.size());
    for (const auto& a : apps)
    {
        senders::GuildTacticsVolunteerRow row;
        row.char_id = a.char_id;
        row.name    = a.name;
        row.level   = a.level;
        row.klass   = a.klass;
        row.region  = 0;
        row.day     = a.day;
        row.point   = a.point;
        row.gold    = a.gold;
        row.silver  = a.silver;
        row.cooper  = a.cooper;
        rows.push_back(std::move(row));
    }
    co_await senders::SendMwGuildTacticsVolunteerListReq(peer, char_id,
        key, rows);
}

} // namespace

boost::asio::awaitable<void>
OnGuildTacticsReplyAck(std::shared_ptr<PeerSession> peer,
                       std::vector<std::byte>       body,
                       const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.guilds || !ctx.peers ||
        !ctx.guild_tactics_wanted)
    {
        spdlog::warn("OnGuildTacticsReplyAck[{}]: registries not wired",
            ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0, target_id = 0;
    std::uint8_t  reply = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(target_id) ||
        !r.Read(reply))
    {
        spdlog::warn("OnGuildTacticsReplyAck[{}]: short body ({} bytes)",
            ip, body.size());
        co_return;
    }

    auto chief = ctx.chars->Find(char_id);
    if (!chief) co_return;
    std::uint32_t actual_key = 0, chief_guild = 0;
    {
        std::lock_guard g(chief->lock);
        actual_key  = chief->key;
        chief_guild = chief->guild_id;
    }
    if (actual_key != key) co_return;
    if (chief_guild == 0) co_return;

    // Reject: drop the application + refresh the chief's list.
    if (reply == 0)
    {
        ctx.guild_tactics_wanted->DelApp(target_id);
        co_await RefreshTacticsVolunteerList(peer, ctx, char_id, key,
            chief_guild);
        spdlog::info("OnGuildTacticsReplyAck[{}]: char_id={} rejected "
                     "target={} guild_id={}", ip, char_id, target_id,
            chief_guild);
        co_return;
    }

    // Accept. Look up the application + its reward fields.
    auto app_opt = ctx.guild_tactics_wanted->FindApp(target_id);
    if (!app_opt)
    {
        co_await senders::SendMwGuildTacticsReplyReq(peer, char_id, key,
            guild::kFail, target_id, chief_guild, std::string{},
            std::string{}, 0, 0, 0);
        co_return;
    }
    const auto& app = *app_opt;
    const std::uint32_t hiring_guild_id = app.wanted_guild_id;

    auto guild = ctx.guilds->Find(hiring_guild_id);
    if (!guild)
    {
        co_await senders::SendMwGuildTacticsReplyReq(peer, char_id, key,
            guild::kFail, target_id, hiring_guild_id, std::string{},
            std::string{}, 0, 0, 0);
        co_return;
    }

    // Snapshot the target char's state.
    auto target = ctx.chars->Find(target_id);
    std::uint32_t target_tactics_guild = 0, target_guild = 0;
    std::uint8_t  target_level = app.level, target_klass = app.klass;
    std::uint8_t  target_msi = 0;
    std::uint32_t target_key = 0;
    std::string   target_name = app.name;
    if (target)
    {
        std::lock_guard g(target->lock);
        target_tactics_guild = target->tactics_guild_id;
        target_guild         = target->guild_id;
        target_level         = target->level;
        target_klass         = target->klass;
        target_msi           = target->main_server_id;
        target_key           = target->key;
        target_name          = target->name;
    }

    // Gate: already a tactics member somewhere.
    if (target_tactics_guild != 0)
    {
        co_await senders::SendMwGuildTacticsReplyReq(peer, char_id, key,
            guild::kHaveGuild, target_id, hiring_guild_id,
            std::string{}, std::string{}, 0, 0, 0);
        co_return;
    }
    // Gate: target is a vice-chief+ of their own guild.
    if (target_guild != 0)
    {
        if (auto og = ctx.guilds->Find(target_guild))
        {
            std::lock_guard gl(og->lock);
            if (const auto* m = og->FindMember(target_id))
                if (m->duty >= guild::kDutyViceChief)
                {
                    co_await senders::SendMwGuildTacticsReplyReq(peer,
                        char_id, key, guild::kNoDuty, target_id,
                        hiring_guild_id, std::string{}, std::string{},
                        0, 0, 0);
                    co_return;
                }
        }
    }

    // The remaining gates + the mutation run under the hiring
    // guild's lock. Compute the cap from the level chart.
    std::uint8_t  fail = 0;
    std::string   guild_name;
    const std::int64_t reward_money =
        guild::CalcMoney(app.gold, app.silver, app.cooper);
    {
        std::lock_guard gl(guild->lock);
        guild_name = guild->name;
        std::uint8_t tactics_cap = 0;
        if (ctx.guild_levels)
            if (const auto* lvl = ctx.guild_levels->Find(guild->level))
                tactics_cap = lvl->tactics_count;

        if (guild->FindTactics(target_id))                fail = guild::kAlreadyMember;
        else if (guild->FindMember(target_id))            fail = guild::kSameGuildTactics;
        else if (guild->pvp_useable_point < app.point)    fail = guild::kNoPoint;
        else if (tactics_cap > 0 &&
                 guild->tactics_members.size() >= tactics_cap)
            fail = guild::kMemberFull;
        else if (guild::CalcMoney(guild->gold, guild->silver,
                                  guild->cooper) < reward_money)
            fail = guild::kNoMoney;

        if (fail == 0)
        {
            // Charge the guild + add the tactics member.
            guild->pvp_useable_point -= app.point;
            const std::int64_t remaining =
                guild::CalcMoney(guild->gold, guild->silver,
                                 guild->cooper) - reward_money;
            guild::SplitMoney(remaining, guild->gold, guild->silver,
                              guild->cooper);

            TTacticsMember m;
            m.id           = target_id;
            m.name         = target_name;
            m.level        = target_level;
            m.klass        = target_klass;
            m.reward_point = app.point;
            m.reward_money = reward_money;
            m.gain_point   = 0;
            m.day          = app.day;
            m.end_time     =
                static_cast<std::int64_t>(std::time(nullptr)) +
                static_cast<std::int64_t>(app.day) * guild::kDaySec;
            guild->tactics_members.push_back(std::move(m));
        }
    }

    if (fail != 0)
    {
        co_await senders::SendMwGuildTacticsReplyReq(peer, char_id, key,
            fail, target_id, hiring_guild_id, std::string{},
            std::string{}, 0, 0, 0);
        spdlog::info("OnGuildTacticsReplyAck[{}]: char_id={} accept "
                     "target={} failed result={}", ip, char_id,
            target_id, fail);
        co_return;
    }

    // Wire the target's tactics back-pointer + clear the app.
    if (target)
    {
        std::lock_guard g(target->lock);
        target->tactics_guild_id = hiring_guild_id;
    }
    ctx.guild_tactics_wanted->DelApp(target_id);

    // Persist the guild's new PvP-point banks (legacy fires
    // SendDM_GUILDPVPOINT_REQ from inside UsePvPoint). The money
    // deduction stays in-memory only for now — there's no
    // set-absolute money repo method (IncrementContribution is
    // an additive delta, not a set, so it can't flush the new
    // balance), and UpdateGuildFull would need every other guild
    // scalar. A dedicated UpdateGuildMoney repo method is a
    // documented follow-up; until then a restart re-warms the
    // money from the canonical DB row.
    if (ctx.guild_repo)
    {
        std::uint32_t t = 0, u = 0, mo = 0;
        {
            std::lock_guard gl(guild->lock);
            t = guild->pvp_total_point; u = guild->pvp_useable_point;
            mo = guild->pvp_month_point;
        }
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [repo = ctx.guild_repo, hiring_guild_id, t, u, mo] {
                repo->UpdatePvPoints(hiring_guild_id, t, u, mo);
            });
    }

    // Notify the new member's map peer (if online) + the chief.
    if (target && target_msi != 0)
    {
        std::shared_ptr<PeerSession> target_peer;
        for (auto& p : ctx.peers->Snapshot())
        {
            if (static_cast<std::uint8_t>(p->Wid() & 0xFF) == target_msi)
            {
                target_peer = std::move(p);
                break;
            }
        }
        if (target_peer)
        {
            co_await senders::SendMwGuildTacticsReplyReq(target_peer,
                target_id, target_key, guild::kSuccess, target_id,
                hiring_guild_id, guild_name, target_name, app.gold,
                app.silver, app.cooper);
        }
    }
    co_await senders::SendMwGuildTacticsReplyReq(peer, char_id, key,
        guild::kSuccess, target_id, hiring_guild_id, guild_name,
        target_name, app.gold, app.silver, app.cooper);

    co_await RefreshTacticsVolunteerList(peer, ctx, char_id, key,
        chief_guild);

    spdlog::info("OnGuildTacticsReplyAck[{}]: char_id={} hired target={} "
                 "('{}') into guild_id={} (point={} money={})",
        ip, char_id, target_id, target_name, hiring_guild_id, app.point,
        reward_money);
}

// --- W3a-34 tactics kickout + list ---------------------------------

boost::asio::awaitable<void>
OnGuildTacticsKickoutAck(std::shared_ptr<PeerSession> peer,
                         std::vector<std::byte>       body,
                         const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.guilds)
    {
        spdlog::warn("OnGuildTacticsKickoutAck[{}]: registries not wired",
            ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0, target = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(target))
    {
        spdlog::warn("OnGuildTacticsKickoutAck[{}]: short body ({} bytes)",
            ip, body.size());
        co_return;
    }

    auto tchar = ctx.chars->Find(char_id);
    if (!tchar) co_return;
    std::uint32_t actual_key = 0, requester_guild = 0;
    {
        std::lock_guard g(tchar->lock);
        actual_key      = tchar->key;
        requester_guild = tchar->guild_id;
    }
    if (actual_key != key) co_return;

    const bool self_leave = (char_id == target);

    // Resolve the hiring guild. Chief-kick uses the requester's
    // own guild; self-leave finds the target's tactics guild via
    // their back-pointer.
    std::uint32_t hiring_guild_id = 0;
    if (!self_leave)
    {
        hiring_guild_id = requester_guild;
    }
    else
    {
        if (auto t = ctx.chars->Find(target))
        {
            std::lock_guard g(t->lock);
            hiring_guild_id = t->tactics_guild_id;
        }
    }
    if (hiring_guild_id == 0) co_return;

    auto guild = ctx.guilds->Find(hiring_guild_id);
    if (!guild) co_return;

    bool removed = false;
    {
        std::lock_guard gl(guild->lock);
        if (guild->FindTactics(target))
            removed = guild->RemoveTactics(target, /*refund=*/self_leave);
    }
    if (!removed)
    {
        spdlog::info("OnGuildTacticsKickoutAck[{}]: target={} not a "
                     "tactics member of guild_id={} — drop",
            ip, target, hiring_guild_id);
        co_return;
    }

    // Clear the target's tactics back-pointer.
    if (auto t = ctx.chars->Find(target))
    {
        std::lock_guard g(t->lock);
        if (t->tactics_guild_id == hiring_guild_id)
            t->tactics_guild_id = 0;
    }

    // Persist the refunded PvP banks on self-leave (money stays
    // in-memory — same deferral as W3a-33).
    if (self_leave && ctx.guild_repo)
    {
        std::uint32_t t = 0, u = 0, mo = 0;
        {
            std::lock_guard gl(guild->lock);
            t = guild->pvp_total_point; u = guild->pvp_useable_point;
            mo = guild->pvp_month_point;
        }
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [repo = ctx.guild_repo, hiring_guild_id, t, u, mo] {
                repo->UpdatePvPoints(hiring_guild_id, t, u, mo);
            });
    }

    // Chief-kick replies KICKOUT + a roster refresh; a self-leave
    // is silent (legacy only replies when char != target).
    if (!self_leave)
    {
        co_await senders::SendMwGuildTacticsKickoutReq(peer, char_id, key,
            guild::kSuccess, target, /*kick=*/1);

        std::vector<senders::GuildTacticsMemberRow> rows;
        {
            std::lock_guard gl(guild->lock);
            rows.reserve(guild->tactics_members.size());
            for (const auto& m : guild->tactics_members)
            {
                senders::GuildTacticsMemberRow row;
                row.id           = m.id;
                row.name         = m.name;
                row.level        = m.level;
                row.klass        = m.klass;
                row.day          = m.day;
                row.reward_point = m.reward_point;
                row.reward_money = m.reward_money;
                row.end_time     = m.end_time;
                row.gain_point   = m.gain_point;
                rows.push_back(std::move(row));
            }
        }
        co_await senders::SendMwGuildTacticsListReq(peer, char_id, key,
            rows);
    }

    spdlog::info("OnGuildTacticsKickoutAck[{}]: char_id={} target={} "
                 "guild_id={} {} (refund={})",
        ip, char_id, target, hiring_guild_id,
        self_leave ? "self-left" : "kicked", self_leave);
}

boost::asio::awaitable<void>
OnGuildTacticsListAck(std::shared_ptr<PeerSession> peer,
                      std::vector<std::byte>       body,
                      const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.guilds)
    {
        spdlog::warn("OnGuildTacticsListAck[{}]: registries not wired",
            ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    if (!r.Read(char_id) || !r.Read(key))
    {
        spdlog::warn("OnGuildTacticsListAck[{}]: short body ({} bytes)",
            ip, body.size());
        co_return;
    }

    auto tchar = ctx.chars->Find(char_id);
    if (!tchar) co_return;
    std::uint32_t actual_key = 0, tactics_guild = 0, full_guild = 0;
    {
        std::lock_guard g(tchar->lock);
        actual_key    = tchar->key;
        tactics_guild = tchar->tactics_guild_id;
        full_guild    = tchar->guild_id;
    }
    if (actual_key != key) co_return;

    // GetCurGuild: tactics guild takes priority, else full guild.
    const std::uint32_t cur_guild =
        tactics_guild != 0 ? tactics_guild : full_guild;
    if (cur_guild == 0) co_return;

    auto guild = ctx.guilds->Find(cur_guild);
    if (!guild) co_return;

    std::vector<senders::GuildTacticsMemberRow> rows;
    {
        std::lock_guard gl(guild->lock);
        rows.reserve(guild->tactics_members.size());
        for (const auto& m : guild->tactics_members)
        {
            senders::GuildTacticsMemberRow row;
            row.id           = m.id;
            row.name         = m.name;
            row.level        = m.level;
            row.klass        = m.klass;
            row.day          = m.day;
            row.reward_point = m.reward_point;
            row.reward_money = m.reward_money;
            row.end_time     = m.end_time;
            row.gain_point   = m.gain_point;
            rows.push_back(std::move(row));
        }
    }

    co_await senders::SendMwGuildTacticsListReq(peer, char_id, key, rows);

    spdlog::info("OnGuildTacticsListAck[{}]: char_id={} guild_id={} "
                 "tactics_members={}", ip, char_id, cur_guild, rows.size());
}

// --- W3a-35 tactics invite + answer (chief-initiated hire) ---------

namespace {

// Find the map peer serving a given main_server_id (msi), or
// nullptr if that map is offline. Shared by the invite/answer
// relay paths.
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
OnGuildTacticsInviteAck(std::shared_ptr<PeerSession> peer,
                        std::vector<std::byte>       body,
                        const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.guilds || !ctx.peers)
    {
        spdlog::warn("OnGuildTacticsInviteAck[{}]: registries not wired",
            ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    std::string   target_name;
    std::uint8_t  day = 0;
    std::uint32_t point = 0, gold = 0, silver = 0, cooper = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.ReadString(target_name) ||
        !r.Read(day) || !r.Read(point) || !r.Read(gold) ||
        !r.Read(silver) || !r.Read(cooper))
    {
        spdlog::warn("OnGuildTacticsInviteAck[{}]: short body ({} bytes)",
            ip, body.size());
        co_return;
    }

    auto chief = ctx.chars->Find(char_id);
    if (!chief) co_return;
    std::uint32_t actual_key = 0, chief_guild = 0;
    std::uint8_t  chief_country = 0;
    std::string   chief_name;
    {
        std::lock_guard g(chief->lock);
        actual_key    = chief->key;
        chief_guild   = chief->guild_id;
        chief_country = chief->country;
        chief_name    = chief->name;
    }
    if (actual_key != key) co_return;
    if (chief_guild == 0) co_return;

    auto guild = ctx.guilds->Find(chief_guild);
    if (!guild) co_return;

    // Snapshot the target by name.
    auto target = ctx.chars->FindByName(target_name);
    std::uint32_t target_id = 0, target_tactics = 0, target_guild = 0;
    std::uint8_t  target_country = 0, target_msi = 0;
    std::uint32_t target_key = 0;
    if (target)
    {
        std::lock_guard g(target->lock);
        target_id      = target->char_id;
        target_tactics = target->tactics_guild_id;
        target_guild   = target->guild_id;
        target_country = target->country;
        target_msi     = target->main_server_id;
        target_key     = target->key;
    }

    const std::int64_t cost_money = guild::CalcMoney(gold, silver, cooper);
    std::string  guild_name;
    std::uint8_t result = guild::kSuccess;
    if (!target)
    {
        result = guild::kNotFound;
    }
    else if (target_country != chief_country)
    {
        result = guild::kFail;
    }
    else
    {
        std::lock_guard gl(guild->lock);
        guild_name = guild->name;
        std::uint8_t tactics_cap = 0;
        if (ctx.guild_levels)
            if (const auto* lvl = ctx.guild_levels->Find(guild->level))
                tactics_cap = lvl->tactics_count;
        const bool target_is_member_here = guild->FindTactics(target_id);

        if (guild->pvp_useable_point < point)            result = guild::kNoPoint;
        else if (guild::CalcMoney(guild->gold, guild->silver,
                                  guild->cooper) < cost_money)
            result = guild::kNoMoney;
        else if (target_tactics != 0 && target_tactics != chief_guild)
            result = guild::kHaveGuild;
        else if (!target_is_member_here && tactics_cap > 0 &&
                 guild->tactics_members.size() >= tactics_cap)
            result = guild::kMemberFull;
        else if (target_guild == chief_guild)
            result = guild::kSameGuildTactics;
    }

    if (result == guild::kSuccess)
    {
        // Relay the invite dialog to the target's map peer.
        auto target_peer = FindMapPeer(ctx, target_msi);
        if (target_peer)
            co_await senders::SendMwGuildTacticsInviteReq(target_peer,
                target_id, target_key, guild_name, chief_name,
                day, point, gold, silver, cooper);
        spdlog::info("OnGuildTacticsInviteAck[{}]: char_id={} invited "
                     "'{}' (target_id={}) day={} point={}",
            ip, char_id, target_name, target_id, day, point);
    }
    else
    {
        // Failure → reply to the chief with the failure code.
        co_await senders::SendMwGuildTacticsAnswerReq(peer, char_id, key,
            result, 0, std::string{}, 0, target_name, gold, silver,
            cooper);
        spdlog::info("OnGuildTacticsInviteAck[{}]: char_id={} invite "
                     "'{}' failed result={}", ip, char_id, target_name,
            result);
    }
}

boost::asio::awaitable<void>
OnGuildTacticsAnswerAck(std::shared_ptr<PeerSession> peer,
                        std::vector<std::byte>       body,
                        const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.guilds || !ctx.peers)
    {
        spdlog::warn("OnGuildTacticsAnswerAck[{}]: registries not wired",
            ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    std::uint8_t  answer = 0, day = 0;
    std::string   inviter_name;
    std::uint32_t point = 0, gold = 0, silver = 0, cooper = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(answer) ||
        !r.ReadString(inviter_name) || !r.Read(day) || !r.Read(point) ||
        !r.Read(gold) || !r.Read(silver) || !r.Read(cooper))
    {
        spdlog::warn("OnGuildTacticsAnswerAck[{}]: short body ({} bytes)",
            ip, body.size());
        co_return;
    }

    auto target = ctx.chars->Find(char_id);
    if (!target) co_return;
    std::uint32_t actual_key = 0, target_tactics = 0, target_guild = 0;
    std::uint8_t  target_country = 0, target_level = 0, target_klass = 0;
    std::string   target_name;
    {
        std::lock_guard g(target->lock);
        actual_key     = target->key;
        target_tactics = target->tactics_guild_id;
        target_guild   = target->guild_id;
        target_country = target->country;
        target_level   = target->level;
        target_klass   = target->klass;
        target_name    = target->name;
    }
    if (actual_key != key) co_return;

    auto origin = ctx.chars->FindByName(inviter_name);
    if (!origin) co_return;
    std::uint32_t origin_id = 0, origin_guild = 0, origin_key = 0;
    std::uint8_t  origin_country = 0, origin_msi = 0;
    {
        std::lock_guard g(origin->lock);
        origin_id      = origin->char_id;
        origin_guild   = origin->guild_id;
        origin_key     = origin->key;
        origin_country = origin->country;
        origin_msi     = origin->main_server_id;
    }
    if (origin_guild == 0) co_return;

    auto guild = ctx.guilds->Find(origin_guild);
    if (!guild) co_return;

    const std::int64_t cost_money = guild::CalcMoney(gold, silver, cooper);
    std::uint8_t result = guild::kSuccess;
    std::string  guild_name;

    if (answer != guild::kAskYes)
    {
        result = guild::kJoinDeny;
        std::lock_guard gl(guild->lock);
        guild_name = guild->name;
    }
    else
    {
        std::lock_guard gl(guild->lock);
        guild_name = guild->name;
        std::uint8_t tactics_cap = 0;
        if (ctx.guild_levels)
            if (const auto* lvl = ctx.guild_levels->Find(guild->level))
                tactics_cap = lvl->tactics_count;
        const bool member_here = guild->FindTactics(char_id);

        if (guild->pvp_useable_point < point)            result = guild::kNoPoint;
        else if (target_country != origin_country)        result = guild::kFail;
        else if (guild::CalcMoney(guild->gold, guild->silver,
                                  guild->cooper) < cost_money)
            result = guild::kNoMoney;
        else if (target_tactics != 0 && target_tactics != origin_guild)
            result = guild::kHaveGuild;
        else if (!member_here && tactics_cap > 0 &&
                 guild->tactics_members.size() >= tactics_cap)
            result = guild::kMemberFull;
        else if (target_guild == origin_guild)
            result = guild::kSameGuildTactics;

        if (result == guild::kSuccess)
        {
            // Renewal: if already a tactics member here, drop the
            // old contract first (legacy GuildTacticsDel bKick=2:
            // no refund, no expiry-queue touch).
            std::int64_t base_time =
                static_cast<std::int64_t>(std::time(nullptr));
            if (auto* existing = guild->FindTactics(char_id))
            {
                base_time = existing->end_time;
                guild->RemoveTactics(char_id, /*refund=*/false);
            }
            // Charge + add.
            guild->pvp_useable_point -= point;
            const std::int64_t remaining =
                guild::CalcMoney(guild->gold, guild->silver,
                                 guild->cooper) - cost_money;
            guild::SplitMoney(remaining, guild->gold, guild->silver,
                              guild->cooper);
            TTacticsMember m;
            m.id           = char_id;
            m.name         = target_name;
            m.level        = target_level;
            m.klass        = target_klass;
            m.reward_point = point;
            m.reward_money = cost_money;
            m.gain_point   = 0;
            m.day          = day;
            m.end_time     = base_time +
                static_cast<std::int64_t>(day) * guild::kDaySec;
            guild->tactics_members.push_back(std::move(m));
        }
    }

    // On accept, wire the target's back-pointer + persist points.
    if (result == guild::kSuccess)
    {
        {
            std::lock_guard g(target->lock);
            target->tactics_guild_id = origin_guild;
        }
        if (ctx.guild_repo)
        {
            std::uint32_t t = 0, u = 0, mo = 0;
            {
                std::lock_guard gl(guild->lock);
                t = guild->pvp_total_point; u = guild->pvp_useable_point;
                mo = guild->pvp_month_point;
            }
            co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
                [repo = ctx.guild_repo, origin_guild, t, u, mo] {
                    repo->UpdatePvPoints(origin_guild, t, u, mo);
                });
        }
    }

    // Echo the outcome to the target (this peer) + the chief.
    co_await senders::SendMwGuildTacticsAnswerReq(peer, char_id, key,
        result, origin_guild, guild_name, char_id, target_name, gold,
        silver, cooper);
    if (auto chief_peer = FindMapPeer(ctx, origin_msi))
        co_await senders::SendMwGuildTacticsAnswerReq(chief_peer,
            origin_id, origin_key, result, origin_guild, guild_name,
            char_id, target_name, gold, silver, cooper);

    spdlog::info("OnGuildTacticsAnswerAck[{}]: char_id={} answer={} "
                 "inviter='{}' guild_id={} result={}",
        ip, char_id, answer, inviter_name, origin_guild, result);
}

// --- W3a-12 volunteer / applicant flow ----------------------------

namespace {

// Forward declarations of helpers defined further down in the
// file (W3a-8 `ResolveRequesterGuild` + W3a-11 `SendWantedList`).
// Used by the W3a-12 handler block which sits between W3a-5 and
// W3a-11 in this file. Keeping the W3a-* sections in numerical
// order matters for code-review navigation; forward decls let us
// do that without reordering established blocks. The
// `GuildHandle` return type lives in the top-of-file anonymous
// namespace so it's complete at this point of the TU. The
// `PromotionResult` type + TryPromoteIntoGuild come from the
// W3a-13 anon namespace block immediately above this one.
GuildHandle ResolveRequesterGuild(const HandlerContext& ctx,
                                   std::uint32_t char_id,
                                   std::uint32_t key);
boost::asio::awaitable<void>
SendWantedList(std::shared_ptr<PeerSession> peer,
               const HandlerContext&        ctx,
               std::uint32_t                char_id,
               std::uint32_t                key,
               std::uint8_t                 country);

// Build the chief-facing applicant list. Each row refreshes
// `region` from the live TChar (legacy SSSender.cpp:1349-1357
// reads region off pTarget->m_dwRegion at LIST time so the chief
// sees where applicants currently are).
//
// TChar.region isn't part of the W3a-3 identity-fields snapshot
// (lands with the W5+ castle-war work); for now we emit 0 and
// document the gap.
std::vector<senders::GuildVolunteerRow>
BuildVolunteerRows(const std::vector<TGuildWantedApp>& apps,
                   const CharRegistry&                 chars)
{
    std::vector<senders::GuildVolunteerRow> rows;
    rows.reserve(apps.size());
    for (const auto& a : apps)
    {
        senders::GuildVolunteerRow r;
        r.char_id = a.char_id;
        r.name    = a.name;
        r.level   = a.level;
        r.klass   = a.klass;
        r.region  = a.region;
        // Refresh region from the live char when online; the
        // applicant's cached level/klass stay (they're the values
        // the applicant submitted, not necessarily the current
        // ones).
        if (auto tchar = chars.Find(a.char_id))
        {
            // TODO W5+: tchar->region — once region tracking ships
            (void)tchar;
        }
        rows.push_back(std::move(r));
    }
    return rows;
}

} // namespace

boost::asio::awaitable<void>
OnGuildVolunteeringAck(std::shared_ptr<PeerSession> peer,
                       std::vector<std::byte>       body,
                       const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.guild_wanted)
    {
        spdlog::warn("OnGuildVolunteeringAck[{}]: registries not wired",
            ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0, wanted_id = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(wanted_id))
    {
        spdlog::warn("OnGuildVolunteeringAck[{}]: short body", ip);
        co_return;
    }

    auto tchar = ctx.chars->Find(char_id);
    if (!tchar) co_return;
    std::uint32_t actual_key = 0, current_guild = 0;
    std::uint8_t  country = 0, level = 0, klass = 0;
    std::string   name;
    {
        std::lock_guard g(tchar->lock);
        actual_key    = tchar->key;
        current_guild = tchar->guild_id;
        country       = tchar->country;
        level         = tchar->level;
        klass         = tchar->klass;
        name          = tchar->name;
    }
    if (actual_key != key) co_return;

    // Legacy gate (SSHandler.cpp:4562): a char already in a guild
    // can't apply elsewhere. Silent drop (no error reply).
    if (current_guild != 0) co_return;

    TGuildWantedApp app;
    app.char_id   = char_id;
    app.wanted_id = wanted_id;
    app.level     = level;
    app.klass     = klass;
    app.name      = name;
    const std::uint8_t result =
        ctx.guild_wanted->AddApp(app, country);

    co_await senders::SendMwGuildVolunteeringReq(peer, char_id, key,
        result);

    if (result == guild::kSuccess)
    {
        if (ctx.guild_repo)
        {
            co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
                [repo = ctx.guild_repo, char_id, wanted_id] {
                    repo->AddVolunteerApp(char_id, wanted_id);
                });
        }
        // Legacy chases the ACK with a fresh wanted-board refresh
        // so the player's "already applied" indicator updates.
        co_await SendWantedList(peer, ctx, char_id, key, country);
    }

    spdlog::info("OnGuildVolunteeringAck[{}]: char_id={} → wanted_id={} "
                 "result={}", ip, char_id, wanted_id, result);
}

boost::asio::awaitable<void>
OnGuildVolunteeringDelAck(std::shared_ptr<PeerSession> peer,
                          std::vector<std::byte>       body,
                          const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.guild_wanted)
    {
        spdlog::warn("OnGuildVolunteeringDelAck[{}]: registries not "
                     "wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    if (!r.Read(char_id) || !r.Read(key))
    {
        spdlog::warn("OnGuildVolunteeringDelAck[{}]: short body", ip);
        co_return;
    }

    auto tchar = ctx.chars->Find(char_id);
    if (!tchar) co_return;
    std::uint32_t actual_key = 0;
    std::uint8_t  country    = 0;
    {
        std::lock_guard g(tchar->lock);
        actual_key = tchar->key;
        country    = tchar->country;
    }
    if (actual_key != key) co_return;

    const bool removed = ctx.guild_wanted->DelApp(char_id);
    co_await senders::SendMwGuildVolunteeringDelReq(peer, char_id, key,
        removed ? guild::kSuccess : guild::kFail);

    if (removed)
    {
        if (ctx.guild_repo)
        {
            co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
                [repo = ctx.guild_repo, char_id] {
                    repo->DelVolunteerApp(char_id);
                });
        }
        co_await SendWantedList(peer, ctx, char_id, key, country);
    }
    spdlog::info("OnGuildVolunteeringDelAck[{}]: char_id={} removed={}",
        ip, char_id, removed);
}

boost::asio::awaitable<void>
OnGuildVolunteerListAck(std::shared_ptr<PeerSession> peer,
                        std::vector<std::byte>       body,
                        const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.guild_wanted)
    {
        spdlog::warn("OnGuildVolunteerListAck[{}]: registries not wired",
            ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    if (!r.Read(char_id) || !r.Read(key))
    {
        spdlog::warn("OnGuildVolunteerListAck[{}]: short body", ip);
        co_return;
    }

    auto h = ResolveRequesterGuild(ctx, char_id, key);
    if (!h.guild) co_return;

    auto apps = ctx.guild_wanted->SnapshotAppsFor(h.guild_id);
    auto rows = BuildVolunteerRows(apps, *ctx.chars);

    co_await senders::SendMwGuildVolunteerListReq(peer, char_id, key, rows);
    spdlog::info("OnGuildVolunteerListAck[{}]: char_id={} guild_id={} "
                 "→ {} applicants", ip, char_id, h.guild_id, rows.size());
}

boost::asio::awaitable<void>
OnGuildVolunteerReplyAck(std::shared_ptr<PeerSession> peer,
                         std::vector<std::byte>       body,
                         const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.guilds || !ctx.peers || !ctx.guild_wanted)
    {
        spdlog::warn("OnGuildVolunteerReplyAck[{}]: registries not wired",
            ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0, target_id = 0;
    std::uint8_t  reply = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(target_id) ||
        !r.Read(reply))
    {
        spdlog::warn("OnGuildVolunteerReplyAck[{}]: short body", ip);
        co_return;
    }

    auto h = ResolveRequesterGuild(ctx, char_id, key);
    if (!h.guild) co_return;

    if (reply == 0)
    {
        // Rejection: just delete the application + refresh chief's
        // applicant list. No reply to the rejected char (legacy
        // parity SSHandler.cpp:4651-4652).
        ctx.guild_wanted->DelApp(target_id);
        if (ctx.guild_repo)
        {
            co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
                [repo = ctx.guild_repo, target_id] {
                    repo->DelVolunteerApp(target_id);
                });
        }
        auto apps = ctx.guild_wanted->SnapshotAppsFor(h.guild_id);
        co_await senders::SendMwGuildVolunteerListReq(peer, char_id, key,
            BuildVolunteerRows(apps, *ctx.chars));
        spdlog::info("OnGuildVolunteerReplyAck[{}]: char_id={} rejected "
                     "target={} guild_id={}", ip, char_id, target_id,
            h.guild_id);
        co_return;
    }

    // Accept path — promote applicant to member. Mirrors the
    // OnGuildInviteAnswerAck YES branch (W3a-6/W3a-7). Re-validate
    // every gate since state may have changed during the dialog:
    // applicant joined another guild, guild filled up, guild
    // disorged.

    auto applicant_char = ctx.chars->Find(target_id);
    std::string   applicant_name;
    std::uint32_t applicant_key = 0;
    std::uint8_t  applicant_msi = 0;
    std::uint8_t  applicant_level = 0;
    if (applicant_char)
    {
        std::lock_guard g(applicant_char->lock);
        applicant_name  = applicant_char->name;
        applicant_key   = applicant_char->key;
        applicant_msi   = applicant_char->main_server_id;
        applicant_level = applicant_char->level;
    }

    // W3a-13: shared promotion path via TryPromoteIntoGuild.
    auto pr = co_await TryPromoteIntoGuild(ctx, h.guild, h.guild_id,
        applicant_char, target_id, applicant_name, applicant_level);

    if (pr.result != guild::kSuccess)
    {
        // Accept failed — fire VOLUNTEERREPLY_REQ to the chief
        // with the right failure code so their UI explains why.
        co_await senders::SendMwGuildVolunteerReplyReq(peer, char_id, key,
            pr.result);
        spdlog::info("OnGuildVolunteerReplyAck[{}]: char_id={} accept "
                     "target={} failed result={}", ip, char_id, target_id,
            pr.result);
        co_return;
    }

    // Mirror the legacy ApplyGuildApp tail: clear app +
    // JOIN_REQ broadcast to new member + chief (the helper
    // already wired the back-pointer + AddMember persist).
    ctx.guild_wanted->DelApp(target_id);
    if (ctx.guild_repo)
    {
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [repo = ctx.guild_repo, target_id] {
                repo->DelVolunteerApp(target_id);
            });
    }

    // Notify the new member's main map peer (if online).
    if (applicant_msi != 0)
    {
        std::shared_ptr<PeerSession> applicant_peer;
        for (auto& p : ctx.peers->Snapshot())
        {
            if (static_cast<std::uint8_t>(p->Wid() & 0xFF) == applicant_msi)
            {
                applicant_peer = std::move(p);
                break;
            }
        }
        if (applicant_peer)
        {
            co_await senders::SendMwGuildJoinReq(applicant_peer, target_id,
                applicant_key, guild::kJoinSuccess, h.guild_id, pr.fame,
                pr.fame_color, pr.guild_name, target_id, applicant_name, 0);
        }
    }
    // Chief sees it too.
    co_await senders::SendMwGuildJoinReq(peer, char_id, key,
        guild::kJoinSuccess, h.guild_id, pr.fame, pr.fame_color,
        pr.guild_name, target_id, applicant_name, 0);

    // Refresh chief's applicant list (now missing the accepted
    // entry).
    auto apps = ctx.guild_wanted->SnapshotAppsFor(h.guild_id);
    co_await senders::SendMwGuildVolunteerListReq(peer, char_id, key,
        BuildVolunteerRows(apps, *ctx.chars));

    spdlog::info("OnGuildVolunteerReplyAck[{}]: char_id={} accepted "
                 "target={} ('{}') into guild_id={}",
        ip, char_id, target_id, applicant_name, h.guild_id);
}

// --- W3a-11 guild wanted board ------------------------------------

namespace {

// Build wanted-list rows snapshot from the registry filtered by
// country. Used by both the explicit LIST handler and by the
// add/del handlers' refresh tail. `applicant_char_id` is the
// requester's id — we'd set already_applied=1 for any row whose
// guild_id matches the requester's pending application, but the
// applicant subsystem ships in W3a-12+, so the flag is always 0
// for now.
std::vector<senders::GuildWantedRow>
BuildWantedRows(const GuildWantedRegistry& reg,
                std::uint8_t               country,
                std::uint32_t              /*applicant_char_id*/)
{
    auto entries = reg.SnapshotByCountry(country);
    std::vector<senders::GuildWantedRow> rows;
    rows.reserve(entries.size());
    for (const auto& w : entries)
    {
        senders::GuildWantedRow r;
        r.guild_id        = w.guild_id;
        r.name            = w.name;
        r.title           = w.title;
        r.text            = w.text;
        r.min_level       = w.min_level;
        r.max_level       = w.max_level;
        r.end_time_unix   = w.end_time;
        r.already_applied = 0;   // TODO W3a-12+: applicant lookup
        rows.push_back(std::move(r));
    }
    return rows;
}

boost::asio::awaitable<void>
SendWantedList(std::shared_ptr<PeerSession> peer,
               const HandlerContext&        ctx,
               std::uint32_t                char_id,
               std::uint32_t                key,
               std::uint8_t                 country)
{
    if (!ctx.guild_wanted)
    {
        co_await senders::SendMwGuildWantedListReq(peer, char_id, key, {});
        co_return;
    }
    auto rows = BuildWantedRows(*ctx.guild_wanted, country, char_id);
    co_await senders::SendMwGuildWantedListReq(peer, char_id, key, rows);
}

} // namespace

boost::asio::awaitable<void>
OnGuildWantedAddAck(std::shared_ptr<PeerSession> peer,
                    std::vector<std::byte>       body,
                    const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.guilds || !ctx.guild_wanted)
    {
        spdlog::warn("OnGuildWantedAddAck[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0, legacy_id = 0;
    std::string   title, text;
    std::uint8_t  min_level = 0, max_level = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(legacy_id) ||
        !r.ReadString(title) || !r.ReadString(text) ||
        !r.Read(min_level) || !r.Read(max_level))
    {
        spdlog::warn("OnGuildWantedAddAck[{}]: short body", ip);
        co_return;
    }
    (void)legacy_id;   // unused after schema migration; kept for wire compat

    // Caps + empty-title gate (legacy SSHandler.cpp:4453-4458 —
    // silent-drop on overflow / empty title; the client UI
    // shouldn't have let this through).
    if (title.empty() || title.size() > guild::kMaxBoardTitle ||
        text.size() > guild::kMaxBoardText)
        co_return;

    auto tchar = ctx.chars->Find(char_id);
    if (!tchar) co_return;
    std::uint32_t guild_id = 0, actual_key = 0;
    std::uint8_t  char_country = 0;
    {
        std::lock_guard g(tchar->lock);
        actual_key   = tchar->key;
        guild_id     = tchar->guild_id;
        char_country = tchar->country;
    }
    if (actual_key != key || guild_id == 0)
    {
        co_await senders::SendMwGuildWantedAddReq(peer, char_id, key,
            guild::kFail);
        co_return;
    }

    auto guild = ctx.guilds->Find(guild_id);
    if (!guild)
    {
        co_await senders::SendMwGuildWantedAddReq(peer, char_id, key,
            guild::kFail);
        co_return;
    }
    std::string guild_name;
    bool        is_disorg = false;
    {
        std::lock_guard gl(guild->lock);
        is_disorg  = (guild->disorg != 0);
        guild_name = guild->name;
    }
    if (is_disorg)
    {
        co_await senders::SendMwGuildWantedAddReq(peer, char_id, key,
            guild::kFail);
        co_return;
    }

    const std::int64_t now =
        static_cast<std::int64_t>(std::time(nullptr));
    const std::int64_t end_time = now + guild::kGuildWantedPeriodSec;

    TGuildWanted entry;
    entry.guild_id  = guild_id;
    entry.country   = char_country;
    entry.min_level = min_level;
    entry.max_level = max_level;
    entry.end_time  = end_time;
    entry.name      = guild_name;
    entry.title     = title;
    entry.text      = text;
    ctx.guild_wanted->AddOrUpdate(entry);

    if (ctx.guild_repo)
    {
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [repo = ctx.guild_repo, guild_id, min_level, max_level,
             title, text, end_time] {
                repo->AddWanted(guild_id, min_level, max_level, title,
                    text, end_time);
            });
    }

    co_await senders::SendMwGuildWantedAddReq(peer, char_id, key,
        guild::kSuccess);
    co_await SendWantedList(peer, ctx, char_id, key, char_country);

    spdlog::info("OnGuildWantedAddAck[{}]: char_id={} guild_id={} "
                 "country={} levels {}–{}", ip, char_id, guild_id,
        char_country, min_level, max_level);
}

boost::asio::awaitable<void>
OnGuildWantedDelAck(std::shared_ptr<PeerSession> peer,
                    std::vector<std::byte>       body,
                    const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.guild_wanted)
    {
        spdlog::warn("OnGuildWantedDelAck[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0, legacy_id = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(legacy_id))
    {
        spdlog::warn("OnGuildWantedDelAck[{}]: short body", ip);
        co_return;
    }
    (void)legacy_id;

    auto tchar = ctx.chars->Find(char_id);
    if (!tchar) co_return;
    std::uint32_t guild_id = 0, actual_key = 0;
    std::uint8_t  char_country = 0;
    {
        std::lock_guard g(tchar->lock);
        actual_key   = tchar->key;
        guild_id     = tchar->guild_id;
        char_country = tchar->country;
    }
    if (actual_key != key || guild_id == 0)
    {
        co_await senders::SendMwGuildWantedDelReq(peer, char_id, key,
            guild::kFail);
        co_return;
    }

    const bool removed = ctx.guild_wanted->Remove(guild_id);
    if (!removed)
    {
        co_await senders::SendMwGuildWantedDelReq(peer, char_id, key,
            guild::kFail);
        co_return;
    }
    if (ctx.guild_repo)
    {
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [repo = ctx.guild_repo, guild_id] {
                repo->DeleteWanted(guild_id);
            });
    }
    co_await senders::SendMwGuildWantedDelReq(peer, char_id, key,
        guild::kSuccess);
    co_await SendWantedList(peer, ctx, char_id, key, char_country);

    spdlog::info("OnGuildWantedDelAck[{}]: char_id={} guild_id={} removed",
        ip, char_id, guild_id);
}

boost::asio::awaitable<void>
OnGuildWantedListAck(std::shared_ptr<PeerSession> peer,
                     std::vector<std::byte>       body,
                     const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.guild_wanted)
    {
        spdlog::warn("OnGuildWantedListAck[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    if (!r.Read(char_id) || !r.Read(key))
    {
        spdlog::warn("OnGuildWantedListAck[{}]: short body", ip);
        co_return;
    }

    auto tchar = ctx.chars->Find(char_id);
    if (!tchar) co_return;
    std::uint32_t actual_key = 0;
    std::uint8_t  char_country = 0;
    {
        std::lock_guard g(tchar->lock);
        actual_key   = tchar->key;
        char_country = tchar->country;
    }
    if (actual_key != key) co_return;

    // TODO W3a-12+ scheduler: SM_EVENTEXPIRED_ACK fan-out for
    // entries whose end_time + DAY_ONE elapsed. Until the timer
    // sweeps land we just hand out everything in the registry
    // and let the client filter on end_time.

    co_await SendWantedList(peer, ctx, char_id, key, char_country);
    spdlog::info("OnGuildWantedListAck[{}]: char_id={} country={} list",
        ip, char_id, char_country);
}

// --- W3a-10 money recover + guild extinction ----------------------

boost::asio::awaitable<void>
OnGuildMoneyRecoverAck(std::shared_ptr<PeerSession> peer,
                       std::vector<std::byte>       body,
                       const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.guilds)
    {
        spdlog::warn("OnGuildMoneyRecoverAck[{}]: guild registry not wired",
            ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t guild_id = 0;
    std::uint32_t price    = 0;
    if (!r.Read(guild_id) || !r.Read(price))
    {
        spdlog::warn("OnGuildMoneyRecoverAck[{}]: short body ({} bytes)",
            ip, body.size());
        co_return;
    }

    auto guild = ctx.guilds->Find(guild_id);
    if (!guild)
    {
        spdlog::warn("OnGuildMoneyRecoverAck[{}]: guild_id={} not in "
                     "registry — dropped", ip, guild_id);
        co_return;
    }

    // Legacy GainMoney(0, 0, dwPrice) — adds dwPrice to cooper,
    // then normalizes via CalcMoney (1000 cooper = 1 silver,
    // 1000 silver = 1 gold). For now we skip the normalization
    // (our CONTRIBUTION path also += each bucket without
    // overflow); a dedicated W3a-11+ pass can add the normaliser
    // if dashboard reports show overflow.
    {
        std::lock_guard gl(guild->lock);
        guild->cooper += price;
    }

    // Persist via the existing IncrementContribution path
    // (legacy: GainMoney emits SendDM_GUILDCONTRIBUTION_REQ with
    // exp=0 + the money delta). dwPrice → cooper; 0s elsewhere.
    if (ctx.guild_repo)
    {
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [repo = ctx.guild_repo, guild_id, price] {
                repo->IncrementContribution(/*char_id=*/0, guild_id,
                    /*exp=*/0, /*gold=*/0, /*silver=*/0, /*cooper=*/price,
                    /*pvp_point=*/0);
            });
    }

    spdlog::info("OnGuildMoneyRecoverAck[{}]: guild_id={} cooper+={}",
        ip, guild_id, price);
}

boost::asio::awaitable<void>
OnGuildExtinctionReq(std::shared_ptr<PeerSession> peer,
                     std::vector<std::byte>       body,
                     const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.guilds || !ctx.peers)
    {
        spdlog::warn("OnGuildExtinctionReq[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t guild_id = 0;
    if (!r.Read(guild_id))
    {
        spdlog::warn("OnGuildExtinctionReq[{}]: short body ({} bytes)",
            ip, body.size());
        co_return;
    }

    auto guild = ctx.guilds->Find(guild_id);
    if (!guild)
    {
        spdlog::info("OnGuildExtinctionReq[{}]: guild_id={} already "
                     "unloaded — DB-only delete", ip, guild_id);
        // Still hit the repo in case the legacy row is stale.
        if (ctx.guild_repo)
        {
            co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
                [repo = ctx.guild_repo, guild_id] {
                    repo->DeleteGuild(guild_id);
                });
        }
        co_return;
    }

    // Snapshot members under guild.lock + remove from registry.
    // The legacy fan-out walks the member list AFTER the registry
    // entry is gone (DeleteTGuild line 3204); we do the same by
    // copying out the member-ids first then dropping the lock,
    // then iterating + cleanup.
    std::vector<std::uint32_t> member_ids;
    std::string                guild_name_for_log;
    {
        std::lock_guard gl(guild->lock);
        guild_name_for_log = guild->name;
        member_ids.reserve(guild->members.size());
        for (const auto& m : guild->members) member_ids.push_back(m.char_id);
    }
    // Drop from the registry — any concurrent Find returns
    // nullptr from this point on; cached shared_ptrs (like the
    // local `guild` here) still work until they die.
    ctx.guilds->Remove(guild_id);

    // Persist (best-effort).
    if (ctx.guild_repo)
    {
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [repo = ctx.guild_repo, guild_id] {
                repo->DeleteGuild(guild_id);
            });
    }

    // Per-member fan-out: clear TChar.guild_id + send the
    // disorg-flavored leave to each online member's main map.
    // The legacy module logs the broadcast count for ops; we
    // match that.
    const std::uint32_t now =
        static_cast<std::uint32_t>(std::time(nullptr));
    std::size_t notified = 0;
    for (auto cid : member_ids)
    {
        auto tchar = ctx.chars->Find(cid);
        if (!tchar) continue;
        std::string char_name;
        std::uint32_t char_key = 0;
        std::uint8_t  main_svr = 0;
        {
            std::lock_guard cg(tchar->lock);
            tchar->guild_id = 0;
            char_name       = tchar->name;
            char_key        = tchar->key;
            main_svr        = tchar->main_server_id;
        }
        if (main_svr == 0) continue;

        std::shared_ptr<PeerSession> target;
        for (auto& p : ctx.peers->Snapshot())
        {
            if (static_cast<std::uint8_t>(p->Wid() & 0xFF) == main_svr)
            {
                target = std::move(p);
                break;
            }
        }
        if (!target) continue;

        co_await senders::SendMwGuildLeaveReq(target, cid, char_key,
            char_name, guild::kLeaveDisorganization, now);
        ++notified;
    }

    spdlog::info("OnGuildExtinctionReq[{}]: guild_id={} ('{}') deleted, "
                 "members={} notified={}", ip, guild_id, guild_name_for_log,
        member_ids.size(), notified);
    // TODO W3a-11+: cascade through TGUILDTACTICSTABLE + alliance
    // links. Legacy DeleteTGuild also walks m_mapTTactics +
    // breaks alliance/enemy edges; both rely on registries we
    // haven't ported yet.
}

// --- W3a-9 single guild info refresh ------------------------------

boost::asio::awaitable<void>
OnGuildInfoAck(std::shared_ptr<PeerSession> peer,
               std::vector<std::byte>       body,
               const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();

    if (!ctx.chars || !ctx.guilds)
    {
        spdlog::warn("OnGuildInfoAck[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    if (!r.Read(char_id) || !r.Read(key))
    {
        spdlog::warn("OnGuildInfoAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    auto tchar = ctx.chars->Find(char_id);
    if (!tchar) co_return;
    std::uint32_t guild_id = 0, actual_key = 0;
    {
        std::lock_guard g(tchar->lock);
        actual_key = tchar->key;
        guild_id   = tchar->guild_id;
    }
    if (actual_key != key) co_return;
    if (guild_id == 0)
    {
        co_await senders::SendMwGuildInfoReq(peer, char_id, key,
            guild::kNotFound, nullptr);
        co_return;
    }

    auto guild = ctx.guilds->Find(guild_id);
    if (!guild)
    {
        co_await senders::SendMwGuildInfoReq(peer, char_id, key,
            guild::kNotFound, nullptr);
        co_return;
    }

    senders::GuildInfoPayload p;
    p.guild_id = guild_id;
    {
        std::lock_guard gl(guild->lock);
        p.name              = guild->name;
        p.establish_time    = guild->establish_time;
        p.member_count      = static_cast<std::uint16_t>(guild->members.size());
        p.level             = guild->level;
        p.fame              = guild->fame;
        p.fame_color        = guild->fame_color;
        p.gi                = guild->gi;
        p.exp               = guild->exp;
        p.guild_points      = guild->guild_points;
        p.status            = guild->status;
        p.gold              = guild->gold;
        p.silver            = guild->silver;
        p.cooper            = guild->cooper;
        p.pvp_total_point   = guild->pvp_total_point;
        p.pvp_useable_point = guild->pvp_useable_point;
        p.pvp_month_point   = guild->pvp_month_point;
        p.rank_total        = guild->rank_total;
        p.rank_month        = guild->rank_month;
        p.stat_level        = guild->stat_level;
        p.stat_point        = guild->stat_point;
        p.stat_exp          = guild->stat_exp;

        // Chief — legacy refuses kNotFound if no chief exists.
        const TGuildMember* chief = guild->FindMember(guild->chief_char_id);
        if (chief)
        {
            p.chief_name = chief->name;
            p.chief_peer = chief->peer;
        }

        // Requester's duty/peer.
        if (const TGuildMember* m = guild->FindMember(char_id))
        {
            p.requester_duty = m->duty;
            p.requester_peer = m->peer;
        }
        // else (legacy tactics-member path) — leaves both at 0,
        // matches legacy SSHandler.cpp:3894 default fall-through.

        // Vice-chief slots: collect up to 2; the sender pads with
        // empty strings (legacy NAME_NULL) to always emit 2.
        std::size_t vc = 0;
        for (const auto& m : guild->members)
        {
            if (vc >= p.vice_chief_names.size()) break;
            if (m.duty == guild::kDutyViceChief)
                p.vice_chief_names[vc++] = m.name;
        }

        // Most-recent article title — legacy m_strArticleTitle.
        if (!guild->articles.empty())
            p.article_title = guild->articles.back().title;
    }

    // Max member + level exp cap from the guild_levels cache.
    if (ctx.guild_levels)
    {
        if (const auto* lvl = ctx.guild_levels->Find(p.level))
        {
            p.max_member = lvl->max_count;
            p.level_exp  = lvl->exp;
        }
    }

    if (p.chief_name.empty())
    {
        co_await senders::SendMwGuildInfoReq(peer, char_id, key,
            guild::kNotFound, nullptr);
        co_return;
    }

    co_await senders::SendMwGuildInfoReq(peer, char_id, key,
        guild::kSuccess, &p);
    spdlog::info("OnGuildInfoAck[{}]: char_id={} guild_id={} info → "
                 "level={} fame={} members={}/{} pvp={}",
        ip, char_id, guild_id, p.level, p.fame, p.member_count,
        p.max_member, p.pvp_total_point);
}

// --- W3a-8 article board ------------------------------------------

namespace {

// Build the wire-shaped article rows from TGuild.articles under
// the guild lock. Date string is formatted yyyy-mm-dd (legacy
// CTime::Format) — clients parse the literal string.
std::vector<senders::GuildArticleRow>
BuildArticleRows(const TGuild& guild)
{
    std::vector<senders::GuildArticleRow> rows;
    rows.reserve(guild.articles.size());
    for (const auto& a : guild.articles)
    {
        senders::GuildArticleRow row;
        row.id     = a.id;
        row.duty   = a.duty;
        row.writer = a.writer;
        row.title  = a.title;
        row.body   = a.body;
        // gmtime + 11-byte buffer covers "YYYY-MM-DD\0".
        const std::time_t t = static_cast<std::time_t>(a.time_unix);
        std::tm tm{};
#ifdef _WIN32
        gmtime_s(&tm, &t);
#else
        gmtime_r(&t, &tm);
#endif
        char buf[40] = {0};
        std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
        row.date = buf;
        rows.push_back(std::move(row));
    }
    return rows;
}

// Resolve the requester's guild + return the locked snapshot the
// article handlers need. Returns (nullptr, 0) when any of the
// legacy gates (char missing, key mismatch, no guild) fail.
// `GuildHandle` itself is defined in the top-of-file anonymous
// namespace so other handler blocks (W3a-12) can use it via
// forward-declared `ResolveRequesterGuild`.
GuildHandle ResolveRequesterGuild(const HandlerContext& ctx,
                                   std::uint32_t char_id,
                                   std::uint32_t key)
{
    GuildHandle out;
    if (!ctx.chars || !ctx.guilds) return out;
    auto tchar = ctx.chars->Find(char_id);
    if (!tchar) return out;
    std::uint32_t actual_key = 0, gid = 0;
    {
        std::lock_guard g(tchar->lock);
        actual_key = tchar->key;
        gid        = tchar->guild_id;
    }
    if (actual_key != key || gid == 0) return out;
    out.guild_id = gid;
    out.guild    = ctx.guilds->Find(gid);
    return out;
}

} // namespace

boost::asio::awaitable<void>
OnGuildArticleListAck(std::shared_ptr<PeerSession> peer,
                      std::vector<std::byte>       body,
                      const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    if (!r.Read(char_id) || !r.Read(key))
    {
        spdlog::warn("OnGuildArticleListAck[{}]: short body", ip);
        co_return;
    }
    auto h = ResolveRequesterGuild(ctx, char_id, key);
    if (!h.guild) co_return;

    std::vector<senders::GuildArticleRow> rows;
    {
        std::lock_guard gl(h.guild->lock);
        rows = BuildArticleRows(*h.guild);
    }
    co_await senders::SendMwGuildArticleListReq(peer, char_id, key, rows);
    spdlog::info("OnGuildArticleListAck[{}]: char_id={} guild_id={} "
                 "→ {} articles",
        ip, char_id, h.guild_id, rows.size());
}

boost::asio::awaitable<void>
OnGuildArticleAddAck(std::shared_ptr<PeerSession> peer,
                     std::vector<std::byte>       body,
                     const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    std::string   title, body_text;
    if (!r.Read(char_id) || !r.Read(key) || !r.ReadString(title) ||
        !r.ReadString(body_text))
    {
        spdlog::warn("OnGuildArticleAddAck[{}]: short body", ip);
        co_return;
    }
    // Legacy SSHandler.cpp:4172-4174: caps are silent-drop, not a
    // FAIL reply — the client UI shouldn't have let this through
    // anyway. We follow legacy.
    if (title.empty() || title.size() > guild::kMaxBoardTitle ||
        body_text.size() > guild::kMaxBoardText)
        co_return;

    auto h = ResolveRequesterGuild(ctx, char_id, key);
    if (!h.guild) co_return;

    // Add under the guild lock + capture state for the persistence
    // call (no I/O while holding the lock).
    std::uint32_t article_id = 0;
    std::uint8_t  writer_duty = 0;
    std::string   writer_name;
    std::uint32_t now_unix = static_cast<std::uint32_t>(std::time(nullptr));
    std::vector<senders::GuildArticleRow> rows;
    {
        std::lock_guard gl(h.guild->lock);
        if (h.guild->articles.size() >= guild::kMaxGuildArticleCount)
        {
            // Cap reached — legacy returns GUILD_FAIL without a
            // LIST refresh. Reply outside the lock.
        }
        else
        {
            const TGuildMember* writer = h.guild->FindMember(char_id);
            if (writer)
            {
                writer_duty = writer->duty;
                writer_name = writer->name;
                ++h.guild->article_index;
                article_id  = h.guild->article_index;
                TGuildArticle a;
                a.id        = article_id;
                a.duty      = writer_duty;
                a.writer    = writer_name;
                a.title     = title;
                a.body      = body_text;
                a.time_unix = now_unix;
                h.guild->articles.push_back(std::move(a));
                rows = BuildArticleRows(*h.guild);
            }
        }
    }

    if (article_id == 0)
    {
        co_await senders::SendMwGuildArticleAddReq(peer, char_id, key,
            guild::kFail);
        co_return;
    }

    if (ctx.guild_repo)
    {
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [repo = ctx.guild_repo, gid = h.guild_id, article_id,
             writer_duty, writer_name, title, body_text, now_unix] {
                repo->AddArticle(gid, article_id, writer_duty, writer_name,
                    title, body_text, now_unix);
            });
    }
    co_await senders::SendMwGuildArticleAddReq(peer, char_id, key,
        guild::kSuccess);
    co_await senders::SendMwGuildArticleListReq(peer, char_id, key, rows);
    spdlog::info("OnGuildArticleAddAck[{}]: char_id={} guild_id={} "
                 "article_id={} title.len={}",
        ip, char_id, h.guild_id, article_id, title.size());
}

boost::asio::awaitable<void>
OnGuildArticleDelAck(std::shared_ptr<PeerSession> peer,
                     std::vector<std::byte>       body,
                     const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0, article_id = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(article_id))
    {
        spdlog::warn("OnGuildArticleDelAck[{}]: short body", ip);
        co_return;
    }

    auto h = ResolveRequesterGuild(ctx, char_id, key);
    if (!h.guild) co_return;

    bool removed = false;
    std::vector<senders::GuildArticleRow> rows;
    {
        std::lock_guard gl(h.guild->lock);
        for (auto it = h.guild->articles.begin();
             it != h.guild->articles.end(); ++it)
        {
            if (it->id == article_id)
            {
                h.guild->articles.erase(it);
                removed = true;
                break;
            }
        }
        if (removed) rows = BuildArticleRows(*h.guild);
    }

    if (!removed)
    {
        co_await senders::SendMwGuildArticleDelReq(peer, char_id, key,
            guild::kFail);
        co_return;
    }
    if (ctx.guild_repo)
    {
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [repo = ctx.guild_repo, gid = h.guild_id, article_id] {
                repo->DelArticle(gid, article_id);
            });
    }
    co_await senders::SendMwGuildArticleDelReq(peer, char_id, key,
        guild::kSuccess);
    co_await senders::SendMwGuildArticleListReq(peer, char_id, key, rows);
    spdlog::info("OnGuildArticleDelAck[{}]: char_id={} guild_id={} "
                 "article_id={} removed", ip, char_id, h.guild_id,
        article_id);
}

boost::asio::awaitable<void>
OnGuildArticleUpdateAck(std::shared_ptr<PeerSession> peer,
                        std::vector<std::byte>       body,
                        const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0, article_id = 0;
    std::string   title, body_text;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(article_id) ||
        !r.ReadString(title) || !r.ReadString(body_text))
    {
        spdlog::warn("OnGuildArticleUpdateAck[{}]: short body", ip);
        co_return;
    }
    if (title.empty() || title.size() > guild::kMaxBoardTitle ||
        body_text.size() > guild::kMaxBoardText)
        co_return;

    auto h = ResolveRequesterGuild(ctx, char_id, key);
    if (!h.guild) co_return;

    bool updated = false;
    std::vector<senders::GuildArticleRow> rows;
    {
        std::lock_guard gl(h.guild->lock);
        for (auto& a : h.guild->articles)
        {
            if (a.id == article_id)
            {
                a.title = title;
                a.body  = body_text;
                updated = true;
                break;
            }
        }
        if (updated) rows = BuildArticleRows(*h.guild);
    }
    if (!updated)
    {
        co_await senders::SendMwGuildArticleUpdateReq(peer, char_id, key,
            guild::kFail);
        co_return;
    }
    if (ctx.guild_repo)
    {
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [repo = ctx.guild_repo, gid = h.guild_id, article_id, title,
             body_text] {
                repo->UpdateArticle(gid, article_id, title, body_text);
            });
    }
    co_await senders::SendMwGuildArticleUpdateReq(peer, char_id, key,
        guild::kSuccess);
    co_await senders::SendMwGuildArticleListReq(peer, char_id, key, rows);
    spdlog::info("OnGuildArticleUpdateAck[{}]: char_id={} guild_id={} "
                 "article_id={} updated", ip, char_id, h.guild_id,
        article_id);
}

// --- W3a-7 member list refresh ------------------------------------

boost::asio::awaitable<void>
OnGuildMemberListAck(std::shared_ptr<PeerSession> peer,
                     std::vector<std::byte>       body,
                     const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.guilds)
    {
        spdlog::warn("OnGuildMemberListAck[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    if (!r.Read(char_id) || !r.Read(key))
    {
        spdlog::warn("OnGuildMemberListAck[{}]: short body ({} bytes)",
            ip, body.size());
        co_return;
    }

    auto tchar = ctx.chars->Find(char_id);
    if (!tchar) co_return;
    std::uint32_t guild_id = 0, actual_key = 0;
    {
        std::lock_guard g(tchar->lock);
        actual_key = tchar->key;
        guild_id   = tchar->guild_id;
    }
    if (actual_key != key) co_return;

    if (guild_id == 0)
    {
        // Legacy SSHandler.cpp:3849 → SendMW_GUILDMEMBERLIST_REQ
        // with kNotFound + null guild → no payload tail.
        co_await senders::SendMwGuildMemberListReq(peer, char_id, key,
            guild::kNotFound, 0, std::string{}, {});
        co_return;
    }

    auto guild = ctx.guilds->Find(guild_id);
    if (!guild)
    {
        co_await senders::SendMwGuildMemberListReq(peer, char_id, key,
            guild::kNotFound, 0, std::string{}, {});
        co_return;
    }

    // Build the row snapshot under guild.lock — we copy out
    // before doing any CharRegistry lookups so we don't hold the
    // guild lock across multiple shared_mutex acquisitions.
    std::vector<senders::GuildMemberListRow> rows;
    std::string guild_name;
    {
        std::lock_guard gl(guild->lock);
        guild_name = guild->name;
        rows.reserve(guild->members.size());
        for (const auto& m : guild->members)
        {
            senders::GuildMemberListRow row;
            row.char_id             = m.char_id;
            row.name                = m.name;
            row.level               = m.level;
            row.klass               = m.klass;
            row.duty                = m.duty;
            row.peer                = m.peer;
            row.online              = 0;   // filled below
            row.region              = 0;   // filled below
            row.castle              = m.castle;
            row.camp                = m.camp;
            row.tactics             = m.tactics;
            row.war_country         = m.war_country;
            row.connected_date_unix = m.connected_date_unix;
            rows.push_back(std::move(row));
        }
    }

    // Per-row online + region lookup. Each TChar fetch goes
    // through CharRegistry's shared_mutex — bounded by O(N
    // members), typical guild < 200, so this is cheap.
    for (auto& row : rows)
    {
        if (auto online_char = ctx.chars->Find(row.char_id))
        {
            row.online = 1;
            // Legacy reads pChar->m_dwRegion; TChar doesn't carry
            // region in W3a-7 (it'll land with the world-area
            // handlers in W5+ castle work). Stay at zero for now;
            // clients render that as "unknown location" which is
            // acceptable until the matching state ships.
            //
            // The legacy also overrides the cached member level
            // with pChar->m_bLevel when online — refresh from the
            // live TChar so level-up isn't stale.
            std::lock_guard g(online_char->lock);
            if (online_char->level != 0) row.level = online_char->level;
        }
    }

    co_await senders::SendMwGuildMemberListReq(peer, char_id, key,
        guild::kSuccess, guild_id, guild_name, rows);

    spdlog::info("OnGuildMemberListAck[{}]: char_id={} guild_id={} "
                 "members={} → MEMBERLIST_REQ",
        ip, char_id, guild_id, rows.size());
}

// --- W3a-6 invite flow --------------------------------------------

namespace {

// Helper for the JOIN-reply error branches that pass zeros for
// every guild meta field. Used by both invite handlers.
boost::asio::awaitable<void>
SendJoinError(std::shared_ptr<PeerSession> p,
              std::uint32_t                char_id,
              std::uint32_t                key,
              std::uint8_t                 result,
              std::uint8_t                 max_member = 0)
{
    co_await senders::SendMwGuildJoinReq(p, char_id, key, result,
        /*guild_id=*/0, /*fame=*/0, /*fame_color=*/0,
        /*guild_name=*/std::string{}, /*member_id=*/0,
        /*member_name=*/std::string{}, max_member);
}

} // namespace

boost::asio::awaitable<void>
OnGuildInviteAck(std::shared_ptr<PeerSession> peer,
                 std::vector<std::byte>       body,
                 const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.guilds || !ctx.peers)
    {
        spdlog::warn("OnGuildInviteAck[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    std::string   target_name;
    if (!r.Read(char_id) || !r.Read(key) || !r.ReadString(target_name))
    {
        spdlog::warn("OnGuildInviteAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    auto requester = ctx.chars->Find(char_id);
    if (!requester) co_return;
    std::uint32_t guild_id = 0, actual_key = 0;
    std::uint8_t  requester_country = 0;
    std::string   requester_name;
    {
        std::lock_guard g(requester->lock);
        actual_key        = requester->key;
        guild_id          = requester->guild_id;
        requester_country = requester->country;
        requester_name    = requester->name;
    }
    if (actual_key != key) co_return;
    if (guild_id == 0)
    {
        co_await SendJoinError(peer, char_id, key, guild::kNotFound);
        co_return;
    }

    auto guild = ctx.guilds->Find(guild_id);
    if (!guild)
    {
        co_await SendJoinError(peer, char_id, key, guild::kNotFound);
        co_return;
    }

    // Member-cap check (legacy CanAddMember via m_pTLEVEL).
    std::string   guild_name;
    std::uint8_t  max_member = 0;
    bool          is_disorg  = false;
    std::size_t   member_cnt = 0;
    {
        std::lock_guard gl(guild->lock);
        is_disorg  = (guild->disorg != 0);
        guild_name = guild->name;
        member_cnt = guild->members.size();
        if (ctx.guild_levels)
            if (const auto* lvl = ctx.guild_levels->Find(guild->level))
                max_member = lvl->max_count;
    }
    if (is_disorg)
    {
        co_await SendJoinError(peer, char_id, key, guild::kNotFound);
        co_return;
    }
    if (max_member > 0 && member_cnt >= max_member)
    {
        co_await SendJoinError(peer, char_id, key, guild::kMemberFull,
            max_member);
        spdlog::info("OnGuildInviteAck[{}]: char_id={} guild_id={} cap "
                     "{}/{} — refusing invite to '{}'",
            ip, char_id, guild_id, member_cnt, max_member, target_name);
        co_return;
    }

    auto target = ctx.chars->FindByName(target_name);
    if (!target)
    {
        // Target offline — legacy silently drops (no error reply).
        // The client times out on its end. We log for ops visibility.
        spdlog::info("OnGuildInviteAck[{}]: target='{}' offline — drop",
            ip, target_name);
        co_return;
    }

    std::uint8_t  target_country = 0;
    std::uint32_t target_guild   = 0;
    std::uint32_t target_char_id = 0;
    std::uint32_t target_key     = 0;
    std::uint8_t  target_msi     = 0;
    {
        std::lock_guard g(target->lock);
        target_country = target->country;
        target_guild   = target->guild_id;
        target_char_id = target->char_id;
        target_key     = target->key;
        target_msi     = target->main_server_id;
    }

    if (target_country != requester_country)
    {
        co_await SendJoinError(peer, char_id, key, guild::kFail);
        co_return;
    }
    if (target_guild != 0)
    {
        co_await SendJoinError(peer, char_id, key, guild::kHaveGuild);
        co_return;
    }

    // Forward the invite to the target's main map peer.
    std::shared_ptr<PeerSession> target_peer;
    for (auto& p : ctx.peers->Snapshot())
    {
        if (static_cast<std::uint8_t>(p->Wid() & 0xFF) == target_msi)
        {
            target_peer = std::move(p);
            break;
        }
    }
    if (!target_peer)
    {
        spdlog::info("OnGuildInviteAck[{}]: target='{}' main_server_id={} "
                     "peer offline — drop", ip, target_name, target_msi);
        co_return;
    }

    co_await senders::SendMwGuildInviteReq(target_peer, target_char_id,
        target_key, guild_name, char_id, requester_name);
    spdlog::info("OnGuildInviteAck[{}]: char_id={} invited target='{}' "
                 "(char_id={}) to guild_id={} '{}'",
        ip, char_id, target_name, target_char_id, guild_id, guild_name);
}

boost::asio::awaitable<void>
OnGuildInviteAnswerAck(std::shared_ptr<PeerSession> peer,
                       std::vector<std::byte>       body,
                       const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.guilds || !ctx.peers)
    {
        spdlog::warn("OnGuildInviteAnswerAck[{}]: registries not wired",
            ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    std::uint8_t  answer = 0;
    std::uint32_t inviter_id = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(answer) ||
        !r.Read(inviter_id))
    {
        spdlog::warn("OnGuildInviteAnswerAck[{}]: short body ({} bytes)",
            ip, body.size());
        co_return;
    }

    auto invited = ctx.chars->Find(char_id);
    if (!invited) co_return;
    std::uint32_t invited_key = 0;
    std::string   invited_name;
    {
        std::lock_guard g(invited->lock);
        invited_key  = invited->key;
        invited_name = invited->name;
    }
    if (invited_key != key) co_return;

    auto inviter = ctx.chars->Find(inviter_id);
    if (!inviter) co_return;
    std::uint32_t inviter_guild_id = 0;
    std::uint32_t inviter_key      = 0;
    std::uint8_t  inviter_msi      = 0;
    {
        std::lock_guard g(inviter->lock);
        inviter_guild_id = inviter->guild_id;
        inviter_key      = inviter->key;
        inviter_msi      = inviter->main_server_id;
    }

    std::shared_ptr<PeerSession> inviter_peer;
    if (inviter_msi != 0)
    {
        for (auto& p : ctx.peers->Snapshot())
        {
            if (static_cast<std::uint8_t>(p->Wid() & 0xFF) == inviter_msi)
            {
                inviter_peer = std::move(p);
                break;
            }
        }
    }
    if (!inviter_peer)
    {
        spdlog::info("OnGuildInviteAnswerAck[{}]: inviter_id={} offline "
                     "— drop", ip, inviter_id);
        co_return;
    }

    if (answer != guild::kAskYes)
    {
        // Notify chief that target declined. Result byte carries
        // the inviter's answer code so the chief's client can show
        // the right message.
        co_await SendJoinError(inviter_peer, inviter_id, inviter_key,
            /*result=*/answer);
        spdlog::info("OnGuildInviteAnswerAck[{}]: char_id={} declined "
                     "invite from {} (answer={})", ip, char_id, inviter_id,
            answer);
        co_return;
    }

    // YES path. Re-validate every gate the W3a-6 OnGuildInviteAck
    // already checked — the world state may have changed during
    // the dialog (chief disbanded, member cap filled, target
    // joined another guild via a parallel invite).
    if (inviter_guild_id == 0)
    {
        co_await SendJoinError(peer,          char_id, key,         guild::kNotFound);
        co_await SendJoinError(inviter_peer,  inviter_id, inviter_key, guild::kNotFound);
        co_return;
    }
    auto guild = ctx.guilds->Find(inviter_guild_id);
    if (!guild)
    {
        co_await SendJoinError(peer,         char_id, key,         guild::kNotFound);
        co_await SendJoinError(inviter_peer, inviter_id, inviter_key, guild::kNotFound);
        co_return;
    }

    std::uint8_t invited_level = 0;
    {
        std::lock_guard g(invited->lock);
        invited_level = invited->level;
    }

    // W3a-13: shared promotion path — validates disorg/have/full
    // gates, atomically adds member + sets TChar.guild_id, then
    // persists via AddMember. The JOIN_REQ broadcast stays here
    // because the dual reply targets differ between paths.
    auto pr = co_await TryPromoteIntoGuild(ctx, guild, inviter_guild_id,
        invited, char_id, invited_name, invited_level);

    if (pr.result == guild::kNotFound)
    {
        co_await SendJoinError(peer,         char_id, key,         guild::kNotFound);
        co_await SendJoinError(inviter_peer, inviter_id, inviter_key, guild::kNotFound);
        co_return;
    }
    if (pr.result == guild::kHaveGuild)
    {
        co_await SendJoinError(peer,         char_id, key,         guild::kHaveGuild);
        co_await SendJoinError(inviter_peer, inviter_id, inviter_key, guild::kHaveGuild);
        co_return;
    }
    if (pr.result == guild::kMemberFull)
    {
        co_await SendJoinError(peer,         char_id, key,         guild::kMemberFull, pr.max_member);
        co_await SendJoinError(inviter_peer, inviter_id, inviter_key, guild::kMemberFull, pr.max_member);
        co_return;
    }

    // Two GUILDJOIN_REQ replies — one to each side. Both carry
    // the full guild meta so each client can render the join
    // notification. Legacy SSHandler.cpp:5109 fires
    // SendMW_GUILDJOIN_REQ with GUILD_JOIN_SUCCESS (=15), not the
    // generic GUILD_SUCCESS (=0). W3a-6 used kSuccess here; that
    // was a silent wire bug — round-trip tests never caught it
    // because both sides just echoed the byte. Fixed to
    // kJoinSuccess so real legacy clients see the right result
    // code on the join notification.
    co_await senders::SendMwGuildJoinReq(peer, char_id, key,
        guild::kJoinSuccess, inviter_guild_id, pr.fame, pr.fame_color,
        pr.guild_name, char_id, invited_name, /*max_member=*/0);
    co_await senders::SendMwGuildJoinReq(inviter_peer, inviter_id,
        inviter_key, guild::kJoinSuccess, inviter_guild_id, pr.fame,
        pr.fame_color, pr.guild_name, char_id, invited_name,
        /*max_member=*/0);

    spdlog::info("OnGuildInviteAnswerAck[{}]: char_id={} ('{}') joined "
                 "guild_id={} '{}'",
        ip, char_id, invited_name, inviter_guild_id, pr.guild_name);
}

} // namespace tworldsvr::handlers
