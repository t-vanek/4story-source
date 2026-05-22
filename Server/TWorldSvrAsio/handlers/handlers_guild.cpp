#include "handlers.h"
#include "../senders/senders.h"
#include "../services/guild_broadcast.h"
#include "../services/guild_constants.h"
#include "../services/guild_peerage.h"
#include "../wire_codec.h"

#include "MessageId.h"

#include "fourstory/db/co_offload.h"

#include <spdlog/spdlog.h>

#include <cstdint>
#include <ctime>
#include <mutex>

namespace tworldsvr::handlers {

namespace {

bool SkipCabinet(wire::Reader&, std::uint16_t count, const std::string& ip)
{
    if (count == 0) return true;
    spdlog::info("OnGuildLoadAck[{}]: cabinet wCount={} — skipping items "
                 "(W3a-4b will parse them)", ip, count);
    return true;
}

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
struct GuildHandle
{
    std::shared_ptr<TGuild> guild;
    std::uint32_t           guild_id = 0;
};

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
    std::uint32_t invited_guild = 0;
    {
        std::lock_guard g(invited->lock);
        invited_key   = invited->key;
        invited_name  = invited->name;
        invited_guild = invited->guild_id;
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

    // Snapshot guild meta + run the in-memory member-add under
    // guild.lock. We also fix up the target's TChar.guild_id
    // back-pointer inside the same critical region — keeping
    // the registry coherent matters more than spinning on the
    // char lock here.
    std::string  guild_name;
    std::uint32_t fame = 0, fame_color = 0;
    std::uint32_t chief_id = 0;
    std::uint8_t max_member = 0;
    bool collide_full     = false;
    bool collide_have     = false;
    bool collide_disorg   = false;
    {
        std::lock_guard gl(guild->lock);
        collide_disorg = (guild->disorg != 0);
        if (!collide_disorg)
        {
            guild_name = guild->name;
            fame       = guild->fame;
            fame_color = guild->fame_color;
            chief_id   = guild->chief_char_id;
            if (ctx.guild_levels)
                if (const auto* lvl = ctx.guild_levels->Find(guild->level))
                    max_member = lvl->max_count;
            if (invited_guild != 0)              collide_have = true;
            else if (max_member > 0 &&
                     guild->members.size() >= max_member) collide_full = true;
            else
            {
                TGuildMember m;
                m.char_id  = char_id;
                m.guild_id = inviter_guild_id;
                m.duty     = guild::kDutyNone;
                m.peer     = 0;
                m.name     = invited_name;
                guild->members.push_back(std::move(m));
            }
        }
    }
    if (collide_disorg)
    {
        co_await SendJoinError(peer,         char_id, key,         guild::kNotFound);
        co_await SendJoinError(inviter_peer, inviter_id, inviter_key, guild::kNotFound);
        co_return;
    }
    if (collide_have)
    {
        co_await SendJoinError(peer,         char_id, key,         guild::kHaveGuild);
        co_await SendJoinError(inviter_peer, inviter_id, inviter_key, guild::kHaveGuild);
        co_return;
    }
    if (collide_full)
    {
        co_await SendJoinError(peer,         char_id, key,         guild::kMemberFull, max_member);
        co_await SendJoinError(inviter_peer, inviter_id, inviter_key, guild::kMemberFull, max_member);
        co_return;
    }

    // Wire the invited char to their new guild.
    {
        std::lock_guard g(invited->lock);
        invited->guild_id = inviter_guild_id;
    }

    // Persist via the W3a-4c IGuildRepository::AddMember.
    if (ctx.guild_repo)
    {
        std::uint8_t invited_level = 0;
        {
            std::lock_guard g(invited->lock);
            invited_level = invited->level;
        }
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [repo = ctx.guild_repo, char_id, inviter_guild_id,
             invited_level] {
                repo->AddMember(char_id, inviter_guild_id, invited_level,
                    guild::kDutyNone);
            });
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
        guild::kJoinSuccess, inviter_guild_id, fame, fame_color, guild_name,
        char_id, invited_name, /*max_member=*/0);
    co_await senders::SendMwGuildJoinReq(inviter_peer, inviter_id,
        inviter_key, guild::kJoinSuccess, inviter_guild_id, fame, fame_color,
        guild_name, char_id, invited_name, /*max_member=*/0);

    spdlog::info("OnGuildInviteAnswerAck[{}]: char_id={} ('{}') joined "
                 "guild_id={} '{}' (chief={})",
        ip, char_id, invited_name, inviter_guild_id, guild_name, chief_id);
}

} // namespace tworldsvr::handlers
