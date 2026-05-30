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
    std::uint32_t  dwMaxHP       = 0;    // spawn HP (for the health bar)
    std::uint32_t  dwHP          = 0;    // current; 0 = dead, will be reaped
};

// TMONATTRCHART row — a monster's combat stats *at a given level*
// (keyed by wID + bLevel; the same monster id has one row per level).
// dwMaxHP is the real spawn HP; the rest feed the damage formula the
// combat layer lands. A subset of the 21 legacy columns — the ones the
// spawn + damage paths need; the recover / crit-rate / level columns are
// added when their consumers do.
struct MonsterAttr
{
    std::uint16_t  wID        = 0;     // → MonsterTemplate.wID
    std::uint8_t   bLevel     = 0;
    std::uint32_t  dwMaxHP    = 0;
    std::uint32_t  dwMaxMP    = 0;
    std::uint16_t  wAP        = 0;     // physical attack power
    std::uint16_t  wMAP       = 0;     // magical attack power
    std::uint16_t  wDP        = 0;     // physical defense
    std::uint16_t  wMDP       = 0;     // magical defense
    std::uint16_t  wMinWAP    = 0;     // weapon attack — min
    std::uint16_t  wMaxWAP    = 0;     // weapon attack — max
    std::uint32_t  dwAtkSpeed = 0;
};

// TMAPMONCHART row — which monster(s) a spawn point may realize. A spawn
// point (SpawnPoint.wID) has N candidate monsters, weighted by bProb,
// with bEssential ones always present and bLeader marking the pack lead.
// This is the spawn-point → monster-template linkage (TMONSPAWNCHART
// carries no monster id; the join key is wSpawnID = SpawnPoint.wID).
struct MapMonEntry
{
    std::uint16_t  wSpawnID   = 0;     // → SpawnPoint.wID
    std::uint16_t  wMonID     = 0;     // → MonsterTemplate.wID
    std::uint8_t   bEssential = 0;     // always spawned (not prob-gated)
    std::uint8_t   bLeader    = 0;     // pack leader
    std::uint8_t   bProb      = 0;     // selection weight (0..100)
};

} // namespace tmapsvr
