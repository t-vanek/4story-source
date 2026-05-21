#pragma once

// Per-character skill service — reads TSKILLTABLE for one char and
// returns the list of learned skills (wSkillID, bLevel, dwRemainTick).
// Used by the F11 extension of OnDMLoadCharReq's success body: after
// F9's inventory section, the encoder emits a uint16 count followed
// by one row per learned skill.
//
// The skill template chart (TSKILLCHART — range / MP cost / cooldown)
// is a separate concern; it will land alongside the actual CS_SKILLUSE
// damage / cooldown logic in a later phase, since that's the only
// path that needs the template fields.

#include "domain/skill.h"

#include <cstdint>
#include <vector>

namespace tmapsvr {

class ISkillService
{
public:
    virtual ~ISkillService() = default;

    // TSKILLTABLE rows for `char_id`. Empty vector on missing rows
    // or DB error (logged at impl level). Wire encoding emits an
    // empty section (count = 0) in either case — legacy treats "no
    // skills" as valid (new chars start with TSTARTSKILL pre-seeded
    // elsewhere; lookup at runtime is purely about reading what's
    // there).
    virtual std::vector<SkillRow>
        LoadSkills(std::uint32_t char_id) = 0;
};

} // namespace tmapsvr
