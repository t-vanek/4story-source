#pragma once

// Character snapshot — the TCHARTABLE row the F8 player service
// returns and the F8/F9/F11/F12/F15 DM_LOADCHAR_ACK encoder
// flattens onto the wire.
//
// Field order matches the legacy CTBLChar query at
// legacy_src/SSHandler.cpp:3400 so encoder + decoder share a
// natural traversal order.

#include <cstdint>
#include <string>

namespace tmapsvr {

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
    std::uint32_t  dwMaxHP          = 1;   // = dwHP at load (real max needs the stat layer)
    std::uint32_t  dwMP             = 1;
    std::uint8_t   bDead            = 0;   // death state (legacy m_bStatus == OS_DEAD)
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

} // namespace tmapsvr
