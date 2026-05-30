#pragma once

// Skill template chart — loaded once at boot from TSKILLCHART, keyed by
// skill id. This slice carries the reuse cooldown (SkillTemplate.dwReuseDelay)
// the cooldown gate needs; the MP/HP cost + effect columns land with the
// later skill waves (they also need the char's max-MP, not yet modelled).
//
// Read-only static content like the other charts — the SOCI impl loads
// every row once; Find is an O(1) lookup.

#include "domain/skill.h"

#include <cstdint>
#include <optional>
#include <unordered_map>

namespace tmapsvr {

class ISkillTemplateChart
{
public:
    virtual ~ISkillTemplateChart() = default;

    virtual std::optional<SkillTemplate>
        Find(std::uint16_t skill_id) const = 0;

    virtual std::size_t Size() const = 0;
};

// In-memory impl + test fake. Add() inserts a row.
class InMemorySkillTemplateChart final : public ISkillTemplateChart
{
public:
    void Add(const SkillTemplate& s) { m_rows[s.wID] = s; }

    std::optional<SkillTemplate>
        Find(std::uint16_t skill_id) const override
    {
        const auto it = m_rows.find(skill_id);
        if (it == m_rows.end()) return std::nullopt;
        return it->second;
    }

    std::size_t Size() const override { return m_rows.size(); }

private:
    std::unordered_map<std::uint16_t, SkillTemplate> m_rows;
};

} // namespace tmapsvr
