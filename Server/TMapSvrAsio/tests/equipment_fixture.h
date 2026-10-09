#pragma once
#include <future>

// Real PostgreSQL and native graph transactions. All item templates/magic are
// from the pinned backup; fixture placement/quantities are deliberately synthetic.
void VerifyEquipmentMoves(soci::session& admin,tmapsvr::PostgreSQLMapService& service,
    const tmapsvr::MapSessionClaim& claim,tmapsvr::CharSnapshot& live) {
    using namespace tmapsvr;
    const auto cid=std::to_string(claim.char_id);
    const auto at=[](const CharSnapshot& s,int bag,int slot)->ItemInstance {
        for(const auto& b:s.payload->bags)if(b.bag.bInvenID==bag)for(const auto& i:b.items)if(i.bItemID==slot)return i;
        throw std::runtime_error("Missing equipment test item");
    };
    const auto rows=[&]{std::string v;admin<<"SELECT jsonb_agg(to_jsonb(i) ORDER BY \"dlID\")::text FROM app_world.\"TITEMTABLE\" i WHERE \"dwOwnerID\"="+cid,soci::into(v);return v;};
    const auto checkpoint=[&]{std::string v;admin<<"SELECT to_jsonb(p)::text FROM app_world.map_checkpoints p WHERE char_id="+cid,soci::into(v);return v;};
    const auto receipts=[&]{return Number(admin,"SELECT count(*) FROM app_world.equipment_operations WHERE char_id="+cid);};
    const auto water=[&]{return Number(admin,"SELECT item_high_water FROM app_world.worlds WHERE group_id=1");};
    const auto head=at(live,254,3);const auto maximum_hp=live.dwMaxHP,maximum_mp=live.dwMaxMP;
    Check(head.source->magic.size()==2&&live.dwHP==maximum_hp&&live.dwMP==maximum_mp,"equipment fixture starts with filled resources and source HP/MP magic");
    const InventoryMoveRequest remove{254,3,255,3,255};
    auto preview=live;auto plan=PlanInventoryMove(live,remove);ApplyInventoryMove(preview,plan);
    const auto old_rows=rows(),old_checkpoint=checkpoint();
    auto invalid=preview;++invalid.dwGold;
    Check(Throws([&]{service.MoveInventoryItems(claim,remove,live,invalid);}),"equipment rejects an unrelated forged currency change");
    auto stale=claim;++stale.connection_id;
    Check(Throws([&]{service.MoveInventoryItems(stale,remove,live,preview);}),"equipment rejects a stale connection generation");
    stale=claim;++stale.authority_epoch;
    Check(Throws([&]{service.MoveInventoryItems(stale,remove,live,preview);}),"equipment rejects a stale authority epoch");
    admin<<"CREATE TRIGGER synthetic_equipment_fault BEFORE INSERT ON app_world.equipment_item_changes FOR EACH ROW EXECUTE FUNCTION public.reject_checkpoint()";
    Check(Throws([&]{service.MoveInventoryItems(claim,remove,live,preview);}),"late equipment item receipt failure rolls back core clamp and item mutation");
    admin<<"DROP TRIGGER synthetic_equipment_fault ON app_world.equipment_item_changes";
    Check(rows()==old_rows&&checkpoint()==old_checkpoint&&receipts()==0,"failed equipment commit preserves exact rows core skills graph and audit");
    const auto commit=service.MoveInventoryItems(claim,remove,live,preview);
    Check(bool(commit.equipment_snapshot),"equipment commit returns fully recalculated durable snapshot");
    auto removed=*commit.equipment_snapshot;
    Check(removed.dwMaxHP<maximum_hp&&removed.dwMaxMP<maximum_mp&&removed.dwHP==removed.dwMaxHP&&removed.dwMP==removed.dwMaxMP,
          "unequip atomically clamps both HP and MP to source-derived new maxima");
    Check(Number(admin,"SELECT \"dwHP\" FROM app_world.\"TCHARTABLE\" WHERE \"dwCharID\"="+cid)==removed.dwHP&&
          Number(admin,"SELECT \"dwMP\" FROM app_world.\"TCHARTABLE\" WHERE \"dwCharID\"="+cid)==removed.dwMP,
          "committed core already contains equipment clamp before reply publication");
    Check(removed.payload->statistics&&removed.payload->skill_attack_timing&&at(removed,255,3).dlID==head.dlID,
          "unequip retains identity and recomputes inspection and skill timing");
    Check(Throws([&]{service.MoveInventoryItems(claim,remove,live,preview);}),"stale equipment replay cannot apply its item twice");
    live=removed;
    const auto apply=[&](InventoryMoveRequest request) {
        auto next=live;const auto p=PlanInventoryMove(live,request);ApplyInventoryMove(next,p);
        const auto result=service.MoveInventoryItems(claim,request,live,next);
        Check(bool(result.equipment_snapshot),"equipment result contains committed graph or normalized state");
        live=*result.equipment_snapshot;return result;
    };
    apply({255,3,254,3,255});
    Check(live.dwMaxHP==maximum_hp&&live.dwMaxMP==maximum_mp&&live.dwHP==removed.dwHP&&live.dwMP==removed.dwMP,
          "reequip restores resource maxima without healing or refilling");
    if(!live.payload->transfer_state) {
        admin<<"UPDATE app_world.\"TITEMTABLE\" SET \"dwTime5\"=\"dwTime5\"+1 WHERE \"dwOwnerID\"="+cid+" AND \"dwStorageID\"=254 AND \"bItemID\"=0";
        auto next=live;ApplyInventoryMove(next,PlanInventoryMove(live,remove));
        Check(Throws([&]{service.MoveInventoryItems(claim,remove,live,next);}),"equipment detects drift in unchanged weapon used for statistics");
        admin<<"UPDATE app_world.\"TITEMTABLE\" SET \"dwTime5\"=\"dwTime5\"-1 WHERE \"dwOwnerID\"="+cid+" AND \"dwStorageID\"=254 AND \"bItemID\"=0";
        admin<<"UPDATE app_world.\"TSKILLTABLE\" SET \"bLevel\"=\"bLevel\"+1 WHERE \"dwCharID\"="+cid+" AND \"wSkillID\"=(SELECT min(\"wSkillID\") FROM app_world.\"TSKILLTABLE\" WHERE \"dwCharID\"="+cid+")";
        Check(Throws([&]{service.MoveInventoryItems(claim,remove,live,next);}),"equipment detects a changed learned rank before deriving permissions");
        admin<<"UPDATE app_world.\"TSKILLTABLE\" SET \"bLevel\"=\"bLevel\"-1 WHERE \"dwCharID\"="+cid+" AND \"wSkillID\"=(SELECT min(\"wSkillID\") FROM app_world.\"TSKILLTABLE\" WHERE \"dwCharID\"="+cid+")";
    }
    const auto noop_rows=rows();const auto noop_count=receipts();
    apply({4,1,254,3,1});
    Check(rows()==noop_rows&&receipts()==noop_count+1&&Number(admin,"SELECT changed_items FROM app_world.equipment_operations WHERE char_id="+cid+" ORDER BY operation_id DESC LIMIT 1")==0,
          "equal carried item onto occupied equipment commits a zero-diff source no-op");
    const InventoryMoveRequest split{4,0,254,3,255};plan=PlanInventoryMove(live,split);preview=live;ApplyInventoryMove(preview,plan);
    Check(plan.items.size()==3&&plan.items.back().created&&at(preview,254,3).dlID==0&&at(preview,4,0).bCount==2,
          "equipment preview displaces occupied slot and forces one-unit split without allocating identity");
    const auto split_rows=rows(),split_checkpoint=checkpoint();const auto split_water=water(),split_receipts=receipts();
    admin<<"CREATE TRIGGER synthetic_equipment_fault BEFORE INSERT ON app_world.equipment_item_changes FOR EACH ROW WHEN (NEW.ordinal=2) EXECUTE FUNCTION public.reject_checkpoint()";
    Check(Throws([&]{service.MoveInventoryItems(claim,split,live,preview);}),"last equipment diff failure rolls back displacement split and identity allocation");
    admin<<"DROP TRIGGER synthetic_equipment_fault ON app_world.equipment_item_changes";
    Check(rows()==split_rows&&checkpoint()==split_checkpoint&&water()==split_water&&receipts()==split_receipts,
          "failed equipment split preserves every item allocator checkpoint and receipt");
    const auto attempt=[&]()->std::optional<InventoryMoveCommit>{try{return service.MoveInventoryItems(claim,split,live,preview);}catch(...){return {};}};
    auto first=std::async(std::launch::async,attempt),second=std::async(std::launch::async,attempt);
    auto a=first.get(),b=second.get();Check(bool(a)!=bool(b),"two concurrent equipment splits have exactly one durable winner");
    const auto result=a?*a:*b;live=*result.equipment_snapshot;
    Check(result.created_id==static_cast<std::uint64_t>(split_water+1)&&water()==split_water+1&&at(live,254,3).dlID==result.created_id,
          "equipment split allocates one shared-world identity and publishes it in the committed snapshot");
    ItemInstance displaced;
    for(const auto& bag:live.payload->bags)for(const auto& i:bag.items)if(i.dlID==head.dlID)displaced=i;
    Check(displaced.dlID==head.dlID&&displaced.bInvenID==255&&at(live,4,0).bCount==2,"displaced head uses original default-bag priority and source stack loses one unit");
    apply({254,3,displaced.bInvenID,displaced.bItemID,1});
    Check(at(live,254,3).dlID==head.dlID&&at(live,displaced.bInvenID,displaced.bItemID).dlID==result.created_id,
          "reverse equipment swap normalizes carried source and preserves both identities");
    // Return the allocated unit to this fixture's extra bag, leaving the
    // original carried-move fixture's destination empty for its return trip.
    const InventoryMoveRequest equal{displaced.bInvenID,displaced.bItemID,4,2,1};
    auto equal_after=live;auto equal_plan=PlanInventoryMove(live,equal);ApplyInventoryMove(equal_after,equal_plan);
    const auto merge=service.MoveInventoryItems(claim,equal,live,equal_after);PublishInventoryMove(live,equal_plan,merge);
    Check(Number(admin,"SELECT count(*) FROM (SELECT o.operation_id FROM app_world.equipment_operations o LEFT JOIN app_world.equipment_item_changes c USING(operation_id) WHERE o.char_id="+cid+" GROUP BY o.operation_id,o.changed_items HAVING count(c.ordinal)<>o.changed_items OR COALESCE(sum(c.before_count-c.after_count),0)<>0) q")==0,
          "all equipment receipts enumerate the exact balanced final identity diff");
}
