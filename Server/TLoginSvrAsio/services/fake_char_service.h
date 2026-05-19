#pragma once

// FakeCharService — TEST-ONLY character-lifecycle backend backed by
// per-user std::vector<CharacterInfo>. No persistence; suitable for
// the ctest suite and the dev-mode binary launch without a DB.
//
// **Not for production.** Production uses SociCharService against
// TCHARTABLE / TITEMTABLE / TALLCHARTABLE / TGUILDMEMBERTABLE /
// TGUILDTABLE in the legacy MSSQL schema.
//
// Behavior matches what TLoginSvr does at the wire level:
//   * List   — returns chars for (user_id, group_id), order by slot
//   * Create — appends if slot is free, name not in use, slot < 6
//   * Delete — removes by char_id; ignores password parameter

#include "char_service.h"

#include <mutex>
#include <unordered_map>

namespace tloginsvr::services {

class FakeCharService : public ICharService
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

    // Test seed for the BR/BOW shard maps: user_id → char_id.
    // Default empty so GetBrCharId / GetBowCharId return 0 unless
    // the test wires them.
    void SetBrChar(std::int32_t user_id, std::int32_t char_id);
    void SetBowChar(std::int32_t user_id, std::int32_t char_id);

    std::int32_t GetBrCharId(std::int32_t user_id) override;
    std::int32_t GetBowCharId(std::int32_t user_id) override;

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
    std::unordered_map<std::int32_t, std::int32_t> m_br_user_to_char;
    std::unordered_map<std::int32_t, std::int32_t> m_bow_user_to_char;
};

} // namespace tloginsvr::services
