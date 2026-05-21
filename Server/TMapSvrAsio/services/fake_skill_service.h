#pragma once

#include "skill_service.h"

#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

namespace tmapsvr {

class FakeSkillService final : public ISkillService
{
public:
    void SetRows(std::uint32_t char_id, std::vector<SkillRow> rows)
    {
        m_rows[char_id] = std::move(rows);
    }

    void Add(std::uint32_t char_id, SkillRow row)
    {
        m_rows[char_id].push_back(row);
    }

    std::vector<SkillRow>
        LoadSkills(std::uint32_t char_id) override
    {
        const auto it = m_rows.find(char_id);
        if (it == m_rows.end()) return {};
        return it->second;
    }

private:
    std::unordered_map<std::uint32_t, std::vector<SkillRow>> m_rows;
};

} // namespace tmapsvr
