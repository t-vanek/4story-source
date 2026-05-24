#pragma once

// Handler dispatch for TWorldSvrAsio.
//
// W1 brought up the scaffold; W2 added char registry + first
// char-lifecycle handlers; W3a-1 added guild infrastructure +
// OnGuildLoadAck; **W3a-2** introduces the PeerSession wrapper,
// PeerRegistry, the sender layer (SSSender counterpart), and the
// RW relay handshake (OnRelaysvrReq).
//
// Every handler now receives a `shared_ptr<PeerSession>` instead
// of a raw `WorldSession` — that gives handlers access to the
// map-server's wID + nation flag set during the relay handshake
// without going through PeerRegistry per packet. PeerRegistry is
// still in HandlerContext for the cross-peer-lookup case (e.g.
// guild member-add fan-out in W3a-3).

#include "../peer_session.h"
#include "../services/bow_registry.h"
#include "../services/br_registry.h"
#include "../services/char_registry.h"
#include "../services/guild_level_cache.h"
#include "../services/guild_registry.h"
#include "../services/guild_repository.h"
#include "../services/guild_wanted_registry.h"
#include "../services/guild_tactics_wanted_registry.h"
#include "../services/party_registry.h"
#include "../services/corps_registry.h"
#include "../services/tms_registry.h"
#include "../services/friend_repository.h"
#include "../services/peer_registry.h"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/thread_pool.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace tworldsvr {

struct HandlerContext
{
    boost::asio::io_context*  io         = nullptr;

    // Worker pool for synchronous SOCI calls. Handlers offload DB
    // roundtrips here via fourstory::db::CoOffloadIf. nullptr →
    // run SOCI in-line on the io_context (test fallback).
    boost::asio::thread_pool* db_pool    = nullptr;

    // Cluster-wide registries. Owned by main; non-null in their
    // owning-phase (chars: W2+, guilds: W3a-1+, peers: W3a-2+).
    CharRegistry*             chars      = nullptr;
    GuildRegistry*            guilds     = nullptr;
    PeerRegistry*             peers      = nullptr;

    IGuildRepository*         guild_repo = nullptr;

    // W4-15: friend / friend-group / soulmate load source. Read at
    // char-online (OnAddCharAck) to hydrate TChar.friends /
    // friend_groups / soulmate. nullptr → no persistence (the
    // in-memory registry is the only store, e.g. the no-[database]
    // dev path + tests that don't seed a fake).
    IFriendRepository*        friend_repo = nullptr;

    // W3a-4d: TGUILDCHART mirror loaded at boot. Handlers consult
    // it for per-level caps (member cap, cabinet slots, peerage
    // slot limits). Read-only after main wires it; nullptr means
    // "not loaded" (dev path without [database] + no fake seed).
    const GuildLevelCache*    guild_levels = nullptr;

    // W3a-11: cluster-wide list of "we are recruiting" postings.
    // One entry per guild, filtered cross-country on read. Owned
    // by main; non-null in W3a-11+ deploys.
    GuildWantedRegistry*      guild_wanted = nullptr;

    // W3a-31: cluster-wide tactics-recruitment postings. Multiple
    // entries per guild (vs. one for guild_wanted), each with a
    // globally-unique id. Owned by main; non-null in W3a-31+.
    GuildTacticsWantedRegistry* guild_tactics_wanted = nullptr;

    // W3b-1: cluster-wide party index. Owned by main; non-null in
    // W3b-1+ deploys. Party-flow handlers consult it for the
    // requester's existing-party state (chief / size gates).
    PartyRegistry*            parties    = nullptr;

    // W3c-1: cluster-wide corps index (party alliances under a
    // general). Owned by main; non-null in W3c-1+ deploys.
    CorpsRegistry*            corps      = nullptr;

    // W4-11: cluster-wide TMS conference index. Owned by main;
    // non-null in W4-11+ deploys.
    TmsRegistry*              tms        = nullptr;

    // W6-24: Bow battleground queue + scoreboard. Owned by main;
    // non-null in W6-24+ deploys.
    BowRegistry*              bow        = nullptr;

    // W6-25: Battle Royale queue + premade teams + map / mode votes.
    // Owned by main; non-null in W6-25+ deploys.
    BrRegistry*               br         = nullptr;

    // Cluster-nation flag (TCONTRY_A/B/N). Mirrors the legacy
    // CTWorldSvrModule::m_bNation. Loaded from TOML; advertised to
    // each peer in RW_RELAYSVR_ACK.
    std::uint8_t              nation     = 0;
};

