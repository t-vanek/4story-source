#pragma once

// Header-only in-memory INpcService for tests + dev. Add() inserts a
// row; FindNpc returns the stored value or nullopt.

#include "npc_service.h"

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <utility>

namespace tmapsvr {

class FakeNpcService final : public INpcService
{
public:
    void Add(NpcRow row)
    {
        const auto id = row.wID;
        m_rows[id] = std::move(row);
    }

    std::optional<NpcRow>
        FindNpc(std::uint16_t npc_id) const override
    {
        const auto it = m_rows.find(npc_id);
        if (it == m_rows.end()) return std::nullopt;
        return it->second;
    }

    std::size_t Size() const override { return m_rows.size(); }

private:
    std::unordered_map<std::uint16_t, NpcRow> m_rows;
};

} // namespace tmapsvr
