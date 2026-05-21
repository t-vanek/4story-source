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

} // namespace tmapsvr
