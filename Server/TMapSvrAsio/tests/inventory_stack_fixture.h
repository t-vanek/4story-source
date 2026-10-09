#pragma once
#include <future>

void VerifyInventoryStacks(soci::session& admin,tmapsvr::PostgreSQLMapService& service,
    const tmapsvr::MapSessionClaim& claim,tmapsvr::CharSnapshot& live,bool retain_split) {
    using namespace tmapsvr;
    const auto cid=std::to_string(claim.char_id);
    const auto at=[](const CharSnapshot& s,int bag,int slot)->ItemInstance {
        for(const auto& b:s.payload->bags)if(b.bag.bInvenID==bag)for(const auto& i:b.items)if(i.bItemID==slot)return i;
        throw std::runtime_error("Missing stack test item");
    };
    const auto rows=[&]{std::string v;admin<<"SELECT jsonb_agg(to_jsonb(i) ORDER BY \"dlID\")::text FROM app_world.\"TITEMTABLE\" i WHERE \"dwOwnerID\"="+cid,soci::into(v);return v;};
    const auto checkpoint=[&]{std::string v;admin<<"SELECT to_jsonb(p)::text FROM app_world.map_checkpoints p WHERE char_id="+cid,soci::into(v);return v;};
    const auto water=[&]{return Number(admin,"SELECT item_high_water FROM app_world.worlds WHERE group_id=1");};
    const auto receipts=[&]{return Number(admin,"SELECT count(*) FROM app_world.inventory_stack_changes WHERE char_id="+cid);};
    const auto original=at(live,4,3);const auto old_rows=rows(),old_checkpoint=checkpoint();const auto old_water=water();
    const InventoryMoveRequest split{4,3,255,2,3};
    const auto split_plan=PlanInventoryMove(live,split);auto preview=live;ApplyInventoryMove(preview,split_plan);
    Check(split_plan.kind==InventoryMoveKind::Split&&at(preview,4,3).bCount==5&&at(preview,255,2).dlID==0,"split preview conserves count without inventing a durable identity");
    admin<<"CREATE TRIGGER synthetic_stack_fault BEFORE INSERT ON app_world.inventory_stack_changes FOR EACH ROW WHEN (NEW.ordinal=1) EXECUTE FUNCTION public.reject_checkpoint()";
    Check(Throws([&]{service.MoveInventoryItems(claim,split,live,preview);}),"late split receipt failure rejects the operation");
    admin<<"DROP TRIGGER synthetic_stack_fault ON app_world.inventory_stack_changes";
    Check(rows()==old_rows&&checkpoint()==old_checkpoint&&water()==old_water&&receipts()==0,"split rollback preserves original rows graph allocator and all receipts");
    auto bad_claim=claim;++bad_claim.authority_epoch;
    Check(Throws([&]{service.MoveInventoryItems(bad_claim,split,live,preview);}),"stale authority cannot allocate or split an item");
    auto tampered=preview;auto payload=std::make_shared<CharacterPayload>(*preview.payload);
    for(auto& b:payload->bags)for(auto& i:b.items)if(!i.dlID)++i.bCount;
    tampered.payload=payload;
    Check(Throws([&]{service.MoveInventoryItems(claim,split,live,tampered);}),"split rejects forged destination quantity");
    // Same starting state submitted concurrently: one winner, one stale writer.
    const auto attempt=[&]()->std::optional<InventoryMoveCommit>{try{return service.MoveInventoryItems(claim,split,live,preview);}catch(...){return {};}};
    auto first=std::async(std::launch::async,attempt);auto second=std::async(std::launch::async,attempt);
    auto a=first.get(),b=second.get();Check(bool(a)!=bool(b),"concurrent split has exactly one durable winner");
    const auto committed=a?*a:*b;PublishInventoryMove(live,split_plan,committed);
    const auto created=at(live,255,2);
    Check(committed.created_id==static_cast<std::uint64_t>(old_water+1)&&water()==old_water+1&&receipts()==2,"split allocates exactly one shared-world identity and two receipts");
    Check(created.dlID!=original.dlID&&created.bCount==3&&at(live,4,3).bCount==5&&StackEquivalent(created,at(live,4,3)),"split preserves raw stack attributes and distinct identities");
    Check(created.source->texture==original.source->texture&&created.source->grade_effect==original.source->grade_effect,"split copies full-width texture and original grade effect");
    const auto apply=[&](InventoryMoveRequest request) {
        const auto plan=PlanInventoryMove(live,request);auto next=live;ApplyInventoryMove(next,plan);
        const auto result=service.MoveInventoryItems(claim,request,live,next);PublishInventoryMove(live,plan,result);
    };
    apply({255,2,4,3,1});
    Check(at(live,4,3).bCount==6&&at(live,255,2).bCount==2&&at(live,255,2).dlID==created.dlID,"partial merge keeps both exact identities and conserves quantity");
    const auto merge_plan=PlanInventoryMove(live,{255,2,4,3,255});preview=live;ApplyInventoryMove(preview,merge_plan);
    const auto saved_checkpoint=checkpoint(),saved_rows=rows();
    admin<<"CREATE TRIGGER synthetic_merge_fault BEFORE INSERT ON app_world.inventory_stack_changes FOR EACH ROW WHEN (NEW.ordinal=1) EXECUTE FUNCTION public.reject_checkpoint()";
    Check(Throws([&]{service.MoveInventoryItems(claim,{255,2,4,3,255},live,preview);}),"late merge receipt failure rolls back source deletion");
    admin<<"DROP TRIGGER synthetic_merge_fault ON app_world.inventory_stack_changes";
    Check(checkpoint()==saved_checkpoint&&rows()==saved_rows&&receipts()==4,"failed merge retains exact source destination and recovery state");
    apply({255,2,4,3,255});
    Check(at(live,4,3).dlID==original.dlID&&at(live,4,3).bCount==8&&Throws([&]{at(live,255,2);}),"complete merge deletes only the source identity and retains destination");
    Check(receipts()==6&&Number(admin,"SELECT count(*) FROM (SELECT operation_id FROM app_world.inventory_stack_changes WHERE char_id="+cid+" GROUP BY operation_id HAVING sum(before_count)<>sum(after_count) OR count(*)<>2) q")==0,"every stack operation has two balanced ordered receipts");
    if(retain_split)apply(split);
}
