// Spec test for FakeNpcService + HardcodedNpcService.
//
// §1  FakeNpcService: AddNpc + GetNpc + FindShopItem
// §2  GetNpc on unknown id → nullptr
// §3  FindShopItem on unknown item → nullopt
// §4  HardcodedNpcService: merchant + healer present

#include "npc_service.h"
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

void TestFakeNpcService()
{
    std::printf("[§1/§2/§3 FakeNpcService]\n");
    tmapsvr::FakeNpcService svc;

    Check(svc.GetNpc(1) == nullptr, "§2 unknown npc → nullptr");

    tmapsvr::NpcData npc{};
    npc.npc_id = 10;
    npc.type   = tmapsvr::NpcType::Item;
    npc.name   = "Merchant";
    npc.shop_items = { { 100, 500u }, { 200, 1000u } };
    svc.AddNpc(npc);

    const auto* found = svc.GetNpc(10);
    Check(found != nullptr, "§1 GetNpc after AddNpc");
    if (found)
    {
        Check(found->name == "Merchant", "name correct");
        Check(found->shop_items.size() == 2, "shop item count");
    }

    auto item = svc.FindShopItem(10, 100);
    Check(item.has_value(), "§1 FindShopItem by item_id");
    if (item) Check(item->price == 500u, "price correct");

    auto miss = svc.FindShopItem(10, 999);
    Check(!miss.has_value(), "§3 unknown item_id → nullopt");

    auto npc_miss = svc.FindShopItem(99, 100);
    Check(!npc_miss.has_value(), "§3 unknown npc_id → nullopt");
}

void TestHardcodedNpcService()
{
    std::printf("[§4 HardcodedNpcService]\n");
    tmapsvr::HardcodedNpcService svc;

    const auto* merchant = svc.GetNpc(1);
    Check(merchant != nullptr, "merchant (npc_id=1) exists");
    if (merchant)
    {
        Check(merchant->type == tmapsvr::NpcType::Item, "type=Item");
        Check(!merchant->shop_items.empty(), "has shop items");
    }

    const auto* healer = svc.GetNpc(2);
    Check(healer != nullptr, "healer (npc_id=2) exists");
    if (healer)
        Check(healer->type == tmapsvr::NpcType::Revival, "type=Revival");

    auto potion = svc.FindShopItem(1, 1);
    Check(potion.has_value(), "HP potion (item_id=1) in merchant shop");
    if (potion) Check(potion->price == 100u, "HP potion price=100g");
}

} // namespace

int main()
{
    std::printf("=== tmapsvr_asio  FakeNpcService + HardcodedNpcService spec ===\n\n");
    try
    {
        TestFakeNpcService();
        TestHardcodedNpcService();
    }
    catch (const std::exception& ex)
    {
        std::printf("  FAIL  unexpected exception: %s\n", ex.what());
        ++g_failed;
    }
    std::printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
