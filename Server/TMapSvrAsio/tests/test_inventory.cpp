// Unit test: inventory slot allocation (FindBlankSlot, the
// CTInven::GetBlankPos port) + the inventory service AddItem loot/pickup
// path (slot assignment + "bag full" failure) exercised through the fake.

#include "services/inventory_slots.h"
#include "services/fake_inventory_service.h"
#include "domain/inventory.h"

#include <cstdint>
#include <cstdio>
#include <vector>

namespace {
int g_fails = 0;
#define EXPECT(cond) do { \
    if (!(cond)) { \
        std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); \
        ++g_fails; \
    } \
} while (0)

using tmapsvr::InventoryRow;

InventoryRow Row(std::uint8_t slot, std::uint16_t item = 1)
{
    InventoryRow r;
    r.bInvenID = slot;
    r.wItemID  = item;
    return r;
}
} // namespace

int main()
{
    using namespace tmapsvr;

    // --- FindBlankSlot: first gap, faithful to GetBlankPos --------------
    {
        EXPECT(FindBlankSlot({}) == 0);                       // empty → 0
        EXPECT(FindBlankSlot({ Row(0), Row(1), Row(2) }) == 3);   // append
        EXPECT(FindBlankSlot({ Row(0), Row(2) }) == 1);       // fill the gap
        EXPECT(FindBlankSlot({ Row(1), Row(2) }) == 0);       // gap at front

        // Equip (254) / tab-marker (255) rows sit outside [0, capacity) and
        // never collide with bag allocation.
        EXPECT(FindBlankSlot({ Row(254), Row(255) }) == 0);
    }

    // --- FindBlankSlot: capacity boundary / full ----------------------
    {
        // Custom small capacity: slots 0,1 used out of 3 → 2; all used → none.
        EXPECT(FindBlankSlot({ Row(0), Row(1) }, 3) == 2);
        EXPECT(!FindBlankSlot({ Row(0), Row(1), Row(2) }, 3).has_value());

        // Full default bag (0..23) → nullopt.
        std::vector<InventoryRow> full;
        for (std::uint8_t i = 0; i < kDefaultBagSlots; ++i)
            full.push_back(Row(i));
        EXPECT(!FindBlankSlot(full).has_value());
    }

    // --- FakeInventoryService::AddItem: assign, fill, overflow --------
    {
        FakeInventoryService inv;
        ItemInstance it; it.wItemID = 555; it.bELD = 1;

        // Empty bag → slot 0, and the row is queryable + carries the item.
        const auto s0 = inv.AddItem(42, it);
        EXPECT(s0 == 0);
        auto rows = inv.LoadInventory(42);
        EXPECT(rows.size() == 1 && rows[0].bInvenID == 0 &&
               rows[0].wItemID == 555 && rows[0].bELD == 1);

        // Next two go to 1, 2.
        EXPECT(inv.AddItem(42, it) == 1);
        EXPECT(inv.AddItem(42, it) == 2);
        EXPECT(inv.LoadInventory(42).size() == 3);

        // A different char is independent.
        EXPECT(inv.AddItem(99, it) == 0);
    }

    {
        // Pre-seeded rows → AddItem fills the first free slot after them.
        FakeInventoryService inv;
        inv.SetRows(7, { Row(0), Row(1), Row(3) });
        ItemInstance it; it.wItemID = 10;
        EXPECT(inv.AddItem(7, it) == 2);    // gap at 2
        EXPECT(inv.AddItem(7, it) == 4);    // then append past 3
    }

    {
        // Bag full → AddItem returns nullopt (caller → MIT_FULLINVEN), and
        // nothing is added.
        FakeInventoryService inv;
        std::vector<InventoryRow> full;
        for (std::uint8_t i = 0; i < kDefaultBagSlots; ++i)
            full.push_back(Row(i));
        inv.SetRows(5, full);
        ItemInstance it; it.wItemID = 1;
        EXPECT(!inv.AddItem(5, it).has_value());
        EXPECT(inv.LoadInventory(5).size() == kDefaultBagSlots);
    }

    if (g_fails == 0)
        std::printf("test_inventory: FindBlankSlot (gap/boundary/full) + "
                    "AddItem assign/fill/overflow OK\n");
    return g_fails == 0 ? 0 : 1;
}
