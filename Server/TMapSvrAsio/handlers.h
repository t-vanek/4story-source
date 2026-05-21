#pragma once

// Per-packet handler entry points. The MapServer's per-connection
// coroutine builds a HandlerContext, calls Dispatch for every
// DecodedPacket it gets out of the AsioSession's wire loop, and
// dispatches on the message id to the OnXxx function below.
//
// Handlers are co_await-able so they can SendPacket back through the
// session without blocking the reactor. Each handler is responsible
// for its own response (CS_xxx_ACK) and for logging unexpected
// inputs; the dispatcher itself only logs unhandled message ids.

#include "asio_session.h"

#include <boost/asio/awaitable.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace tmapsvr {

class IMapSessionValidator;

// Per-session context handed to every handler. Pointers are
// non-owning; the MapServer keeps the lifetimes.
struct HandlerContext
{
    IMapSessionValidator*  validator      = nullptr;
    std::uint8_t           expected_group = 0;     // [server] / world group id
    // Future phases will add: IPlayerService, IWorldClient,
    // IMapState, ISpawnManager, … each owned by main() and pointed
    // at here.
};

// Top-level dispatcher. Looks up the wId in a switch, calls the
// matching OnXxx coroutine. Unknown ids log + drop.
//
// The caller is responsible for copying the body out of the
// AsioSession's internal recv buffer before spawning Dispatch — the
// DecodedPacket span the wire loop hands its callback is only valid
// during the synchronous callback frame, not across co_awaits.
boost::asio::awaitable<void> Dispatch(
    std::shared_ptr<tnetlib::AsioSession> sess,
    std::uint16_t                         wId,
    std::vector<std::byte>                body,
    const HandlerContext&                 ctx);

// Per-handler entry points. One per message id. All take a span over
// the decoded body — they own the copy if they need to outlive the
// dispatch call.
boost::asio::awaitable<void> OnConnectReq(
    std::shared_ptr<tnetlib::AsioSession> sess,
    std::vector<std::byte>                body,
    const HandlerContext&                 ctx);

} // namespace tmapsvr