namespace handlers {

// Top-level dispatch. The session arg now carries map-server
// identity (wID + nation) once RW_RELAYSVR_REQ has run.
boost::asio::awaitable<void> Dispatch(
    std::shared_ptr<PeerSession>  peer,
    std::uint16_t                 wId,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W2: char lifecycle (handlers_char.cpp) ------------------------

boost::asio::awaitable<void> OnAddCharAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

boost::asio::awaitable<void> OnCloseCharAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W4-20: login finalization (handlers_char.cpp) -----------------
//
// MW_ENTERSVR_ACK — the char finished loading on its map; world
// finalizes the cluster-side identity (name + base attrs + position
// + region, the bulk-set the incremental W3a-3/W4-8/W4-9 handlers
// later update) and fires the friend connect-presence fan-out
// (NotifyFriendsOnLogin) now that the name/region are known. The
// big CHARINFO_REQ composite reply (guild/duty/peer/castle), the
// relay CHANGEMAP, and the failure replies (DELCHAR / INVALIDCHAR /
// CONRESULT) are deferred — this slice covers the identity store +
// presence, which is what unblocks login presence.
//
// Wire (SSHandler.cpp:1218): DWORD char_id, key, STRING name,
//   BYTE level, real_sex, class, race, sex, face, hair, helmet_hide,
//   country, aid_country, DWORD region, BYTE channel, WORD map_id,
//   FLOAT pos_x, pos_y, pos_z, BYTE logout, save, result,
//   WORD title_id, DWORD rank_point, user_ip
boost::asio::awaitable<void> OnEnterSvrAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W3a-1: guild lifecycle (handlers_guild.cpp) -------------------

boost::asio::awaitable<void> OnGuildLoadAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// W3a-4: char chose to leave their guild. Map server already
// confirmed locally; world removes the member from the guild's
// in-memory member list, clears TChar.guild_id, and pings the
// originating peer with MW_GUILDLEAVE_REQ so it can forward the
// confirmation back to the client. **W3a-4b** wires the
// IGuildRepository::RemoveMember persistence step.
//
// Wire layout (SSHandler.cpp:3571): DWORD dwCharID, DWORD dwKey
boost::asio::awaitable<void> OnGuildLeaveAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W3a-4b: guild mutating handlers (handlers_guild.cpp) ---------

// Inbound from a map server's DB worker. The chief asked for the
// guild to (start | cancel) disbanding; world flips the flag,
// persists via IGuildRepository::SetDisorg, and ACKs the
// originating peer so the client sees the state change.
//
// Wire layout (SSHandler.cpp:3203):
//   DWORD dwCharID, DWORD dwKey, DWORD dwGuildID, BYTE bDisorg
boost::asio::awaitable<void> OnGuildDisorganizationReq(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// Inbound from a map server: chief promoted / demoted a member.
// World validates the requester is chief (& target isn't), runs
// the legacy gate (no double-vicechief, etc.), updates the
// member's bDuty, persists, and broadcasts MW_GUILDDUTY_REQ.
//
// Wire layout (SSHandler.cpp:3408):
//   DWORD dwCharID, DWORD dwKey, STRING strTarget, BYTE bDuty
boost::asio::awaitable<void> OnGuildDutyAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// Inbound from a map server: chief changed the guild's fame
// banner. Charges PvP points, updates dwFame + dwFameColor,
// broadcasts the new fame to every online guild member's main
// map peer. NoPoint result code if the guild can't afford it.
//
// Wire layout (SSHandler.cpp:4346):
//   DWORD dwCharID, DWORD dwKey, DWORD dwFame, DWORD dwFameColor
boost::asio::awaitable<void> OnGuildFameAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W3a-4c: more guild mutating handlers (handlers_guild.cpp) ----

// Inbound from a map server: chief kicked a member by name.
// World removes the member, clears their TChar.guild_id, replies
// to the requester AND to the kicked char's main peer with
// MW_GUILDLEAVE_REQ(reason=kLeaveKick). Persistence via
// IGuildRepository::RemoveMember.
//
// Wire layout (SSHandler.cpp:3340):
//   DWORD dwCharID, DWORD dwKey, STRING strTarget
boost::asio::awaitable<void> OnGuildKickoutAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// Inbound from a map server: a member earned exp / gold / silver /
// cooper / pvp points for the guild. World validates (guild not
// max-level if exp>0, not disorg, not in tactics guild), applies
// the delta to TGuild totals + member's service score, replies
// with the success result code.
//
// Wire layout (SSHandler.cpp:4021):
//   DWORD dwCharID, DWORD dwKey, DWORD dwExp, DWORD dwGold,
//   DWORD dwSilver, DWORD dwCooper, DWORD dwPvPoint
boost::asio::awaitable<void> OnGuildContributionAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// Inbound from a map server's DB worker: persist a new guild
// member row. Pure DB write — the in-memory insert happens on
// the OnMW_GUILDINVITEANSWER_ACK side (W3a-5+). No reply.
//
// Wire layout (SSHandler.cpp:3317):
//   DWORD dwGuildID, DWORD dwCharID, BYTE bLevel, BYTE bDuty
boost::asio::awaitable<void> OnGuildMemberAddReq(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W3a-5: peerage + cabinet (handlers_guild.cpp) ----------------

// Inbound from a map server: chief promoted/demoted a member's
// peerage rank. World runs CheckPeerage against the requester's
// duty + guild level, updates the member's bPeer, persists, and
// broadcasts MW_GUILDPEER_REQ to both the requesting chief and
// (if online elsewhere) the target's main map peer.
//
// Wire layout (SSHandler.cpp:3500):
//   DWORD dwCharID, DWORD dwKey, STRING strTarget, BYTE bPeer
boost::asio::awaitable<void> OnGuildPeerAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W3a-7: member list refresh (handlers_guild.cpp) --------------

// Inbound from a map server: client opened the guild window and
// the map asks world for the canonical member list. World finds
// the requester's guild, builds a snapshot with one row per
// member (online bool + region pulled from the CharRegistry),
// and replies MW_GUILDMEMBERLIST_REQ.
//
// Wire layout (SSHandler.cpp:3830):
//   DWORD dwCharID, DWORD dwKey
boost::asio::awaitable<void> OnGuildMemberListAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W3a-13: PvP point persistence (handlers_guild.cpp) -----------

// Inbound from a map server's DB worker: PvP outcome on the map
// side changed a guild's PvP bank (total + useable + month).
// World mirrors the new values into the registry + persists via
// IGuildRepository::UpdatePvPoints. No reply (clients see the
// new values via the next OnGuildInfoAck refresh).
//
// Wire layout (SSHandler.cpp:10405):
//   DWORD dwGuildID, DWORD dwTotalPoint, DWORD dwUseablePoint,
//   DWORD dwMonthPoint
boost::asio::awaitable<void> OnGuildPvPointReq(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W3a-14: DB-side fan-in handlers (handlers_guild.cpp) ---------
//
// All five run when the DB server pushes a state change back to
// the world (admin tool, cross-server sync, scheduled job). Each
// is a thin "read wire fields → repo call" wrapper around an
// existing IGuildRepository method. No in-memory mutation: the
// authoritative state lives on the DB side; world's in-memory
// caches refresh via the next ACTIVECHARUPDATE / GUILDLOAD round.
// (DM_GUILDLEVEL_REQ defensively updates the registry's level
// field because it affects member-cap arithmetic for the
// peerage gate — keeping that stale would cascade silently.)

boost::asio::awaitable<void> OnGuildDutyReq(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

boost::asio::awaitable<void> OnGuildPeerReq(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

boost::asio::awaitable<void> OnGuildContributionReq(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

boost::asio::awaitable<void> OnGuildLevelReq(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

boost::asio::awaitable<void> OnGuildPointRewardReq(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W3a-15: fame + article DB fan-in (handlers_guild.cpp) --------
//
// FAME + 3 ARTICLE handlers extending the W3a-14 DB-side fan-in
// cohort. FAME defensively mirrors fame/fame_color into the
// registry (read by GuildInfoAck + Establish broadcasts).
// Article handlers skip the in-memory mirror — TGuild.articles is
// owned by the article_index counter incremented on
// OnGuildArticleAddAck; DB-pushed article rows arrive with an
// article_id chosen DB-side that might collide with the local
// counter, so we defer to the next OnGuildArticleListAck refresh
// (same behavior as legacy SSHandler.cpp:4201/4264/4323).

boost::asio::awaitable<void> OnGuildFameReq(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

boost::asio::awaitable<void> OnGuildArticleAddReq(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

boost::asio::awaitable<void> OnGuildArticleDelReq(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

boost::asio::awaitable<void> OnGuildArticleUpdateReq(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W3a-16: wanted/volunteer DB fan-in (handlers_guild.cpp) ------
//
// WANTED ADD/DEL + VOLUNTEERING ADD/DEL extending the W3a-14/15
// DB-side fan-in cohort. All 4 do a defensive in-memory mirror
// to GuildWantedRegistry — keeping it stale would cause the next
// player-driven LIST or "already applied" hint to show wrong
// data. The VOLUNTEERING pair filters on bType: kMember (=0)
// flows through to the registry, kTactics (=1) gets dropped
// with a deferred-log (tactics subsystem ships in W3a-* later).
//
// Bypasses AddApp's validation gates because the DB is
// authoritative for fan-in — if our local registry would reject
// the row (already-applied / wanted-expired / level-mismatch),
// we still persist + log the divergence. The next full reload
// reconciles.

boost::asio::awaitable<void> OnGuildWantedAddReq(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

boost::asio::awaitable<void> OnGuildWantedDelReq(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

boost::asio::awaitable<void> OnGuildVolunteeringReq(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

boost::asio::awaitable<void> OnGuildVolunteeringDelReq(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W3a-17: leave/kickout DB fan-in (handlers_guild.cpp) ---------
//
// Membership-lifecycle DB-side fan-in completing the pair that
// W3a-4c started with OnDM_GUILDMEMBERADD_REQ. Both drop the
// member from guild->members and clear the char's
// guild_id back-pointer, then persist via repo->RemoveMember.
// No wire reply: the legacy MW_GUILDLEAVE_REQ broadcast lives
// on the player-action path (OnGuildLeaveAck), not on this
// DB-driven fan-in.
//
// OnDM_GUILDLEAVE_REQ carries 4 fields (guildID, charID, bLeave,
// dwTime) where bLeave is the leave-type code and dwTime is the
// Unix-epoch second. We log both for audit but don't persist
// them separately — the modern SOCI repo has no leave-log table
// yet; the legacy SP `TGuildLeave` may write one on production
// schemas (deferred).

boost::asio::awaitable<void> OnGuildLeaveReq(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

boost::asio::awaitable<void> OnGuildKickoutReq(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W3a-18: guild establishment (handlers_guild.cpp) -------------
//
// The "create new guild" gameplay flow. Legacy splits this across
// 4 packets: map → MW_GUILDESTABLISH_ACK → world → DM_GUILDESTABLISH_REQ
// → DB → DM_GUILDESTABLISH_ACK → world → MW_GUILDESTABLISH_REQ →
// map. Our SOCI-direct architecture collapses it into a single
// coroutine: validate, repo->CreateGuild (returns new id),
// build TGuild in registry, persist chief membership, reply.
//
// Wire layout (SSHandler.cpp:3027):
//   DWORD dwCharID, DWORD dwKEY, STRING strGuildName
//
// Failure modes mirror legacy:
//   - char missing / key mismatch → silent drop
//   - char already in a guild  → bRet = kHaveGuild
//   - name length > kGuildMaxNameLen → bRet = kFail
//   - duplicate name (DB side) → bRet = kAlreadyGuildName
//   - other DB failure → bRet = kEstablishErr
boost::asio::awaitable<void> OnGuildEstablishAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W3a-20: vestigial DB-server confirmation echoes --------------
//
// Legacy TWorldSvr runs in a 3-process cluster (TWorld + DB + BATCH);
// every guild-mutating DM_*_REQ that TWorld sends to DB triggers a
// DM_*_ACK that BATCH fans back to every world shard so they can
// refresh in-memory state. Our SOCI-direct port collapses each
// REQ+ACK pair into a single coroutine (W3a-4b for DISORGANIZATION,
// W3a-10 for EXTINCTION, W3a-18 for ESTABLISH), so the ACK side is
// redundant in single-shard deployments.
//
// These stubs accept the ACK packets and drop them at info-level so
// hybrid deployments — where a legacy BATCH server is still
// broadcasting confirmations — don't pollute the log with
// "unknown wID" warnings on every guild mutation. No state change:
// the synchronous REQ-side handlers already did the work.

boost::asio::awaitable<void> OnGuildEstablishAckEcho(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

boost::asio::awaitable<void> OnGuildDisorganizationAckEcho(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

boost::asio::awaitable<void> OnGuildExtinctionAckEcho(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W3a-21: PvP record audit log (handlers_guild.cpp) ------------
//
// DB-side fan-in: receives a batched PvP record write request
// from the map server (PvP outcomes accumulated since the last
// flush) and persists each row via repo->LogPvPRecord. No reply,
// no in-memory mirror — this is a pure append-only audit log
// that nothing currently reads back through TWorldSvr (the
// existing MW_GUILDPVPRECORD_ACK in legacy reads from in-memory
// weekrecord state which we haven't ported yet — deferred).
//
// Wire layout (SSHandler.cpp:10456):
//   DWORD dwGuildID, DWORD dwMemberID, WORD wCount,
//   then wCount rows of:
//     DWORD dwDate, WORD wKillCount, WORD wDieCount,
//     DWORD points[kPvPEventCount]  (=8 DWORDs)
boost::asio::awaitable<void> OnPvPRecordReq(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W3a-22: full-row guild update fan-in (handlers_guild.cpp) ----
//
// Admin / bulk-load tool path. Overwrites the scalar columns of
// TGUILDTABLE for one guild in one shot. Mirrors legacy
// CSPGuildUpdate (SSHandler.cpp:2979). The wire packet also
// carries alliance + enemy ID lists (legacy stores them as
// comma-separated DWORDs in szAlliance + szEnemy columns); our
// schema doesn't model those yet so we parse + drain them with a
// log note and skip the persistence (deferred to W5+ war system).
//
// Defensive in-memory mirror: fame / guild_points / level /
// status / chief_char_id / gi / exp / disorg_time on the
// matching registry entry. Most are wire-truncated to BYTE
// (legacy quirk — bFame especially is BYTE here but DWORD in
// OnDM_GUILDLOAD_ACK).
//
// Wire layout (SSHandler.cpp:2979):
//   DWORD dwID, BYTE bFame, BYTE bGPoint, BYTE bLevel,
//   BYTE bStatus, DWORD dwChief, DWORD dwExp, DWORD dwGI,
//   DWORD dwTime,
//   BYTE allyCount, DWORD ally[allyCount],
//   BYTE enemyCount, DWORD enemy[enemyCount]
boost::asio::awaitable<void> OnGuildUpdateReq(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W3a-23: PvP record list reader (handlers_guild.cpp) ----------
//
// Player opens the guild "PvP statistics" UI panel; the map
// server forwards the open request to world via this handler.
// We snapshot every member of the requester's guild + emit
// their rolling weekrecord aggregate.
//
// Pairs with W3a-21 (OnPvPRecordReq) which persists the per-row
// audit log. The aggregate that this handler returns is fed by
// CalcWeekRecord summing the last 7 days of vRecord entries in
// legacy; our port hasn't ported the per-day fan-in (war-result
// path) yet, so weekrecord is zero-initialized at member load
// time and stays that way. The reader still works — clients
// just see an empty record per member until the fan-in lands.
//
// Wire layout (SSHandler.cpp:10379):
//   DWORD dwCharID, DWORD dwKey
boost::asio::awaitable<void> OnGuildPvPRecordAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W3a-24: per-period war-result fan-in (handlers_guild.cpp) ----
//
// Map server flushes a batch of PvP outcomes per period (one
// guild + one or more members per guild row). World accumulates
// the deltas into TGuildMember.weekrecord so the W3a-23 reader
// returns live data instead of zeros.
//
// Wire layout (SSHandler.cpp:10139):
//   DWORD win_guild_id, DWORD guild_point, WORD guild_count,
//     [guild_count times]:
//       DWORD guild_id, WORD record_count,
//         [record_count times]:
//           DWORD char_id, WORD kill_count, WORD die_count,
//           DWORD points[kPvPEventCount=8]
//
// Simplifications vs legacy:
// - Tactics-member branch is skipped (the tactics subsystem
//   hasn't ported yet; the W3a-23 reader's tactics shortcut is
//   on the same deferred list).
// - The dwWinGuildID + dwGuildPoint header fields (war-bonus
//   award path for winning B-country tactics-guilds) are read
//   and logged but not acted on — also tactics-subsystem
//   dependent.
// - No per-day vRecord history. weekrecord just accumulates;
//   `CalcWeekRecord`'s 7-day trim doesn't run. A production
//   deployment will want a periodic weekly clear (similar to
//   the W3a-19 wanted-board sweep) — deferred until per-day
//   vRecord lands or operators ask for it explicitly.
//
// Reply: none.
boost::asio::awaitable<void> OnLocalRecordAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W3a-26/W3a-37: cabinet flow (handlers_guild.cpp) -------------
//
// Guild storage vault. W3a-26 shipped LIST as an empty-list stub;
// W3a-37 adds the item codec so LIST emits real items + wires
// PUTIN (store an item) and TAKEOUT (withdraw / decrement).
//
// Wire layouts (SSHandler.cpp:3938/3960/3991):
//   LIST    : DWORD char_id, key
//   PUTIN   : DWORD char_id, key, slot_id, <WrapItem>
//   TAKEOUT : DWORD char_id, key, slot_id, BYTE count
boost::asio::awaitable<void> OnGuildCabinetListAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

boost::asio::awaitable<void> OnGuildCabinetPutinAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

boost::asio::awaitable<void> OnGuildCabinetTakeoutAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W3a-38: disband + point-reward player actions ---------------
//
// The map→world player-action entry points whose DB-side fan-in
// already landed earlier. DISORGANIZATION_ACK is the player
// requesting their guild disband (3-field wire; resolves
// guild_id from the char — vs. the W3a-4b DM_*_REQ 4-field
// fan-in). POINTREWARD_ACK is a chief granting PvP-useable
// points to a member by name (charges the bank, logs to
// point_log, persists, relays a gain toast to the recipient).
//
// Wire layouts (SSHandler.cpp:3171/10309):
//   DISORGANIZATION : DWORD char_id, key, BYTE disorg
//   POINTREWARD     : DWORD char_id, key, STRING target_name,
//                     DWORD point, STRING message
boost::asio::awaitable<void> OnGuildDisorganizationAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

boost::asio::awaitable<void> OnGuildPointRewardAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W3a-27: PvP point reward log reader (handlers_guild.cpp) -----
//
// Player opens the guild PvP-point-reward audit log UI. Pairs
// with the W3a-14 OnGuildPointRewardReq writer — that handler
// now appends to TGuild.point_log in addition to persisting to
// TGUILDPVPOINTREWARDTABLE, so this reader returns live data on
// the next call. Legacy SELECT TOP 50
// (CTBLGuildPvPointReward) trims to the latest 50 entries; our
// in-memory log is per-process and load-from-DB wiring lives
// in a later batch.
//
// Wire layout (SSHandler.cpp:10286):
//   DWORD dwCharID, DWORD dwKey
boost::asio::awaitable<void> OnGuildPointLogAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W3a-29: PvP-point gain/use fan-in (handlers_guild.cpp) -------
//
// Map server reports a PvP-point delta (gain or use). The owner
// is either a character (TOWNER_CHAR — relay the toast to the
// char's main map peer) or a guild (TOWNER_GUILD — apply the
// delta to the guild's total/useable/month banks + persist via
// repo->UpdatePvPoints). Mirrors legacy GainPvPoint /
// UsePvPoint semantics (TGuild.cpp:564/585): gain bumps month
// when TOTAL is set; use never touches month.
//
// Wire layout (SSHandler.cpp:10090):
//   BYTE owner_type, DWORD owner_id, DWORD point,
//   BYTE event, BYTE type, BYTE gain,
//   STRING name, BYTE klass, BYTE level
boost::asio::awaitable<void> OnGainPvPointAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W3a-31: tactics wanted board (handlers_guild.cpp) ------------
//
// Tactics-recruitment counterpart to the W3a-11 guild wanted
// board. A guild posts "we need tactics members" entries (chief
// action); players browse them filtered by country. Unlike the
// guild board (one entry per guild) a guild may hold up to
// kMaxTacticsWantedPerGuild postings, each with a globally-
// unique id + reward fields (point/gold/silver/cooper/day).
// In-memory only for now — DB persistence (legacy
// TGUILDTACTICSWANTEDTABLE) deferred like the W3a-25
// alliance/enemy state.
//
// Wire layouts (SSHandler.cpp:4668/4746/4778):
//   ADD : DWORD char_id, key, id, STRING title, text,
//         BYTE day, min_level, max_level,
//         DWORD point, gold, silver, cooper
//   DEL : DWORD char_id, key, id
//   LIST: DWORD char_id, key
boost::asio::awaitable<void> OnGuildTacticsWantedAddAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

boost::asio::awaitable<void> OnGuildTacticsWantedDelAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

boost::asio::awaitable<void> OnGuildTacticsWantedListAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W3a-32: tactics volunteer (applicant) flow -------------------
//
// Tactics-recruitment applicant lifecycle, parallel to the
// W3a-12 guild volunteer flow. Players apply to a tactics-wanted
// posting; chiefs browse applicants. The accept/reject REPLY
// handler is deferred to W3a-33 (accept needs the tactics-member
// promotion model that hasn't ported yet).
//
// Wire layouts (SSHandler.cpp:4812/4854/4882):
//   VOLUNTEERING    : DWORD char_id, key, guild_id, wanted_id
//   VOLUNTEERINGDEL : DWORD char_id, key
//   VOLUNTEERLIST   : DWORD char_id, key
boost::asio::awaitable<void> OnGuildTacticsVolunteeringAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

boost::asio::awaitable<void> OnGuildTacticsVolunteeringDelAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

boost::asio::awaitable<void> OnGuildTacticsVolunteerListAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W3a-33: tactics reply (accept/reject hire) -------------------
//
// Chief accepts or rejects a tactics-wanted applicant. Reject
// just drops the application. Accept promotes the applicant to a
// hired tactics member (TGuild.tactics_members) for a fixed term,
// charging the guild's PvP-useable points + money up front, then
// fires the dual TACTICSREPLY broadcast (new member + chief).
//
// Wire layout (SSHandler.cpp:4897):
//   DWORD char_id, key, target_char_id, BYTE reply
boost::asio::awaitable<void> OnGuildTacticsReplyAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W3a-34: tactics kickout + list -------------------------------
//
// KICKOUT removes a hired tactics member: chief-kick (char !=
// target, forfeits the up-front payment) or member self-leave
// (char == target, refunds the payment to the guild). LIST
// returns the current guild's tactics roster (the "current
// guild" is the char's tactics guild if any, else their full
// guild — legacy GetCurGuild).
//
// Wire layouts (SSHandler.cpp:4935/5219):
//   KICKOUT : DWORD char_id, key, target
//   LIST    : DWORD char_id, key
boost::asio::awaitable<void> OnGuildTacticsKickoutAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

boost::asio::awaitable<void> OnGuildTacticsListAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W3a-35: tactics invite + answer (chief-initiated hire) -------
//
// The chief-initiated hire dialog (vs. the W3a-32/33 volunteer-
// initiated path). INVITE: chief offers a contract to a player
// by name; the validated offer is relayed to the target's map
// peer as a dialog. ANSWER: the target accepts/declines; on
// accept the tactics-member promotion runs (same charge + add
// as the W3a-33 reply path), with the outcome echoed to both
// the target's and chief's peers.
//
// Wire layouts (SSHandler.cpp:5006/5094):
//   INVITE : DWORD char_id, key, STRING target_name, BYTE day,
//            DWORD point, gold, silver, cooper
//   ANSWER : DWORD char_id, key, BYTE answer, STRING inviter_name,
//            BYTE day, DWORD point, gold, silver, cooper
boost::asio::awaitable<void> OnGuildTacticsInviteAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

boost::asio::awaitable<void> OnGuildTacticsAnswerAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W3a-12: volunteer / applicant flow (handlers_guild.cpp) ------

// Player applies to a wanted-board entry. World runs the 5
// legacy gates (already-applied / wanted-missing / country /
// expired / level-out-of-range) via
// GuildWantedRegistry::AddApp, then persists + replies +
// refreshes the wanted board for the requester.
//
// Wire layout (SSHandler.cpp:4547):
//   DWORD dwCharID, DWORD dwKey, DWORD dwWantedID
boost::asio::awaitable<void> OnGuildVolunteeringAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// Player cancels their pending application.
//
// Wire layout (SSHandler.cpp:4586):
//   DWORD dwCharID, DWORD dwKey
boost::asio::awaitable<void> OnGuildVolunteeringDelAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// Chief lists applicants pending against their guild's wanted
// entry.
//
// Wire layout (SSHandler.cpp:4614):
//   DWORD dwCharID, DWORD dwKey
boost::asio::awaitable<void> OnGuildVolunteerListAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// Chief accepts (bReply=1) or rejects (bReply=0) an applicant.
// Reject just deletes the application; accept promotes the
// applicant to a guild member (same path as OnGuildInviteAnswerAck
// YES branch, including the dual JOIN_REQ kJoinSuccess broadcast
// + AddMember persistence). Reply with VOLUNTEERREPLY_REQ only
// on accept-failure (legacy parity); list refresh either way.
//
// Wire layout (SSHandler.cpp:4629):
//   DWORD dwCharID, DWORD dwKey, DWORD dwTarget, BYTE bReply
boost::asio::awaitable<void> OnGuildVolunteerReplyAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W3a-11: guild wanted board (handlers_guild.cpp) --------------

// Chief posts a "we are recruiting" entry. World validates caps
// + member-of-guild gate + not-disorg gate + writes the entry
// to GuildWantedRegistry (upserts on second post from the same
// chief) + persists via IGuildRepository::AddWanted + replies
// MW_GUILDWANTEDADD_REQ + a fresh LIST refresh.
//
// Wire layout (SSHandler.cpp:4432):
//   DWORD dwCharID, DWORD dwKey, DWORD dwID (unused after legacy
//   schema migration; kept for wire compat),
//   STRING strTitle, STRING strText, BYTE bMinLevel, BYTE bMaxLevel
boost::asio::awaitable<void> OnGuildWantedAddAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// Chief removes their guild's wanted entry.
//
// Wire layout (SSHandler.cpp:4483):
//   DWORD dwCharID, DWORD dwKey, DWORD dwID (unused — see above)
boost::asio::awaitable<void> OnGuildWantedDelAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// Player opens the recruitment board → server replies with every
// wanted entry filtered by the player's country. Includes the
// "I already applied here" flag derived from a per-char
// applicant record (W3a-12+ — for now always false).
//
// Wire layout (SSHandler.cpp:4515):
//   DWORD dwCharID, DWORD dwKey
boost::asio::awaitable<void> OnGuildWantedListAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W3a-10: money recover + guild extinction (handlers_guild.cpp)

// Inbound from a map server: chief sold a cash-shop item priced
// in guild treasury cooper; world bumps the guild's cooper
// balance. No reply (cluster-wide refresh comes from the next
// MW_GUILDINFO_ACK).
//
// Wire layout (SSHandler.cpp:10539):
//   DWORD dwGuildID, DWORD dwPrice
boost::asio::awaitable<void> OnGuildMoneyRecoverAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// Inbound from a map server's DB worker: extinction-timer fired
// for a disorganized guild → cascade delete. World runs DB
// delete + walks the in-memory member list, clearing each
// member's TChar.guild_id + sending MW_GUILDLEAVE_REQ
// (kLeaveDisorganization), then removes the guild from the
// registry.
//
// Wire layout (SSHandler.cpp:3283):
//   DWORD dwGuildID
boost::asio::awaitable<void> OnGuildExtinctionReq(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W3a-9: single guild info refresh (handlers_guild.cpp) --------

// Inbound from a map server: client opened the guild info pane.
// World assembles the canonical guild summary (chief + vice-chief
// slots + fame/exp/gold + per-level exp cap + article title +
// PvP totals) + the requester's own duty/peer, replies with
// MW_GUILDINFO_REQ.
//
// Wire layout (SSHandler.cpp:3866):
//   DWORD dwCharID, DWORD dwKey
boost::asio::awaitable<void> OnGuildInfoAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W3a-8: guild articles board (handlers_guild.cpp) -------------

// Client opened the guild board → respond with the full list of
// articles.
//
// Wire layout (SSHandler.cpp:4132):
//   DWORD dwCharID, DWORD dwKey
boost::asio::awaitable<void> OnGuildArticleListAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// Member posts a new article. Caps: title ≤ 256, body ≤ 2048,
// guild articles ≤ 100. Persist + return ACK + LIST refresh.
//
// Wire layout (SSHandler.cpp:4154):
//   DWORD dwCharID, DWORD dwKey, STRING strTitle, STRING strArticle
boost::asio::awaitable<void> OnGuildArticleAddAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// Member deletes an article by ID.
//
// Wire layout (SSHandler.cpp:4233):
//   DWORD dwCharID, DWORD dwKey, DWORD dwArticleID
boost::asio::awaitable<void> OnGuildArticleDelAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// Member updates an article's title + body by ID.
//
// Wire layout (SSHandler.cpp:4281):
//   DWORD dwCharID, DWORD dwKey, DWORD dwArticleID,
//   STRING strTitle, STRING strArticle
boost::asio::awaitable<void> OnGuildArticleUpdateAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W3a-6: guild invite flow (handlers_guild.cpp) ----------------

// Inbound from a map server: chief sent an invite to a target by
// name. World validates guild not-disorg + not-full + same country
// + target not already in a guild, then forwards
// MW_GUILDINVITE_REQ to the target's main map peer so their
// client pops a "join guild?" dialog.
//
// Wire layout (SSHandler.cpp:3745):
//   DWORD dwCharID, DWORD dwKey, STRING strTarget
boost::asio::awaitable<void> OnGuildInviteAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// Inbound from the target's map server: the invited char answered
// the dialog. On YES + guild still has room + char still has no
// guild, world adds the member in-memory + sets TChar.guild_id +
// fires MW_GUILDJOIN_REQ replies to both inviter and invited.
//
// Wire layout (SSHandler.cpp:3627):
//   DWORD dwCharID, DWORD dwKey, BYTE bAnswer, DWORD dwInviter
boost::asio::awaitable<void> OnGuildInviteAnswerAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// Inbound from a map server's DB worker: chief expanded the
// guild's cabinet slot count (paid via the cash-shop flow on
// the map side). Persists TGUILDTABLE.bMaxCabinet + mirrors the
// new cap into the in-memory TGuild + ACKs back so the map can
// refresh its cabinet UI.
//
// Wire layout (SSHandler.cpp:4086):
//   DWORD dwGuildID, BYTE bMaxCabinet
//
// (No char_id/key in the legacy wire — this is a pure-DB DM_*
// handler. We don't emit a reply over the same socket because
// the legacy module forwards the ACK over a separate channel
// the map side already subscribes to via the W3a-4c contribution
// path. Operators see the change reflected on the next
// MW_GUILDINFO_ACK refresh.)
boost::asio::awaitable<void> OnGuildCabinetMaxReq(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W3a-2: RW relay handshake (handlers_relay.cpp) ----------------
//
// First packet a map server sends after TCP connect. Carries the
// peer's wID, which the world server records on the PeerSession +
// registers in PeerRegistry. The reply (RW_RELAYSVR_ACK) tells the
// peer the cluster nation flag, the operator list, and the per-
// server motd table.
//
// Wire layout (RWHandler.cpp:5):
//   WORD wID
boost::asio::awaitable<void> OnRelaysvrReq(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W3a-3: char base update + relay lookup (handlers_*.cpp) -------

// Inbound from a map server: a char's base attributes changed
// (face, hair, race, sex, country, name). World propagates the
// change cluster-wide. W3a-3 ports the simple field branches plus
// the name-index update (FACE / HAIR / RACE / SEX / COUNTRY /
// AIDCOUNTRY / NAME). Friend / soulmate / guild app side-effects
// on the NAME branch defer to W3a-4 / W4 (they need the matching
// registries).
//
// Wire layout (SSHandler.cpp:9705):
//   DWORD dwCharID, DWORD dwKey, BYTE bType, BYTE bValue,
//   WORD wTitleID, STRING strName
boost::asio::awaitable<void> OnChangeCharBaseAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// Inbound from the relay map server (legacy RWHandler.cpp:30):
// "I have a client trying to enter as <name, dwCharID> — what's
// its cluster state?". World answers with RW_ENTERCHAR_ACK
// carrying the char's identity + guild/party/corps/tactics ids +
// last known map + unit position.
//
// Wire layout: DWORD dwCharID, STRING strName
boost::asio::awaitable<void> OnEnterCharReq(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// Inbound from the relay map server (RWHandler.cpp:113): "please
// open a relay channel for this char on its main map". World
// routes to the char's main_server_id and forwards
// MW_RELAYCONNECT_REQ with bRelayOn=1.
//
// Wire layout: DWORD dwCharID
boost::asio::awaitable<void> OnRelayConnectReq(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W3b-1: party invite relay (handlers_party.cpp) ----------------
//
// The first slice of the party subsystem (the W3a guild vertical's
// W3b sibling). MW_PARTYADD_ACK is the chief/solo player asking to
// invite another player into a party. World runs the legacy
// validation gate (target online / not mid-invite / not already
// partied / same war-country / requester is chief of a non-full,
// non-arena party) and either relays the failure result back to the
// requester's map or forwards PARTY_AGREE to the target's map so
// their client pops the "join party?" dialog. On AGREE the target
// is flagged party_waiter to block a second concurrent invite.
//
// No party is created here — formation happens when the target
// answers (MW_PARTYJOIN_ACK, W3b-2). The requester's combat stats
// ride along on the packet and are stashed on their TChar for the
// later JOIN broadcast (legacy SetCharStatus).
//
// Wire layout (SSHandler.cpp:2486):
//   STRING strRequest, STRING strTarget, BYTE bObtainType,
//   DWORD dwMaxHP, DWORD dwHP, DWORD dwMaxMP, DWORD dwMP
boost::asio::awaitable<void> OnPartyAddAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W3b-2: party formation (handlers_party.cpp) -------------------
//
// MW_PARTYJOIN_ACK is the invitee's answer to the PARTY_AGREE
// dialog that OnPartyAddAck forwarded. On ASK_YES (and the gates
// still hold) world forms the party: if the inviter already has a
// party the invitee joins it, otherwise a fresh TParty is created
// with the inviter as chief. Either way the JoinParty fan-out
// fires pairwise MW_PARTYJOIN_REQ packets (each member learns the
// joiner, the joiner learns each member) followed by a
// MW_PARTYATTR_REQ HUD refresh; the joining char's party_id
// back-pointer is set. Denials / stale-state failures relay a
// MW_PARTYADD_REQ result back to the inviter (or invitee).
//
// Wire layout (SSHandler.cpp:2623):
//   STRING strOrigin, STRING strTarget, BYTE bObtainType,
//   BYTE bResponse, DWORD dwMaxHP, DWORD dwHP, DWORD dwMaxMP,
//   DWORD dwMP
boost::asio::awaitable<void> OnPartyJoinAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W3b-3: party leave / kick (handlers_party.cpp) ----------------
//
// MW_PARTYDEL_ACK removes one member from a party — voluntary
// leave (bKick=0, char removes self) or chief kick (bKick=1, the
// map server already validated chief authority). World runs the
// legacy LeaveParty: if the party still has ≥2 members afterwards
// it survives (with chief succession to the next member if the
// leaver was chief), otherwise it disbands and the last remaining
// member is pulled out too. Every member is told via
// MW_PARTYDEL_REQ + the survivors get a MW_PARTYATTR_REQ refresh;
// the leaver's party_id back-pointer is cleared.
//
// Wire layout (SSHandler.cpp:2817):
//   WORD wPartyID, DWORD dwCharID, BYTE bKick
boost::asio::awaitable<void> OnPartyDelAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W3b-4: party attribute changes (handlers_party.cpp) -----------
//
// Three small chief-/member-driven party mutations, each fanning a
// broadcast to the party roster.
//
// MANSTAT — a member's HP/MP/level changed on the map side; world
// updates that member's stored combat stats and re-broadcasts to
// every member so their roster HUD refreshes.
//   Wire (SSHandler.cpp:2840): WORD party_id, DWORD member_id,
//     BYTE type, BYTE level, DWORD max_hp, DWORD hp, DWORD max_mp,
//     DWORD mp
boost::asio::awaitable<void> OnPartyManstatAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// CHGPARTYCHIEF — chief hands leadership to another member. Gates:
// both online + both in the same party + requester is chief +
// target isn't already chief. On success sets the new chief +
// re-broadcasts MW_PARTYATTR_REQ to every member.
//   Wire (SSHandler.cpp:2334): DWORD chief_id, DWORD key,
//     DWORD target_id
boost::asio::awaitable<void> OnChgPartyChiefAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// CHGPARTYTYPE — chief changes the loot-distribution mode. Gates:
// requester in a party + is chief. On success updates the party's
// obtain_type + broadcasts MW_CHGPARTYTYPE_REQ to every member.
//   Wire (SSHandler.cpp:2447): DWORD char_id, DWORD key,
//     BYTE party_type
boost::asio::awaitable<void> OnChgPartyTypeAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W3b-5: party member recall (handlers_party.cpp) ---------------
//
// The recall-scroll teleport flow between two party members. A
// member either summons another to their location (TP_RECALL) or
// moves themselves to another member (TP_MOVETO).
//
// RECALL_ACK is the initiator's request: world validates the pair
// (same party + same map for a summon; same map + same war-country
// for a move-to) and forwards MW_PARTYMEMBERRECALLANS_REQ to the
// other party's map so their client confirms; on a failed gate it
// relays MW_PARTYMEMBERRECALL_REQ(IU_TARGETBUSY) back to the
// initiator. RECALLANS_ACK is the confirmation coming back with the
// destination position; world re-checks (same map, not a small
// meeting room) and relays MW_PARTYMEMBERRECALL_REQ to the char
// being teleported.
//
// Wire (SSHandler.cpp:8710): DWORD char_id, key, BYTE inven_id,
//   item_id, STRING origin_name, target_name
boost::asio::awaitable<void> OnPartyMemberRecallAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// Wire (SSHandler.cpp:8799): BYTE result, STRING user_name,
//   target_name, BYTE type, inven_id, item_id, channel,
//   WORD map_id, FLOAT pos_x, pos_y, pos_z
boost::asio::awaitable<void> OnPartyMemberRecallAnsAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W3b-6: party round-robin loot (handlers_party.cpp) ------------
//
// MW_PARTYORDERTAKEITEM_ACK — a monster dropped an item for a
// PT_ORDER (turn-based loot) party. World picks the next looter via
// the party's turn cursor among the eligible members (those in
// range of the drop) and forwards MW_PARTYORDERTAKEITEM_REQ with
// the item to that looter's map. A stale party id replies
// MW_ADDITEMRESULT_REQ(MIT_NOTFOUND) to the reporting map.
//
// Wire (SSHandler.cpp:5693): DWORD char_id, key, WORD party_id,
//   BYTE server_id, channel, WORD map_id, DWORD mon_id,
//   WORD temp_mon_id, BYTE member_count, DWORD member[member_count],
//   <CreateItem>
boost::asio::awaitable<void> OnPartyOrderTakeItemAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W3c-5: corps squad reshuffle (handlers_party.cpp) -------------
//
// MW_PARTYMOVE_ACK — a corps general moves a member between squads.
// Move mode (empty dest name): the target leaves their party and
// joins party `target_party`. Swap mode (dest name set): the target
// and the named dest char trade parties (both parties must have ≥2
// members). Reuses the party LeaveParty + JoinParty machinery; the
// map server has already validated the general's authority. Result:
// MW_PARTYMOVE_REQ (CORPS_SUCCESS / CORPS_NOT_COMMANDER /
// CORPS_WRONG_TARGET).
//
// Wire (SSHandler.cpp:7303): DWORD char_id, key, STRING target_name,
//   STRING dest_name, WORD target_party
boost::asio::awaitable<void> OnPartyMoveAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W6-7: solo-instance party lifecycle (handlers_party.cpp) ------
//
// ENTERSOLOMAP — a char enters a solo instance; world spins up a
// one-member PT_SOLO party (if it has none) and mirrors the party
// state to each of the char's map connections. LEAVESOLOMAP tears the
// solo party back down. Uses the existing PartyRegistry.
//   Wire (SSHandler.cpp:6687/6663): DWORD char_id, key
boost::asio::awaitable<void> OnEnterSoloMapAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);
boost::asio::awaitable<void> OnLeaveSoloMapAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W3c-1: corps invite relay (handlers_corps.cpp) ----------------
//
// Opens the corps subsystem (the party subsystem's parent: a corps
// is a set of parties/squads under a general). MW_CORPSASK_ACK is
// a party chief inviting another party's chief to ally into a
// corps. World validates both are party chiefs of the same
// war-country, neither party is in an arena, and the CheckCorpsJoin
// gate (not both already in a corps; neither corps at the
// MAX_CORPS_PARTY cap), then forwards MW_CORPSASK_REQ to the target
// chief's map so their client pops the confirm dialog. Failures
// relay MW_CORPSREPLY_REQ back to the inviter. No corps is created
// here — formation happens on the answer (MW_CORPSREPLY_ACK, W3c-2).
//
// Wire (SSHandler.cpp:6873): DWORD char_id, key, STRING target_name
boost::asio::awaitable<void> OnCorpsAskAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W3c-2: corps formation (handlers_corps.cpp) -------------------
//
// MW_CORPSREPLY_ACK is the invited chief's answer to the CORPSASK
// dialog. On ASK_YES (gates still hold) world forms the corps: if
// the inviter already has one the answerer's party joins it, if the
// answerer has one the inviter's party joins it, otherwise a fresh
// TCorps is created with the inviter's party as commander. The
// NotifyCorpsJoin fan-out announces every squad to every other
// squad's members (pairwise MW_ADDSQUAD_REQ), sets the joining
// party's corps_id, and pushes each joining member MW_CORPSJOIN_REQ
// + a MW_PARTYATTR_REQ carrying the commander. Denials relay
// MW_CORPSREPLY_REQ.
//
// Wire (SSHandler.cpp:6953): DWORD char_id, key, BYTE reply,
//   STRING requester_name
boost::asio::awaitable<void> OnCorpsReplyAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W3c-3: corps leave / dissolve (handlers_corps.cpp) ------------
//
// MW_CORPSLEAVE_ACK removes a squad (party) from a corps — a chief
// pulling their own squad out, or the general kicking any squad.
// World mutual-DELSQUADs the leaving squad against the rest, then
// either dissolves the corps (when it drops to one squad — pulling
// the last squad out too) or, if the leaver was the commander,
// promotes the next squad and refreshes the survivors. The leaving
// squad's members are told their corps/commander are now 0.
//
// Wire (SSHandler.cpp:7091): DWORD char_id, key, WORD squad_party_id
boost::asio::awaitable<void> OnCorpsLeaveAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W3c-4: change corps commander (handlers_corps.cpp) ------------
//
// MW_CHGCORPSCOMMANDER_ACK — the general hands the commander role
// to another squad in the corps. Only the general (the chief of the
// current commander party) may do this. On success the corps'
// commander_party_id + general_char_id move to the target squad and
// every squad's HUD is refreshed (CorpsJoin with the new
// commander). Failure codes: NO_PARTY / NOT_COMMANDER /
// WRONG_TARGET (already commander) / TARGET_NO_PARTY.
//
// Wire (SSHandler.cpp:6778): DWORD char_id, key, WORD party_id
boost::asio::awaitable<void> OnChgCorpsCommanderAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W3c-6: corps command broadcast (handlers_corps.cpp) -----------
//
// MW_CORPSCMD_ACK — the general issues a movement/attack order. It
// is relayed (MW_CORPSCMD_REQ) to every member of every squad in
// the corps (or just the issuer's party when corps-less). Legacy
// also caches the order on the squad's + commander's m_command for
// late-joiner ADDSQUAD; that per-member command state isn't
// modelled yet (ADDSQUAD emits it as 0 — W3c-2 note), so the cache
// step is deferred. The broadcast itself is byte-identical.
//
// Wire (SSHandler.cpp:7125): DWORD general, key, WORD map_id,
//   squad_id, DWORD char_id, BYTE cmd, DWORD target_id,
//   BYTE target_type, WORD pos_x, pos_z
boost::asio::awaitable<void> OnCorpsCmdAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W3c-7: corps enemy-list family + HP (handlers_corps.cpp) ------
//
// Six opaque chief-to-chief relays (legacy BroadcastCorps): the
// commander of a corps squad pushes a payload — the shared
// enemy/target list (ENEMYLIST / ADD / DEL / MOVE-ENEMY), the unit
// reorder (MOVE-UNIT), or a member-HP sync (CORPSHP) — to every
// other squad's chief. World only forwards the body (leading
// char_id + key swapped for each recipient); the payload tail is
// untouched. Fires only when the sender is the chief of a party in
// a corps.
//
// Wire (SSHandler.cpp:7207..): DWORD char_id, key, <opaque tail>
boost::asio::awaitable<void> OnCorpsEnemyListAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);
boost::asio::awaitable<void> OnMoveCorpsEnemyAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);
boost::asio::awaitable<void> OnMoveCorpsUnitAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);
boost::asio::awaitable<void> OnAddCorpsEnemyAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);
boost::asio::awaitable<void> OnDelCorpsEnemyAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);
boost::asio::awaitable<void> OnCorpsHpAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W6-9: friend-protected refuse relay (handlers_friend.cpp) -----
//
// MW_FRIENDPROTECTEDASK_ACK — a char tried to friend a target that
// has friend-protection on (the map gates that); world relays the
// auto-refuse to the target's map, naming the requester. World's
// whole role is the relay — the protection state lives map/DB-side.
//   Wire (SSHandler.cpp:5847): DWORD char_id, key, STRING target
boost::asio::awaitable<void> OnFriendProtectedAskAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W4-21: friend-protected presence sync (handlers_friend.cpp) ---
//
// MW_PROTECTEDCHECK_ACK — the symmetric partner to W6-9. A char's
// map is asking "is this protected friend still actually online?" or
// reporting the connect/disconnect transition. World iterates the
// char's friend list to find the named friend, then, when both sides
// hold each other in their friend lists (mutual edges), mirrors the
// connect/disconnect status across both directed edges + relays an
// MW_FRIENDCONNECTION_REQ to the target's main map (skipping the
// FT_TARGET pending-edge case, legacy parity). On disconnect, also
// persists the requester's edge-to-friend erasure via the W4-17
// IFriendRepository::EraseFriend write-back. Missing char / friend /
// mutual edge → silent drop, matching legacy.
//   Wire (SSHandler.cpp:5769): DWORD char_id, key, BYTE connect,
//     STRING protected_name
boost::asio::awaitable<void> OnProtectedCheckAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W4-1: friend invite (handlers_friend.cpp) ---------------------
//
// Opens the social-graph (friend) subsystem. MW_FRIENDASK_ACK is a
// char requesting to befriend another by name. World gates
// (target online + same country + not already a friend + neither
// at MAX_FRIEND). If both chars already hold a pending FT_TARGET
// for each other the friendship completes immediately (both
// upgraded to FT_FRIENDFRIEND, requester gets MW_FRIENDADD_REQ
// SUCCESS); otherwise MW_FRIENDASK_REQ is forwarded to the target's
// map so their client confirms (→ MW_FRIENDREPLY_ACK, W4-2).
// Failure gates relay MW_FRIENDADD_REQ to the requester. Friend-row
// persistence (legacy DM_FRIENDINSERT_REQ) is in-memory only for
// now — deferred to the friend repository slice.
//
// Wire (SSHandler.cpp:5880): DWORD char_id, key, STRING target_name
boost::asio::awaitable<void> OnFriendAskAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W4-2: friend reply + erase (handlers_friend.cpp) --------------
//
// MW_FRIENDREPLY_ACK — the invited char's answer to the FRIENDASK
// dialog. On ASK_YES both sides' friend entries are upserted to
// FT_FRIENDFRIEND (connected, each other's region) and both get
// MW_FRIENDADD_REQ SUCCESS; on reject the inviter gets the answer
// code. Missing-char branches relay FRIEND_NOTFOUND.
//   Wire (SSHandler.cpp:6015): DWORD char_id, key, STRING inviter,
//     BYTE reply
boost::asio::awaitable<void> OnFriendReplyAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// MW_FRIENDERASE_ACK — remove a friend. Legacy round-trips through
// the DB then runs EraseFriend; the SOCI-direct port runs the
// in-memory removal here (persistence deferred). A mutual
// (FT_FRIENDFRIEND) friend is demoted (self → FT_TARGET, the
// online other side → FT_FRIEND); a one-way (FT_FRIEND) friend is
// fully removed from both lists (when the other is online).
//   Wire (SSHandler.cpp:6157): DWORD char_id, key, DWORD target_id
boost::asio::awaitable<void> OnFriendEraseAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W4-3: friend groups (handlers_friend.cpp) ---------------------
//
// Per-char named friend buckets (legacy m_mapFRIENDGROUP, capped at
// MAX_FRIENDGROUP). MAKE creates a group (id + name, both unique),
// DELETE removes an empty group, CHANGE moves a friend into a group
// (0 = ungrouped), NAME renames a group. Each replies the matching
// MW_FRIENDGROUP*_REQ; persistence (legacy DM_FRIENDGROUP*_REQ) is
// deferred.
//   Wire (SSHandler.cpp:6238/6307/6360/6410):
//     MAKE   : DWORD char_id, key, BYTE group, STRING name
//     DELETE : DWORD char_id, key, BYTE group
//     CHANGE : DWORD char_id, key, DWORD friend_id, BYTE group
//     NAME   : DWORD char_id, key, BYTE group, STRING name
boost::asio::awaitable<void> OnFriendGroupMakeAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);
boost::asio::awaitable<void> OnFriendGroupDeleteAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);
boost::asio::awaitable<void> OnFriendGroupChangeAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);
boost::asio::awaitable<void> OnFriendGroupNameAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W4-4: friend list (handlers_friend.cpp) -----------------------
//
// MW_FRIENDLIST_ACK — the client opened the friend window. World
// replies MW_FRIENDLIST_REQ with the char's friend groups + every
// non-pending friend; each friend's level/class/connected/region
// is resolved live from the CharRegistry (online → real, offline →
// 0). The soulmate slot is the "none" sentinel until soulmate
// ports.
//   Wire (SSHandler.cpp:1830): DWORD char_id, key
boost::asio::awaitable<void> OnFriendListAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W4-5: chat relay (handlers_chat.cpp) --------------------------
//
// MW_CHAT_ACK — a chat message routed by channel (CHAT_GROUP) to the
// right audience: GUILD / TACTICS (guild + hired tactics members),
// PARTY (target party), FORCE (the sender's corps), MAP/WORLD/SHOW
// (every map peer, global), or WHISPER (a direct recipient, echoed
// back to the sender, with a war-country gate waived for peace).
// Each recipient gets MW_CHAT_REQ. The operator-whisper sub-case
// (whisper to "GM") is deferred — it needs the operator list +
// server-message table.
//
// Wire (SSHandler.cpp:5248): BYTE channel, DWORD sender, key,
//   STRING sender_name, BYTE type, group, DWORD target,
//   STRING name, talk
boost::asio::awaitable<void> OnChatAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W4-19: GM chat ban (handlers_chat.cpp) ------------------------
//
// MW_CHATBAN_ACK — a GM bans a player from chat for N minutes (0 =
// unban). World resolves the target by name, sets/extends/clears the
// ban timer on the target's TChar, tells the target's map to enforce
// it (MW_CHATBAN_REQ), and echoes the result to the issuing GM's map.
// The cluster-wide ban list + RW relay-server propagation (so the
// ban survives the target reconnecting on another map) are deferred
// — they need the operator/ban-list infra (same family as the
// RW_RELAYSVR operator list).
//   Wire (SSHandler.cpp:13098): STRING target, WORD minutes,
//     DWORD char_id, DWORD key
boost::asio::awaitable<void> OnChatBanAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W6-8: GM char message relay (handlers_chat.cpp) ---------------
//
// CT_CHARMSG_ACK — the control server sends a system/GM message to a
// char by name; world routes it (truncated to 1 KiB) to the char's
// main map as MW_CHARMSG_REQ.
//   Wire (SSHandler.cpp:111): STRING name, STRING message
boost::asio::awaitable<void> OnCharMsgAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W5-1: territory occupation broadcasts (handlers_occupy.cpp) ---
//
// CASTLE / LOCAL / MISSION occupy — a castle / territory / mission
// objective changed hands; world fans the new owner+flag to every
// map peer so the cluster shows it consistently. LOCAL applies the
// legacy B-country display flip. The guild stat-exp award (CASTLE /
// LOCAL) and the castle-apply reset (CASTLE) are deferred — the
// guild-stat level formula (CALCULATE_NEXTGEXP) + *_STATEXP award
// constants are absent from the source tree, and the castle-apply
// subsystem hasn't ported.
//
// Wire (SSHandler.cpp:7780/7840/7875):
//   CASTLE : BYTE type, WORD castle, DWORD guild_id, BYTE country,
//            DWORD lose_guild
//   LOCAL  : BYTE type, WORD local, BYTE country, DWORD guild_id,
//            BYTE cur_country
//   MISSION: BYTE type, WORD local, BYTE country
boost::asio::awaitable<void> OnCastleOccupyAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);
boost::asio::awaitable<void> OnLocalOccupyAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);
boost::asio::awaitable<void> OnMissionOccupyAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W5-2: castle-war apply (handlers_occupy.cpp) ------------------
//
// MW_CASTLEAPPLY_ACK — a guild chief assigns a member (or hired
// tactics member) to defend/attack a castle. World validates chief +
// target-in-guild + the 49-applicant cap (CanApplyWar), toggles the
// member's castle/camp (re-applying to the same castle cancels),
// replies to the chief + the assigned member, and re-broadcasts the
// applicant count for the vacated + joined castle (NotifyCastleApply).
// DB persistence is deferred (castle/camp aren't loaded yet).
//
// Wire (SSHandler.cpp:7894): DWORD char_id, key, WORD castle,
//   DWORD target, BYTE camp
boost::asio::awaitable<void> OnCastleApplyAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W5-4: war-window enable broadcast (handlers_occupy.cpp) -------
//
// SM_BATTLESTATUS_REQ — the scheduler opens/closes a war window;
// world fans the matching LOCAL / CASTLE / MISSION enable packet to
// every map peer. The BS_PEACE peace-time bookkeeping (record-date
// reset + CalcWeekRecord + castle-war-info clear) and SKYGARDEN are
// deferred.
//
// Wire (SSHandler.cpp:9870): BYTE type, BYTE status, DWORD start,
//   DWORD second
boost::asio::awaitable<void> OnBattleStatusReq(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W6-1: timed-event broadcast (handlers_event.cpp) --------------
//
// SM_EVENTQUARTER_REQ — the scheduler starts a "present quarter"
// timed event; world picks the present bucket once and fans
// MW_EVENTQUARTER_REQ to every map peer. SM_EVENTQUARTERNOTIFY_REQ —
// a world-chat announcement of the event, broadcast via the existing
// chat sender (operator display name deferred, as in W4-5).
//
// Wire (SSHandler.cpp:8640/8658):
//   QUARTER : BYTE day, hour, minute, STRING present
//   NOTIFY  : STRING announce
boost::asio::awaitable<void> OnEventQuarterReq(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);
boost::asio::awaitable<void> OnEventQuarterNotifyReq(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W6-2: combat / taming cross-server relays (handlers_combat.cpp)
//
// MAGICMIRROR (spell reflection) / MONTEMPT (taming attempt) /
// MONTEMPTEVO (taming evolution) — when the attacker and the affected
// object are on different map servers, the effect is relayed to the
// attacker's map. Each resolves the attacker char by id and forwards
// to its main map. (GETBLOOD belongs to this family but is deferred —
// its routing branches on OT_PC, an object-type enum absent from the
// source tree.)
//
// Wire (SSHandler.cpp:8117/8008/8031):
//   MAGICMIRROR : DWORD host, attack, target, BYTE atk_type, tgt_type
//   MONTEMPT    : DWORD atk_id, WORD mon_id
//   MONTEMPTEVO : DWORD atk_id, host_id, BYTE host_type
boost::asio::awaitable<void> OnMagicMirrorAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);
boost::asio::awaitable<void> OnMonTemptAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);
boost::asio::awaitable<void> OnMonTemptEvoAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W6-6: monster-result relays (handlers_combat.cpp) -------------
//
// MONSTERDIE / TAKEMONMONEY — a monster result the map asked world
// about, routed verbatim back to the char's main map (found by
// id+key). (MONSTERBUY belongs here but is deferred — it spends guild
// money + needs the MSB_* result enum, absent from the tree.)
//   Wire (SSHandler.cpp:5622/5599): DWORD char_id, key, <opaque>
boost::asio::awaitable<void> OnMonsterDieAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);
boost::asio::awaitable<void> OnTakeMonMoneyAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W6-3: global announcement broadcasts (handlers_rank.cpp) ------
//
// FAMERANKUPDATE (fame-ranking refresh, forwarded verbatim) and
// HEROSELECT (a battle-zone hero was chosen) are fanned to every map
// peer so the cluster shows them consistently.
//
// Wire (SSHandler.cpp:11252/9847):
//   FAMERANKUPDATE : opaque (forwarded verbatim)
//   HEROSELECT     : WORD battle_zone, STRING hero_name, INT64 time
boost::asio::awaitable<void> OnFameRankUpdateAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);
boost::asio::awaitable<void> OnHeroSelectAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W6-4: recall-mon (summoned creature) sync (handlers_recallmon.cpp)
//
// CREATE / DATA / DEL — a char's summoned recall monster is mirrored
// across all the char's valid map connections so each client renders
// it. World assigns the recall id on CREATE (when the map sends 0)
// and otherwise forwards the body verbatim. The DB-seed of the id
// counter at boot is deferred.
//
// Wire (SSHandler.cpp:8144/9678/8279): all lead with DWORD char_id,
//   key (DEL keys off char_id only); the remainder is opaque.
boost::asio::awaitable<void> OnCreateRecallMonAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);
boost::asio::awaitable<void> OnRecallMonDataAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);
boost::asio::awaitable<void> OnRecallMonDelAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W6-5: companion-mon (spolecnik) sync (handlers_recallmon.cpp) -
//
// Recall-mon's sibling — CREATE (assigns the same recall id when the
// map sent 0) + DEL, mirrored to the char's valid connections. Same
// opaque-passthrough shape; shares the recall-id counter.
//   Wire (SSHandler.cpp:8320/8455): lead DWORD char_id, key (DEL keys
//   off char_id only); remainder opaque.
boost::asio::awaitable<void> OnCreateSpolecnikMonAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);
boost::asio::awaitable<void> OnSpolecnikMonDelAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W4-7: social presence on logout -------------------------------
//
// Called from OnCloseCharAck for a char that just went offline.
// NotifyFriendsOnLogout marks each connected friend's reverse entry
// offline and pushes MW_FRIENDCONNECTION_REQ(DISCONNECTION) to the
// online ones (legacy LeaveFriend). NotifySoulmateOnLogout marks
// the partner's soulmate entry offline (legacy LeaveSoulmate — no
// packet). `who` is the removed char (kept alive by the caller's
// shared_ptr after CharRegistry::Remove). The connect side
// (login presence) + friend/soulmate DB load are deferred.
boost::asio::awaitable<void> NotifyFriendsOnLogout(
    const HandlerContext& ctx, std::shared_ptr<TChar> who);
boost::asio::awaitable<void> NotifySoulmateOnLogout(
    const HandlerContext& ctx, std::shared_ptr<TChar> who);

// W4-20: connect-side mirror — fired from OnEnterSvrAck once the
// char's identity (name/region) is loaded. Tells each already-online
// friend this char came online + flips their reverse entry connected.
boost::asio::awaitable<void> NotifyFriendsOnLogin(
    const HandlerContext& ctx, std::shared_ptr<TChar> who);

// W4-12: drop a logging-out char from every TMS conference it was
// in, telling the surviving members (legacy LeaveTMS). Declared
// here next to its W4-7 friend/soulmate siblings; defined in
// handlers_tms.cpp. Called from OnCloseCharAck.
boost::asio::awaitable<void> NotifyTmsOnLogout(
    const HandlerContext& ctx, std::shared_ptr<TChar> who);

// W6-19 — full char teardown (legacy CTWorldSvrModule::CloseChar).
// Removes a char from the cluster: if a main-session handoff was in
// flight, voids it on the would-be new main (MW_INVALIDCHAR_REQ); tells
// every map the char is connected to — dead_cons first, then live cons —
// to drop it (MW_DELCHAR_REQ, with the logout/save flags only on the
// main connection); drops it from the registry (clearing the name
// index); then fans the offline presence out to friends / soulmate /
// TMS. Used by OnCloseCharAck and by the connection-cluster error paths
// (BeginTeleport / CheckConnect when the main map is gone). Party-leave
// + guild/tactics DB persistence remain deferred.
boost::asio::awaitable<void> CloseChar(
    std::shared_ptr<TChar> ch, const HandlerContext& ctx);

// --- W4-6: soulmate (handlers_soulmate.cpp) ------------------------
//
// The marriage/pairing flow. SEARCH matchmakes among online chars
// (same country, within SOULMATE_LEVEL, with real-sex / soulmate /
// avatar-sex tiebreakers) and pairs the best candidate. REG
// registers (bReg=1) or previews (bReg=0) a named pairing. END
// dissolves the current pairing. The legacy DB round-trip
// (DM_SOULMATEREG/END_REQ) is collapsed to an in-memory mutual
// pairing; persistence is deferred.
//
// Wire (SSHandler.cpp:9354/9467/9549):
//   SEARCH : DWORD char_id, key, BYTE min_level, npc_inven, npc_item
//   REG    : DWORD char_id, key, STRING name, BYTE reg, npc_inven,
//            npc_item
//   END    : DWORD char_id, key
boost::asio::awaitable<void> OnSoulmateSearchAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);
boost::asio::awaitable<void> OnSoulmateRegAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);
boost::asio::awaitable<void> OnSoulmateEndAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W4-8: region update (handlers_char.cpp) -----------------------
//
// MW_REGION_ACK — a char moved to a new zone. Stores TChar.region
// and mirrors it into the soulmate partner's + each connected
// real-friend's reverse entry so their presence views stay current
// (legacy OnMW_REGION_ACK). No outbound packet.
//   Wire (SSHandler.cpp:8496): DWORD char_id, key, DWORD region
boost::asio::awaitable<void> OnRegionAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W4-9: level update (handlers_char.cpp) ------------------------
//
// MW_LEVELUP_ACK — a char's level changed. Stores TChar.level (the
// authoritative source the party/guild/friend/soulmate displays
// read), fans MW_LEVELUP_REQ to the char's other map connections,
// and syncs the new level into the soulmate partner's view —
// auto-dissolving the pairing if the level gap now exceeds
// SOULMATE_LEVEL (legacy CheckSoulmateEnd). The legacy war-country
// level-gap index is deferred to W5. DB persistence deferred.
//   Wire (SSHandler.cpp:2910): DWORD char_id, key, BYTE level
boost::asio::awaitable<void> OnLevelUpAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W4-10: inspect-player stat relay (handlers_char.cpp) ----------
//
// The two-step "inspect another player's stats" relay.
// CHARSTATINFO_ACK is the requester asking about a target: world
// routes MW_CHARSTATINFOANS_REQ to the target's map to gather the
// stat block. CHARSTATINFOANS_ACK carries that block back (leading
// with the requester id); world relays it verbatim as
// MW_CHARSTATINFO_REQ to the requester's map (opaque passthrough).
//   Wire (SSHandler.cpp:6739): DWORD req_char_id, DWORD char_id
//   Wire (SSHandler.cpp:6759): DWORD req_char_id, <stat block>
boost::asio::awaitable<void> OnCharStatInfoAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);
boost::asio::awaitable<void> OnCharStatInfoAnsAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W4-14: per-character visual state sync (handlers_char.cpp) ----
//
// Continues the W4-8/W4-9 "live per-char state propagation" theme.
//   PETRIDING — a char mounted / dismounted; store TChar.riding and
//     fan MW_PETRIDING_REQ to the char's *other* (non-originating)
//     map sessions so each client renders the mount.
//     Wire (SSHandler.cpp:8604): DWORD char_id, key, DWORD riding
//   HELMETHIDE — a char toggled helmet visibility; store
//     TChar.helmet_hide and confirm MW_HELMETHIDE_REQ back to the
//     originating map.
//     Wire (SSHandler.cpp:8683): DWORD char_id, key, BYTE hide
boost::asio::awaitable<void> OnPetRidingAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);
boost::asio::awaitable<void> OnHelmetHideAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W4-11: TMS conference channels (handlers_tms.cpp) -------------
//
// The "temporary messaging system" — the in-game multi-party
// conference chat. A TMS group has a roster of members and a
// rolling id; messages fan out to every member. The four ACK
// handlers cover the full lifecycle:
//
//   SEND       — post a message to a conference. A solo (size==1)
//                conference re-pairs its last departed member via an
//                invite-ask dialog; otherwise the message is fanned
//                to every member as TMSRECV_REQ.
//   INVITEASK  — answer to that invite-ask dialog. On accept the
//                target joins the conference (roster broadcast via
//                TMSINVITE_REQ); either way the pending message is
//                delivered to the roster.
//   INVITE     — open / expand a conference with a target list
//                (filtered to same-war-country, online targets).
//                Handles the 1:1 re-pair shortcut and fresh-group
//                creation, then broadcasts the roster.
//   OUT        — leave a conference. The roster is told via
//                TMSOUT_REQ, the leaver is dropped, and an emptied
//                conference is destroyed (its last member's name is
//                stashed for a future re-pair).
//
// The legacy NORECEIVER server-message (BuildNetString +
// GetSvrMsg) is replaced by an empty message — the server-message
// table isn't ported (same deferral as the chat operator-whisper).
//
// Wire layouts (SSHandler.cpp):
//   SEND      (7431): DWORD char_id, key, tms, STRING message
//   INVITEASK (7501): DWORD char_id, key, target_id, target_key,
//                     BYTE result, DWORD tms, STRING message
//   INVITE    (7576): DWORD char_id, key, tms, BYTE count,
//                     DWORD target[count]
//   OUT       (7700): DWORD char_id, key, tms
boost::asio::awaitable<void> OnTmsSendAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);
boost::asio::awaitable<void> OnTmsInviteAskAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);
boost::asio::awaitable<void> OnTmsInviteAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);
boost::asio::awaitable<void> OnTmsOutAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W4-13: mail delivery relay (handlers_post.cpp) ----------------
//
// World's whole role in the mail/post system: route a "you have new
// mail" notification to the recipient's map. The mailbox itself
// (list / read / delete) lives DB-side + map-side; world only
// forwards the delivery ping, keyed by the target's name. Two entry
// points share one opaque-passthrough relay:
//   POSTRECV (player→player) and RESERVEDPOSTRECV (DB-side system /
//   scheduled mail). The reserved-post *generator* poll
//   (DM_RESERVEDPOSTSEND_REQ, a stored-proc sweep) is deferred — it
//   needs the reserved-post table/SP.
//
// Wire (SSHandler.cpp:7750 / 6600):
//   DWORD post_id, STRING sender, STRING target, STRING title,
//   BYTE type
boost::asio::awaitable<void> OnPostRecvAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);
boost::asio::awaitable<void> OnReservedPostRecvAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W6-11: day-change guild ranking (handlers_guild.cpp) ----------
//
// SM_CHANGEDAY_REQ — daily rollover; recompute every guild's PvP rank
// (total + month) from the in-memory points (legacy CalcGuildRanking).
// Read back by OnGuildInfoAck; no reply.
boost::asio::awaitable<void> OnChangeDayReq(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W6-10: item-result relays (handlers_item.cpp) ----------------
//
// ADDITEMRESULT — an item-add result (from the DB / another map)
// relayed to the requesting map server (by bMapSvrID). DEALITEMERROR
// — a trade/deal error relayed to the affected char's main map (by
// name). Both reuse existing senders.
//   Wire (SSHandler.cpp:6628/8095):
//     ADDITEMRESULT : DWORD char_id, key, BYTE map_svr, channel,
//                     WORD map_id, DWORD mon_id, BYTE item_id, result
//     DEALITEMERROR : STRING target, error_char, BYTE error
boost::asio::awaitable<void> OnAddItemResultAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);
boost::asio::awaitable<void> OnDealItemErrorAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W6-12: GM user-tracking relays (handlers_admin.cpp) ----------
//
// CT_USERPOSITION_ACK — a GM's "locate this player" request, relayed
// to the target's map (MW_USERPOSITION_REQ; both target + GM must be
// online). CT_USERMOVE_ACK — a GM force-move, relayed to the user's
// map (re-sent as CT_USERMOVE_ACK, the legacy world→map form).
//   Wire (SSHandler.cpp:49/17):
//     USERPOSITION : STRING target, gm
//     USERMOVE     : STRING user, BYTE channel, WORD map, FLOAT x,y,z,
//                    WORD party_id
boost::asio::awaitable<void> OnUserPositionAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);
boost::asio::awaitable<void> OnUserMoveAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W6-13: connection-list reconcile (handlers_conn.cpp) ---------
//
// MW_CONLIST_ACK / MW_MAPSVRLIST_ACK — a map server reports the set
// of map servers a char is connected to. World reconciles its own
// `cons` table against that set: connections the map no longer
// reports are moved to `dead_cons`; servers the char must newly
// connect to are requested from the main map (MW_ROUTELIST_REQ);
// when there are no new servers the main session is re-confirmed on
// every remaining connection (MW_CHECKMAIN_REQ). The two handlers
// share byte-identical logic (the only difference in legacy is the
// packet id) — both delegate to the same reconcile.
//   Wire (SSHandler.cpp:2020 / 2133):
//     DWORD char_id, key, BYTE count, × count: BYTE server_id
boost::asio::awaitable<void> OnConListAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);
boost::asio::awaitable<void> OnMapSvrListAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// MW_CHECKMAIN_ACK — a map answers the CHECKMAIN broadcast claiming
// (or declining) the char's main session. When the responder *is*
// the main, world drains the char's dead_cons (MW_CLOSECHAR_REQ each,
// legacy ClearDeadCON) and green-lights the connection set back to
// the main (MW_CONRESULT_REQ / CN_SUCCESS). When the responder is a
// *different* map, world hands the main session off: it tells the
// old main to release (MW_RELEASEMAIN_REQ) and re-points main at the
// responder. Errors: unknown char / key mismatch → MW_DELCHAR_REQ;
// old main offline → MW_INVALIDCHAR_REQ.
//   Wire (SSHandler.cpp:1095): DWORD char_id, key
boost::asio::awaitable<void> OnCheckMainAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// MW_RELEASEMAIN_ACK — the old main confirms it released the char's
// main session (completing the W6-14 handoff it was asked to start).
// World forwards the released char verbatim to the new main (the
// map main_server_id was re-pointed at in CHECKMAIN_ACK) re-tagged as
// MW_ENTERSVR_REQ, and records the old main in chg_main_id. Errors:
// unknown char / key mismatch → MW_DELCHAR_REQ; new main offline →
// MW_INVALIDCHAR_REQ(release_main=1).
//   Wire (SSHandler.cpp:2284): BYTE db_load, DWORD char_id, key, …
boost::asio::awaitable<void> OnReleaseMainAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// MW_BEGINTELEPORT_ACK — a map asks world to start teleporting a char.
// A same-channel teleport just records the new channel. Otherwise the
// request is pushed onto the char's cession queue (so overlapping
// teleports/connects serialise); if it's the only entry it runs now,
// broadcasting MW_STARTTELEPORT_REQ to every map the char is connected
// to. Deferred entries replay when the one ahead completes (PopConCess,
// driven by CHECKMAIN_ACK). Unknown char / key mismatch → MW_DELCHAR_REQ.
//   Wire (SSHandler.cpp:8554): DWORD char_id, key, BYTE same_channel,
//     channel, WORD map_id, FLOAT pos_x, pos_y, pos_z
boost::asio::awaitable<void> OnBeginTeleportAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// MW_CHECKCONNECT_ACK — a map reports the char's position + the servers
// it should be connected to. Like BEGINTELEPORT it serialises on the
// cession queue; when it runs it updates the char's position and
// reconciles the connection set (drop stale → dead_cons, ROUTELIST new
// servers via main, else CHECKMAIN sweep). A count of 0 short-circuits
// to the CHECKMAIN sweep. The reporting map is NOT auto-added to the
// needed set (unlike CONLIST/MAPSVRLIST). Unknown char → MW_DELCHAR_REQ.
//   Wire (SSHandler.cpp:3839): DWORD char_id, key, BYTE channel,
//     WORD map_id, FLOAT pos_x, pos_y, pos_z, BYTE count, count×BYTE sid
boost::asio::awaitable<void> OnCheckConnectAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W6-20: connection-completion sub-flow (handlers_conn.cpp) ----
//
// Closes the loop W6-13/W6-18 opened: world sent MW_ROUTELIST_REQ to
// the char's main map asking it to route the char to the newly-needed
// servers, and the main answers with one of these three packets that
// drive the new connections to ready.
//
// MW_ROUTE_ACK — main's answer to ROUTELIST. Either the char needs no
// new connections (count==0) and the main is asked for a CHARDATA
// round-trip, or `count` new (ip/port/server_id) tuples are registered
// as *pending* TCharCons (valid=false, ready=false; any matching entry's
// valid bit is preserved across the replace) and forwarded back to the
// reporting (main) map as MW_ADDCONNECT_REQ so the client opens the new
// TCP connections. Unknown char / key mismatch → MW_DELCHAR_REQ.
//   Wire (SSHandler.cpp:1903): DWORD char_id, key, BYTE count,
//     × count: DWORD ip_addr, WORD port, BYTE server_id
boost::asio::awaitable<void> OnRouteAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// MW_ENTERCHAR_ACK — the per-connection entry handshake reply: the
// map that just accepted a new connection tells world "I'm ready".
// World flips that con's `ready` flag and, once every con is ready,
// fires CheckMainCon to re-confirm the main session across the whole
// new connection set. Missing TCharCon for the reporting map →
// MW_INVALIDCHAR_REQ; unknown char → MW_DELCHAR_REQ.
//   Wire (SSHandler.cpp:1038): DWORD char_id, key
boost::asio::awaitable<void> OnEnterCharAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W6-24: Bow battleground (handlers_bow.cpp) -------------------
//
// The smallest of the W6 content subsystems — three handlers covering
// the player-driven queue actions for the Bow PvP battleground. State
// lives in BowRegistry (services/bow_registry.h); deferred: the BS_PEACE
// / BS_ALARM status gate + match creation + teleportation + the per-
// guild queue grouping (those need a scheduler the world doesn't have
// yet — separate slice).

// MW_ADDTOBOWQUEUE_REQ — char wants to join the Bow queue. World
// derives an effective country (m_bCountry, falling back to
// m_bAidCountry when the primary is past TCONTRY_C — legacy gate),
// picks tactics_id > guild_id > 0 for the queue-grouping hint, and
// runs BowRegistry::AddPlayer. Reply MW_ADDTOBOWQUEUE_ACK carries
// the result byte + the registry tick.
//   Wire (SSHandler.cpp:14027): DWORD char_id, key
boost::asio::awaitable<void> OnAddToBowQueueReq(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// MW_CANCELBOWQUEUE_REQ — char wants to leave the queue. World runs
// BowRegistry::RemovePlayer and replies MW_CANCELBOWQUEUE_ACK with
// the result + registry tick. (Legacy also tries BR_MODULE::Erase on
// fall-through; BR battleground isn't ported yet — deferred.)
//   Wire (SSHandler.cpp:14062): DWORD char_id, key
boost::asio::awaitable<void> OnCancelBowQueueReq(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// MW_BOWPOINTSUPDATE_REQ — a Bow-side score event. World increments
// the scoreboard for the named country. No reply.
//   Wire (SSHandler.cpp:14099): BYTE country
boost::asio::awaitable<void> OnBowPointsUpdateReq(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W6-25: Battle Royale (handlers_br.cpp) -----------------------
//
// Five player-driven handlers + the UPDATEBRTEAM broadcast helper.
// Mirrors W6-24's Bow shape, with a premade team model on top:
// chief invites teammates by name; on accept the mate joins the
// chief's team; the team broadcasts UPDATEBRTEAM to every member's
// map. State lives in BrRegistry (services/br_registry.h).

// MW_ADDTOBRQUEUE_REQ — char joins the BR queue OR signals ready.
// When `only_ready` is set, world flips the char's `ready` flag in
// its premade team (or solo queue) and broadcasts UPDATEBRTEAM;
// otherwise it inserts into the solo queue and replies
// MW_ADDTOBRQUEUE_ACK(result, char_id, key, tick).
//   Wire (SSHandler.cpp:14133): DWORD char_id, key, BYTE only_ready
boost::asio::awaitable<void> OnAddToBrQueueReq(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// MW_BRTEAMMATEADD_REQ — chief asks to invite a teammate by name.
// World resolves the target by name; missing / self target →
// MW_BRTEAMMATEADD_ACK(NOTFOUND) on the chief's map. Otherwise
// forwards MW_BRTEAMMATEADD_ACK(SUCCESS, inviter_name) to the
// *target's* map so their client pops the join dialog. The actual
// team mutation waits for OnBrTeamMateAddResultAck.
//   Wire (SSHandler.cpp:14181): DWORD char_id, key, STRING name
boost::asio::awaitable<void> OnBrTeamMateAddReq(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// MW_BRTEAMMATEDEL_REQ — chief drops a member (or a member self-
// leaves). World runs BrRegistry::ErasePlayerFromPremade. No reply
// (legacy parity).
//   Wire (SSHandler.cpp:14234): DWORD char_id, key, STRING name
boost::asio::awaitable<void> OnBrTeamMateDelReq(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// MW_BRTEAMMATEADDRESULT_ACK — the invited mate's reply to the
// MW_BRTEAMMATEADD_ACK(SUCCESS) dialog forwarded by OnBrTeamMateAddReq.
// On SUCCESS: gate against duplicate-in-team + team-full
// (BR_TEAMMATE_MAX_COUNT(BR_3V3)=3 — chief + 2 mates), then
// BrRegistry::JoinPremadeTeam, then UPDATEBRTEAM broadcast. On
// non-SUCCESS: forward the result code back to the chief's map.
//   Wire (SSHandler.cpp:14259): DWORD char_id (mate), key (mate),
//     BYTE result, STRING chief_name
boost::asio::awaitable<void> OnBrTeamMateAddResultAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// MW_VOTEFORBRMAP_REQ — char votes for a map (`map` non-empty) or
// the battle mode (`map` empty AND mode != 0xFF). First vote wins,
// per-user. No reply.
//   Wire (SSHandler.cpp:14346): DWORD char_id, key, STRING map,
//     BYTE mode
boost::asio::awaitable<void> OnVoteForBrMapReq(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W6-26: leave-battlefield cleanup (handlers_bow.cpp) ----------
//
// MW_LEAVEBATTLEFIELD_REQ — char is on its way out of a battlefield
// map; world cleans the matching subsystem's lingering state. Legacy
// SSHandler.cpp:14112 routes by the char's location:
//   * channel == BR_SERVER_ID → BrRegistry::ReleaseSinglePlayer
//   * else map_id == BOW_MAP_ID → BowRegistry::ReleaseSinglePlayer
// Both registry methods are best-effort drops from queue / premade
// (the legacy teleport-home is deferred — we don't model active match
// state yet). No reply.
//   Wire (SSHandler.cpp:14112): DWORD char_id, key
boost::asio::awaitable<void> OnLeaveBattlefieldReq(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W6-27: BattleMode status + CM teleport (handlers_bow.cpp) -----
//
// Two related queries / actions for the battle-mode subsystems
// (Bow + BR). Mirrors legacy SSHandler.cpp:570 and 14377.

// MW_BATTLEMODESTATUS_REQ — char's map asks for the current Bow + BR
// status snapshot. World replies MW_BATTLEMODESTATUS_ACK on the
// char's main map. W6-27 emits the "no module" / quiescent values
// (Bow status/start = 0, winner = TCONTRY_N; BR status/start/type
// = 0) since the scheduler / status state machine isn't ported yet
// (same family as W6-24/25 deferrals).
//   Wire (SSHandler.cpp:570): DWORD char_id, key
boost::asio::awaitable<void> OnBattleModeStatusReq(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// MW_CMTELEPORTBATTLEMODE_REQ — admin / GM "force-add" to a battle
// mode: SYSTEM_BOW (0) → BowRegistry::AddPlayer with country=
// TCONTRY_C + the legacy's `Admin=TRUE` bypass intent (our registry
// doesn't model the status gate so all adds succeed today); SYSTEM_BR
// (1) → legacy body is empty (a TODO in the original), so we no-op
// with a log. No reply (legacy parity).
//   Wire (SSHandler.cpp:14377): DWORD char_id, key, BYTE system_type
boost::asio::awaitable<void> OnCmTeleportBattleModeReq(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// MW_CHARDATA_ACK — main's answer to the CHARDATA_REQ world sent on the
// count==0 ROUTE branch (or when world otherwise asked for a CHARDATA
// round-trip). World refreshes the char's level + HP/MP from the
// packet and, if every con is ready, fires CheckMainCon. The legacy
// non-ready branch fans MW_ENTERCHAR_REQ to each not-yet-ready con
// carrying the full guild/tactics/party/soulmate composite — that fan-
// out is deferred (the composite is the same one priority #3 of the
// gaps audit, "Fresh-login ENTERSVR completion", owns); in the typical
// count==0 path every con is already ready so the deferred branch
// doesn't fire. Errors: unknown char → DELCHAR; main offline →
// INVALIDCHAR.
//   Wire (SSHandler.cpp:847): DWORD char_id, key, BYTE start_act,
//     level, DWORD max_hp, hp, max_mp, mp, BYTE country, mode,
//     <opaque tail>
boost::asio::awaitable<void> OnCharDataAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W6-21: teleport confirm (handlers_conn.cpp) ------------------
//
// MW_TELEPORT_ACK — the originating map's confirmation that a teleport
// has reached the destination map (legacy SSHandler.cpp:1490). Completes
// the W6-17 BEGINTELEPORT chain. World clears the char's party_waiter
// and forwards MW_TELEPORT_REQ(TPR_SUCCESS) back to the responder so it
// can hand the result to the client, then fires MW_CONLIST_REQ to the
// destination map so it re-enters the W6-13 reconcile and joins the
// char's connection set. Destination map offline → MW_TELEPORT_REQ
// (TPR_NODESTINATION) + CloseChar (the W6-19 helper, since the legacy
// CloseChar tears the char down here). Unknown char / key mismatch →
// MW_DELCHAR_REQ.
//   Wire (SSHandler.cpp:1490): DWORD char_id, key, BYTE dest_server_id
boost::asio::awaitable<void> OnTeleportAck(
    std::shared_ptr<PeerSession>  peer,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

} // namespace handlers
} // namespace tworldsvr
