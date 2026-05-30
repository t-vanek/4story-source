// Unit test: TakeCorpseItem — the corpse-item → bag take decision the
// CS_MONITEMTAKE handler runs. Covers the not-found, success (item moves
// corpse → bag), and bag-full (item LEFT on the corpse, no loss) paths.

#include "services/loot_take.h"
#include "services/corpse_registry.h"
#include "services/fake_inventory_service.h"
#include "services/inventory_slots.h"
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
} // namespace

int main()
{
    using namespace tmapsvr;

    // --- not found: no corpse / no such slot --------------------------
    {
        InMemoryCorpseRegistry corpses;
        FakeInventoryService   inv;
        auto a = TakeCorpseItem(corpses, inv, /*char=*/1, /*mon=*/100, /*slot=*/0);
        EXPECT(a.result == MIT_NOTFOUND && !a.taken);

        corpses.Put(100, MakeCorpse(0, { 111 }));   // only slot 0 exists
        auto b = TakeCorpseItem(corpses, inv, 1, 100, /*slot=*/5);
        EXPECT(b.result == MIT_NOTFOUND && !b.taken);
    }

    // --- success: item moves corpse → bag -----------------------------
    {
        InMemoryCorpseRegistry corpses;
        FakeInventoryService   inv;
        corpses.Put(100, MakeCorpse(0, { 111, 222 }));   // slots 0, 1

        auto out = TakeCorpseItem(corpses, inv, /*char=*/7, 100, /*slot=*/0);
        EXPECT(out.result == MIT_SUCCESS);
        EXPECT(out.taken && out.taken->wItemID == 111);
        EXPECT(out.taken && out.taken->bItemID == 0);    // assigned bag slot

        // Gone from the corpse, present in the bag.
        EXPECT(!corpses.ItemAt(100, 0).has_value());
        EXPECT(corpses.ItemAt(100, 1).has_value());      // the other remains
        const auto rows = inv.LoadInventory(7);
        EXPECT(rows.size() == 1 && rows[0].wItemID == 111);
    }

    // --- bag full: item LEFT on the corpse (MIT_FULLINVEN, no loss) ----
    {
        InMemoryCorpseRegistry corpses;
        FakeInventoryService   inv;
        corpses.Put(100, MakeCorpse(0, { 333 }));

        // Fill the bag to capacity first.
        std::vector<InventoryRow> full;
        for (std::uint8_t i = 0; i < kDefaultBagSlots; ++i)
        {
            InventoryRow r; r.bInvenID = i; r.wItemID = 1;
            full.push_back(r);
        }
        inv.SetRows(7, full);

        auto out = TakeCorpseItem(corpses, inv, 7, 100, /*slot=*/0);
        EXPECT(out.result == MIT_FULLINVEN && !out.taken);
        EXPECT(corpses.ItemAt(100, 0).has_value());      // still on the corpse
        EXPECT(inv.LoadInventory(7).size() == kDefaultBagSlots);  // unchanged
    }

    if (g_fails == 0)
        std::printf("test_loot_take: notfound + success (corpse→bag) + "
                    "full-leaves-corpse OK\n");
    return g_fails == 0 ? 0 : 1;
}
