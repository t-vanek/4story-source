#include "postgresql_map_service.h"
#include "main_transfer_codec.h"
#include "main_transfer_runtime.h"
#include "skill_reagent.h"
#include "postgresql_skill_targets.h"
#include <openssl/sha.h>
#include <soci/soci.h>
#include <algorithm>
#include <stdexcept>
#include <tuple>
#include <bit>
#include <map>

namespace tmapsvr {
namespace {
std::string Hash(std::span<const std::byte> bytes) {
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(bytes.data()),bytes.size(),digest);
    constexpr char digits[]="0123456789abcdef";std::string out;
    for(auto b:digest){out+=digits[b>>4];out+=digits[b&15];}return out;
}
}
std::string PostgreSQLMapService::GraphItemFingerprint(const transfer::Item& item) {
    return Hash(transfer::EncodeItem(item));
}
PostgreSQLMapService::ConsumptionKind PostgreSQLMapService::ValidateSkillConsumption(soci::session& sql,const MapSessionClaim& c,
    std::uint16_t skill,std::uint8_t hits,const std::vector<SkillItemDebit>& debits,const CharSnapshot& after,const transfer::State* graph) {
    const int sid=std::bit_cast<std::int16_t>(skill);int reagent=0,mask=0,start=0,next=0,target_hit=0;
    sql<<"SELECT \"wItemID\",\"dwWeaponID\",\"bLevel\",\"bNextLevel\",\"bTargetHit\" FROM character_compat.\"TSKILLCHART\" WHERE \"wID\"=:id",
        soci::use(sid),soci::into(reagent),soci::into(mask),soci::into(start),soci::into(next),soci::into(target_hit);
    if(!sql.got_data())throw std::runtime_error("Missing consumption skill template");
    if(reagent&&mask==0&&debits.size()==1&&debits[0].count==1&&static_cast<std::uint16_t>(reagent)==debits[0].before.wItemID)return ConsumptionKind::Reagent;
    if(reagent||!mask)throw std::runtime_error("Unsupported skill consumption requirement");
    // The authoritative learned rank must agree with the cast projection. Fresh
    // rows are locked; transferred characters use only the complete checkpoint.
    int rank=0;
    if(graph) {
        for(const auto& learned:graph->skills)if(learned.wSkillID==skill)rank=learned.bLevel;
    }else {
        const int world=c.group,character=c.char_id;
        sql<<"SELECT \"bLevel\" FROM app_world.\"TSKILLTABLE\" WHERE \"bWorldID\"=:w AND \"dwCharID\"=:c AND \"wSkillID\"=:s FOR UPDATE",
            soci::use(world,"w"),soci::use(character,"c"),soci::use(sid,"s"),soci::into(rank);
        if(!sql.got_data())throw std::runtime_error("Consumption learned skill disappeared");
    }
    const auto learned=std::find_if(after.payload->skills.begin(),after.payload->skills.end(),[&](const auto& row){return row.wSkillID==skill;});
    if(!rank||learned==after.payload->skills.end()||learned->bLevel!=rank)throw std::runtime_error("Consumption learned rank changed");
    SkillTemplate t;t.wID=skill;t.bStartLevel=start;t.bNextLevel=next;double growth=0;
    sql<<"SELECT \"fRateX\" FROM character_compat.\"TFORMULACHART\" WHERE \"bID\"=34",soci::into(growth);
    if(!sql.got_data())throw std::runtime_error("Consumption skill growth formula missing");
    t.f1stRateX=static_cast<float>(growth);
    const auto multi=ReadNativeMultiAttack(sql,t,rank,target_hit);
    if(multi&&(!multi->count||multi->count>16||hits!=multi->count))throw std::runtime_error("Expanded ammunition count disagrees with learned skill");
    struct Entry {std::uint64_t id;int bag,slot,kind,use,count;bool powered;std::string hash;int item,stack;};
    std::vector<Entry> items;
    if(graph) {
        std::map<std::uint16_t,std::array<int,3>> charts;
        for(const auto& item:graph->items)if(item.storage==0) {
            auto it=charts.find(item.item);
            if(it==charts.end()) {
                const int id=std::bit_cast<std::int16_t>(item.item);std::array<int,3> t{};
                sql<<"SELECT \"bKind\",\"bUseItemKind\",\"bUseItemCount\" FROM character_compat.\"TITEMCHART\" WHERE \"wItemID\"=:id",
                    soci::use(id),soci::into(t[0]),soci::into(t[1]),soci::into(t[2]);
                if(!sql.got_data())throw std::runtime_error("Graph ammunition template missing");
                it=charts.emplace(item.item,t).first;
            }
            const auto& t=it->second;
            items.push_back({item.id,static_cast<int>(item.storage_id),item.slot,t[0],t[1],t[2],!item.durability_max||item.durability,{},item.item,item.count});
        }
    }else {
        const int world=c.group,owner=c.char_id;
        // Lock the inventory used for the selection, including the equipped
        // weapon. Its loaded full-row hash also fences stale timing/equipment.
        soci::rowset<soci::row> rows=(sql.prepare<<
            "SELECT i.\"dlID\",i.\"dwStorageID\",i.\"bItemID\",t.\"bKind\",t.\"bUseItemKind\",t.\"bUseItemCount\","
            "CASE WHEN i.\"dwDuraMax\"=0 OR i.\"dwDuraCur\"<>0 THEN 1 ELSE 0 END,app_world.item_fingerprint(i),i.\"wItemID\",i.\"bCount\" "
            "FROM app_world.\"TITEMTABLE\" i JOIN character_compat.\"TITEMCHART\" t ON t.\"wItemID\"=i.\"wItemID\" "
            "WHERE i.\"bWorldID\"=:w AND i.\"dwOwnerID\"=:c AND i.\"bOwnerType\"=0 AND i.\"bStorageType\"=0 "
            "ORDER BY i.\"dwStorageID\",i.\"bItemID\" FOR UPDATE OF i",soci::use(world,"w"),soci::use(owner,"c"));
        for(const auto& r:rows)items.push_back({static_cast<std::uint64_t>(r.get<long long>(0)),r.get<int>(1),r.get<int>(2),
            r.get<int>(3),r.get<int>(4),r.get<int>(5),r.get<int>(6)!=0,r.get<std::string>(7),r.get<int>(8),r.get<int>(9)});
    }
    std::sort(items.begin(),items.end(),[](const auto& a,const auto& b){return std::tie(a.bag,a.slot)<std::tie(b.bag,b.slot);});
    const auto weapon=std::find_if(items.begin(),items.end(),[&](const auto& i){return i.bag==254&&i.kind>0&&i.kind<=32&&
        (static_cast<std::uint32_t>(mask)&(std::uint32_t{1}<<(i.kind-1)));});
    if(weapon==items.end()||!weapon->powered||!weapon->use||weapon->count!=1)
        throw std::runtime_error("Ammunition weapon changed or requires unsupported count");
    if(!graph) {
        bool exact=false;
        for(const auto& bag:after.payload->bags)for(const auto& i:bag.items)
            if(i.dlID==weapon->id&&i.bInvenID==254&&i.bItemID==weapon->slot&&i.durable_hash==weapon->hash)exact=true;
        if(!exact)throw std::runtime_error("Ammunition weapon differs from loaded snapshot");
    }
    std::vector<ItemInstance> inventory;
    for(const auto& row:items) {
        if(row.bag<0||row.bag>255)throw std::runtime_error("Unsupported ammunition inventory id");
        ItemInstance i;i.dlID=row.id;i.bInvenID=row.bag;i.bItemID=row.slot;i.bKind=row.kind;
        i.wItemID=static_cast<std::uint16_t>(row.item);i.bCount=row.stack;inventory.push_back(i);
    }
    const auto selected=SelectSkillAmmunition(std::move(inventory),weapon->use,hits);
    if(selected.empty()||selected.size()!=debits.size())throw std::runtime_error("Ammunition stack selection changed");
    for(std::size_t i=0;i<selected.size();++i) {
        const auto& expected=selected[i].before;const auto& before=debits[i].before;
        if(selected[i].count!=debits[i].count||expected.dlID!=before.dlID||expected.bInvenID!=before.bInvenID||
           expected.bItemID!=before.bItemID||expected.wItemID!=before.wItemID||expected.bKind!=before.bKind||expected.bCount!=before.bCount)
            throw std::runtime_error("Ammunition debit differs from ordered source selection");
    }
    return multi?ConsumptionKind::MultiAttackAmmunition:ConsumptionKind::Ammunition;
}
PostgreSQLMapService::ReagentGraphPlan PostgreSQLMapService::ValidateGraphReagent(
    soci::session& sql,const MapSessionClaim& c,std::uint16_t skill,std::uint8_t hits,const std::vector<SkillItemDebit>& debits,const CharSnapshot& after) const {
    // The caller holds the exact primary claim and checkpoint row locks. Full
    // graph inventory is authoritative even when normalized item/skill rows lag.
    const int world=c.group,character=c.char_id;std::string hex,core_hash;ReagentGraphPlan plan;
    sql<<"SELECT encode(transfer_body,'hex'),transfer_hash,fingerprint FROM app_world.map_checkpoints "
         "WHERE world_id=:w AND char_id=:c AND recovery_contract=2 AND character_manifest=:cm AND routing_manifest=:rm AND actor_manifest=:am",
        soci::use(world,"w"),soci::use(character,"c"),soci::use(m_config.character_manifest,"cm"),
        soci::use(m_config.routing_manifest,"rm"),soci::use(m_config.actor_manifest,"am"),
        soci::into(hex),soci::into(plan.before_hash),soci::into(core_hash);
    if(!sql.got_data())throw std::runtime_error("Reagent graph catalog receipt changed");
    std::vector<std::byte> body;body.reserve(hex.size()/2);
    for(std::size_t i=0;i<hex.size();i+=2)body.push_back(static_cast<std::byte>(std::stoul(hex.substr(i,2),nullptr,16)));
    auto stored=transfer::Decode(body);
    if(!stored||stored->db_load||stored->character.dwCharID!=c.char_id||stored->key!=c.key||Hash(body)!=plan.before_hash)
        throw std::runtime_error("Invalid reagent recovery graph");
    auto old_core=stored->character;old_core.payload=after.payload;
    if(CoreFingerprint(c,old_core)!=core_hash)throw std::runtime_error("Reagent graph core disagrees with recovery receipt");
    plan.kind=ValidateSkillConsumption(sql,c,skill,hits,debits,after,&*stored);
    for(const auto& debit:debits) {
        const auto& before=debit.before;
        auto selected=stored->items.end();
        for(auto it=stored->items.begin();it!=stored->items.end();++it)if(it->storage==0&&(plan.kind!=ConsumptionKind::Reagent?it->id==before.dlID:it->item==before.wItemID)) {
            if(selected==stored->items.end()||std::tie(it->storage_id,it->slot)<std::tie(selected->storage_id,selected->slot))selected=it;
        }
        if(selected==stored->items.end()||selected->owner_type||selected->owner_id!=c.char_id||selected->storage_id!=before.bInvenID||
           selected->slot!=before.bItemID||selected->id!=before.dlID||selected->count!=before.bCount||
           GraphItemFingerprint(*selected)!=before.durable_hash)
            throw std::runtime_error("Consumption graph item changed since hydration");
        selected->count-=debit.count;
        if(!selected->count){stored->items.erase(selected);plan.item_hashes.emplace_back();}
        else plan.item_hashes.push_back(GraphItemFingerprint(*selected));
    }

    SkillCooldownTracker timers;timers.Restore(c.char_id,after.payload->skills,0);
    auto candidate=transfer::Capture(after,c.key,timers,0);
    // Allow the live core and sampled cooldown durations to advance. The cast
    // cannot learn/forget/change ranks, reorder or discard unrelated graph data.
    if(stored->skills.size()!=candidate.skills.size())throw std::runtime_error("Reagent graph learned skills changed");
    for(std::size_t i=0;i<stored->skills.size();++i) {
        if(stored->skills[i].wSkillID!=candidate.skills[i].wSkillID||stored->skills[i].bLevel!=candidate.skills[i].bLevel)
            throw std::runtime_error("Reagent graph learned rank changed");
        stored->skills[i].dwRemainTick=candidate.skills[i].dwRemainTick;
    }
    stored->character=candidate.character;
    const auto encoded=transfer::Encode(candidate);
    if(transfer::Encode(*stored)!=encoded)throw std::runtime_error("Reagent plan changes unrelated transfer state");
    plan.after_hash=Hash(encoded);return plan;
}
}
