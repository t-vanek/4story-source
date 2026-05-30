// Unit test: the monster corpse registry — MakeCorpse (drop roll → corpse
// slots) + the Put / Find / TakeMoney / ItemAt / RemoveItem / IsEmpty /
// Remove operations the loot handlers drive.

#include "services/corpse_registry.h"
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

    // --- MakeCorpse: money + sequential item slots --------------------
    {
        const auto c = MakeCorpse(1500, { 100, 200, 300 });
        EXPECT(c.dwMoney == 1500);
        EXPECT(c.items.size() == 3);
        EXPECT(c.items[0].bItemID == 0 && c.items[0].wItemID == 100 &&
               c.items[0].bCount == 1);
        EXPECT(c.items[1].bItemID == 1 && c.items[1].wItemID == 200);
        EXPECT(c.items[2].bItemID == 2 && c.items[2].wItemID == 300);

        const auto empty = MakeCorpse(0, {});
        EXPECT(empty.dwMoney == 0 && empty.items.empty());
    }

    // --- Put / Find -----------------------------------------------------
    {
        InMemoryCorpseRegistry reg;
        EXPECT(reg.Size() == 0);
        EXPECT(!reg.Find(5000).has_value());      // absent
        EXPECT(reg.IsEmpty(5000));                // absent counts as empty

        reg.Put(5000, MakeCorpse(900, { 10, 20 }));
        EXPECT(reg.Size() == 1);
        const auto c = reg.Find(5000);
        EXPECT(c.has_value());
        if (c) { EXPECT(c->dwMoney == 900 && c->items.size() == 2); }
        EXPECT(!reg.IsEmpty(5000));
    }

    // --- TakeMoney: returns + zeroes ----------------------------------
    {
        InMemoryCorpseRegistry reg;
        reg.Put(1, MakeCorpse(750, {}));
        EXPECT(reg.TakeMoney(1) == 750);
        EXPECT(reg.TakeMoney(1) == 0);            // already taken
        EXPECT(reg.TakeMoney(999) == 0);          // absent corpse
        EXPECT(reg.IsEmpty(1));                    // money-only corpse now empty
    }

    // --- ItemAt / RemoveItem -----------------------------------------
    {
        InMemoryCorpseRegistry reg;
        reg.Put(7, MakeCorpse(0, { 11, 22, 33 }));   // slots 0,1,2

        const auto at1 = reg.ItemAt(7, 1);
        EXPECT(at1.has_value() && at1->wItemID == 22);
        EXPECT(!reg.ItemAt(7, 9).has_value());        // no such slot
        EXPECT(!reg.ItemAt(999, 0).has_value());      // no such corpse

        reg.RemoveItem(7, 1);                          // take slot 1
        EXPECT(!reg.ItemAt(7, 1).has_value());
        EXPECT(reg.ItemAt(7, 0)->wItemID == 11);       // others intact
        EXPECT(reg.ItemAt(7, 2)->wItemID == 33);
        EXPECT(!reg.IsEmpty(7));                        // still has 2 items

        reg.RemoveItem(7, 0);
        reg.RemoveItem(7, 2);
        EXPECT(reg.IsEmpty(7));                          // drained
    }

    // --- Remove ---------------------------------------------------------
    {
        InMemoryCorpseRegistry reg;
        reg.Put(3, MakeCorpse(100, { 5 }));
        reg.Remove(3);
        EXPECT(!reg.Find(3).has_value());
        EXPECT(reg.Size() == 0);
        reg.Remove(3);                                  // idempotent, no crash
    }

    if (g_fails == 0)
        std::printf("test_corpse: MakeCorpse + put/find/takemoney/itemat/"
                    "removeitem/isempty/remove OK\n");
    return g_fails == 0 ? 0 : 1;
}
