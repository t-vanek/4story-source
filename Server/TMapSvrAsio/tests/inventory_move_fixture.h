#pragma once
#include "services/inventory_move.h"

void VerifyInventoryMoves(soci::session& admin,SessionPool& pool,tmapsvr::PostgreSQLMapService& source,
    const char* connection,const char* manifest,const char* routing,const char* actor,tmapsvr::MapSessionClaim primary,
    bool graph,bool recover=false) {
    using namespace tmapsvr;
    const auto cid=std::to_string(primary.char_id);
    admin<<"INSERT INTO app_world.\"TINVENTABLE\"(\"bWorldID\",\"dwCharID\",\"bInvenID\",\"wItemID\",\"dEndTime\") VALUES(1,"+cid+",4,4,'1900-01-01')";
    admin<<"INSERT INTO app_world.\"TITEMTABLE\" SELECT (jsonb_populate_record(NULL::app_world.\"TITEMTABLE\",to_jsonb(i)||jsonb_build_object('dlID',9200000+\"dwOwnerID\",'bItemID',0,'wItemID',8401,'bCount',8))).* FROM app_world.\"TITEMTABLE\" i WHERE \"dwOwnerID\"="+cid+" AND \"dwStorageID\"=255 AND \"bItemID\"=1";
    admin<<"UPDATE app_world.\"TITEMTABLE\" SET \"wItemID\"=11054,\"bCount\"=3 WHERE \"dwOwnerID\"="+cid+" AND \"dwStorageID\"=255 AND \"bItemID\"=1";
    const auto item_rows=[&]{std::string rows;admin<<"SELECT jsonb_agg(to_jsonb(i) ORDER BY \"dlID\")::text FROM app_world.\"TITEMTABLE\" i WHERE \"dwOwnerID\"="+cid,soci::into(rows);return rows;};
    const auto checkpoint=[&]{std::string row;admin<<"SELECT to_jsonb(p)::text FROM app_world.map_checkpoints p WHERE char_id="+cid,soci::into(row);return row;};
    const auto original_items=item_rows();
    std::unique_ptr<SyntheticReplicaPartition> partition;
    std::unique_ptr<PostgreSQLMapOwner> owner;
    std::unique_ptr<PostgreSQLMapService> target;
    if(graph) {
        partition=std::make_unique<SyntheticReplicaPartition>(admin);
        owner=std::make_unique<PostgreSQLMapOwner>(connection,1,2);
        target=std::make_unique<PostgreSQLMapService>(pool,PostgreSQLMapConfig{1,2,owner->Token(),manifest,routing,actor});
    }
    primary.endpoint_ip=0x0100007f;primary.endpoint_port=5815;
    Check(source.ClaimSession(primary,*source.LookupSession(primary.user_id,primary.key)).has_value(),"inventory source claims native handoff");
    auto live=*source.LoadAuthorized(primary);auto active=primary;auto* service=&source;
    live.wMapID=0;live.fPosX=4080;live.fPosY=80;live.fPosZ=3584;
    source.MarkReady(primary,live);
    if(graph) {
        Check(source.AuthorizeReplicas(primary,0,4080,3584,{{0x0100007f,5815,2}}),"inventory graph replica grant");
        auto secondary=primary;secondary.connection_id+=1000;
        Check(target->ClaimSession(secondary,*target->LookupSession(primary.user_id,primary.key)).has_value(),"inventory graph replica claimed");
        secondary.role=MapSessionRole::Replica;
        Check(target->LoadReplica(secondary,0,4080,3584),"inventory graph replica loaded");target->MarkReady(secondary,live);
        live.fPosX=4100;SkillCooldownTracker timers;timers.Restore(primary.char_id,live.payload->skills,0);
        auto state=transfer::Capture(live,primary.key,timers,0);
        state.quests.push_back({12345,67890,1,1,1});
        for(auto& item:state.items)if(item.storage==0&&item.storage_id==255&&(item.slot==0||item.slot==1)) {
            item.id=0xfedcba9876543200ULL+item.slot;item.texture=0xfedcba98;item.magic={{3,100}};
        }
        const auto body=transfer::Encode(state);
        Check(source.PrepareTransfer(primary,live,body),"inventory graph transfer prepared");
        auto received=target->AcceptTransfer(secondary,body);Check(received.has_value(),"inventory graph promoted");
        active=secondary;active.role=MapSessionRole::Primary;active.authority_epoch=received->authority_epoch;
        live=received->snapshot;service=target.get();service->MarkReady(active,live);
    }

    const auto item_at=[](const CharSnapshot& s,int bag,int slot)->ItemInstance {
        for(const auto& b:s.payload->bags)if(b.bag.bInvenID==bag)for(const auto& i:b.items)if(i.bItemID==slot)return i;
        throw std::runtime_error("Missing native move fixture item");
    };
    const auto src=item_at(live,255,0),dst=item_at(live,255,1);
    Check(PlanInventoryMove(live,{255,0,4,3,8}).items.size()==1,"native bag capacities permit source-backed last slot");
    Check(Throws([&]{PlanInventoryMove(live,{255,0,4,4,8});}),"native bag refuses invisible slot beyond source capacity");
    const InventoryMoveRequest swap{255,0,255,1,1};auto after=live;
    auto plan=PlanInventoryMove(live,swap);ApplyInventoryMove(after,plan);
    const auto old_checkpoint=checkpoint();
    auto wrong=live;auto bad=std::make_shared<CharacterPayload>(*live.payload);
    for(auto& b:bad->bags)for(auto& i:b.items)if(i.dlID==dst.dlID)i.durable_hash=std::string(64,'0');
    wrong.payload=bad;auto wrong_after=wrong;ApplyInventoryMove(wrong_after,PlanInventoryMove(wrong,swap));
    Check(Throws([&]{service->MoveInventoryItems(active,swap,wrong,wrong_after);}),"stale second inventory fingerprint refuses the whole swap");
    wrong=live;bad=std::make_shared<CharacterPayload>(*live.payload);
    for(auto& b:bad->bags)for(auto& i:b.items)if(i.dlID==src.dlID) {
        ++i.bCount;auto raw=std::make_shared<transfer::Item>(*i.source);raw->count=i.bCount;i.source=raw;
    }
    wrong.payload=bad;wrong_after=wrong;ApplyInventoryMove(wrong_after,PlanInventoryMove(wrong,swap));
    Check(Throws([&]{service->MoveInventoryItems(active,swap,wrong,wrong_after);}),"move rejects a changed source count even with the original row fingerprint");
    auto tampered=after;auto changed=std::make_shared<CharacterPayload>(*after.payload);
    for(auto& b:changed->bags)for(auto& i:b.items)if(i.dlID==src.dlID)++i.bCount;
    tampered.payload=changed;
    Check(Throws([&]{service->MoveInventoryItems(active,swap,live,tampered);}),"move cannot smuggle an item quantity change");
    tampered=after;++tampered.dwMP;
    Check(Throws([&]{service->MoveInventoryItems(active,swap,live,tampered);}),"move cannot smuggle a core mutation");
    admin<<"CREATE TRIGGER synthetic_move_fault BEFORE INSERT ON app_world.inventory_movements FOR EACH ROW WHEN (NEW.source_slot=1) EXECUTE FUNCTION public.reject_checkpoint()";
    Check(Throws([&]{service->MoveInventoryItems(active,swap,live,after);}),"second inventory receipt failure rolls back both swapped positions");
    admin<<"DROP TRIGGER synthetic_move_fault ON app_world.inventory_movements";
    Check(checkpoint()==old_checkpoint&&item_rows()==original_items&&Number(admin,"SELECT count(*) FROM app_world.inventory_movements WHERE char_id="+cid)==0,
          "failed swap preserves exact inventory checkpoint core and audit");
    auto hashes=service->MoveInventoryItems(active,swap,live,after);
    Check(hashes.size()==2&&hashes[0]!=src.durable_hash&&hashes[1]!=dst.durable_hash,"swap returns both new exact fingerprints");
    for(std::size_t i=0;i<plan.items.size();++i)PublishReagentHash(after,plan.items[i].before.dlID,hashes[i]);
    Check(item_at(after,255,1).dlID==src.dlID&&item_at(after,255,1).bCount==8&&item_at(after,255,0).dlID==dst.dlID&&item_at(after,255,0).bCount==3,
          "different-template swap conserves identities and full counts despite request count one");
    Check(Number(admin,"SELECT count(*) FROM app_world.inventory_movements WHERE char_id="+cid)==2&&
          Number(admin,"SELECT count(DISTINCT operation_id) FROM app_world.inventory_movements WHERE char_id="+cid)==1&&
          Number(admin,"SELECT count(*) FROM app_world.inventory_movements WHERE char_id="+cid+" AND state_contract="+(graph?"2":"3"))==2,
          "both swapped receipts share one durable operation and correct storage contract");
    Check(Throws([&]{service->MoveInventoryItems(active,swap,live,after);}),"stale swap replay cannot mutate either item");
    live=after;const InventoryMoveRequest move{255,1,4,3,255};plan=PlanInventoryMove(live,move);after=live;ApplyInventoryMove(after,plan);
    if(!graph) {
        // Simulate a writer that filled the destination after the client snapshot.
        admin<<"INSERT INTO app_world.\"TITEMTABLE\" SELECT (jsonb_populate_record(NULL::app_world.\"TITEMTABLE\",to_jsonb(i)||jsonb_build_object('dlID',9100000+\"dwOwnerID\",'dwStorageID',4,'bItemID',3))).* FROM app_world.\"TITEMTABLE\" i WHERE \"dwOwnerID\"="+cid+" AND \"dwStorageID\"=255 AND \"bItemID\"=0";
        Check(Throws([&]{service->MoveInventoryItems(active,move,live,after);}),"newly occupied destination cannot overwrite another owned item");
        admin<<"DELETE FROM app_world.\"TITEMTABLE\" WHERE \"dlID\"=9100000+"+cid;
    }
    hashes=service->MoveInventoryItems(active,move,live,after);PublishReagentHash(after,src.dlID,hashes.at(0));
    Check(item_at(after,4,3).dlID==src.dlID&&item_at(after,4,3).bCount==8&&item_at(after,4,3).source->storage_id==4,
          "cross-bag whole-stack move preserves source identity and raw attributes");
    Check(Number(admin,"SELECT count(*) FROM app_world.inventory_movements WHERE char_id="+cid)==3,"move adds exactly one receipt after swap");
    if(graph) {
        SkillCooldownTracker timers;timers.Restore(primary.char_id,after.payload->skills,0);
        Check(transfer::Encode(StoredReagentGraph(admin,primary.char_id))==transfer::Encode(transfer::Capture(after,primary.key,timers,0)),
              "inventory move preserves complete graph with unsigned identities extensions and quest");
        Check(item_rows()==original_items,"graph inventory moves never overwrite stale normalized rows");
        if(recover) {
            owner.reset();owner=std::make_unique<PostgreSQLMapOwner>(connection,1,2);
            Check(owner->RecoveredSessions()==1&&transfer::Encode(StoredReagentGraph(admin,primary.char_id))==transfer::Encode(transfer::Capture(after,primary.key,timers,0)),
                  "replacement recovers committed inventory positions before periodic checkpoint");
            return;
        }
        after.fPosX=4080;const auto back=transfer::Encode(transfer::Capture(after,primary.key,timers,0));
        Check(target->PrepareTransfer(active,after,back),"inventory graph prepares return transfer");
        auto returning=primary;returning.role=MapSessionRole::Replica;
        auto received=source.AcceptTransfer(returning,back);Check(received&&received->authority_epoch==2,"inventory return retains authority epoch");
        active=primary;active.authority_epoch=2;after=received->snapshot;service=&source;source.MarkReady(active,after);
    }
    live=after;const InventoryMoveRequest back{4,3,255,1,8};plan=PlanInventoryMove(live,back);ApplyInventoryMove(after,plan);
    hashes=service->MoveInventoryItems(active,back,live,after);PublishReagentHash(after,src.dlID,hashes.at(0));
    Check(item_at(after,255,1).dlID==src.dlID&&after.payload->bags.size()==3,"returned primary can move the same original item again");
    service->SaveAuthorized(active,after);
}
