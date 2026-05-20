#pragma once

// IPlayerService — abstraction over character data storage.
//
// F2b scope: LoadChar reads the full character record needed for:
//   * DM_LOADCHAR_ACK  (MapSvr → WorldSvr, after WorldSvr asks)
//   * CS_CHARINFO_ACK  (MapSvr → Client, after MW_ADDCHAR_ACK arrives)
//
// The (char_id, user_id, dw_key) triple is the same token used in
// CS_CONNECT_REQ / TCURRENTUSER — one lookup validates both the
// session token AND retrieves the character row, which is how the
// legacy SSHandler.cpp:OnDM_LOADCHAR_REQ path works.
//
// Production impl (SociPlayerService) issues a blocking SOCI query on
// a worker thread (CoOffload pattern, same as TControlSvrAsio).
// FakePlayerService holds an in-memory allow-list for unit tests
// and dev smoke runs. Ships in F2b Part 2.

#include "legacy_port/types_layer3.h"

#include <cstdint>
#include <optional>
#include <unordered_map>

namespace tmapsvr {

class IPlayerService
{
public:
    virtual ~IPlayerService() = default;

    // Load the character record for (char_id). Returns nullopt if the
    // character does not exist OR if the (user_id, dw_key) pair does
    // not match the stored session token (stale / stolen token).
    // Mirrors the DM_LOADCHAR_REQ lookup in SSHandler.cpp.
    virtual std::optional<legacy::CharSnapshot>
        LoadChar(std::uint32_t char_id,
                 std::uint32_t user_id,
                 std::uint32_t dw_key) = 0;
};

// In-memory stub — pre-loaded allow-list, no DB. Use in unit tests
// and dev smoke runs. Not thread-safe (single-threaded tests only).
class FakePlayerService : public IPlayerService
{
public:
    // Insert or replace the stored snapshot for snap.char_id.
    void AddChar(legacy::CharSnapshot snap)
    {
        const auto id = snap.char_id;
        m_chars[id]   = std::move(snap);
    }

    std::optional<legacy::CharSnapshot>
    LoadChar(std::uint32_t char_id,
             std::uint32_t user_id,
             std::uint32_t dw_key) override
    {
        const auto it = m_chars.find(char_id);
        if (it == m_chars.end())
            return std::nullopt;
        const auto& snap = it->second;
        if (snap.user_id != user_id || snap.dw_key != dw_key)
            return std::nullopt;
        return snap;
    }

private:
    std::unordered_map<std::uint32_t, legacy::CharSnapshot> m_chars;
};

} // namespace tmapsvr
