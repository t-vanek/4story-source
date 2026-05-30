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

    // Combat stats realized at spawn from TMONATTRCHART (MonsterAttr) so
    // the registry holds everything the damage formula needs without a
    // per-hit chart lookup. wDP defends incoming player hits; wAP / wWAP
    // arm the monster's own melee in the AI tick. Server-side only — not
    // on the CS_ADDMON wire.
    std::uint16_t  wAP           = 0;    // physical attack power
    std::uint16_t  wMinWAP       = 0;    // weapon attack — min
    std::uint16_t  wMaxWAP       = 0;    // weapon attack — max
    std::uint16_t  wDP           = 0;    // physical defense
    std::uint16_t  wMDP          = 0;    // magical defense
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

// MONITEM_PROB — indices into MonItemEntry::bItemProb, verbatim from the
// legacy enum (TMapType.h:395). NORMAL1..4 are the four per-item drop
// gates; MAGIC / SET / RARE feed the special-item roll (deferred).
enum MonItemProb : std::uint8_t
{
    MIP_NORMAL1 = 0,
    MIP_NORMAL2,
    MIP_NORMAL3,
    MIP_NORMAL4,
    MIP_MAGIC,
    MIP_SET,
    MIP_RARE,
    MIP_COUNT      // == 7
};

// TMONITEMCHART row — one drop-table entry for a monster (keyed by wMonID;
// a monster has N rows). Faithful to tagTMONITEM (TMapType.h:1491) minus
// the two runtime pointer members the legacy resolves at load. On death
// CTMonster::AddItem (TMonster.cpp:1102) rolls the table bDropCount times:
// weight-select an entry by wWeight, then gate on the four NORMAL probs.
// The drop roll lives in services/loot.h.
struct MonItemEntry
{
    std::uint16_t  wMonID     = 0;     // → MonsterTemplate.wID (group key)
    std::uint8_t   bChartType = 0;     // 1 = fixed chart item (wItemID); 0 = magic item (deferred)
    std::uint16_t  wItemID    = 0;     // dropped item template id (chart-type)
    std::uint16_t  wItemIDMin = 0;     // item-id range for MonChoiceItem pick (deferred)
    std::uint16_t  wItemIDMax = 0;
    std::uint16_t  wWeight    = 0;     // weighted-selection weight (TRand over the sum)
    std::uint8_t   bLevelMin  = 0;
    std::uint8_t   bLevelMax  = 0;
    std::uint8_t   bItemProb[MIP_COUNT] = {};   // per-item gates, indexed by MonItemProb
    std::uint8_t   bItemMagicOpt = 0;  // magic-option count (MakeSpecialItem, deferred)
    std::uint8_t   bItemRareOpt  = 0;  // rare-option count  (MakeSpecialItem, deferred)
};

} // namespace tmapsvr
