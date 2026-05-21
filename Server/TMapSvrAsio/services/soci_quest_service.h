#pragma once

#include "quest_service.h"

#include <cstdint>
#include <vector>

namespace fourstory::db { class SessionPool; }

namespace tmapsvr {

// SOCI-backed quest service. Two queries per LoadProgress:
//   - SELECT TQUESTTABLE rows for the char
//   - SELECT TQUESTTERMTABLE rows for the char, grouped onto matching
//     QuestProgressRow in code (no JOIN — keeps the index plan
//     simple, and a char's term list is short).
class SociQuestService final : public IQuestService
{
public:
    explicit SociQuestService(fourstory::db::SessionPool& pool);

    std::vector<QuestProgressRow>
        LoadProgress(std::uint32_t char_id) override;

private:
    fourstory::db::SessionPool& m_pool;
};

} // namespace tmapsvr
