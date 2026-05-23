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
#include "../services/char_registry.h"
#include "../services/guild_level_cache.h"
#include "../services/guild_registry.h"
#include "../services/guild_repository.h"
#include "../services/guild_wanted_registry.h"
#include "../services/guild_tactics_wanted_registry.h"
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

// --- W3a-26: cabinet LIST stub (handlers_guild.cpp) ----------------
//
// Player opens the guild storage UI; map server forwards the
// open request to world. Legacy enumerates the in-memory
// m_mapTCabinet item list back through MW_GUILDCABINETLIST_REQ.
// Our port doesn't have the TItem state model yet so the stub
// always returns an empty list — wire-compat (the client just
// sees "no items"). PUTIN / TAKEOUT handlers + DM_*
// cabinet fan-in are deferred together; without those nothing
// populates the cabinet anyway so the empty-list reply is also
// semantically truthful.
//
// Wire layout (SSHandler.cpp:3938):
//   DWORD dwCharID, DWORD dwKey
boost::asio::awaitable<void> OnGuildCabinetListAck(
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

} // namespace handlers
} // namespace tworldsvr
