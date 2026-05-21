#pragma once

// Player service — loads a character snapshot from TCHARTABLE.
//
// The CharSnapshot struct holds the fields the F8 DM_LOADCHAR_ACK
// success branch encodes onto the wire. Later phases extend the
// snapshot (and the encoded body) with items (F9), skills (F11),
// quests (F12), …; each phase fills in the section it owns. The
// trailing sub-sections of legacy DM_LOADCHAR_ACK (secure code, aid
// table, PC bang, post info, inventory, cabinet, …) are emitted with
// default / empty values in F8 and replaced as the responsible
// service comes online.

#include <cstdint>
#include <optional>
#include <string>

namespace tmapsvr {

// Subset of TCHARTABLE columns the F8 player service reads. Fields
// match the wire order in legacy SSHandler.cpp::OnDM_LOADCHAR_REQ
// (the CN_SUCCESS branch at line 3400) so encoder + decoder share a
// natural order.
struct CharSnapshot
{
    std::uint32_t  dwCharID         = 0;
    std::string    szNAME;
    std::uint8_t   bStartAct        = 0;
    std::uint8_t   bRealSex         = 0;
    std::uint8_t   bClass           = 0;
    std::uint8_t   bLevel           = 1;
    std::uint8_t   bRace            = 0;
    std::uint8_t   bCountry         = 0;
    std::uint8_t   bOriCountry      = 0;
    std::uint8_t   bSex             = 0;
    std::uint8_t   bHair            = 0;
    std::uint8_t   bFace            = 0;
    std::uint8_t   bBody            = 0;
    std::uint8_t   bPants           = 0;
    std::uint8_t   bHand            = 0;
    std::uint8_t   bFoot            = 0;
    std::uint8_t   bHelmetHide      = 0;
    std::uint32_t  dwGold           = 0;
    std::uint32_t  dwSilver         = 0;
    std::uint32_t  dwCooper         = 0;
    std::uint32_t  dwEXP            = 0;
    std::uint32_t  dwHP             = 1;
    std::uint32_t  dwMP             = 1;
    std::uint16_t  wSkillPoint      = 0;
    std::uint32_t  dwRegion         = 0;
    std::uint8_t   bGuildLeave      = 0;
    std::uint32_t  dwGuildLeaveTime = 0;
    std::uint16_t  wMapID           = 0;
    std::uint16_t  wSpawnID         = 0;
    std::uint16_t  wLastSpawnID     = 0;
    std::uint32_t  dwLastDestination= 0;
    std::uint16_t  wTemptedMon      = 0;
    std::uint8_t   bAftermath       = 0;
    float          fPosX            = 0.f;
    float          fPosY            = 0.f;
    float          fPosZ            = 0.f;
    std::uint16_t  wDIR             = 0;
    std::uint8_t   bStatLevel       = 0;
    std::uint8_t   bStatPoint       = 0;
    std::uint32_t  dwStatExp        = 0;
};

class IPlayerService
{
public:
    virtual ~IPlayerService() = default;

    // Load the snapshot for `char_id`, or nullopt when the row is
    // missing / the DB is in trouble. The handler distinguishes
    // "no row" (CN_NOCHAR in legacy) from "lookup error" by checking
    // the optional and the configured pool's health independently.
    virtual std::optional<CharSnapshot>
        LoadChar(std::uint32_t char_id) = 0;
};

} // namespace tmapsvr
