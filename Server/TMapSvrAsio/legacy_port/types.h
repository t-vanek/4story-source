#pragma once

// Layer-1 leaf POD types — modern equivalents of the smallest tagged
// structs in legacy `Server/TMapSvr/TMapType.h`. These types have no
// internal references (no pointers / class members / containers
// holding other custom types), so they port verbatim with the
// standard cleanup pass:
//
//   * Hungarian-notation member prefixes dropped (`m_wMapID → map_id`)
//   * `WORD/BYTE/DWORD/FLOAT` replaced with fixed-width / float
//   * `_T()` literals + `TCHAR` dropped (no strings in this layer)
//   * `CString` not used here (string-bearing types arrive later)
//
// Each ported type carries a `// Source:` comment pointing at the
// legacy file:line so a reviewer can diff the layout against the
// original. Wire-format invariants are preserved where they matter
// (the legacy memcpy's some of these straight onto a packet body);
// the per-type comment calls those out explicitly.

#include <cstdint>

namespace tmapsvr::legacy {

// Six-stat block — STR / DEX / CON / INT / WIS / MEN. Used as the
// per-class / per-race base stats on `ObjBase`. Not memcpy'd onto
// the wire directly; it shows up as a value-type field on player
// snapshots, which serialize each field individually.
//
// Source: Server/TMapSvr/TMapType.h:1713-1722  (tagTSTAT)
struct Stat
{
    std::uint16_t str = 0;
    std::uint16_t dex = 0;
    std::uint16_t con = 0;

    std::uint16_t intelligence = 0;   // `INT` is a Win32 typedef; spell out
    std::uint16_t wis = 0;
    std::uint16_t men = 0;
};

// Spawn-point row: id + map id + spawn type + 3D world position.
// The legacy `tagTSPAWNPOS` is also used in `TSPAWNPOSCHART` reads
// (see SOCI map locator in TLoginSvrAsio); the column order there
// is the same as the field order here.
//
// Source: Server/TMapSvr/TMapType.h:1505-1514  (tagTSPAWNPOS)
struct SpawnPos
{
    std::uint16_t id     = 0;
    std::uint16_t map_id = 0;
    std::uint8_t  type   = 0;

    float         pos_x  = 0.0f;
    float         pos_y  = 0.0f;
    float         pos_z  = 0.0f;
};

// Per-level table row — `TLEVELCHART`-style chart loaded once at boot
// and shared across players. Costs are per-action (register / search
// / gamble / rep / repair / refine); PvP fields are tournament-only.
// Loaded from DB at startup; never mutates per-player.
//
// Source: Server/TMapSvr/TMapType.h:1724-1740  (tagTLEVEL)
struct Level
{
    std::uint8_t  level         = 0;
    std::uint32_t exp           = 0;
    std::uint32_t hp            = 0;
    std::uint32_t mp            = 0;
    std::uint8_t  skill_point   = 0;
    std::uint32_t money         = 0;
    std::uint32_t reg_cost      = 0;
    std::uint32_t search_cost   = 0;
    std::uint32_t gamble_cost   = 0;
    std::uint32_t rep_cost      = 0;
    std::uint32_t repair_cost   = 0;
    std::uint32_t refine_cost   = 0;
    std::uint16_t pv_point      = 0;
    std::uint32_t pvp_money     = 0;
};

// AI tick scratch — handles to the active AI command + its
// event/host/target/party context. Reused per-monster, per-tick.
// Cheap to copy + value-init.
//
// Source: Server/TMapSvr/TMapType.h:1697-1711  (tagTAIBUF)
struct AiBuf
{
    std::uint32_t cmd_handle = 0;
    std::uint32_t event_host = 0;
    std::uint32_t rh_id      = 0;
    std::uint8_t  rh_type    = 0;
    std::uint32_t host_key   = 0;
    std::uint32_t mon_id     = 0;
    std::uint32_t delay      = 0;
    std::uint32_t tick       = 0;

