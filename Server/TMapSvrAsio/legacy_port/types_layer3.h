#pragma once

// Layer-3 aggregate types — character record. Depends on the layer-1
// POD types (CharAppearance, CharPosition, InvenItem, ActiveSkill,
// MaintainSkill) and adds std::string + std::vector containers, so
// this layer is NOT trivially copyable.
//
// `CharSnapshot` is the in-memory representation of a player's DB row
// as loaded by IPlayerService::LoadChar. It is the source for:
//   * DM_LOADCHAR_ACK (MapSvr → WorldSvr) — MapSvr fills from DB
//   * CS_CHARINFO_ACK  (MapSvr → Client)  — sent after MW_ADDCHAR_ACK
//
// Fields follow the DM_LOADCHAR_ACK wire order from SSHandler.cpp so
// a future "serialize to wire" helper can walk the struct in field
// order and match the legacy byte sequence exactly.

#include "legacy_port/types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace tmapsvr::legacy {

// Full in-memory character record.
//
// Source: Server/TMapSvr/SSHandler.cpp  (DM_LOADCHAR_ACK write path)
//         Server/TMapSvr/CSSender.cpp   (CS_CHARINFO_ACK write path)
struct CharSnapshot
{
    std::uint32_t char_id = 0;   // m_dwID   / dwCharID
    std::uint32_t user_id = 0;   // dwUserID (from TCURRENTUSER)
    std::uint32_t dw_key  = 0;   // dwKEY    (session token)

    std::string   name;           // m_szNAME / m_strNAME

    // Core progression
    std::uint8_t  level       = 0;  // m_bLevel
    std::uint32_t exp         = 0;  // m_dwEXP
    std::uint32_t hp          = 0;  // m_dwHP  (current, not max)
    std::uint32_t mp          = 0;  // m_dwMP
    std::uint16_t skill_point = 0;  // m_wSkillPoint
    std::uint8_t  stat_level  = 0;  // m_bStatLevel
    std::uint8_t  stat_point  = 0;  // m_bStatPoint
    std::uint32_t stat_exp    = 0;  // m_dwStatExp

    // Visual + social identity
    CharAppearance appearance{};

    // World position
    CharPosition   position{};

    // Economy
    std::uint32_t gold   = 0;  // m_dwGold
    std::uint32_t silver = 0;  // m_dwSilver
    std::uint32_t copper = 0;  // m_dwCooper

    // Social / guild
    std::uint8_t  guild_leave      = 0;  // m_bGuildLeave
    std::uint32_t guild_leave_time = 0;  // m_dwGuildLeaveTime

    // Tutorial
    std::uint16_t tempted_mon = 0;  // m_wTemptedMon (tutorial monster id)

    // Death aftermath step (0 = none, 1 = HELP, 2 = GHOST, 3 = ATONCE)
    std::uint8_t  aftermath = 0;   // m_bAftermath

    // Inventory snapshot
    std::vector<InvenItem>     inventory;

    // Skill slots
    std::vector<ActiveSkill>   skills;
    std::vector<MaintainSkill> maintain_skills;
};

} // namespace tmapsvr::legacy
