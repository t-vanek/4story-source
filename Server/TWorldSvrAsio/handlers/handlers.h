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
// confirmation back to the client. Persistence to TGUILDMEMBERTABLE
// follows in W3a-4b once the IGuildRepository write API ships.
//
// Wire layout (SSHandler.cpp:3571): DWORD dwCharID, DWORD dwKey
boost::asio::awaitable<void> OnGuildLeaveAck(
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
