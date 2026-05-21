#pragma once

#include "quest_service.h"

#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

namespace tmapsvr {

class FakeQuestService final : public IQuestService
{
public:
    void SetRows(std::uint32_t char_id, std::vector<QuestProgressRow> rows)
    {
        m_rows[char_id] = std::move(rows);
    }

    std::vector<QuestProgressRow>
        LoadProgress(std::uint32_t char_id) override
    {
        const auto it = m_rows.find(char_id);
        if (it == m_rows.end()) return {};
        return it->second;
    }

private:
    std::unordered_map<std::uint32_t, std::vector<QuestProgressRow>> m_rows;
};

} // namespace tmapsvr
