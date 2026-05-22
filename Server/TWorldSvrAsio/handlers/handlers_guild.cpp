#include "handlers.h"
#include "../senders/senders.h"
#include "../services/guild_broadcast.h"
#include "../services/guild_constants.h"
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

} // namespace tworldsvr::handlers
