#include "postgresql_map_service.h"
#include "inventory_move.h"
#include "main_transfer_codec.h"
#include "main_transfer_runtime.h"
#include <soci/soci.h>
#include <openssl/sha.h>
#include <bit>
#include <limits>

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
    InventoryStoragePlan out;out.move=PlanInventoryMove(before,request);
    if(out.move.result!=InventoryMoveResult::Success||out.move.items.empty())throw std::runtime_error("Rejected inventory move cannot commit");
    auto expected=before;ApplyInventoryMove(expected,out.move);
    const auto candidate=CaptureInventory(after,c.key);
    if(transfer::Encode(CaptureInventory(expected,c.key))!=transfer::Encode(candidate))
        throw std::runtime_error("Inventory move changes unrelated character state");
    const bool graph=bool(before.payload->transfer_state);
    if(graph!=bool(after.payload->transfer_state))throw std::runtime_error("Inventory storage contract changed");
    const int world=c.group,character=c.char_id;
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
        out.after_graph=TransferFingerprint(c,after);
    }
    // Claim/checkpoint locks serialize supported inventory writers. Re-read bag
    // metadata and pinned capacities in this transaction; do not trust the DTO.
    for(const auto id:{request.source_bag,request.destination_bag}) {
        const auto bag=std::find_if(before.payload->bags.begin(),before.payload->bags.end(),[&](const auto& b){return b.bag.bInvenID==id;});
        const int template_id=std::bit_cast<std::int16_t>(bag->bag.wItemID);int capacity=0,type=0;
        sql<<"SELECT \"bSlotCount\",\"bType\" FROM character_compat.\"TITEMCHART\" WHERE \"wItemID\"=:i",
            soci::use(template_id),soci::into(capacity),soci::into(type);
        if(!sql.got_data()||type!=11||capacity!=bag->slot_count||!capacity)
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
    for(const auto& move:out.move.items) {
        const auto& item=move.before;
        if(graph) {
            if(GraphItemFingerprint(*item.source)!=item.durable_hash)throw std::runtime_error("Inventory graph item fingerprint changed");
            const auto moved=std::find_if(candidate.items.begin(),candidate.items.end(),[&](const auto& i){return i.id==item.dlID;});
            if(moved==candidate.items.end())throw std::runtime_error("Inventory item missing after move");
            out.hashes.push_back(GraphItemFingerprint(*moved));
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
        if(out.move.items.size()==1) {
            const int bag=request.destination_bag,slot=request.destination_slot;int present=0;
            sql<<"SELECT 1 FROM app_world.\"TITEMTABLE\" WHERE \"bWorldID\"=:w AND \"dwOwnerID\"=:c AND \"dwStorageID\"=:b AND \"bItemID\"=:s FOR UPDATE",
                soci::use(world,"w"),soci::use(character,"c"),soci::use(bag,"b"),soci::use(slot,"s"),soci::into(present);
            if(sql.got_data())throw std::runtime_error("Inventory destination became occupied");
        }
        sql<<"SET CONSTRAINTS app_world.item_slot DEFERRED";
        for(const auto& move:out.move.items) {
            const long long id=move.before.dlID;const int bag=move.bag,slot=move.slot;std::string hash;
            sql<<"UPDATE app_world.\"TITEMTABLE\" i SET \"dwStorageID\"=:b,\"bItemID\"=:s "
                 "WHERE \"bWorldID\"=:w AND \"dlID\"=:id RETURNING app_world.item_fingerprint(i)",
                soci::use(bag,"b"),soci::use(slot,"s"),soci::use(world,"w"),soci::use(id,"id"),soci::into(hash);
            if(!sql.got_data())throw std::runtime_error("Inventory move lost its item");
            out.hashes.push_back(hash);
        }
        sql<<"SET CONSTRAINTS app_world.item_slot IMMEDIATE";
    }
    return out;
}
}
