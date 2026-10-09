#include "postgresql_map_service.h"
#include "inventory_move.h"
#include "main_transfer_codec.h"
#include "main_transfer_runtime.h"
#include <soci/soci.h>
#include <openssl/sha.h>
#include <bit>
#include <limits>
#include <map>

namespace tmapsvr {
namespace {
transfer::State CaptureInventory(const CharSnapshot& s,std::uint32_t key) {
    SkillCooldownTracker timers;timers.Restore(s.dwCharID,s.payload->skills,0);
    return transfer::Capture(s,key,timers,0);
}
std::string InventoryHash(std::span<const std::byte> bytes) {
    unsigned char digest[SHA256_DIGEST_LENGTH];SHA256(reinterpret_cast<const unsigned char*>(bytes.data()),bytes.size(),digest);
    constexpr char digits[]="0123456789abcdef";std::string out;
    for(auto b:digest){out+=digits[b>>4];out+=digits[b&15];}return out;
}
}
PostgreSQLMapService::InventoryStoragePlan PostgreSQLMapService::ValidateInventoryMove(soci::session& sql,
    const MapSessionClaim& c,const InventoryMoveRequest& request,const CharSnapshot& before,const CharSnapshot& after) const {
    const bool equipment=request.source_bag==254||request.destination_bag==254;
    auto verified_before=before;
    if(equipment)RefreshEquipment(sql,verified_before,false);
    InventoryStoragePlan out;out.move=PlanInventoryMove(verified_before,request);
    if(out.move.result!=InventoryMoveResult::Success||(!equipment&&out.move.items.empty()))throw std::runtime_error("Rejected inventory move cannot commit");
    auto expected=before;ApplyInventoryMove(expected,out.move);
    const auto candidate=CaptureInventory(after,c.key);
    if(transfer::Encode(CaptureInventory(expected,c.key))!=transfer::Encode(candidate))
        throw std::runtime_error("Inventory move changes unrelated character state");
    const bool graph=bool(before.payload->transfer_state);
    if(graph!=bool(after.payload->transfer_state))throw std::runtime_error("Inventory storage contract changed");
    const int world=c.group,character=c.char_id;
    if(equipment&&!graph) {
        // Stats and displacement inspect more than the two requested slots.
        // Fence every fresh item and learned rank, including unchanged equipment.
        std::map<long long,std::string> durable;
        soci::rowset<soci::row> rows=(sql.prepare<<"SELECT \"dlID\",app_world.item_fingerprint(i) AS hash FROM app_world.\"TITEMTABLE\" i "
            "WHERE \"bWorldID\"=:w AND \"dwOwnerID\"=:c ORDER BY \"dlID\" FOR UPDATE",soci::use(world,"w"),soci::use(character,"c"));
        for(const auto& row:rows)durable.emplace(row.get<long long>(0),row.get<std::string>(1));
        std::size_t count=0;
        for(const auto& bag:before.payload->bags)for(const auto& item:bag.items) {
            ++count;const auto i=durable.find(std::bit_cast<long long>(item.dlID));
            if(i==durable.end()||i->second!=item.durable_hash)throw std::runtime_error("Equipment inventory changed since hydration");
        }
        if(count!=durable.size())throw std::runtime_error("Equipment inventory membership changed");
        std::map<std::uint16_t,int> ranks;
        soci::rowset<soci::row> skills=(sql.prepare<<"SELECT \"wSkillID\",\"bLevel\" FROM app_world.\"TSKILLTABLE\" WHERE \"bWorldID\"=:w AND \"dwCharID\"=:c",soci::use(world,"w"),soci::use(character,"c"));
        for(const auto& row:skills)ranks.emplace(static_cast<std::uint16_t>(row.get<int>(0)),row.get<int>(1));
        if(ranks.size()!=before.payload->skills.size())throw std::runtime_error("Equipment learned skill membership changed");
        for(const auto& skill:before.payload->skills)if(!ranks.contains(skill.wSkillID)||ranks.at(skill.wSkillID)!=skill.bLevel)
            throw std::runtime_error("Equipment learned skill rank changed");
    }
    if(graph) {
        std::string hex,core;
        sql<<"SELECT encode(transfer_body,'hex'),transfer_hash,fingerprint FROM app_world.map_checkpoints "
             "WHERE world_id=:w AND char_id=:c AND recovery_contract=2 AND character_manifest=:cm AND routing_manifest=:rm AND actor_manifest=:am",
            soci::use(world,"w"),soci::use(character,"c"),soci::use(m_config.character_manifest,"cm"),
            soci::use(m_config.routing_manifest,"rm"),soci::use(m_config.actor_manifest,"am"),
            soci::into(hex),soci::into(out.before_graph),soci::into(core);
        if(!sql.got_data())throw std::runtime_error("Inventory graph checkpoint missing");
        std::vector<std::byte> body;
        for(std::size_t i=0;i<hex.size();i+=2)body.push_back(static_cast<std::byte>(std::stoul(hex.substr(i,2),nullptr,16)));
        auto decoded=transfer::Decode(body);
        if(!decoded||decoded->db_load||decoded->character.dwCharID!=c.char_id||decoded->key!=c.key||InventoryHash(body)!=out.before_graph)
            throw std::runtime_error("Inventory recovery graph is invalid");
        auto stored=std::move(*decoded),live=CaptureInventory(before,c.key);
        auto old_core=stored.character;old_core.payload=before.payload;
        if(CoreFingerprint(c,old_core)!=core)throw std::runtime_error("Inventory graph core disagrees with receipt");
        if(stored.skills.size()!=live.skills.size())throw std::runtime_error("Inventory learned skills changed");
        for(std::size_t i=0;i<stored.skills.size();++i) {
            if(stored.skills[i].wSkillID!=live.skills[i].wSkillID||stored.skills[i].bLevel!=live.skills[i].bLevel)
                throw std::runtime_error("Inventory learned ranks changed");
            stored.skills[i].dwRemainTick=live.skills[i].dwRemainTick;
        }
        stored.character=live.character;
        if(transfer::Encode(stored)!=transfer::Encode(live))throw std::runtime_error("Inventory graph changed since hydration");

    }
    // Claim/checkpoint locks serialize supported inventory writers. Re-read bag
    // metadata and pinned capacities in this transaction; do not trust the DTO.
    std::vector<std::uint8_t> inventories;
    if(equipment)for(const auto& bag:before.payload->bags)inventories.push_back(bag.bag.bInvenID);
    else inventories={request.source_bag,request.destination_bag};
    for(const auto id:inventories) {
        const auto bag=std::find_if(before.payload->bags.begin(),before.payload->bags.end(),[&](const auto& b){return b.bag.bInvenID==id;});
        const int template_id=std::bit_cast<std::int16_t>(bag->bag.wItemID);int capacity=0,type=0;
        sql<<"SELECT \"bSlotCount\",\"bType\" FROM character_compat.\"TITEMCHART\" WHERE \"wItemID\"=:i",
            soci::use(template_id),soci::into(capacity),soci::into(type);
        const bool equipment_bag=id==254&&template_id==2&&type==0&&capacity==19;
        if(!sql.got_data()||(id==254?!equipment_bag:type!=11)||capacity!=bag->slot_count||!capacity)
            throw std::runtime_error("Inventory bag capacity disagrees with pinned template");
        if(!graph) {
            const int inventory=id;int actual_item=0,eld=0,permanent=0;
            sql<<"SELECT \"wItemID\",\"bELD\",CASE WHEN \"dEndTime\"<TIMESTAMP '2000-01-01' THEN 1 ELSE 0 END "
                 "FROM app_world.\"TINVENTABLE\" WHERE \"bWorldID\"=:w AND \"dwCharID\"=:c AND \"bInvenID\"=:i",
                soci::use(world,"w"),soci::use(character,"c"),soci::use(inventory,"i"),
                soci::into(actual_item),soci::into(eld),soci::into(permanent);
            if(!sql.got_data()||actual_item!=template_id||eld!=bag->bag.bELD||!permanent)
                throw std::runtime_error("Inventory bag changed since hydration");
        }
    }
    if(out.move.kind==InventoryMoveKind::Merge) {
        const auto& destination=out.move.items.back().before;
        const int template_id=std::bit_cast<std::int16_t>(destination.wItemID);int limit=0;
        sql<<"SELECT \"bStack\" FROM character_compat.\"TITEMCHART\" WHERE \"wItemID\"=:i",
            soci::use(template_id),soci::into(limit);
        if(!sql.got_data()||limit!=destination.stack_limit)throw std::runtime_error("Pinned stack capacity changed");
    }
    out.after=after;
    if(std::any_of(out.move.items.begin(),out.move.items.end(),[](const auto& move){return move.created;})) {
        long long allocated=0;
        sql<<"UPDATE app_world.worlds SET item_high_water=item_high_water+1 WHERE group_id=:w "
             "AND item_high_water::numeric+1 < (item_world::numeric+1)*72057594037927936 RETURNING item_high_water",
            soci::use(world),soci::into(allocated);
        if(!sql.got_data()||allocated<=0)throw std::runtime_error("Split item identity range exhausted");
        out.committed.created_id=static_cast<std::uint64_t>(allocated);
        out.after=before;ApplyInventoryMove(out.after,out.move,out.committed.created_id);
    }
    if(equipment)RefreshEquipment(sql,out.after,true);
    const auto committed_graph=CaptureInventory(out.after,c.key);
    if(graph)out.after_graph=TransferFingerprint(c,out.after);
    for(const auto& move:out.move.items) {
        const auto& item=move.before;
        if(graph) {
            if(GraphItemFingerprint(*item.source)!=item.durable_hash)throw std::runtime_error("Inventory graph item fingerprint changed");
            if(!move.count){out.committed.hashes.emplace_back();continue;}
            const auto id=move.created?out.committed.created_id:item.dlID;
            const auto moved=std::find_if(committed_graph.items.begin(),committed_graph.items.end(),[&](const auto& i){return i.id==id;});
            if(moved==committed_graph.items.end())throw std::runtime_error("Inventory item missing after move");
            out.committed.hashes.push_back(GraphItemFingerprint(*moved));
        }else {
            if(item.dlID>static_cast<std::uint64_t>(std::numeric_limits<long long>::max()))throw std::runtime_error("Unsupported fresh item identity");
            const long long id=item.dlID;const int bag=item.bInvenID,slot=item.bItemID;std::string hash;int count=0,template_id=0;
            sql<<"SELECT app_world.item_fingerprint(i),\"bCount\",\"wItemID\" FROM app_world.\"TITEMTABLE\" i "
                 "WHERE \"bWorldID\"=:w AND \"dwOwnerID\"=:c AND \"dlID\"=:id AND \"dwStorageID\"=:bag AND \"bItemID\"=:slot FOR UPDATE",
                soci::use(world,"w"),soci::use(character,"c"),soci::use(id,"id"),soci::use(bag,"bag"),soci::use(slot,"slot"),
                soci::into(hash),soci::into(count),soci::into(template_id);
            if(!sql.got_data()||hash!=item.durable_hash||count!=item.bCount||static_cast<std::uint16_t>(template_id)!=item.wItemID)
                throw std::runtime_error("Inventory item changed since hydration");
        }
    }
    if(!graph) {
        if(out.move.kind==InventoryMoveKind::Move||out.move.kind==InventoryMoveKind::Split) {
            const int bag=request.destination_bag,slot=request.destination_slot;int present=0;
            sql<<"SELECT 1 FROM app_world.\"TITEMTABLE\" WHERE \"bWorldID\"=:w AND \"dwOwnerID\"=:c AND \"dwStorageID\"=:b AND \"bItemID\"=:s FOR UPDATE",
                soci::use(world,"w"),soci::use(character,"c"),soci::use(bag,"b"),soci::use(slot,"s"),soci::into(present);
            if(sql.got_data())throw std::runtime_error("Inventory destination became occupied");
        }
        sql<<"SET CONSTRAINTS app_world.item_slot DEFERRED";
        for(const auto& move:out.move.items) {
            const long long original=move.before.dlID,id=move.created?out.committed.created_id:move.before.dlID;
            const int bag=move.bag,slot=move.slot,count=move.count;std::string hash;
            if(move.created) {
                // Copy the locked original row including opaque fields. Only
                // identity, quantity and position differ; no client projection
                // is used to reconstruct raw magic or timestamp values.
                sql<<"INSERT INTO app_world.\"TITEMTABLE\" AS i SELECT (jsonb_populate_record(NULL::app_world.\"TITEMTABLE\","
                     "to_jsonb(src)||jsonb_build_object('dlID',CAST(:id AS bigint),'dwStorageID',CAST(:b AS integer),"
                     "'bItemID',CAST(:s AS smallint),'bCount',CAST(:n AS smallint)))).* "
                     "FROM app_world.\"TITEMTABLE\" src WHERE \"bWorldID\"=:w AND \"dlID\"=:original "
                     "RETURNING app_world.item_fingerprint(i)",
                    soci::use(id,"id"),soci::use(bag,"b"),soci::use(slot,"s"),soci::use(count,"n"),
                    soci::use(world,"w"),soci::use(original,"original"),soci::into(hash);
            }else if(!count) {
                long long removed=0;
                sql<<"DELETE FROM app_world.\"TITEMTABLE\" WHERE \"bWorldID\"=:w AND \"dlID\"=:id RETURNING \"dlID\"",
                    soci::use(world,"w"),soci::use(id,"id"),soci::into(removed);
            }else {
                sql<<"UPDATE app_world.\"TITEMTABLE\" i SET \"dwStorageID\"=:b,\"bItemID\"=:s,\"bCount\"=:n "
                     "WHERE \"bWorldID\"=:w AND \"dlID\"=:id RETURNING app_world.item_fingerprint(i)",
                    soci::use(bag,"b"),soci::use(slot,"s"),soci::use(count,"n"),soci::use(world,"w"),soci::use(id,"id"),soci::into(hash);
            }
            if(!sql.got_data())throw std::runtime_error("Inventory move lost its item");
            out.committed.hashes.push_back(hash);
        }
        sql<<"SET CONSTRAINTS app_world.item_slot IMMEDIATE";
    }
    if(equipment) {
        auto p=std::make_shared<CharacterPayload>(*out.after.payload);
        for(std::size_t n=0;n<out.move.items.size();++n) {
            const auto& move=out.move.items[n];const auto id=move.created?out.committed.created_id:move.before.dlID;
            for(auto& bag:p->bags)for(auto& item:bag.items)if(item.dlID==id)item.durable_hash=out.committed.hashes.at(n);
        }
        out.after.payload=std::move(p);out.committed.equipment_snapshot=std::make_shared<const CharSnapshot>(out.after);
    }
    return out;
}
}
