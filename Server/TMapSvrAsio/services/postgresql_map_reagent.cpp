#include "postgresql_map_service.h"
#include "main_transfer_codec.h"
#include "main_transfer_runtime.h"
#include <openssl/sha.h>
#include <soci/soci.h>
#include <algorithm>
#include <stdexcept>
#include <tuple>

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
PostgreSQLMapService::ReagentGraphPlan PostgreSQLMapService::ValidateGraphReagent(
    soci::session& sql,const MapSessionClaim& c,const ItemInstance& before,const CharSnapshot& after) const {
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
    auto selected=stored->items.end();
    for(auto it=stored->items.begin();it!=stored->items.end();++it)if(it->storage==0&&it->item==before.wItemID) {
        if(selected==stored->items.end()||std::tie(it->storage_id,it->slot)<std::tie(selected->storage_id,selected->slot))selected=it;
    }
    if(selected==stored->items.end()||selected->owner_type||selected->owner_id!=c.char_id||selected->storage_id!=before.bInvenID||
       selected->slot!=before.bItemID||selected->id!=before.dlID||selected->count!=before.bCount||
       GraphItemFingerprint(*selected)!=before.durable_hash)
        throw std::runtime_error("Reagent graph item changed since hydration");
    if(--selected->count==0)stored->items.erase(selected);
    else plan.item_hash=GraphItemFingerprint(*selected);

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
