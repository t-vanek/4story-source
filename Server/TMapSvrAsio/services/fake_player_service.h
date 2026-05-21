#pragma once

// Header-only in-memory IPlayerService for tests + dev runs without a
// configured database. Pre-populate via Add(); LoadChar returns the
// stored snapshot or nullopt if char_id isn't known.

#include "player_service.h"

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <utility>

namespace tmapsvr {

class FakePlayerService final : public IPlayerService
{
public:
    void Add(CharSnapshot snap)
    {
        m_rows[snap.dwCharID] = std::move(snap);
    }

    std::optional<CharSnapshot>
        LoadChar(std::uint32_t char_id) override
    {
        const auto it = m_rows.find(char_id);
        if (it == m_rows.end()) return std::nullopt;
        return it->second;
    }

private:
    std::unordered_map<std::uint32_t, CharSnapshot> m_rows;
};

} // namespace tmapsvr
