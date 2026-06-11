#pragma once

// Skill-data chart — every TSKILLDATA effect row, loaded once at boot and
// grouped by skill id (the legacy CTSkillTemp::m_vData vector per
// template). Read-only static content; ForSkill is an O(1) lookup that
// returns an empty vector for a skill with no effect rows — mirrors
// IMonItemChart.

#include "domain/skill_data.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace tmapsvr {

class ISkillDataChart
{
public:
    virtual ~ISkillDataChart() = default;

    virtual const std::vector<SkillDataRow>&
        ForSkill(std::uint16_t skill_id) const = 0;

    virtual std::size_t Size() const = 0;   // total rows across all skills
};

// In-memory impl + test fake. Add() appends one row to a skill's list.
class InMemorySkillDataChart final : public ISkillDataChart
{
public:
    void Add(std::uint16_t skill_id, const SkillDataRow& d)
    {
        m_rows[skill_id].push_back(d);
        ++m_total;
    }

    const std::vector<SkillDataRow>&
        ForSkill(std::uint16_t skill_id) const override
    {
        static const std::vector<SkillDataRow> kEmpty;
        const auto it = m_rows.find(skill_id);
        return it == m_rows.end() ? kEmpty : it->second;
    }

    std::size_t Size() const override { return m_total; }

private:
    std::unordered_map<std::uint16_t, std::vector<SkillDataRow>> m_rows;
    std::size_t m_total = 0;
};

} // namespace tmapsvr
