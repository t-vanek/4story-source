#pragma once

// SpawnManager — realises the static monster population at boot.
//
// Joins the three charts the data-model spike untangled:
//   TMONSPAWNCHART (ISpawnChart)  — where + how many monsters spawn
//   TMAPMONCHART   (IMapMonChart) — which monster ids each spawn point
//                                    may realise (essential/leader/prob)
//   TMONSTERCHART  (IMonsterChart)— the template (level, …) per monster
// and inserts the resulting MonsterInstances into the registry.
//
// This is the *static* spawn: a one-shot population at boot. The
// respawn-on-death timer, the prob-weighted / essential / leader
// selection semantics, and the roam / chase / attack AI tick are the
// next phase (TAICmd* via a register table — see DISPATCH.md). Monster
// HP is a level-scaled placeholder here because the real combat stats
// live in TMONATTRCHART, which the combat layer loads.

#include "domain/monster.h"

#include <cstddef>
#include <cstdint>

namespace tmapsvr {

class ISpawnChart;
class IMapMonChart;
class IMonsterChart;
class IMonsterRegistry;

// Populate `registry` from the charts. Instance ids are drawn from
// `next_instance_id`, which is advanced past the last id used. Monsters
// are placed on `channel`. Returns the number of monsters spawned.
//
// A spawn point with no TMAPMONCHART rows, or whose monster id has no
// TMONSTERCHART template, is skipped (a data hole, counted in the log).
std::size_t SpawnAllStatic(const ISpawnChart&    spawns,
                           const IMapMonChart&   map_mon,
                           const IMonsterChart&  monsters,
                           IMonsterRegistry&     registry,
                           std::uint32_t&        next_instance_id,
                           std::uint8_t          channel = 0);

} // namespace tmapsvr
