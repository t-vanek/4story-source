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
