#pragma once

// WarCountryIndex — the war-country level-gap matchmaking index
// (legacy m_mapWarCountry[2][WARCOUNTRY_MAXGAP], rebuilt from
// TACTIVECHARTABLE at boot + on the day-change tick,
// SSHandler.cpp:13574). Two war countries (TCONTRY_D=0 / TCONTRY_C=1)
// × five 10-level gap buckets starting at BROA_BASELEVEL(130).
//
// Read today by MW_WARCOUNTRYBALANCE_ACK (bucket sizes); the Bow/BR
// matchmaking slices will read the char-id sets themselves.

#include <array>
#include <cstdint>
#include <mutex>
#include <unordered_set>
#include <vector>

namespace tworldsvr {

inline constexpr std::uint8_t kWarCountryCount   = 2;   // D + C
inline constexpr std::uint8_t kWarCountryMaxGap  = 5;   // WARCOUNTRY_MAXGAP
inline constexpr std::uint8_t kBroaBaseLevel     = 130; // BROA_BASELEVEL

// Legacy GetWarCountryGap (TWorldSvr.cpp:7724): below the base level
// -> MAXGAP (invalid); else (level-130)/10 (capped by callers).
inline std::uint8_t WarCountryGapOf(std::uint8_t level)
{
    if (level < kBroaBaseLevel)
        return kWarCountryMaxGap;
    return static_cast<std::uint8_t>((level - kBroaBaseLevel) / 10);
}

struct ActiveCharRow;

class WarCountryIndex
{
public:
    WarCountryIndex() = default;
    WarCountryIndex(const WarCountryIndex&) = delete;
    WarCountryIndex& operator=(const WarCountryIndex&) = delete;

    // Drop + re-derive the whole index from TACTIVECHARTABLE rows.
    // Classification mirrors legacy OnDM_ACTIVECHARUPDATE_ACK:
    // war country = bCountry when < TCONTRY_B(2), else the aid
    // country (NULL aid -> neutral); anything >= 2 is skipped; gap
    // >= WARCOUNTRY_MAXGAP is skipped.
    void Rebuild(const std::vector<ActiveCharRow>& rows);

    std::size_t Count(std::uint8_t war_country, std::uint8_t gap) const;

    std::size_t TotalSize() const;

private:
    mutable std::mutex m_mtx;
    std::array<std::array<std::unordered_set<std::uint32_t>,
                          kWarCountryMaxGap>,
               kWarCountryCount> m_index;
};

} // namespace tworldsvr
