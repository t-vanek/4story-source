#pragma once

// Quest definition chart — loaded once at boot from TQUESTCHART, with
// TQUESTTERMCHART terms and TQREWARDCHART rewards grouped under each
// quest id. The quest engine looks defs up by id (Find) to advance a
// player's progress and to grant rewards on completion.
//
// Read-only static content like the other charts (mon / spawn / drop).
// The SOCI impl loads all three tables once and joins them in code (no
// SQL JOIN), mirroring how soci_quest_service joins TQUESTTABLE +
// TQUESTTERMTABLE for the per-char progress side.

#include "domain/quest_def.h"

#include <cstddef>
#include <cstdint>
#include <unordered_map>

namespace tmapsvr {

class IQuestChart
{
public:
    virtual ~IQuestChart() = default;

    // The quest definition for `quest_id`, or nullptr when unknown.
    virtual const QuestDef* Find(std::uint32_t quest_id) const = 0;

    // Total quest definitions loaded (boot log).
    virtual std::size_t Size() const = 0;
};

// Header-only in-memory impl — also the test fake. Add() inserts a def
// keyed by its quest id (later Add with the same id replaces it).
class InMemoryQuestChart final : public IQuestChart
{
public:
    void Add(const QuestDef& d) { m_defs[d.dwQuestID] = d; }

    const QuestDef* Find(std::uint32_t quest_id) const override
    {
        const auto it = m_defs.find(quest_id);
        return it == m_defs.end() ? nullptr : &it->second;
    }

    std::size_t Size() const override { return m_defs.size(); }

private:
    std::unordered_map<std::uint32_t, QuestDef> m_defs;
};

} // namespace tmapsvr
