#pragma once

// Header-only in-memory IInventoryService for tests + dev runs without
// a database. Inventories are stored per-char as a vector; Add() and
// SetRows() let tests populate before calling LoadInventory.

#include "inventory_service.h"

#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

namespace tmapsvr {

class FakeInventoryService final : public IInventoryService
{
public:
    void SetRows(std::uint32_t char_id, std::vector<InventoryRow> rows)
    {
        m_rows[char_id] = std::move(rows);
    }

    void Add(std::uint32_t char_id, InventoryRow row)
    {
        m_rows[char_id].push_back(row);
    }

    std::vector<InventoryRow>
        LoadInventory(std::uint32_t char_id) override
    {
        const auto it = m_rows.find(char_id);
        if (it == m_rows.end()) return {};
        return it->second;
    }

private:
    std::unordered_map<std::uint32_t, std::vector<InventoryRow>> m_rows;
};

} // namespace tmapsvr
