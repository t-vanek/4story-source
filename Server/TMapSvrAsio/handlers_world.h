#pragma once

// Per-packet dispatch for inbound traffic FROM the World peer. The
// AsioWorldClient's Run() coroutine hands every DecodedPacket to
// DispatchWorld, which switches on the message id and dispatches to
// the matching OnXxx coroutine.
//
// Sibling of handlers.h (CS_ client dispatch) — kept separate because
// the inbound semantics differ: there's no per-connection AsioSession
// to address responses to. The response goes back through the
// shared IWorldClient pointer in HandlerContext.

#include "handlers.h"

#include <boost/asio/awaitable.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace tmapsvr {

boost::asio::awaitable<void> DispatchWorld(
    std::uint16_t                wId,
    std::vector<std::byte>       body,
    const HandlerContext&        ctx);

// Per-handler entry points. Each takes the body (owned vector copied
// out of the AsioSession recv buffer) and the shared HandlerContext.
boost::asio::awaitable<void> OnDMLoadCharReq(
    std::vector<std::byte>       body,
    const HandlerContext&        ctx);

// MW_ENTERSVR_REQ (World→Map): TWorld asks this map to bring a char
// fully resident. The map resolves the char identity and replies
// MW_ENTERSVR_ACK so TWorld can fan out the char's CharInfo / route /
// friend-list. Legacy: SSHandler.cpp:3072 (OnMW_ENTERSVR_REQ).
boost::asio::awaitable<void> OnMWEnterSvrReq(
    std::vector<std::byte>       body,
    const HandlerContext&        ctx);

// MW_ENTERCHAR_REQ (World→Map): the per-connection entry handshake.
// TWorld pushes the char's cluster state (guild/party/corps/soulmate +
// recall-mon tail) as a fat composite and waits for the map to confirm
// the connection is ready. The map replies MW_ENTERCHAR_ACK; TWorld's
// CheckMainCon stays blocked until every con has ACKed.
// Legacy: SSHandler.cpp:1447 (OnMW_ENTERCHAR_REQ).
boost::asio::awaitable<void> OnMWEnterCharReq(
    std::vector<std::byte>       body,
    const HandlerContext&        ctx);

// MW_ADDCONNECT_REQ (World→Map): TWorld hands the map the peer-server
// list for a char's cross-server connections; the map relays it down to
// the client as CS_ADDCONNECT_ACK. Legacy: SSHandler.cpp:6779.
boost::asio::awaitable<void> OnMWAddConnectReq(
    std::vector<std::byte>       body,
    const HandlerContext&        ctx);

// MW_CHECKMAIN_REQ (World→Map): TWorld asks whether this map owns the
// cell the char stands in; the owner replies MW_CHECKMAIN_ACK so TWorld
// can settle the authoritative main session. Legacy: SSHandler.cpp:1300.
boost::asio::awaitable<void> OnMWCheckMainReq(
    std::vector<std::byte>       body,
    const HandlerContext&        ctx);

// MW_CONRESULT_REQ (World→Map): TWorld's settled connect verdict + the
// cross-server id list; the map relays it to the client as the
// authoritative CS_CONNECT_ACK and closes the session on rejection.
// Legacy: SSHandler.cpp:1332.
boost::asio::awaitable<void> OnMWConResultReq(
    std::vector<std::byte>       body,
    const HandlerContext&        ctx);

// MW_CLOSECHAR_REQ (World→Map): TWorld orders the char closed on this
// map; the map shuts the client down (CS_SHUTDOWN_ACK) and closes the
// socket, letting the teardown hook persist + unbind. Legacy:
// SSHandler.cpp:2196.
boost::asio::awaitable<void> OnMWCloseCharReq(
    std::vector<std::byte>       body,
    const HandlerContext&        ctx);

// MW_ROUTELIST_REQ (World→Map): TWorld asks the map to resolve a set of
// cluster server ids to their live endpoints; the map answers
// MW_ROUTE_ACK with the (ip, port, server_id) tuples via
// IServerRouteResolver. Legacy: SSHandler.cpp:6478.
boost::asio::awaitable<void> OnMWRouteListReq(
    std::vector<std::byte>       body,
    const HandlerContext&        ctx);

} // namespace tmapsvr
