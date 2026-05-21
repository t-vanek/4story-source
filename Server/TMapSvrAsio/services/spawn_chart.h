#pragma once

// Monster spawn-point chart — loaded once at boot from TMONSPAWNCHART.
// Each row defines where a group of monsters spawns on which map,
// with what density / range / direction. The SpawnManager (F13.x
// consolidation) drives a timer over these rows and instantiates
// monsters into the MonsterRegistry; F13 just loads the chart so the
// data is available.

#include "domain/monster.h"

#include <vector>

namespace tmapsvr {

class ISpawnChart
{
public:
    virtual ~ISpawnChart() = default;

    // All spawn points (in load order). The SpawnManager will index
    // them by map / group as needed in the consolidation pass.
    virtual const std::vector<SpawnPoint>& All() const = 0;

    virtual std::size_t Size() const = 0;
};

} // namespace tmapsvr
