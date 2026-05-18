#pragma once

// InMemoryCharService — character lifecycle backed by per-user
// std::vector<CharacterInfo>. No persistence; suitable for tests +
// dev-mode binary launch without a DB.
//
// Behavior matches what TLoginSvr does at the wire level:
//   * List   — returns chars for (user_id, group_id), order by slot
//   * Create — appends if slot is free, name not in use, slot < 6
//   * Delete — removes by char_id; ignores password parameter

#include "char_service.h"

#include <mutex>
#include <unordered_map>

namespace tloginsvr::services {

class InMemoryCharService : public ICharService
{
public:
    // Seed helper for tests / dev mode.
    void AddCharacter(std::int32_t user_id,
                      std::uint8_t group_id,
                      CharacterInfo info);

    std::vector<CharacterInfo>
    List(std::int32_t user_id, std::uint8_t group_id) override;

    CharacterCreateResponse
    Create(const CharacterCreateRequest& req) override;

    DeleteCharResult
    Delete(std::int32_t user_id,
           std::uint8_t group_id,
           std::int32_t char_id,
           const std::string& password) override;

    VeteranLevels GetVeteranLevels() const override { return m_veteran_levels; }

    // Seed for tests / dev. Overwrites all three slots.
    void SetVeteranLevels(VeteranLevels levels) { m_veteran_levels = levels; }

private:
    // Single composite key — (user_id, group_id) bundled in a 64-bit
    // value so the std::unordered_map needs no extra hash work.
    static std::uint64_t MakeKey(std::int32_t user_id, std::uint8_t group_id)
    {
        return (static_cast<std::uint64_t>(user_id) << 8)
             | static_cast<std::uint64_t>(group_id);
    }

    mutable std::mutex m_mtx;
    std::unordered_map<std::uint64_t, std::vector<CharacterInfo>> m_chars;
    std::int32_t       m_next_char_id = 1;
    // Cross-group name uniqueness — legacy enforced same.
    std::unordered_map<std::string, std::int32_t> m_name_to_char_id;
    VeteranLevels      m_veteran_levels{};
};

} // namespace tloginsvr::services
