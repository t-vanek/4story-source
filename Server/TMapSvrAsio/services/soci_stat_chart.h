#pragma once

#include "stat_chart.h"

#include <cstddef>
#include <cstdint>
#include <unordered_map>

namespace fourstory::db { class SessionPool; }

namespace tmapsvr {

// Production IStatChart — loads TFORMULACHART + TCLASSCHART + TRACECHART
// once at construction (boot).
class SociStatChart final : public IStatChart
{
public:
    explicit SociStatChart(fourstory::db::SessionPool& pool);

    FormulaRow Formula(std::uint8_t ftype) const override
    {
        const auto it = m_formula.find(ftype);
        return it == m_formula.end() ? FormulaRow{} : it->second;
    }
    StatBlock ClassStats(std::uint8_t class_id) const override
    {
        const auto it = m_class.find(class_id);
        return it == m_class.end() ? StatBlock{} : it->second;
    }
    StatBlock RaceStats(std::uint8_t race_id) const override
    {
        const auto it = m_race.find(race_id);
        return it == m_race.end() ? StatBlock{} : it->second;
    }
    std::size_t FormulaCount() const override { return m_formula.size(); }
    std::size_t ClassCount()   const override { return m_class.size(); }
    std::size_t RaceCount()    const override { return m_race.size(); }

private:
    std::unordered_map<std::uint8_t, FormulaRow> m_formula;
    std::unordered_map<std::uint8_t, StatBlock>  m_class;
    std::unordered_map<std::uint8_t, StatBlock>  m_race;
};

} // namespace tmapsvr
