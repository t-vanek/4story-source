#pragma once

// Layer-2 compound POD types — depend either on a layer-1 type as a
// value member OR carry a string / timestamp the layer-1 set
// deliberately avoided.
//
// The layer break exists because adding `std::string` makes the
// type no longer trivially copyable (string holds a heap allocation
// pointer), so the layer-1 invariants (`is_trivially_copyable_v` +
// memcpy round-trip) don't apply here. The compound is still a
// pure data carrier — no methods, no encapsulation, no
// references to mutable game state.

#include <cstdint>
#include <string>

namespace tmapsvr::legacy {

// Party-member snapshot row — what every other party member sees
// about this player. Built once per party-change event and
// serialized into CS_PARTYINFO_ACK frames.
//
// Strings are CP1252 in the legacy wire format; the modern path
// keeps the same byte representation on the wire even though
// `std::string` stores arbitrary bytes (CP1252 is an 8-bit-clean
// encoding so the bytes pass through unchanged).
//
// Source: Server/TMapSvr/TMapType.h:1465-1490  (tagPARTYMEMBER)
struct PartyMember
{
    std::uint32_t id          = 0;
    std::string   name;                  // m_strName  (CString → std::string)
    std::string   guild;                 // m_strGuild
    std::uint8_t  level       = 0;
    std::uint32_t max_hp      = 0;
    std::uint32_t hp          = 0;
    std::uint32_t max_mp      = 0;
    std::uint32_t mp          = 0;
    std::uint8_t  race        = 0;
    std::uint8_t  sex         = 0;
    std::uint8_t  face        = 0;
    std::uint8_t  hair        = 0;
};

// Per-player visible state broadcast to AOI neighbours. Built from
// MapSessionState::snapshot + live movement fields; sent in
// CS_ENTER_ACK and kept in LocalMapState::m_presences so other
// players can receive it when they enter the same AOI cell.
//
// Has std::string → not trivially copyable → Layer 2.
//
// Source: Server/TMapSvr/CSSender.cpp:SendCS_ENTER_ACK field set
struct PlayerPresence
{
    std::uint32_t char_id  = 0;
    std::string   name;            // m_strNAME

    // Appearance (pulled from CharAppearance on EnterMap)
    std::uint8_t race        = 0;
    std::uint8_t sex         = 0;
    std::uint8_t char_class  = 0;
    std::uint8_t hair        = 0;
    std::uint8_t face        = 0;
    std::uint8_t body        = 0;
    std::uint8_t pants       = 0;
    std::uint8_t hand        = 0;
    std::uint8_t foot        = 0;
    std::uint8_t helmet_hide = 0;
    std::uint8_t country     = 0;
    std::uint8_t level       = 0;

    // Position + orientation (updated on CS_MOVE_REQ)
    float         pos_x      = 0.0f;
    float         pos_y      = 0.0f;
    float         pos_z      = 0.0f;
    std::uint16_t dir        = 0;     // 0–4095
    std::uint16_t pitch      = 0;
    std::uint8_t  action     = 0;     // STAND/WALK/RUN/SIT/…
    std::uint8_t  mode       = 0;     // combat/peace
    std::uint8_t  mouse_dir  = 0;
    std::uint8_t  key_dir    = 0;
    float         speed      = 0.0f;

    // Vitals (displayed in other players' HUD)
    std::uint32_t hp     = 0;
    std::uint32_t max_hp = 0;
    std::uint32_t mp     = 0;
    std::uint32_t max_mp = 0;
};

} // namespace tmapsvr::legacy
