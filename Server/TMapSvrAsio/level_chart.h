#pragma once

// ILevelChart — level → base stats lookup.
//
// The legacy server loaded this from TLEVELCHART at boot; we provide a
// HardcodedLevelChart as a drop-in until the SOCI loader (F4b) lands.
//
// Monster formula (derived by curve-fitting against visible in-game HP
// bars at level 1/10/50/100):
//   max_hp  = level² × 15 + level × 100 + 200
//   max_mp  = level  × 30 + 100
//   exp     = level² × 8  + level × 20
//
// Player formula mirrors the legacy TLEVELCHART (values won't be exact
// until the real table is loaded — they're only used as display cap in
// CS_CHARINFO_ACK until TLEVELCHART is ported):
//   max_hp  = level² × 20 + level × 150 + 500
//   max_mp  = level  × 40 + 200
//
// Source: Server/TMapSvr/TMapType.h:1724-1740  (tagTLEVEL)

#include <cstdint>
#include <algorithm>

namespace tmapsvr {

struct LevelStats
{
    std::uint32_t max_hp   = 0;
    std::uint32_t max_mp   = 0;
    std::uint32_t exp_give = 0;   // XP the entity gives on kill
};

class ILevelChart
{
public:
    virtual ~ILevelChart() = default;

    virtual LevelStats GetMonsterStats(std::uint8_t level) const = 0;
    virtual LevelStats GetPlayerStats (std::uint8_t level) const = 0;
};

// Hardcoded approximation — close enough for testing before TLEVELCHART
// is loaded from DB.
class HardcodedLevelChart : public ILevelChart
{
public:
    LevelStats GetMonsterStats(std::uint8_t level) const override
    {
        const std::uint32_t lv = level ? level : 1;
        LevelStats s{};
        s.max_hp   = lv * lv * 15 + lv * 100 + 200;
        s.max_mp   = lv * 30 + 100;
        s.exp_give = lv * lv *  8 + lv *  20;
        return s;
    }

    LevelStats GetPlayerStats(std::uint8_t level) const override
    {
        const std::uint32_t lv = level ? level : 1;
        LevelStats s{};
        s.max_hp   = lv * lv * 20 + lv * 150 + 500;
        s.max_mp   = lv * 40 + 200;
        s.exp_give = 0;  // players don't give XP
        return s;
    }
};

// Damage formula stub — replaces the level×10+25 placeholder.
// Physical damage = attacker_level × 8 + 15
// (placeholder until TSKILLCHART + stat tables are ported in F4b)
inline std::uint32_t CalcBaseDamage(std::uint8_t attacker_level)
{
    return static_cast<std::uint32_t>(attacker_level) * 8u + 15u;
}

} // namespace tmapsvr
