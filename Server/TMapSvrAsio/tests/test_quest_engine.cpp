// Unit test: quest_engine.h — the pure kill-count quest core. Advances
// a QTT_HUNT term on monster death, caps at the goal, flags SUCCESS,
// and reports completion. Faithful to CQuest::FindRunningTerm /
// CheckComplete (Quest.cpp / QuestComplete.cpp).

#include "services/quest_engine.h"
#include "services/quest_chart.h"
#include "domain/quest.h"
#include "domain/quest_def.h"

#include <cstdint>
#include <cstdio>

namespace {
int g_fails = 0;
#define EXPECT(cond) do { \
    if (!(cond)) { \
        std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); \
        ++g_fails; \
    } \
} while (0)

// A "kill 3 of monster-kind 50" quest definition.
tmapsvr::QuestDef MakeHuntQuest()
{
    tmapsvr::QuestDef d;
    d.dwQuestID    = 100;
    d.bType        = tmapsvr::QT_DIEMON;
    d.bTriggerType = tmapsvr::TT_KILLMON;
    tmapsvr::QuestTermDef t;
    t.dwTermID   = 50;                  // monster kind
    t.bTermType  = tmapsvr::QTT_HUNT;
    t.bGoalCount = 3;
    d.terms.push_back(t);
    return d;
}
} // namespace

int main()
{
    using namespace tmapsvr;
    namespace qe = tmapsvr::quest_engine;

    const QuestDef def = MakeHuntQuest();

    // --- FindHuntTerm ------------------------------------------------
    {
        EXPECT(qe::FindHuntTerm(def, 50) != nullptr);
        EXPECT(qe::FindHuntTerm(def, 99) == nullptr);  // not hunted
    }

    // --- AdvanceHunt: increment, cap, status -------------------------
    {
        QuestProgressRow p;
        p.dwQuestID = 100;

        // A kill of an unrelated monster does nothing.
        auto miss = qe::AdvanceHunt(p, def, 99);
        EXPECT(!miss.advanced);
        EXPECT(p.terms.empty());          // no term materialised

        // First matching kill lazily creates the running term at 1.
        auto a1 = qe::AdvanceHunt(p, def, 50);
        EXPECT(a1.advanced);
        EXPECT(a1.count == 1 && a1.goal == 3);
        EXPECT(a1.status == QTS_RUN);
        EXPECT(p.terms.size() == 1);
        EXPECT(!qe::IsComplete(p, def));

        auto a2 = qe::AdvanceHunt(p, def, 50);
        EXPECT(a2.count == 2 && a2.status == QTS_RUN);

        auto a3 = qe::AdvanceHunt(p, def, 50);
        EXPECT(a3.count == 3 && a3.status == QTS_SUCCESS);
        EXPECT(qe::IsComplete(p, def));

        // Over-kill stays capped at the goal, still SUCCESS.
        auto a4 = qe::AdvanceHunt(p, def, 50);
        EXPECT(a4.count == 3 && a4.status == QTS_SUCCESS);
        EXPECT(p.terms[0].bCount == 3);
    }

    // --- IsComplete / FirstIncompleteTerm ----------------------------
    {
        QuestProgressRow p;
        p.dwQuestID = 100;
        EXPECT(!qe::IsComplete(p, def));   // nothing killed yet
        const QuestTermDef* inc = qe::FirstIncompleteTerm(p, def);
        EXPECT(inc != nullptr && inc->dwTermID == 50);

        qe::AdvanceHunt(p, def, 50);
        qe::AdvanceHunt(p, def, 50);
        qe::AdvanceHunt(p, def, 50);
        EXPECT(qe::IsComplete(p, def));
        EXPECT(qe::FirstIncompleteTerm(p, def) == nullptr);
    }

    // --- QTT_COMPQUEST is a turn-in link, not a counted objective ----
    {
        QuestDef d;
        d.dwQuestID = 200;
        d.bType     = QT_COMPLETE;
        QuestTermDef link;            // names the quest to turn in
        link.dwTermID = 100; link.bTermType = QTT_COMPQUEST; link.bGoalCount = 0;
        QuestTermDef hunt;            // plus an actual objective
        hunt.dwTermID = 50; hunt.bTermType = QTT_HUNT; hunt.bGoalCount = 1;
        d.terms.push_back(link);
        d.terms.push_back(hunt);

        QuestProgressRow p;
        // COMPQUEST is skipped; only the hunt term gates completion.
        EXPECT(!qe::IsComplete(p, d));
        qe::AdvanceHunt(p, d, 50);
        EXPECT(qe::IsComplete(p, d));
    }

    // --- InMemoryQuestChart lookup -----------------------------------
    {
        InMemoryQuestChart chart;
        chart.Add(def);
        EXPECT(chart.Size() == 1);
        EXPECT(chart.Find(100) != nullptr);
        EXPECT(chart.Find(100)->terms.size() == 1);
        EXPECT(chart.Find(999) == nullptr);
    }

    if (g_fails == 0)
        std::printf("test_quest_engine: hunt advance/cap/complete + chart OK\n");
    return g_fails == 0 ? 0 : 1;
}
