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
#include <memory>
#include "domain/character_payload.h"

namespace tmapsvr {

struct CharacterClusterState {
    std::uint32_t guild{},fame{},fame_color{},tactics{},party_chief{},riding{},soulmate{},soul_silence{};
    std::uint16_t party{},commander{},castle{};
    std::uint8_t guild_country{3},aid_country{3},duty{},peer{},camp{},party_type{},mode{};
    std::int64_t chat_ban_time{};
    std::string guild_name,tactics_name,soulmate_name,comment;
    bool hydrated{};
};
struct CharSnapshot
{
    // Unknown outcome of an immediate item transaction: retain the reservation
    // and let process recovery inspect the last committed receipt.
    bool persistence_uncertain = false;
    std::shared_ptr<const CharacterPayload> payload;
    CharacterClusterState cluster;
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
    std::uint32_t  dwMaxHP          = 1;   // derived maximum, separate from current HP
    std::uint32_t  dwMP             = 1;
    std::uint32_t  dwMaxMP          = 1;   // native formula, equipment and passive derivation
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
