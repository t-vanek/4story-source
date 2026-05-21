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

#include "domain/monster.h"

#include <cstdint>
#include <optional>

namespace tmapsvr {

class IMonsterChart
{
public:
    virtual ~IMonsterChart() = default;

    virtual std::optional<MonsterTemplate>
        Find(std::uint16_t template_id) const = 0;

    virtual std::size_t Size() const = 0;
};

} // namespace tmapsvr
