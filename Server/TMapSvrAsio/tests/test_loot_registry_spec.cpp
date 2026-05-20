// Spec test for LocalLootRegistry + GenerateStubLoot.
//
// §1  SetLoot + GetLoot + HasLoot
// §2  TakeItem — removes correct slot, returns item
// §3  ClearLoot
// §4  GenerateStubLoot — level-based gold + item drop logic

#include "loot_registry.h"
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

void TestSetGetHas()
{
    std::printf("[§1 SetLoot + GetLoot + HasLoot]\n");
    tmapsvr::LocalLootRegistry r;

    Check(!r.HasLoot(1u), "no loot before SetLoot");

    tmapsvr::MonsterLoot loot{};
    loot.gold = 100; loot.silver = 5;

    tmapsvr::ItemInstance item{};
    item.item_id = 42; item.inven_id = 0; item.count = 1;
    loot.items.push_back(item);

    r.SetLoot(1u, std::move(loot));
    Check(r.HasLoot(1u), "HasLoot true after SetLoot");

    const auto* got = r.GetLoot(1u);
    Check(got != nullptr, "GetLoot returns non-null");
    if (got)
    {
        Check(got->gold == 100u, "gold correct");
        Check(got->items.size() == 1u, "item count = 1");
    }
}

void TestTakeItem()
{
    std::printf("[§2 TakeItem]\n");
    tmapsvr::LocalLootRegistry r;
    tmapsvr::MonsterLoot loot{};

    tmapsvr::ItemInstance a{}; a.item_id = 10; a.inven_id = 0;
    tmapsvr::ItemInstance b{}; b.item_id = 20; b.inven_id = 1;
    loot.items = { a, b };
    r.SetLoot(5u, std::move(loot));

    auto taken = r.TakeItem(5u, 0);
    Check(taken.has_value(), "TakeItem(slot=0) returns item");
    if (taken) Check(taken->item_id == 10u, "correct item taken");

    const auto* remaining = r.GetLoot(5u);
    Check(remaining && remaining->items.size() == 1u,
        "one item remains after take");

    auto miss = r.TakeItem(5u, 0);
    Check(!miss.has_value(), "second TakeItem(slot=0) returns nullopt");

    auto unknown = r.TakeItem(99u, 0);
    Check(!unknown.has_value(), "TakeItem from unknown mon returns nullopt");
}

void TestClearLoot()
{
    std::printf("[§3 ClearLoot]\n");
    tmapsvr::LocalLootRegistry r;
    tmapsvr::MonsterLoot loot{};
    loot.gold = 50;
    r.SetLoot(3u, std::move(loot));
    Check(r.HasLoot(3u), "loot set");
    r.ClearLoot(3u);
    Check(!r.HasLoot(3u), "loot cleared");
}

void TestGenerateStubLoot()
{
    std::printf("[§4 GenerateStubLoot]\n");

    auto loot1 = tmapsvr::GenerateStubLoot(1);
    Check(loot1.copper > 0, "level 1 copper > 0");

    auto loot10 = tmapsvr::GenerateStubLoot(10);
    Check(loot10.copper > loot1.copper, "higher level → more copper");

    // Level 2 (even) drops item, level 1 (odd) does not
    auto even = tmapsvr::GenerateStubLoot(2);
    auto odd  = tmapsvr::GenerateStubLoot(1);
    Check(!even.items.empty(), "even level drops item");
    Check(odd.items.empty(),   "odd level drops no item");
}

} // namespace

int main()
{
    std::printf("=== tmapsvr_asio  LocalLootRegistry spec ===\n\n");
    try
    {
        TestSetGetHas();
        TestTakeItem();
        TestClearLoot();
        TestGenerateStubLoot();
    }
    catch (const std::exception& ex)
    {
        std::printf("  FAIL  unexpected exception: %s\n", ex.what());
        ++g_failed;
    }
    std::printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
