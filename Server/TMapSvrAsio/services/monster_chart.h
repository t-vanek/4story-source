#pragma once

// Monster template chart — loaded once at boot from TMONSTERCHART.
// Templates define the static properties of a monster type (level,
// AI behavior, range, drop rate, EXP) and are keyed by wID. The AI
// tick + spawn loop look up by template id when realizing a monster
// from a spawn point.
//
// F13 ships a subset of the legacy ~40+ columns — enough to know
// what a monster IS (name / level / AI type / range / EXP / drops).
// The full combat-table columns (attack power, defense, magic
// resist, animation timings) land with the F-X combat consolidation
// pass; the encoder doesn't need them yet because no live monster
// is fighting anyone in F13.

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace tmapsvr {

struct MonsterTemplate
{
    std::uint16_t  wID         = 0;
    std::string    szName;
    std::uint8_t   bRace       = 0;
    std::uint8_t   bClass      = 0;
    std::uint16_t  wKind       = 0;
    std::uint8_t   bLevel      = 0;
    std::uint8_t   bAIType     = 0;
    std::uint8_t   bRange      = 0;
    std::uint16_t  wChaseRange = 0;
    std::uint8_t   bRoamProb   = 0;
    std::uint8_t   bMoneyProb  = 0;
    std::uint32_t  dwMinMoney  = 0;
    std::uint32_t  dwMaxMoney  = 0;
    std::uint8_t   bItemProb   = 0;
    std::uint8_t   bDropCount  = 0;
    std::uint16_t  wExp        = 0;
    std::uint8_t   bIsSelf     = 0;
    std::uint8_t   bRecallType = 0;
    std::uint8_t   bCanSelect  = 0;
};

class IMonsterChart
{
public:
    virtual ~IMonsterChart() = default;

    virtual std::optional<MonsterTemplate>
        Find(std::uint16_t template_id) const = 0;

    virtual std::size_t Size() const = 0;
};

} // namespace tmapsvr
