#pragma once

// Per-char companion roster — TCOMPANIONTABLE. A companion is a
// persistent AI-controlled ally summoned by the player (not the same
// as recall monsters or pets, which live in their own tables).
//
// F15 reads the columns the DM_LOADCHAR_ACK companion section emits.
// CS_COMPANION* handlers (summon / dismiss / level-up) are gameplay
// work that lands in the consolidation pass.

#include "domain/companion.h"

#include <cstdint>
#include <vector>

namespace tmapsvr {

class ICompanionService
{
public:
    virtual ~ICompanionService() = default;

    virtual std::vector<CompanionRow>
        LoadCompanions(std::uint32_t char_id) = 0;
};

} // namespace tmapsvr