    std::uint8_t  channel    = 0;
    std::uint16_t map_id     = 0;
    std::uint16_t party_id   = 0;
};

// Inventory-slot descriptor: which inventory id holds which item id.
// The smallest legacy POD. Used as a value type when serializing the
// inventory snapshot to the client.
//
// Source: Server/TMapSvr/TMapType.h:1742-1746  (tagTINVENDESC)
struct InvenDesc
{
    std::uint8_t  inven_id = 0;
    std::uint16_t item_id  = 0;
};

// Tournament inventory entry — granted during a tournament event,
// expires at `end_time_unix`. ELD is the per-item enchant/level
// difficulty grade. Snapshot-only POD, no pointers back into the
// rest of the world.
//
// Source: Server/TMapSvr/TMapType.h:1927-1933  (tagTTNMTINVEN)
struct TtnmtInven
{
    std::uint8_t  inven_id      = 0;
    std::uint16_t item_id       = 0;
    std::uint8_t  eld           = 0;
    std::int64_t  end_time_unix = 0;   // legacy __time64_t (seconds since epoch)
};

// Skill chart data row — chart-loaded once at boot, immutable.
// Defines per-skill action/type/attribute kind + base value + level
// progression. `calc` selects the per-formula evaluator at runtime.
//
// Source: Server/TMapSvr/TMapType.h:1945-1955  (tagTSKILLDATA)
struct SkillData
{
    std::uint8_t  action    = 0;
    std::uint8_t  type      = 0;
    std::uint8_t  attr      = 0;
    std::uint8_t  exec      = 0;
    std::uint8_t  inc       = 0;
    std::uint16_t value     = 0;
    std::uint16_t value_inc = 0;
    std::uint8_t  calc      = 0;
};

// Quest termination condition — completion target id + termination
// kind (kill / collect / talk / …) + required count. One quest
// carries a list of these; the engine ticks them down as the player
// progresses.
//
// Source: Server/TMapSvr/TMapType.h:2009-2014  (tagQUESTTERM)
struct QuestTerm
{
    std::uint32_t term_id   = 0;
    std::uint8_t  term_type = 0;
    std::uint8_t  count     = 0;
};

// "Aftermath" — post-death penalty state. Step progresses from
// AFTERMATH_NONE → HELP → GHOST → ATONCE; tick counts down to the
// next step transition; stat_dec / reuse_inc are multiplicative
// penalty rates applied while the aftermath is active.
//
// Source: Server/TMapSvr/TMapType.h:1272-1278  (tagAFTERMATH)
struct Aftermath
{
    std::uint8_t  step       = 0;     // AFTERMATH_NONE / HELP / GHOST / ATONCE
    std::uint32_t tick       = 0;     // ms until next step transition
    float         stat_dec   = 0.0f;  // multiplicative stat penalty (1.0 = none)
    float         reuse_inc  = 0.0f;  // multiplicative skill-cooldown penalty
};

// Portal definition row — per-portal entry in the destination
// table. Condition byte + condition id are checked before the
// portal opens to the player. Loaded once at boot from
// `TPORTALCHART`, immutable thereafter.
//
// Source: Server/TMapSvr/TMapType.h:1289-?  (tagPORTAL)
struct Portal
{
    std::uint16_t portal_id    = 0;
    std::uint16_t local_id     = 0;
    std::uint8_t  country      = 0;
    std::uint16_t spawn_id     = 0;
    std::uint8_t  condition    = 0;
    std::uint32_t condition_id = 0;
};

// Visual + social identity packed per-player. Every neighbouring
// player receives these 14 bytes to render your character model.
// Sent verbatim in DM_LOADCHAR_ACK and CS_CHARINFO_ACK.
// All fields uint8_t → struct is trivially copyable.
//
// Source: Server/TMapSvr/TPlayer.h  (m_bRace…m_bHelmetHide block)
//         Server/TMapSvr/SSHandler.cpp:DM_LOADCHAR_ACK write path
struct CharAppearance
{
    std::uint8_t race        = 0;  // m_bRace
    std::uint8_t sex         = 0;  // m_bSex  (client-visible sex)
    std::uint8_t real_sex    = 0;  // m_bRealSex (used in stat calc)
    std::uint8_t char_class  = 0;  // m_bClass
    std::uint8_t hair        = 0;  // m_bHair
    std::uint8_t face        = 0;  // m_bFace
    std::uint8_t body        = 0;  // m_bBody  (armour top)
    std::uint8_t pants       = 0;  // m_bPants
    std::uint8_t hand        = 0;  // m_bHand
    std::uint8_t foot        = 0;  // m_bFoot
    std::uint8_t helmet_hide = 0;  // m_bHelmetHide (0 = show)
    std::uint8_t country     = 0;  // m_bCountry
    std::uint8_t ori_country = 0;  // m_bOriCountry (origin)
    std::uint8_t start_act   = 0;  // m_bStartAct (idle animation)
};

// World position + facing for one character. Stored/loaded as a
// unit on the DB row. `dir` encodes facing angle as a WORD
// (0–4095 = 0°–360°). All fields are trivially copyable scalars.
//
// Source: Server/TMapSvr/TPlayer.h  (position block)
//         Server/TMapSvr/SSHandler.cpp:DM_LOADCHAR_ACK (fPosX/Y/Z + wDIR)
struct CharPosition
{
    std::uint16_t map_id           = 0;
    std::uint16_t spawn_id         = 0;
    std::uint16_t last_spawn_id    = 0;
    std::uint32_t last_destination = 0;  // m_dwLastDestination
    std::uint32_t region           = 0;  // m_dwRegion (local id)
    float         pos_x            = 0.0f;
    float         pos_y            = 0.0f;
    float         pos_z            = 0.0f;
    std::uint16_t dir              = 0;  // m_wDIR 0–4095
};

// Single hotkey slot: bar position + item/skill type + id.
// Used in the HotKeyInven packed array (MAX_HOTKEY_POS entries).
// One slot is 4 bytes (BYTE + BYTE + WORD), trivially copyable.
// Sent to the client in CS_HOTKEY_ACK.
//
// Source: Server/TMapSvr/TMapType.h:1448-1453  (tagHOTKEY)
struct HotKey
{
    std::uint8_t  pos  = 0;  // m_bPos  bar slot index (0-based)
    std::uint8_t  type = 0;  // m_bType 0=skill 1=item 2=emote …
    std::uint16_t id   = 0;  // m_wID   skill_id or item_id per type
};

// One slot in the player's inventory snapshot. Sent in
// DM_LOADCHAR_ACK and CS_CHARINFO_ACK inventory sections.
// `end_time` is __time64_t (Unix seconds, 0 = permanent item).
// `eld` is the enchant/level/difficulty grade byte per item.
//
// Source: Server/TMapSvr/SSHandler.cpp:DM_LOADCHAR_ACK inven loop
struct InvenItem
{
    std::uint8_t  inven_id = 0;  // m_bInvenID  slot index
    std::uint16_t item_id  = 0;  // m_wItemID
    std::int64_t  end_time = 0;  // m_dEndTime  Unix ts; 0 = no expiry
    std::uint8_t  eld      = 0;  // m_bELD      enchant grade
};

// One learned skill with its remaining cooldown tick. Sent in
// CS_CHARINFO_ACK skill section. `reuse_tick` is milliseconds
// remaining on the cooldown; 0 means the skill is ready.
//
// Source: Server/TMapSvr/CSSender.cpp:CS_CHARINFO_ACK skill loop
struct ActiveSkill
{
    std::uint16_t skill_id   = 0;  // m_wID
    std::uint8_t  level      = 0;  // m_bLevel
    std::uint32_t reuse_tick = 0;  // ReuseRemainTick (ms)
};

// Active persistent buff or debuff on the player. Listed in
// CS_CHARINFO_ACK maintain-skill section. `remain_tick` counts
// down to 0 in milliseconds when the effect ends.
//
// Source: Server/TMapSvr/CSSender.cpp:CS_CHARINFO_ACK maintain loop
struct MaintainSkill
{
    std::uint16_t skill_id    = 0;  // skill id of the buff/debuff
    std::uint8_t  level       = 0;  // effect level
    std::uint32_t remain_tick = 0;  // ms until expiry
};

} // namespace tmapsvr::legacy
