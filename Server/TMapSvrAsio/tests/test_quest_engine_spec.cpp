// Spec test for LocalQuestEngine + HardcodedQuestChart.
//
// §1  HardcodedQuestChart: quest 101 + 102 present, NPC 1 has both
// §2  StartQuest — success on first start, fail on duplicate
// §3  StartQuest — fail for unknown quest_id
// §4  OnMonsterKilled — hunt term progresses
// §5  OnMonsterKilled — quest completes when all terms done
// §6  OnMonsterKilled — specific template_id (quest 102 target_id=10)
// §7  DropQuest — removes active quest
// §8  GetActiveQuests / GetCompletedQuestIds

#include "quest_engine.h"
#include <cstdio>
#include <exception>

namespace {

int g_passed = 0;
int g_failed = 0;

void Check(bool ok, const char* label)
{
    if (ok) { ++g_passed; std::printf("  PASS  %s\n", label); }
    else    { ++g_failed; std::printf("  FAIL  %s\n", label); }
}

void TestQuestChart()
{
    std::printf("[§1 HardcodedQuestChart]\n");
    tmapsvr::HardcodedQuestChart chart;

    Check(chart.GetQuest(101) != nullptr, "quest 101 exists");
    Check(chart.GetQuest(102) != nullptr, "quest 102 exists");
    Check(chart.GetQuest(999) == nullptr, "unknown quest → nullptr");

    auto npc_quests = chart.GetNpcQuests(1);
    Check(npc_quests.size() == 2, "NPC 1 has 2 quests");
    Check(chart.GetNpcQuests(99).empty(), "unknown NPC has no quests");
}

void TestStartQuest()
{
    std::printf("[§2/§3 StartQuest]\n");
    tmapsvr::HardcodedQuestChart chart;
    tmapsvr::LocalQuestEngine    engine(chart);

    Check(engine.StartQuest(100u, 101u), "start quest 101 = true");
    Check(!engine.StartQuest(100u, 101u), "duplicate start = false");
    Check(!engine.StartQuest(100u, 999u), "unknown quest_id = false");

    const auto active = engine.GetActiveQuests(100u);
    Check(active.size() == 1, "one active quest");
    if (!active.empty())
    {
        Check(active[0]->quest_id == 101u, "active quest_id = 101");
        Check(active[0]->terms.size() == 1, "one term in quest 101");
    }
}

void TestHuntProgress()
{
    std::printf("[§4 OnMonsterKilled — term progresses]\n");
    tmapsvr::HardcodedQuestChart chart;
    tmapsvr::LocalQuestEngine    engine(chart);
    engine.StartQuest(1u, 101u);  // kill 5 any monsters

    auto ev1 = engine.OnMonsterKilled(1u, 0u);  // any template
    Check(ev1.size() == 1, "one kill event");
    if (!ev1.empty())
    {
        Check(ev1[0].quest_id == 101u, "correct quest_id");
        Check(ev1[0].new_count == 1u,  "count = 1 after first kill");
        Check(!ev1[0].term_complete,   "term not yet complete");
        Check(!ev1[0].quest_complete,  "quest not yet complete");
    }
}

void TestQuestCompletion()
{
    std::printf("[§5 quest completes after all terms done]\n");
    tmapsvr::HardcodedQuestChart chart;
    tmapsvr::LocalQuestEngine    engine(chart);
    engine.StartQuest(2u, 101u);  // kill 5 monsters

    tmapsvr::QuestKillEvent last;
    for (int i = 0; i < 5; ++i)
    {
        auto evs = engine.OnMonsterKilled(2u, 1u);
        if (!evs.empty()) last = evs[0];
    }

    Check(last.new_count == 5u,     "count = 5 after 5 kills");
    Check(last.term_complete,       "term complete at 5 kills");
    Check(last.quest_complete,      "quest complete at 5 kills");

    // Quest removed from active
    Check(engine.GetActiveQuests(2u).empty(),
        "active quests empty after completion");

    // Quest in completed list
    const auto done = engine.GetCompletedQuestIds(2u);
    Check(done.size() == 1 && done[0] == 101u,
        "quest 101 in completed list");

    // Can't start again
    Check(!engine.StartQuest(2u, 101u),
        "cannot start already-completed quest");
}

void TestSpecificTemplateId()
{
    std::printf("[§6 quest 102 — specific template_id=10]\n");
    tmapsvr::HardcodedQuestChart chart;
    tmapsvr::LocalQuestEngine    engine(chart);
    engine.StartQuest(3u, 102u);  // kill 3 template_id=10

    // Kill wrong template — should NOT progress
    auto ev_wrong = engine.OnMonsterKilled(3u, 99u);
    Check(ev_wrong.empty(), "kill wrong template → no events");

    // Kill correct template
    for (int i = 0; i < 3; ++i)
        engine.OnMonsterKilled(3u, 10u);

    Check(engine.GetActiveQuests(3u).empty(),
        "quest 102 complete after 3 kills of template_id=10");
}

void TestDropQuest()
{
    std::printf("[§7 DropQuest]\n");
    tmapsvr::HardcodedQuestChart chart;
    tmapsvr::LocalQuestEngine    engine(chart);
    engine.StartQuest(4u, 101u);

    Check(engine.DropQuest(4u, 101u),   "DropQuest returns true");
    Check(!engine.DropQuest(4u, 101u),  "second drop returns false");
    Check(engine.GetActiveQuests(4u).empty(), "no active quests after drop");
}

} // namespace

int main()
{
    std::printf("=== tmapsvr_asio  LocalQuestEngine spec ===\n\n");
    try
    {
        TestQuestChart();
        TestStartQuest();
        TestHuntProgress();
        TestQuestCompletion();
        TestSpecificTemplateId();
        TestDropQuest();
    }
    catch (const std::exception& ex)
    {
        std::printf("  FAIL  unexpected exception: %s\n", ex.what());
        ++g_failed;
    }
    std::printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
