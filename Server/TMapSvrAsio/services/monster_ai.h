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

#include "domain/position.h"

#include <chrono>
#include <cstddef>
#include <vector>

#include <boost/asio/awaitable.hpp>

namespace tmapsvr {

class IMonsterRegistry;
class IChannelPresence;

// A monster's chosen next (x, z) for a tick, and whether it's chasing a
// player (vs idle-roaming).
struct Move2D
{
    float x        = 0.f;
    float z        = 0.f;
    bool  chasing  = false;
};

// Decide a monster's next position: chase the nearest player within
// `aggro_range` (stepping `chase_step` toward it, or landing on it when
// closer), else apply the roam offset (`roam_dx`, `roam_dz`). Players at
// the exact origin are ignored (not yet positioned). Pure / synchronous
// so the roam-vs-chase decision is unit-testable; the AI tick supplies
// random roam offsets + the live player positions.
Move2D DecideMonsterMove(float mx, float mz,
                         const std::vector<Position>& players,
                         float aggro_range, float chase_step,
                         float roam_dx, float roam_dz);

boost::asio::awaitable<void> RunMonsterAi(
    IMonsterRegistry&         registry,
    IChannelPresence&         presence,
    std::chrono::milliseconds interval     = std::chrono::seconds(2),
    std::size_t               max_per_tick = 256);

} // namespace tmapsvr
