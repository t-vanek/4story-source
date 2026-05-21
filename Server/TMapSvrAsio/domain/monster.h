#pragma once

// Monster chart + runtime types.
//   MonsterTemplate — static row from TMONSTERCHART (F13 chart loader)
//   SpawnPoint      — static row from TMONSPAWNCHART
//   MonsterInstance — live runtime state held in IMonsterRegistry

#include <cstdint>
#include <string>

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
    std::uint8_t   bCount      = 0;     // how many monsters at this point
    std::uint8_t   bRange      = 0;
    std::uint8_t   bArea       = 0;
    std::uint8_t   bLink       = 0;
    std::uint8_t   bProb       = 0;
    std::uint8_t   bRoamType   = 0;
};

struct MonsterInstance
{
    std::uint32_t  dwInstanceID  = 0;    // unique runtime id
    std::uint16_t  wTemplateID   = 0;    // → MonsterTemplate.wID
    std::uint16_t  wSpawnID      = 0;    // → SpawnPoint.wID
    std::uint16_t  wMapID        = 0;
    std::uint8_t   bChannel      = 0;
    float          fPosX         = 0.f;
    float          fPosY         = 0.f;
    float          fPosZ         = 0.f;
    std::uint32_t  dwHP          = 0;    // 0 = dead, will be reaped
};

} // namespace tmapsvr
