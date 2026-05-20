// Spec test for FakeInventoryService + ItemInstance.
//
// §1  ItemInstance value-init + copy
// §2  LoadCharItems + GetItems + FindItem
// §3  MoveItem — within same inven type
// §4  MoveItem — equip (src=Main, dst=Equip)
// §5  RemoveItem
// §6  AddItem
// §7  UnloadCharItems removes all char data

#include "inventory_service.h"

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

tmapsvr::ItemInstance MakeItem(std::uint16_t item_id,
                                std::uint8_t  inven_type,
                                std::uint8_t  inven_id,
                                std::uint8_t  count = 1)
{
    tmapsvr::ItemInstance i{};
    i.item_id    = item_id;
    i.inven_type = inven_type;
    i.inven_id   = inven_id;
    i.count      = count;
    i.dura_max   = 1000;
    i.dura_cur   = 1000;
    return i;
}

void TestItemInstance()
{
    std::printf("[§1 ItemInstance value-init + copy]\n");
    tmapsvr::ItemInstance i{};
    Check(i.item_id == 0 && i.count == 0, "default count=0");
    Check(i.magic.empty(), "magic list empty on default");

    tmapsvr::ItemMagic m{1, 500};
    i.item_id = 1042; i.count = 5; i.magic.push_back(m);
    auto copy = i;
    Check(copy.item_id == 1042 && copy.count == 5, "copy preserves fields");
    Check(copy.magic.size() == 1 && copy.magic[0].value == 500,
        "copy preserves magic list");
}

void TestLoadAndGet()
{
    std::printf("[§2 LoadCharItems + GetItems + FindItem]\n");
    tmapsvr::FakeInventoryService svc;

    Check(svc.GetItems(1, tmapsvr::InvenType::Main).empty(),
        "empty before load");

    std::vector<tmapsvr::ItemInstance> items = {
        MakeItem(100, tmapsvr::InvenType::Main,  0),
        MakeItem(101, tmapsvr::InvenType::Main,  1),
        MakeItem(200, tmapsvr::InvenType::Equip, 0),
    };
    svc.LoadCharItems(1, std::move(items));

    auto main_items = svc.GetItems(1, tmapsvr::InvenType::Main);
    Check(main_items.size() == 2, "GetItems(Main) returns 2");

    auto equip_items = svc.GetItems(1, tmapsvr::InvenType::Equip);
    Check(equip_items.size() == 1, "GetItems(Equip) returns 1");

    const auto* found = svc.FindItem(1, tmapsvr::InvenType::Main, 1);
    Check(found != nullptr && found->item_id == 101, "FindItem by slot");

    const auto* miss = svc.FindItem(1, tmapsvr::InvenType::Main, 99);
    Check(miss == nullptr, "FindItem on missing slot returns nullptr");
}

void TestMoveItem()
{
    std::printf("[§3 MoveItem within inventory]\n");
    tmapsvr::FakeInventoryService svc;
    svc.LoadCharItems(1, { MakeItem(50, tmapsvr::InvenType::Main, 3) });

    Check(svc.MoveItem(1,
        tmapsvr::InvenType::Main, 3,
        tmapsvr::InvenType::Main, 7, 1),
        "MoveItem returns true on success");

    Check(svc.FindItem(1, tmapsvr::InvenType::Main, 3) == nullptr,
        "old slot empty after move");
    const auto* dest = svc.FindItem(1, tmapsvr::InvenType::Main, 7);
    Check(dest != nullptr && dest->item_id == 50,
        "new slot has item after move");

    // Move non-existent
    Check(!svc.MoveItem(1,
        tmapsvr::InvenType::Main, 99,
        tmapsvr::InvenType::Main, 0, 1),
        "MoveItem returns false for unknown src slot");
}

void TestEquip()
{
    std::printf("[§4 MoveItem Main→Equip]\n");
    tmapsvr::FakeInventoryService svc;
    svc.LoadCharItems(1, { MakeItem(200, tmapsvr::InvenType::Main, 5) });

    Check(svc.MoveItem(1,
        tmapsvr::InvenType::Main,  5,
        tmapsvr::InvenType::Equip, 2, 1),
        "equip succeeds");

    Check(svc.FindItem(1, tmapsvr::InvenType::Main,  5) == nullptr,
        "main slot empty after equip");
    const auto* eq = svc.FindItem(1, tmapsvr::InvenType::Equip, 2);
    Check(eq != nullptr && eq->item_id == 200, "equip slot has item");
}

void TestRemove()
{
    std::printf("[§5 RemoveItem]\n");
    tmapsvr::FakeInventoryService svc;
    svc.LoadCharItems(1, { MakeItem(77, tmapsvr::InvenType::Main, 0) });

    auto removed = svc.RemoveItem(1, tmapsvr::InvenType::Main, 0);
    Check(removed.has_value(), "RemoveItem returns item");
    if (removed) Check(removed->item_id == 77, "removed item has correct id");
    Check(svc.GetItems(1, tmapsvr::InvenType::Main).empty(),
        "inventory empty after remove");

    auto miss = svc.RemoveItem(1, tmapsvr::InvenType::Main, 0);
    Check(!miss.has_value(), "second remove returns nullopt");
}

void TestAddItem()
{
    std::printf("[§6 AddItem]\n");
    tmapsvr::FakeInventoryService svc;
    svc.LoadCharItems(1, {});

    Check(svc.AddItem(1, MakeItem(999, tmapsvr::InvenType::Main, 10)),
        "AddItem returns true");
    Check(svc.GetItems(1, tmapsvr::InvenType::Main).size() == 1,
        "item count = 1 after add");
}

void TestUnload()
{
    std::printf("[§7 UnloadCharItems cleans up]\n");
    tmapsvr::FakeInventoryService svc;
    svc.LoadCharItems(1, { MakeItem(1, tmapsvr::InvenType::Main, 0) });
    svc.UnloadCharItems(1);
    Check(svc.GetItems(1, tmapsvr::InvenType::Main).empty(),
        "items gone after unload");
}

} // namespace

int main()
{
    std::printf("=== tmapsvr_asio  FakeInventoryService spec ===\n\n");
    try
    {
        TestItemInstance();
        TestLoadAndGet();
        TestMoveItem();
        TestEquip();
        TestRemove();
        TestAddItem();
        TestUnload();
    }
    catch (const std::exception& ex)
    {
        std::printf("  FAIL  unexpected exception: %s\n", ex.what());
        ++g_failed;
    }
    std::printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
