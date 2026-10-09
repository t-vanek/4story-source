// Unit test: inventory slot allocation (FindBlankSlot, the
// CTInven::GetBlankPos port) + the inventory service AddItem loot/pickup
// path (slot assignment + "bag full" failure) exercised through the fake.

#include "services/inventory_slots.h"
#include "services/inventory_move.h"
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

    {
        CharSnapshot s;s.dwCharID=42;auto p=std::make_shared<CharacterPayload>();
        const auto item=[](std::uint64_t id,std::uint16_t tpl,std::uint8_t slot,std::uint8_t count){
            ItemInstance i;i.dlID=id;i.wItemID=tpl;i.bInvenID=255;i.bItemID=slot;i.bCount=count;i.durable_hash=std::string(64,'a');
            auto raw=std::make_shared<transfer::Item>();raw->id=id;raw->item=tpl;raw->slot=slot;raw->count=count;
            raw->storage_id=255;raw->owner_id=42;raw->texture=0xfedcba98;raw->magic={{3,987}};i.source=raw;i.stack_limit=10;return i;
        };
        p->bags={{{255,3,0,0},{item(100,8401,0,8),item(200,11054,1,3)},16},{{4,4,0,0},{},4}};s.payload=p;
        auto plan=PlanInventoryMove(s,{255,0,4,3,255});auto moved=s;ApplyInventoryMove(moved,plan);
        EXPECT(plan.items.size()==1&&moved.payload->bags[1].items[0].dlID==100&&moved.payload->bags[1].items[0].bCount==8);
        EXPECT(s.payload->bags[0].items.size()==2&&moved.payload->bags[0].items.size()==1);
        const auto raw=moved.payload->bags[1].items[0].source;
        EXPECT(raw->storage_id==4&&raw->slot==3&&raw->texture==0xfedcba98&&raw->magic[0].value==987);
        plan=PlanInventoryMove(s,{255,0,255,1,1});auto swapped=s;ApplyInventoryMove(swapped,plan);
        EXPECT(plan.items.size()==2&&swapped.payload->bags[0].items[0].dlID==200&&swapped.payload->bags[0].items[1].dlID==100);
        EXPECT(swapped.payload->bags[0].items[1].bCount==8&&swapped.payload->bags[0].items[0].bCount==3);
        EXPECT(PlanInventoryMove(s,{5,0,255,0,1}).result==InventoryMoveResult::NoSourceBag);
        EXPECT(PlanInventoryMove(s,{255,7,255,0,1}).result==InventoryMoveResult::NoSourceItem);
        EXPECT(PlanInventoryMove(s,{255,0,5,0,1}).result==InventoryMoveResult::NoDestinationBag);
        EXPECT(PlanInventoryMove(s,{255,0,255,0,1}).result==InventoryMoveResult::SamePosition);
        EXPECT(PlanInventoryMove(s,{255,0,255,0,0}).result==InventoryMoveResult::NoSourceItem);
        const auto refuses=[&](InventoryMoveRequest r){try{PlanInventoryMove(s,r);return false;}catch(...){return true;}};
        EXPECT(refuses({255,0,4,4,8}));EXPECT(refuses({255,0,252,0,8}));
        plan=PlanInventoryMove(s,{255,0,4,3,3});auto split=s;
        PublishInventoryMove(split,plan,{{std::string(64,'b'),std::string(64,'c')},300});
        EXPECT(plan.kind==InventoryMoveKind::Split&&split.payload->bags[0].items[0].bCount==5);
        EXPECT(split.payload->bags[1].items[0].dlID==300&&split.payload->bags[1].items[0].bCount==3);
        EXPECT(split.payload->bags[1].items[0].source->texture==0xfedcba98&&StackEquivalent(split.payload->bags[0].items[0],split.payload->bags[1].items[0]));
        plan=PlanInventoryMove(split,{4,3,255,0,1});auto merged=split;ApplyInventoryMove(merged,plan);
        EXPECT(plan.kind==InventoryMoveKind::Merge&&merged.payload->bags[0].items[0].bCount==6&&merged.payload->bags[1].items[0].bCount==2);
        plan=PlanInventoryMove(merged,{4,3,255,0,255});ApplyInventoryMove(merged,plan);
        EXPECT(merged.payload->bags[1].items.empty()&&merged.payload->bags[0].items[0].dlID==100&&merged.payload->bags[0].items[0].bCount==8);
        auto capped=std::make_shared<CharacterPayload>(*split.payload);capped->bags[0].items[0].stack_limit=6;split.payload=capped;
        plan=PlanInventoryMove(split,{4,3,255,0,255});ApplyInventoryMove(split,plan);
        EXPECT(split.payload->bags[0].items[0].bCount==6&&split.payload->bags[1].items[0].bCount==2);
        plan=PlanInventoryMove(split,{4,3,255,0,255});
        EXPECT(plan.kind==InventoryMoveKind::Merge&&plan.items[0].count==2&&plan.items[1].count==6);
        auto lhs=item(10,8401,1,1),rhs=item(11,8401,2,4);
        auto variant=std::make_shared<transfer::Item>(*rhs.source);variant->grade_effect=7;rhs.source=variant;
        EXPECT(StackEquivalent(lhs,rhs)); // source equality intentionally ignores grade effect/count/ID/slot
        variant->texture^=0x10000;EXPECT(!StackEquivalent(lhs,rhs));
        variant->texture^=0x10000;variant->magic[0].value++;EXPECT(!StackEquivalent(lhs,rhs));
        auto changed=std::make_shared<CharacterPayload>(*p);changed->bags[0].items[1].wItemID=8401;s.payload=changed;
        EXPECT(refuses({255,0,255,1,8}));
        variant=std::make_shared<transfer::Item>(*changed->bags[0].items[1].source);variant->item=8401;variant->gem=1;
        changed->bags[0].items[1].source=variant;changed->bags[0].items[1].bGem=1;
        EXPECT(PlanInventoryMove(s,{255,0,255,1,1}).kind==InventoryMoveKind::Swap);
        changed->bags[1].bag.dEndTime=100;EXPECT(refuses({255,0,4,3,8}));
        changed->bags[1].bag.dEndTime=0;changed->bags[1].bag.bInvenID=254;
        EXPECT(refuses({255,0,254,0,8}));
        changed->bags[0].items[0].source.reset();EXPECT(refuses({255,0,255,2,8}));
    }
    // Source equipment transitions: independent expected slots and packet order.
    {
        const auto item=[](std::uint64_t id,std::uint8_t bag,std::uint8_t slot,std::uint16_t type,std::uint8_t count=1) {
            ItemInstance i;i.dlID=id;i.bInvenID=bag;i.bItemID=slot;i.wItemID=type;i.bCount=count;i.bKind=13;i.stack_limit=20;
            i.durable_hash=std::string(64,'a');i.equipment=EquipmentRules{1U<<3,2,3,255,10,0};
            auto raw=std::make_shared<transfer::Item>();raw->id=id;raw->owner_id=42;raw->storage_id=bag;raw->slot=slot;raw->item=type;raw->count=count;i.source=raw;return i;
        };
        const auto state=[&](std::vector<ItemInstance> carried,std::vector<ItemInstance> equip,unsigned capacity=4) {
            CharSnapshot s;s.dwCharID=42;s.bClass=1;s.bLevel=10;s.dwHP=50;
            auto p=std::make_shared<CharacterPayload>();
            p->bags={CharacterBag{Row(254),std::move(equip),19},CharacterBag{Row(255),std::move(carried),static_cast<std::uint8_t>(capacity)}};
            s.payload=p;return s;
        };
        auto s=state({item(1,255,0,100)},{});auto plan=PlanInventoryMove(s,{255,0,254,3,255});
        EXPECT(plan.kind==InventoryMoveKind::Equipment&&plan.items.size()==1&&plan.wire.size()==2);
        EXPECT(plan.wire[0].kind==InventoryWireKind::Delete&&plan.wire[1].kind==InventoryWireKind::Add);
        ApplyInventoryMove(s,plan);EXPECT(s.payload->bags[0].items[0].dlID==1&&s.payload->bags[1].items.empty());
        plan=PlanInventoryMove(s,{254,3,255,2,255});ApplyInventoryMove(s,plan);
        EXPECT(s.payload->bags[0].items.empty()&&s.payload->bags[1].items[0].bItemID==2);
        s=state({item(1,255,0,100)},{item(2,254,3,101)});plan=PlanInventoryMove(s,{255,0,254,3,1});
        EXPECT(plan.wire.size()==2&&plan.wire[0].item.dlID==1&&plan.wire[0].item.bInvenID==254&&plan.wire[1].item.dlID==2);
        ApplyInventoryMove(s,plan);EXPECT(s.payload->bags[0].items[0].dlID==1&&s.payload->bags[1].items[0].dlID==2);
        plan=PlanInventoryMove(s,{254,3,255,0,1});EXPECT(plan.wire[0].item.dlID==2&&plan.wire[0].item.bInvenID==254);
        auto two=item(1,255,0,200);two.bKind=3;two.equipment=EquipmentRules{1,2,0,1,10,0};
        auto off=item(2,254,1,201);off.equipment=EquipmentRules{2,2,1,255,10,0};
        s=state({two},{off});plan=PlanInventoryMove(s,{255,0,254,1,1});
        EXPECT(plan.wire.size()==4&&plan.wire[0].item.bInvenID==254&&plan.wire[0].item.bItemID==1);
        EXPECT(plan.wire[1].item.bInvenID==255&&plan.wire[1].item.bItemID==1&&plan.wire[3].item.bItemID==0);
        ApplyInventoryMove(s,plan);EXPECT(s.payload->bags[0].items[0].dlID==1&&s.payload->bags[1].items[0].dlID==2);
        auto secondary=off;secondary.bInvenID=255;secondary.bItemID=0;auto raw=std::make_shared<transfer::Item>(*secondary.source);raw->storage_id=255;raw->slot=0;secondary.source=raw;
        s=state({secondary},{s.payload->bags[0].items[0]});EXPECT(PlanInventoryMove(s,{255,0,254,1,1}).result==InventoryMoveResult::BothHands);
        s=state({two},{off},1);EXPECT(PlanInventoryMove(s,{255,0,254,1,1}).result==InventoryMoveResult::InventoryFull);
        EXPECT(s.payload->bags[1].items[0].bCount==1&&s.payload->bags[0].items[0].dlID==2);
        // A full bag can still accept displaced equipment into an equal stack.
        // Its UPDATE precedes the incoming weapon's DEL/ADD sequence.
        s=state({two,item(3,255,1,201,19)},{off},2);plan=PlanInventoryMove(s,{255,0,254,1,1});
        EXPECT(plan.result==InventoryMoveResult::Success&&plan.items.size()==3&&plan.wire.size()==4);
        EXPECT(plan.wire[1].kind==InventoryWireKind::Update&&plan.wire[1].item.bCount==20);
        ApplyInventoryMove(s,plan);EXPECT(s.payload->bags[1].items.size()==1&&s.payload->bags[1].items[0].dlID==3&&s.payload->bags[1].items[0].bCount==20);
        EXPECT(PlanInventoryMove(s,{254,0,254,1,1}).result==InventoryMoveResult::SamePosition);
        const auto throws=[](auto operation){try{operation();return false;}catch(...){return true;}};
        // Preserve the source pointer-lifetime condition for automatic stance:
        // a whole move to empty deletes pTItemSRC, whereas an unequal swap does not.
        auto warrior_two=two;warrior_two.equipment->classes=1;
        s=state({warrior_two},{});s.bClass=0;
        EXPECT(PlanInventoryMove(s,{255,0,254,0,1}).result==InventoryMoveResult::Success);
        auto first=item(4,254,0,202);first.equipment=EquipmentRules{1,1,0,255,1,0};
        s=state({warrior_two},{first});s.bClass=0;
        EXPECT(PlanInventoryMove(s,{255,0,254,0,1}).auto_posture==132);
        auto shield=item(5,255,0,203);shield.bKind=12;shield.equipment=EquipmentRules{2,1,1,255,1,0};
        s=state({shield},{});s.bClass=0;EXPECT(PlanInventoryMove(s,{255,0,254,1,1}).auto_posture==131);
        auto stack=item(1,255,0,100,3);s=state({stack},{item(2,254,3,101)});
        plan=PlanInventoryMove(s,{255,0,254,3,255});EXPECT(plan.items.size()==3&&plan.items.back().created&&plan.wire.size()==4);
        ApplyInventoryMove(s,plan,99);EXPECT(s.payload->bags[0].items[0].dlID==99&&s.payload->bags[0].items[0].bCount==1);
        EXPECT(s.payload->bags[1].items[0].bCount==2&&s.payload->bags[1].items[1].dlID==2);
        s=state({item(1,255,0,100)},{item(2,254,3,100)});plan=PlanInventoryMove(s,{255,0,254,3,1});
        EXPECT(plan.items.empty()&&plan.wire.empty());ApplyInventoryMove(s,plan);
        auto candidate=item(1,255,0,100);raw=std::make_shared<transfer::Item>(*candidate.source);raw->wrap=256;candidate.source=raw;
        s=state({candidate},{});EXPECT(PlanInventoryMove(s,{255,0,254,3,1}).result==InventoryMoveResult::Wrapped);
        raw->wrap=0;raw->eld=10;s.bLevel=9;EXPECT(CanEquipItem(s,candidate,3)==InventoryMoveResult::LowLevel);
        raw->eld=1;EXPECT(CanEquipItem(s,candidate,3)==InventoryMoveResult::Success);
        raw->eld=256;EXPECT(CanEquipItem(s,candidate,3)==InventoryMoveResult::LowLevel);
        candidate.equipment->skill_required=1;EXPECT(CanEquipItem(s,candidate,2)==InventoryMoveResult::NoSkill);
        auto p=std::make_shared<CharacterPayload>(*s.payload);p->equipment_kinds[13]=true;s.payload=p;
        EXPECT(CanEquipItem(s,candidate,2)==InventoryMoveResult::CannotEquip);
        s.bClass=2;EXPECT(CanEquipItem(s,candidate,3)==InventoryMoveResult::WrongClass);
        s.bClass=1;EXPECT(CanEquipItem(s,candidate,3)==InventoryMoveResult::LowLevel);
    }
    if (g_fails == 0)
        std::printf("test_inventory: FindBlankSlot (gap/boundary/full) + "
                    "AddItem assign/fill/overflow OK\n");
    return g_fails == 0 ? 0 : 1;
}
