#include "war_country_index.h"

#include "war_ops_repository.h"

namespace tworldsvr {

namespace {
// TCONTRY_B / TCONTRY_N in the country enum (NetCode.h:1093 —
// D=0, C=1, B=2, N=3).
constexpr std::uint8_t kContryB = 2;
constexpr std::uint8_t kContryN = 3;
} // namespace

void WarCountryIndex::Rebuild(const std::vector<ActiveCharRow>& rows)
{
    std::lock_guard g(m_mtx);
    for (auto& country : m_index)
        for (auto& bucket : country)
            bucket.clear();

    for (const auto& r : rows)
    {
        const std::uint8_t war_country =
            r.country < kContryB
                ? r.country
                : (r.has_aid ? r.aid_country : kContryN);
        if (war_country >= kContryB)
            continue;
        const std::uint8_t gap = WarCountryGapOf(r.level);
        if (gap >= kWarCountryMaxGap)
            continue;
        m_index[war_country][gap].insert(r.char_id);
    }
}

std::size_t WarCountryIndex::Count(std::uint8_t war_country,
                                   std::uint8_t gap) const
{
    std::lock_guard g(m_mtx);
    if (war_country >= kWarCountryCount || gap >= kWarCountryMaxGap)
        return 0;
    return m_index[war_country][gap].size();
}

std::size_t WarCountryIndex::TotalSize() const
{
    std::lock_guard g(m_mtx);
    std::size_t n = 0;
    for (const auto& country : m_index)
        for (const auto& bucket : country)
            n += bucket.size();
    return n;
}

} // namespace tworldsvr
