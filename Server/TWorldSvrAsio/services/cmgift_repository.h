#pragma once

// ICmGiftRepository — DB surface for the W6-45 CMGift family:
// TCMGIFTCHART boot load + the TCMGiftCanTake / TCMGiftAdd /
// TCMGiftSet / TCMGiftDel SPs (DBAccess.h:1490 / 2914-2999).

#include "cmgift_registry.h"

#include <optional>

namespace tworldsvr {

class ICmGiftRepository
{
public:
    virtual ~ICmGiftRepository() = default;

    // SELECT * FROM TCMGIFTCHART (legacy CTBLCMGiftChart boot load).
    virtual std::vector<CmGift> LoadChart() = 0;

    // { ? = CALL TCMGiftCanTake(name, gift) } — returns the
    // CMGIFT_RESULT code; DB failure → CMGIFT_FAIL (legacy inits
    // m_bRET to FAIL before the call).
    virtual std::uint8_t CanTake(const std::string& target_name,
                                 std::uint16_t gift_id) = 0;

    // TCMGiftAdd — the SP assigns the new gift id (OUTPUT param).
    virtual std::optional<std::uint16_t> Add(const CmGift& gift) = 0;
    virtual bool Set(const CmGift& gift) = 0;
    virtual bool Del(std::uint16_t gift_id) = 0;
};

} // namespace tworldsvr
