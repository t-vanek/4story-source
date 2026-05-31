#pragma once

// In-memory active-quest log — per-character QuestProgressRow list for the
// duration of a session. Seeded lazily from IQuestService (the
// TQUESTTABLE / TQUESTTERMTABLE progress rows) the first time the kill hook
// or a quest handler touches a char, mutated as the player accepts / kills
// / completes / drops.
//
// Runtime state store, like char_state / channel_presence — not the DB,
// not a chart. Header-only; InMemoryQuestLog is also the test fake.
//
// NOTE: this slice keeps quest progress / status in memory for the session.
// Persisting accept / term-advance / complete back to TQUESTTABLE +
// TQUESTTERMTABLE is a documented follow-up (the *rewards* — gold / EXP /
// items — already persist through char_state / the inventory service). So a
// relog resets in-progress quests until the quest-write path lands.

#include "domain/quest.h"
#include "quest_service.h"

#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

namespace tmapsvr {

class IQuestLog
{
public:
    virtual ~IQuestLog() = default;

    // Has this char's quests been seeded yet?
    virtual bool Has(std::uint32_t char_id) const = 0;

    // Install the char's active quests (replaces any existing set).
    virtual void Seed(std::uint32_t char_id,
                      std::vector<QuestProgressRow> rows) = 0;

    // The char's active quests (creates an empty set if unseeded).
    virtual std::vector<QuestProgressRow>& ForChar(std::uint32_t char_id) = 0;

    // The progress row for one quest, or nullptr.
    virtual QuestProgressRow* Find(std::uint32_t char_id,
                                   std::uint32_t quest_id) = 0;

    // Append a freshly accepted quest.
    virtual void Add(std::uint32_t char_id, QuestProgressRow row) = 0;

    // Drop one quest; returns false if it wasn't active.
    virtual bool Remove(std::uint32_t char_id, std::uint32_t quest_id) = 0;
};

class InMemoryQuestLog final : public IQuestLog
{
public:
    bool Has(std::uint32_t char_id) const override
    { return m_by_char.find(char_id) != m_by_char.end(); }

    void Seed(std::uint32_t char_id,
              std::vector<QuestProgressRow> rows) override
    { m_by_char[char_id] = std::move(rows); }

    std::vector<QuestProgressRow>& ForChar(std::uint32_t char_id) override
    { return m_by_char[char_id]; }

    QuestProgressRow* Find(std::uint32_t char_id,
                           std::uint32_t quest_id) override
    {
        const auto it = m_by_char.find(char_id);
        if (it == m_by_char.end()) return nullptr;
        for (auto& q : it->second)
            if (q.dwQuestID == quest_id) return &q;
        return nullptr;
    }

    void Add(std::uint32_t char_id, QuestProgressRow row) override
    { m_by_char[char_id].push_back(std::move(row)); }

    bool Remove(std::uint32_t char_id, std::uint32_t quest_id) override
    {
        const auto it = m_by_char.find(char_id);
        if (it == m_by_char.end()) return false;
        auto& v = it->second;
        for (auto i = v.begin(); i != v.end(); ++i)
            if (i->dwQuestID == quest_id) { v.erase(i); return true; }
        return false;
    }

private:
    std::unordered_map<std::uint32_t, std::vector<QuestProgressRow>> m_by_char;
};

// Seed a char's active quests from the DB once (idempotent). Lets the kill
// hook + quest handlers operate on a live in-memory log without threading a
// load into the enter path.
inline void EnsureQuestsLoaded(IQuestLog& log, IQuestService& svc,
                               std::uint32_t char_id)
{
    if (!log.Has(char_id))
        log.Seed(char_id, svc.LoadProgress(char_id));
}

} // namespace tmapsvr
