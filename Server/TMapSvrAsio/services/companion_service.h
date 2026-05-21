#pragma once

// Per-char companion roster — TCOMPANIONTABLE. A companion is a
// persistent AI-controlled ally summoned by the player (not the same
// as recall monsters or pets, which live in their own tables).
//
// F15 reads the columns the DM_LOADCHAR_ACK companion section emits.
// CS_COMPANION* handlers (summon / dismiss / level-up) are gameplay
// work that lands in the consolidation pass.

#include <cstdint>
#include <string>
#include <vector>

namespace tmapsvr {

struct CompanionRow
{
    std::uint8_t   bSlot          = 0;
    std::uint32_t  dwMonID        = 0;   // template id
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

class ICompanionService
{
public:
    virtual ~ICompanionService() = default;

    virtual std::vector<CompanionRow>
        LoadCompanions(std::uint32_t char_id) = 0;
};

} // namespace tmapsvr
