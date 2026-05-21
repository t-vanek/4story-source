#pragma once

// Companion row — per-char companion roster from TCOMPANIONTABLE.

#include <cstdint>
#include <string>

namespace tmapsvr {

struct CompanionRow
{
    std::uint8_t   bSlot          = 0;
    std::uint32_t  dwMonID        = 0;   // monster template id
    std::uint8_t   bLevel         = 0;
    std::string    strName;
    std::uint32_t  dwExp          = 0;
    std::uint16_t  wLife          = 0;
    std::uint8_t   bStatusPoints  = 0;
    std::uint8_t   bEffect        = 0;
    std::uint16_t  wSTR           = 0;
    std::uint16_t  wDEX           = 0;
    std::uint16_t  wCON           = 0;
    std::uint16_t  wINT           = 0;
    std::uint16_t  wWIS           = 0;
    std::uint16_t  wMEN           = 0;
    std::uint16_t  wBonusID       = 0;
};

} // namespace tmapsvr
