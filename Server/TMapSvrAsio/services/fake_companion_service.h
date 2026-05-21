#pragma once

// Header-only in-memory ICompanionService for tests + dev without a
// configured database.

#include "companion_service.h"

#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

namespace tmapsvr {

class FakeCompanionService final : public ICompanionService
{
public:
    void SetRows(std::uint32_t char_id, std::vector<CompanionRow> rows)
    {
        m_rows[char_id] = std::move(rows);
    }

    void Add(std::uint32_t char_id, CompanionRow row)
    {
        m_rows[char_id].push_back(std::move(row));
    }

    std::vector<CompanionRow>
        LoadCompanions(std::uint32_t char_id) override
    {
        const auto it = m_rows.find(char_id);
        if (it == m_rows.end()) return {};
        return it->second;
    }

private:
    std::unordered_map<std::uint32_t, std::vector<CompanionRow>> m_rows;
};

} // namespace tmapsvr
