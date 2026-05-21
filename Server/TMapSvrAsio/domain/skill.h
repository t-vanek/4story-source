#pragma once

// Skill row — per-char learned skill from TSKILLTABLE.

#include <cstdint>

namespace tmapsvr {

struct SkillRow
{
    std::uint16_t  wSkillID     = 0;
    std::uint8_t   bLevel       = 0;
    std::uint32_t  dwRemainTick = 0;   // cooldown remaining (0 = ready)
};

} // namespace tmapsvr
