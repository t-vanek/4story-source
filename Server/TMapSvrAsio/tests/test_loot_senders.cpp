// Unit test: the loot wire encoders — EncodeItemDescriptor (the faithful
// CTItem::WrapPacketClient port, the byte format the client parses for
// every item it sees), EncodeMonItemListAck (the corpse loot window), and
// EncodeMonItemTakeAck. Byte layout is the contract with the real client,
// so the test pins every field + offset.

#include "services/client_senders.h"
#include "domain/inventory.h"
#include "wire_codec.h"

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

using tmapsvr::ItemInstance;

// An item with a distinct value in every field so a mis-ordered or
// mis-sized write shows up as a wrong read.
ItemInstance SampleItem()
{
    ItemInstance it;
    it.dlID         = 0xCAFEBABEull;      // not on the wire
    it.bItemID      = 7;
    it.bInvenID     = 3;                  // not on the wire (placement only)
    it.wItemID      = 0x1234;
    it.bLevel       = 5;
    it.bGem         = 2;
    it.wMoggItemID  = 0xABCD;
    it.wCompanion   = 0x1111;
    it.bCount       = 9;
    it.dwDuraMax    = 1000000;
    it.dwDuraCur    = 999999;
    it.bRefineMax   = 12;
    it.bRefineCur   = 3;
    it.bGLevel      = 4;
    it.dEndTime     = 0x1122334455667788LL;
    it.bGradeEffect = 1;
    it.bELD         = 6;
    it.bWrap        = 1;
    it.wColor       = 0x2222;
    it.wCustomTex   = 0x3333;
    it.dwGuildBound = 4242;
    return it;
}

// Read + verify the descriptor body (after the optional slot byte).
void CheckBody(tmapsvr::wire::Reader& r, const ItemInstance& it,
               std::uint8_t expect_reg_guild)
{
    std::uint16_t wItemID = 0, mogg = 0, comp = 0, color = 0, ctex = 0;
    std::uint8_t  level = 0, gem = 0, count = 0, rmax = 0, rcur = 0, glevel = 0,
                  grade = 0, eld = 0, wrap = 0, reg = 0xFF, magic = 0xFF;
    std::uint32_t dmax = 0, dcur = 0;
    std::int64_t  endt = 0;
    EXPECT(r.Read(wItemID) && wItemID == it.wItemID);
    EXPECT(r.Read(level)   && level   == it.bLevel);
    EXPECT(r.Read(gem)     && gem     == it.bGem);
    EXPECT(r.Read(mogg)    && mogg    == it.wMoggItemID);
    EXPECT(r.Read(comp)    && comp    == it.wCompanion);
    EXPECT(r.Read(count)   && count   == it.bCount);
    EXPECT(r.Read(dmax)    && dmax    == it.dwDuraMax);
    EXPECT(r.Read(dcur)    && dcur    == it.dwDuraCur);
    EXPECT(r.Read(rmax)    && rmax    == it.bRefineMax);
    EXPECT(r.Read(rcur)    && rcur    == it.bRefineCur);
    EXPECT(r.Read(glevel)  && glevel  == it.bGLevel);
    EXPECT(r.Read(endt)    && endt    == it.dEndTime);
    EXPECT(r.Read(grade)   && grade   == it.bGradeEffect);
    EXPECT(r.Read(eld)     && eld     == it.bELD);
    EXPECT(r.Read(wrap)    && wrap    == it.bWrap);
    EXPECT(r.Read(color)   && color   == it.wColor);
    EXPECT(r.Read(ctex)    && ctex    == it.wCustomTex);
    EXPECT(r.Read(reg)     && reg     == expect_reg_guild);
    EXPECT(r.Read(magic)   && magic   == 0);   // magic options deferred
}
} // namespace

int main()
{
    using namespace tmapsvr;
    const ItemInstance it = SampleItem();

    // --- descriptor with slot id (CS_MONITEMLIST / CS_ADDITEM path) ----
    {
        // Guild-bound to the viewer → reg_guild = 1.
        auto b = EncodeItemDescriptor(it, /*viewer=*/4242, /*add_item_id=*/true);
        EXPECT(b.size() == 38);
        wire::Reader r(b.data(), b.size());
        std::uint8_t slot = 0;
        EXPECT(r.Read(slot) && slot == it.bItemID);
        CheckBody(r, it, /*reg_guild=*/1);
        EXPECT(r.Eof());
    }

    // --- descriptor without slot id (CS_MONITEMLOTTERY path) -----------
    {
        // Guild-bound to someone else → reg_guild = 0.
        auto b = EncodeItemDescriptor(it, /*viewer=*/9999, /*add_item_id=*/false);
        EXPECT(b.size() == 37);
        wire::Reader r(b.data(), b.size());
        CheckBody(r, it, /*reg_guild=*/0);
        EXPECT(r.Eof());
    }

    // --- CS_MONITEMLIST_ACK: header + count + descriptors (ret 0) ------
    {
        std::vector<ItemInstance> items{ it, it };
        auto b = EncodeMonItemListAck(/*ret=*/0, /*update=*/1, /*mon=*/0xDEAD,
                                      /*gold=*/10, /*silver=*/20, /*cooper=*/30,
                                      items, /*viewer=*/4242);
        EXPECT(b.size() == 18 + 1 + 2 * 38);   // 18-byte header + count + 2 items
        wire::Reader r(b.data(), b.size());
        std::uint8_t ret = 0xFF, upd = 0xFF, count = 0xFF;
        std::uint32_t mon = 0, g = 0, s = 0, c = 0;
        EXPECT(r.Read(ret) && ret == 0);
        EXPECT(r.Read(upd) && upd == 1);
        EXPECT(r.Read(mon) && mon == 0xDEAD);
        EXPECT(r.Read(g) && g == 10 && r.Read(s) && s == 20 && r.Read(c) && c == 30);
        EXPECT(r.Read(count) && count == 2);
        std::uint8_t slot = 0;
        EXPECT(r.Read(slot) && slot == it.bItemID);   // first descriptor begins
        CheckBody(r, it, 1);
        EXPECT(r.Read(slot) && slot == it.bItemID);   // second descriptor
        CheckBody(r, it, 1);
        EXPECT(r.Eof());
    }

    // --- CS_MONITEMLIST_ACK: failure (ret != 0) omits the item list ----
    {
        std::vector<ItemInstance> items{ it };
        auto b = EncodeMonItemListAck(/*ret=*/1, 0, 0xBEEF, 5, 6, 7,
                                      items, 4242);
        EXPECT(b.size() == 18);   // header only, no count / descriptors
        wire::Reader r(b.data(), b.size());
        std::uint8_t ret = 0xFF;
        EXPECT(r.Read(ret) && ret == 1);
    }

    // --- CS_MONITEMTAKE_ACK: one result byte --------------------------
    {
        auto b = EncodeMonItemTakeAck(/*MIT_FULLINVEN=*/1);
        EXPECT(b.size() == 1);
        wire::Reader r(b.data(), b.size());
        std::uint8_t res = 0xFF;
        EXPECT(r.Read(res) && res == 1);
        EXPECT(r.Eof());
    }

    if (g_fails == 0)
        std::printf("test_loot_senders: item descriptor (38B) + MONITEMLIST + "
                    "MONITEMTAKE byte layouts OK\n");
    return g_fails == 0 ? 0 : 1;
}
