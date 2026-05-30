#pragma once

// Monster AI tick — the bounded idle-roam loop.
//
// A single detached coroutine driven by a steady_timer. Each tick it
// nudges up to `max_per_tick` monsters a small random step and broadcasts
// the move (CS_MONMOVE_ACK) to everyone on that monster's channel, so the
// world visibly moves.
//
// Deliberately a foundation, not the legacy AI: the real TAICmd* system
// (host/aggro acquisition, MoveNext pathing, chase / attack / flee, group
// leaders) is a major subsystem and lands behind the register-table
// dispatch from DISPATCH.md. There is also no spatial AOI yet, so the
// broadcast is channel-wide — `max_per_tick` caps the per-tick fan-out so
// it stays bounded even with a large monster population; proper per-cell
// AOI + per-monster AI scheduling are the refinements.

#include <chrono>
#include <cstddef>

#include <boost/asio/awaitable.hpp>

namespace tmapsvr {

class IMonsterRegistry;
class IChannelPresence;

boost::asio::awaitable<void> RunMonsterAi(
    IMonsterRegistry&         registry,
    IChannelPresence&         presence,
    std::chrono::milliseconds interval     = std::chrono::seconds(2),
    std::size_t               max_per_tick = 256);

} // namespace tmapsvr
