#pragma once

// NPC chart row — static map fixture loaded from TNPCCHART at
// boot. F10's INpcService::FindNpc returns this. Combat-table
// fields (HP, damage, defense) are intentionally absent — NPCs
// aren't combatants, monsters are.

#include <cstdint>
#include <string>

namespace tmapsvr {

struct NpcRow
{
    std::uint16_t  wID            = 0;
    std::string    szName;
    std::uint8_t   bType          = 0;     // shop / quest / gate / …
    std::uint8_t   bCountryID     = 0;
    std::uint16_t  wLocalID       = 0;     // occupation-zone id (country control)
    std::uint8_t   bCondition     = 0;     // shop discount-condition flag
    std::uint8_t   bDiscountRate  = 0;
    std::uint8_t   bAddProb       = 0;
    std::uint16_t  wItemID        = 0;
    std::uint16_t  wMapID         = 0;
    float          fPosX          = 0.f;
    float          fPosY          = 0.f;
    float          fPosZ          = 0.f;
};

} // namespace tmapsvr
