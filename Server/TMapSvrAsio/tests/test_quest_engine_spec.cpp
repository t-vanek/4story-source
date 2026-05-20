// Spec test for LocalQuestEngine + HardcodedQuestChart.
//
// §1  HardcodedQuestChart: quest 101-104 present, NPC 1 has all four
// §2  StartQuest — success on first start, fail on duplicate
// §3  StartQuest — fail for unknown quest_id
// §4  OnMonsterKilled — hunt term progresses
// §5  OnMonsterKilled — quest completes when all terms done
// §6  OnMonsterKilled — specific template_id (quest 102 target_id=10)
// §7  DropQuest — removes active quest
// §8  GetActiveQuests / GetCompletedQuestIds
// §9  OnItemPickedUp — GETITEM term (quest 103 collect 3 of item 5)
// §10 OnNpcTalked    — TALK term (quest 104 talk to NPC 2)

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
    Check(chart.GetQuest(103) != nullptr, "quest 103 exists (collect)");
    Check(chart.GetQuest(104) != nullptr, "quest 104 exists (talk)");
    Check(chart.GetQuest(999) == nullptr, "unknown quest → nullptr");

    auto npc_quests = chart.GetNpcQuests(1);
    Check(npc_quests.size() == 4, "NPC 1 has 4 quests");
    Check(chart.GetNpcQuests(99).empty(), "unknown NPC has no quests");

    // Verify term types match legacy wire values
    const auto* q101 = chart.GetQuest(101);
    Check(q101 && !q101->terms.empty() &&
          q101->terms[0].term_type == tmapsvr::QuestTermType::Hunt,
          "quest 101 term type = Hunt=3");
    const auto* q103 = chart.GetQuest(103);
    Check(q103 && !q103->terms.empty() &&
          q103->terms[0].term_type == tmapsvr::QuestTermType::GetItem,
          "quest 103 term type = GetItem=2");
    const auto* q104 = chart.GetQuest(104);
    Check(q104 && !q104->terms.empty() &&
          q104->terms[0].term_type == tmapsvr::QuestTermType::Talk,
          "quest 104 term type = Talk=13");
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

    tmapsvr::QuestProgressEvent last;
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

void TestCollectItem()
{
    std::printf("[§9 OnItemPickedUp — GETITEM term]\n");
    tmapsvr::HardcodedQuestChart chart;
    tmapsvr::LocalQuestEngine    engine(chart);
    engine.StartQuest(5u, 103u);  // collect 3 of item_id=5

    // Wrong item should not progress
    auto ev_wrong = engine.OnItemPickedUp(5u, 99u);
    Check(ev_wrong.empty(), "pick up wrong item → no events");

    // Two correct pickups — progresses but not complete
    auto ev1 = engine.OnItemPickedUp(5u, 5u);
    Check(ev1.size() == 1, "first pickup → 1 event");
    if (!ev1.empty())
    {
        Check(ev1[0].quest_id == 103u,                 "correct quest_id");
        Check(ev1[0].term_type == tmapsvr::QuestTermType::GetItem,
                                                        "term_type = GetItem=2");
        Check(ev1[0].new_count == 1u,                   "count = 1");
        Check(!ev1[0].term_complete,                    "term not complete");
        Check(!ev1[0].quest_complete,                   "quest not complete");
    }
    engine.OnItemPickedUp(5u, 5u);

    // Third pickup completes quest
    auto ev3 = engine.OnItemPickedUp(5u, 5u);
    Check(!ev3.empty() && ev3[0].term_complete,  "term complete at 3 pickups");
    Check(!ev3.empty() && ev3[0].quest_complete, "quest complete at 3 pickups");
    Check(engine.GetActiveQuests(5u).empty(),    "no active quests after collect completion");

    const auto done = engine.GetCompletedQuestIds(5u);
    Check(done.size() == 1 && done[0] == 103u,   "quest 103 in completed list");
}

void TestNpcTalk()
{
    std::printf("[§10 OnNpcTalked — TALK term]\n");
    tmapsvr::HardcodedQuestChart chart;
    tmapsvr::LocalQuestEngine    engine(chart);
    engine.StartQuest(6u, 104u);  // talk to NPC 2

    // Talk to wrong NPC — no progress
    auto ev_wrong = engine.OnNpcTalked(6u, 99u);
    Check(ev_wrong.empty(), "talk to wrong NPC → no events");

    // Talk to correct NPC — one-shot completion
    auto ev = engine.OnNpcTalked(6u, 2u);
    Check(ev.size() == 1, "talk to NPC 2 → 1 event");
    if (!ev.empty())
    {
        Check(ev[0].quest_id == 104u,                  "correct quest_id");
        Check(ev[0].term_type == tmapsvr::QuestTermType::Talk,
                                                        "term_type = Talk=13");
        Check(ev[0].term_complete,                     "term complete immediately");
        Check(ev[0].quest_complete,                    "quest complete");
        Check(ev[0].new_count == 1u,                   "count = target = 1");
    }
    Check(engine.GetActiveQuests(6u).empty(),   "no active quests after talk completion");

    const auto done = engine.GetCompletedQuestIds(6u);
    Check(done.size() == 1 && done[0] == 104u,  "quest 104 in completed list");
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
        TestCollectItem();
        TestNpcTalk();
    }
    catch (const std::exception& ex)
    {
        std::printf("  FAIL  unexpected exception: %s\n", ex.what());
        ++g_failed;
    }
    std::printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
