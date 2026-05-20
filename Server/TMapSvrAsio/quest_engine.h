#pragma once

// Quest engine — F7 Part 1.
//
// Quest lifecycle:
//   1. Player talks to NPC → CS_QUESTLIST_POSSIBLE_ACK shows available quests
//   2. Player accepts quest → CS_QUESTEXEC_REQ → IQuestEngine::StartQuest
//   3. Player kills monsters / picks items → IQuestEngine::OnMonsterKilled
//      → CS_QUESTUPDATE_ACK when term progresses
//   4. All terms complete → CS_QUESTCOMPLETE_ACK → CS_EXP_ACK reward
//   5. Player abandons quest → CS_QUESTDROP_REQ → IQuestEngine::DropQuest
//
// Quest template (IQuestChart):
//   Loaded from TQUESTCHART at boot; defines quest_id, trigger_npc,
//   terms (kill N of template_id X), rewards.
//   HardcodedQuestChart ships two kill quests for smoke testing.
//
// Source:
//   CSHandler.cpp:3535 — OnCS_QUESTEXEC_REQ
//   CSHandler.cpp:3590 — OnCS_QUESTDROP_REQ
//   CSSender.cpp:1825  — SendCS_QUESTUPDATE_ACK
//   CSSender.cpp:1843  — SendCS_QUESTCOMPLETE_ACK
//   CSSender.cpp:1861  — SendCS_QUESTLIST_ACK
//   TMapType.h:2009    — tagQUESTTERM

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tmapsvr {

// ---------------------------------------------------------------------------
// Quest term types (legacy QTT_* enum)
// ---------------------------------------------------------------------------
namespace QuestTermType {
    constexpr std::uint8_t Hunt     = 1;  // kill N monsters
    constexpr std::uint8_t Timer    = 2;  // time-limited
    constexpr std::uint8_t Item     = 3;  // collect N items
    constexpr std::uint8_t Talk     = 4;  // talk to NPC
    constexpr std::uint8_t Deliver  = 5;  // deliver item to NPC
}

// Quest term status
namespace QuestTermStatus {
    constexpr std::uint8_t Running  = 0;
    constexpr std::uint8_t Success  = 1;
    constexpr std::uint8_t Failed   = 2;
}

// Quest result
namespace QuestResult {
    constexpr std::uint8_t Success  = 0;
    constexpr std::uint8_t Drop     = 1;
    constexpr std::uint8_t Failed   = 2;
}

// ---------------------------------------------------------------------------
// Runtime quest state
// ---------------------------------------------------------------------------

struct QuestTermProgress
{
    std::uint32_t term_id       = 0;
    std::uint8_t  term_type     = 0;
    std::uint8_t  target_count  = 0;   // required
    std::uint8_t  current_count = 0;   // achieved
    std::uint32_t target_id     = 0;   // monster/item template_id for this term
    std::uint8_t  status        = QuestTermStatus::Running;

    bool IsComplete() const
    {
        return status == QuestTermStatus::Success ||
               current_count >= target_count;
    }
};

struct QuestState
{
    std::uint32_t               quest_id   = 0;
    std::uint8_t                quest_type = 0;
    std::uint8_t                count_max  = 0;
    bool                        complete   = false;
    std::vector<QuestTermProgress> terms;

    bool AllTermsComplete() const
    {
        return !terms.empty() &&
               std::all_of(terms.begin(), terms.end(),
                   [](const QuestTermProgress& t) { return t.IsComplete(); });
    }
};

// ---------------------------------------------------------------------------
// Quest template (chart data)
// ---------------------------------------------------------------------------

struct QuestTermTemplate
{
    std::uint32_t term_id      = 0;
    std::uint8_t  term_type    = QuestTermType::Hunt;
    std::uint8_t  count        = 1;    // required count
    std::uint32_t target_id    = 0;    // monster/item template_id
};

struct QuestRewardTemplate
{
    std::uint16_t item_id     = 0;
    std::uint8_t  count       = 1;
    std::uint32_t exp_reward  = 0;
    std::uint32_t gold        = 0;
};

struct QuestTemplate
{
    std::uint32_t               quest_id    = 0;
    std::uint8_t                quest_type  = 0;
    std::uint8_t                count_max   = 1;
    std::uint32_t               trigger_npc = 0;  // NPC that gives the quest
    std::string                 name;
    std::vector<QuestTermTemplate>  terms;
    QuestRewardTemplate             reward;
};

class IQuestChart
{
public:
    virtual ~IQuestChart() = default;
    virtual const QuestTemplate* GetQuest(std::uint32_t quest_id) const = 0;
    // All quests available from a given NPC
    virtual std::vector<const QuestTemplate*>
        GetNpcQuests(std::uint32_t npc_id) const = 0;
};

// ---------------------------------------------------------------------------
// IQuestEngine
// ---------------------------------------------------------------------------

struct QuestKillEvent
{
    std::uint32_t quest_id = 0;   // which quest progressed
    std::uint32_t term_id  = 0;   // which term progressed
    std::uint8_t  new_count = 0;
    bool          term_complete = false;
    bool          quest_complete = false;
};

class IQuestEngine
{
public:
    virtual ~IQuestEngine() = default;

    // Accept a quest (returns false if already active or invalid).
    virtual bool StartQuest(std::uint32_t char_id,
                            std::uint32_t quest_id) = 0;

    // Abandon active quest.
    virtual bool DropQuest(std::uint32_t char_id,
                           std::uint32_t quest_id) = 0;

    // Report monster kill; returns events for each affected quest term.
    virtual std::vector<QuestKillEvent>
        OnMonsterKilled(std::uint32_t char_id,
                        std::uint32_t monster_template_id) = 0;

    // Get all active QuestStates for char.
    virtual std::vector<const QuestState*>
        GetActiveQuests(std::uint32_t char_id) const = 0;

    // Get completed quest ids.
    virtual std::vector<std::uint32_t>
        GetCompletedQuestIds(std::uint32_t char_id) const = 0;

    // Collect chart pointer (for quest list queries).
    virtual const IQuestChart* Chart() const = 0;
};

// ---------------------------------------------------------------------------
// HardcodedQuestChart — two kill quests for smoke testing
// ---------------------------------------------------------------------------

class HardcodedQuestChart : public IQuestChart
{
public:
    HardcodedQuestChart()
    {
        // Quest 101 — kill 5 monsters (any, template_id=0 = any)
        {
            QuestTemplate q{};
            q.quest_id    = 101;
            q.name        = "First Hunt";
            q.trigger_npc = 1;
            QuestTermTemplate t{};
            t.term_id   = 1001; t.term_type = QuestTermType::Hunt;
            t.count     = 5;    t.target_id = 0;  // 0 = any monster
            q.terms.push_back(t);
            q.reward.exp_reward = 200; q.reward.gold = 50;
            m_quests[q.quest_id] = q;
            m_npc_quests[1].push_back(q.quest_id);
        }
        // Quest 102 — kill 3 specific monsters (template_id=10)
        {
            QuestTemplate q{};
            q.quest_id    = 102;
            q.name        = "Wolf Hunt";
            q.trigger_npc = 1;
            QuestTermTemplate t{};
            t.term_id   = 1002; t.term_type = QuestTermType::Hunt;
            t.count     = 3;    t.target_id = 10; // template_id=10
            q.terms.push_back(t);
            q.reward.exp_reward = 500; q.reward.gold = 100;
            m_quests[q.quest_id] = q;
            m_npc_quests[1].push_back(q.quest_id);
        }
    }

    const QuestTemplate* GetQuest(std::uint32_t quest_id) const override
    {
        auto it = m_quests.find(quest_id);
        return it != m_quests.end() ? &it->second : nullptr;
    }

    std::vector<const QuestTemplate*>
    GetNpcQuests(std::uint32_t npc_id) const override
    {
        std::vector<const QuestTemplate*> result;
        auto it = m_npc_quests.find(npc_id);
        if (it == m_npc_quests.end()) return result;
        for (auto qid : it->second)
        {
            auto qit = m_quests.find(qid);
            if (qit != m_quests.end())
                result.push_back(&qit->second);
        }
        return result;
    }

private:
    std::unordered_map<std::uint32_t, QuestTemplate>             m_quests;
    std::unordered_map<std::uint32_t, std::vector<std::uint32_t>> m_npc_quests;
};

// ---------------------------------------------------------------------------
// LocalQuestEngine
// ---------------------------------------------------------------------------

class LocalQuestEngine : public IQuestEngine
{
public:
    explicit LocalQuestEngine(const IQuestChart& chart)
        : m_chart(chart) {}

    bool StartQuest(std::uint32_t char_id,
                    std::uint32_t quest_id) override
    {
        const auto* tmpl = m_chart.GetQuest(quest_id);
        if (!tmpl) return false;
        auto& active = m_active[char_id];
        // Already active?
        for (const auto& q : active)
            if (q.quest_id == quest_id) return false;
        // Already completed?
        auto& done = m_completed[char_id];
        if (done.count(quest_id)) return false;

        QuestState state{};
        state.quest_id   = quest_id;
        state.quest_type = tmpl->quest_type;
        state.count_max  = tmpl->count_max;
        for (const auto& t : tmpl->terms)
        {
            QuestTermProgress tp{};
            tp.term_id      = t.term_id;
            tp.term_type    = t.term_type;
            tp.target_count = t.count;
            tp.target_id    = t.target_id;
            state.terms.push_back(tp);
        }
        active.push_back(std::move(state));
        return true;
    }

    bool DropQuest(std::uint32_t char_id,
                   std::uint32_t quest_id) override
    {
        auto it = m_active.find(char_id);
        if (it == m_active.end()) return false;
        auto& vec = it->second;
        const auto before = vec.size();
        vec.erase(std::remove_if(vec.begin(), vec.end(),
            [quest_id](const QuestState& q) {
                return q.quest_id == quest_id;
            }), vec.end());
        return vec.size() < before;
    }

    std::vector<QuestKillEvent>
    OnMonsterKilled(std::uint32_t char_id,
                    std::uint32_t monster_template_id) override
    {
        std::vector<QuestKillEvent> events;
        auto it = m_active.find(char_id);
        if (it == m_active.end()) return events;

        for (auto& quest : it->second)
        {
            if (quest.complete) continue;
            for (auto& term : quest.terms)
            {
                if (term.term_type != QuestTermType::Hunt) continue;
                if (term.IsComplete()) continue;
                // target_id=0 means "any monster"
                if (term.target_id != 0 &&
                    term.target_id != monster_template_id) continue;

                ++term.current_count;
                const bool term_done =
                    term.current_count >= term.target_count;
                if (term_done)
                    term.status = QuestTermStatus::Success;

                const bool quest_done = quest.AllTermsComplete();
                if (quest_done)
                {
                    quest.complete = true;
                    m_completed[char_id].insert(quest.quest_id);
                }

                QuestKillEvent ev{};
                ev.quest_id       = quest.quest_id;
                ev.term_id        = term.term_id;
                ev.new_count      = term.current_count;
                ev.term_complete  = term_done;
                ev.quest_complete = quest_done;
                events.push_back(ev);
            }
        }

        // Remove completed quests from active list
        it->second.erase(
            std::remove_if(it->second.begin(), it->second.end(),
                [](const QuestState& q) { return q.complete; }),
            it->second.end());

        return events;
    }

    std::vector<const QuestState*>
    GetActiveQuests(std::uint32_t char_id) const override
    {
        std::vector<const QuestState*> result;
        auto it = m_active.find(char_id);
        if (it == m_active.end()) return result;
        for (const auto& q : it->second) result.push_back(&q);
        return result;
    }

    std::vector<std::uint32_t>
    GetCompletedQuestIds(std::uint32_t char_id) const override
    {
        std::vector<std::uint32_t> result;
        auto it = m_completed.find(char_id);
        if (it == m_completed.end()) return result;
        for (auto id : it->second) result.push_back(id);
        return result;
    }

    const IQuestChart* Chart() const override { return &m_chart; }

private:
    const IQuestChart&                                                    m_chart;
    std::unordered_map<std::uint32_t, std::vector<QuestState>>            m_active;
    std::unordered_map<std::uint32_t, std::unordered_set<std::uint32_t>> m_completed;
};

} // namespace tmapsvr
