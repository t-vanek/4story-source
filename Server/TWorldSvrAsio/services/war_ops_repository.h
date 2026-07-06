#pragma once

// IWarOpsRepository — DB surface for the W6-43 war/castle extras:
//
//   TACTIVECHARTABLE prune + load (legacy CTBLActiveCharDel /
//     CTBLActiveCharTable, DBAccess.h:1454) — feeds the war-country
//     level-gap index (legacy m_mapWarCountry, rebuilt at boot and
//     on the day-change tick via DM_ACTIVECHARUPDATE).
//   TSaveCastleApplicant(castle, char_id, camp) — the castle-apply
//     persistence the legacy DM_CASTLEAPPLY_REQ round-trip carried
//     (SSHandler.cpp:10483); collapsed into OnCastleApplyAck (apply/
//     toggle values) and ResetCastleApply (0, char, 0).

#include <cstdint>
#include <vector>

namespace tworldsvr {

// One TACTIVECHARTABLE join row (char + country + optional aid
// country + level).
struct ActiveCharRow
{
    std::uint32_t char_id     = 0;
    std::uint8_t  country     = 0;
    bool          has_aid     = false;
    std::uint8_t  aid_country = 0;
    std::uint8_t  level       = 0;
};

class IWarOpsRepository
{
public:
    virtual ~IWarOpsRepository() = default;

    // DELETE rows older than `prune_before_unix`, then SELECT the
    // remaining actives joined against TCHARTABLE / TAIDTABLE
    // (legacy runs the same del + select back-to-back).
    virtual std::vector<ActiveCharRow>
        LoadActiveChars(std::int64_t prune_before_unix) = 0;

    // EXEC TSaveCastleApplicant. castle==0 clears the row.
    virtual bool SaveCastleApplicant(std::uint16_t castle,
                                     std::uint32_t char_id,
                                     std::uint8_t  camp) = 0;
};

} // namespace tworldsvr
