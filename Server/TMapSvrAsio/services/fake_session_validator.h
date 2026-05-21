#pragma once

// Header-only in-memory session validator. Used by:
//   - dev runs without a configured [database]
//   - unit tests under tests/ that exercise handler logic without
//     standing up a SOCI / ODBC chain
//
// Keyed by (dwUserID, dwKEY). Insertions are explicit (Add); lookup
// returns the stored MapSessionInfo verbatim. Not thread-safe — wrap
// calls in a strand or external mutex if your test fixture spans
// multiple coroutines.

#include "session_validator.h"

#include <cstdint>
#include <map>
#include <optional>
#include <utility>

namespace tmapsvr {

class FakeMapSessionValidator final : public IMapSessionValidator
{
public:
    void Add(MapSessionInfo info)
    {
        const auto key = std::make_pair(info.dwUserID, info.dwKEY);
        m_rows[key] = std::move(info);
    }

    std::optional<MapSessionInfo>
        LookupSession(std::uint32_t user_id, std::uint32_t key) override
    {
        const auto it = m_rows.find(std::make_pair(user_id, key));
        if (it == m_rows.end()) return std::nullopt;
        return it->second;
    }

private:
    std::map<std::pair<std::uint32_t, std::uint32_t>, MapSessionInfo> m_rows;
};

} // namespace tmapsvr
