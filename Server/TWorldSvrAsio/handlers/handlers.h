#pragma once

// Handler dispatch for TWorldSvrAsio.
//
// W1 brought up the scaffold; W2 lands the char registry and the
// first batch of char-lifecycle handlers (OnMW_ADDCHAR_ACK,
// OnMW_CLOSECHAR_ACK). W3a introduces guild handlers; W3b party
// + corps; … see Server/TWorldSvrAsio/README.md §4 for the rest.
//
// HandlerContext is the non-owning view that every handler receives.
// Test fakes substitute individual services without rebuilding the
// whole context.

#include "../world_session.h"
#include "../services/char_registry.h"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/thread_pool.hpp>

#include <cstdint>
#include <memory>
#include <vector>

namespace tworldsvr {

struct HandlerContext
{
    boost::asio::io_context*  io      = nullptr;

    // Worker pool for synchronous SOCI calls. Wired in W2 when
    // [database] is configured. Guild/friend/soulmate handlers in
    // W3+ offload their DB roundtrips here via fourstory::db::
    // CoOffloadIf. nullptr → handlers run SOCI in-line (test
    // fallback; not OK in production).
    boost::asio::thread_pool* db_pool = nullptr;

    // Cluster-wide char index. Owned by main; non-null in W2+.
    // Per-char actor model — see Server/TWorldSvrAsio/services/
    // char_registry.h for the locking contract.
    CharRegistry*             chars   = nullptr;

    // W3a fields land here:
    //   IGuildRepository*  guild_repo = nullptr;
    //   GuildRegistry*     guilds     = nullptr;
    // W3b:
    //   PartyRegistry*     parties    = nullptr;
    //   CorpsRegistry*     corps      = nullptr;
};

namespace handlers {

// Top-level dispatch. Unrecognised wIDs are logged and dropped so
// the legacy framer can keep sending packets — one unknown ID
// must NOT tear down the peer link.
boost::asio::awaitable<void> Dispatch(
    std::shared_ptr<WorldSession> sess,
    std::uint16_t                 wId,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// --- W2: char lifecycle ---------------------------------------------
//
// Inbound from a map server: the map sees a client connect with a
// valid session token, looks up the char in its local cache, and
// forwards the metadata here. World inserts the char into the
// cluster-wide registry and pings every map server about the new
// active user (legacy SendMW_ENTERSVR_REQ — wired in W3 when the
// peer dialer ships).
//
// Body layout (matches Server/TWorldSvr/SSHandler.cpp:704):
//   DWORD dwCharID, DWORD dwKEY, DWORD dwIPAddr, WORD wPort,
//   DWORD dwUserID
boost::asio::awaitable<void> OnAddCharAck(
    std::shared_ptr<WorldSession> sess,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

// Inbound from a map server: the client logged out or the map
// closed the session. Body: { DWORD dwCharID, DWORD dwKEY }.
// W2 simply removes from the registry; the side-effects (guild
// member-offline broadcast, party leave, soulmate notify) land
// in W3+.
boost::asio::awaitable<void> OnCloseCharAck(
    std::shared_ptr<WorldSession> sess,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

} // namespace handlers
} // namespace tworldsvr
