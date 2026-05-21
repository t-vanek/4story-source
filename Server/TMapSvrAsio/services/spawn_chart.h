#pragma once

// Monster spawn-point chart — loaded once at boot from TMONSPAWNCHART.
// Each row defines where a group of monsters spawns on which map,
// with what density / range / direction. The SpawnManager (F13.x
// consolidation) drives a timer over these rows and instantiates
// monsters into the MonsterRegistry; F13 just loads the chart so the
// data is available.

#include <cstdint>
#include <vector>

namespace tmapsvr {

struct SpawnPoint
{
    std::uint16_t  wID         = 0;
    std::uint16_t  wGroup      = 0;
    std::uint16_t  wLocalID    = 0;     // occupation-zone id
    std::uint16_t  wMapID      = 0;
    float          fPosX       = 0.f;
    float          fPosY       = 0.f;
    float          fPosZ       = 0.f;
    std::uint16_t  wDir        = 0;
    std::uint8_t   bCountry    = 0;
    std::uint8_t   bCount      = 0;     // how many monsters live at this point
    std::uint8_t   bRange      = 0;     // spawn radius
    std::uint8_t   bArea       = 0;
    std::uint8_t   bLink       = 0;
    std::uint8_t   bProb       = 0;
    std::uint8_t   bRoamType   = 0;
};

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
