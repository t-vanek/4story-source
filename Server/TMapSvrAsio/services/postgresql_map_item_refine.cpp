#include "postgresql_map_service.h"
#include "native_item_power.h"
#include "native_item_effects.h"
#include "native_npc_discount.h"
#include "legacy_random.h"
#include "posture_effects.h"
#include "secure_code.h"
#include "domain/main_transfer.h"
#include "MessageId.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <map>
#include <optional>

namespace tmapsvr {
namespace {
const ItemInstance* FindItem(const CharSnapshot& s,std::uint8_t inventory,std::uint8_t slot) {
    for(const auto& bag:s.payload->bags)if(bag.bag.bInvenID==inventory)
        for(const auto& item:bag.items)if(item.bItemID==slot)return &item;
    return nullptr;
}
soci::row ItemChart(soci::session& sql,const ItemInstance& item) {
    const int id=std::bit_cast<std::int16_t>(item.wItemID);soci::row row;
    sql << "SELECT * FROM character_compat.\"TITEMCHART\" WHERE \"wItemID\"=:id",soci::use(id),soci::into(row);
    if(!sql.got_data())throw std::runtime_error("Refine item source template disappeared");
    return row;
}
std::uint8_t RefineGrade(std::uint8_t grade) {
    // Keep signed division toward zero for grade zero, then BYTE assignment.
    return static_cast<std::uint8_t>(grade<34?((int(grade)-1)/3)*3+1:(int(grade)/2)*2);
}
struct Magic {
    std::uint8_t id{},exclusive{},option{};
    std::uint32_t kinds{};
};
std::string StoreRefinedItem(soci::session& sql,const MapSessionClaim& c,
    const ItemInstance& before,const transfer::Item& raw) {
    if(raw.magic.size()>6)throw std::runtime_error("Refine magic exceeds original six slots");
    // Only the fields REFINE changes are writable here. This cannot replace
    // ownership, item count, extensions or the expiry with an old projection.
    std::string json="{",fields,values;
    const auto field=[&](const std::string& name,auto value) {
        if(json.size()>1){json+=',';fields+=',';values+=',';}
        json+='"'+name+"\":"+std::to_string(value);
        fields+='"'+name+'"';values+="v.\""+name+'"';
    };
    field("dwDuraMax",std::bit_cast<std::int32_t>(raw.durability_max));
    field("dwDuraCur",std::bit_cast<std::int32_t>(raw.durability));field("bRefineCur",raw.refine);
    for(unsigned i=0;i<6;++i) {
        field("bMagic"+std::to_string(i+1),i<raw.magic.size()?raw.magic[i].id:0);
        field("wValue"+std::to_string(i+1),i<raw.magic.size()?std::bit_cast<std::int16_t>(raw.magic[i].value):0);
    }
    json+='}';
    const int world=c.group,owner=std::bit_cast<std::int32_t>(c.char_id);
    const long long id=std::bit_cast<long long>(raw.id);std::string hash;
    sql << "WITH v AS (SELECT * FROM jsonb_populate_record(NULL::app_world.\"TITEMTABLE\",CAST(:item AS jsonb))) "
        "UPDATE app_world.\"TITEMTABLE\" i SET ("+fields+")=("+values+") FROM v "
        "WHERE i.\"bWorldID\"=:w AND i.\"dwOwnerID\"=:c AND i.\"dlID\"=:id AND i.\"bOwnerType\"=0 AND i.\"bStorageType\"=0 "
        "AND app_world.item_fingerprint(i)=:hash RETURNING app_world.item_fingerprint(i)",
        soci::use(json,"item"),soci::use(world,"w"),soci::use(owner,"c"),soci::use(id,"id"),
        soci::use(before.durable_hash,"hash"),soci::into(hash);
    if(!sql.got_data())throw std::runtime_error("Refined item ownership changed");
    return hash;
}
}

std::uint8_t PostgreSQLMapService::ItemMutationProbability(soci::session& sql,const MapSessionClaim& c,
    CharSnapshot& s,bool transfer,std::uint8_t base,std::uint8_t npc_bonus,const QuestMapFacts& facts,QuestPackets& packets) {
    if(!facts.world_registered||!facts.world_events||!facts.world_events->connection)
        throw std::runtime_error("Item probability requires a live World event projection");
    if(!SupportedPostures(*s.payload))throw std::runtime_error("Item probability requires admitted maintained effects");
    // CTMapSvrModule::CalcProb: WORD additions wrap, then the result is capped
    // at 100. The BYTE base was narrowed by the caller BEFORE this calculation.
    std::uint16_t result=static_cast<std::uint16_t>(base+npc_bonus);
    result=static_cast<std::uint16_t>(result+unsigned(base)*facts.world_events->values[transfer?6:5]/100U);
    const int status=transfer?59:58; // SDT_STATUS_TRANSPROB / REFINEPROB
    std::optional<std::size_t> first_matching;
    std::uint16_t probability=0;
    for(std::size_t i=0;i<s.payload->effects->size();++i) {
        const auto raw=NativeItemBuffValue(sql,s.payload->effects->at(i).skill,status);
        if(!raw)continue;
        if(!first_matching)first_matching=i;
        probability=*raw;
        if(probability)break;
    }
    if(probability) {
        result=transfer?static_cast<std::uint16_t>(result+unsigned(base)*probability/100U):100;
        // HaveItemProbBuff skips zero values; EraseItemProbBuff independently
        // removes the FIRST skill containing the status, even if its value was
        // zero. Keep this source distinction and the END -> stat packet order.
        EraseProgressEffect(sql,c,s,*first_matching,packets);
    }
    return static_cast<std::uint8_t>(std::min<unsigned>(result,100));
}

bool PostgreSQLMapService::RefineNativeItem(soci::session& sql,const MapSessionClaim& c,CharSnapshot& s,
    std::span<const std::byte> body,const QuestMapFacts& facts,QuestPackets& packets) {
    using tnetlib::protocol::MessageId;
    if(s.payload->security.Blocks()) {
        const auto prompt=SecureUnlockPrompt(s.payload->security);packets.emplace_back(prompt.first,prompt.second);return false;
    }
    wire::Reader reader(body.data(),body.size());
    std::uint8_t quote=0,inventory=0,slot=0,count=0,call_inventory=0,call_slot=0;std::uint16_t npc=0;
    if(!reader.Read(quote)||!reader.Read(inventory)||!reader.Read(slot)||!reader.Read(count)||!reader.Read(npc)||
       !reader.Read(call_inventory)||!reader.Read(call_slot)||body.size()!=8U+2U*count)
        throw std::runtime_error("Invalid REFINE request");
    const auto reply=[&](std::uint8_t result,std::uint8_t bag,const ItemInstance* item=nullptr) {
        std::vector<std::byte> ack;wire::WritePOD(ack,result);wire::WritePOD(ack,bag);
        if(item) {auto descriptor=EncodeItemDescriptor(*item,c.char_id,true);ack.insert(ack.end(),descriptor.begin(),descriptor.end());}
        packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_REFINE_ACK),std::move(ack));
    };
    const auto* found=FindItem(s,inventory,slot);
    // Zero materials is an explicit safety repair: the source later divides
    // by bAddCount and uses an uninitialised bAddInven for NPC-call errors.
    if(!found||!count||count>3||inventory==254||!found->bCount){reply(1,inventory);return false;}
    const auto target=*found;
    if(!target.source)throw std::runtime_error("Refine requires the original target projection");
    const auto chart=ItemChart(sql,target);
    if(!chart.get<int>("bCanRepair")||target.bGLevel){reply(3,inventory);return false;}
    if(chart.get<int>("bRefineMax")<=target.bRefineCur){reply(4,inventory);return false;}
    const auto grade=RefineGrade(NativeItemPowerLevel(sql,target,chart));
    std::vector<ItemInstance> materials;
    std::uint8_t sum=0,refine_count=0;
    std::uint32_t durability=0;
    for(unsigned i=0;i<count;++i) {
        std::uint8_t bag=0,position=0;
        if(!reader.Read(bag)||!reader.Read(position))throw std::runtime_error("Truncated REFINE material");
        const auto* item=FindItem(s,bag,position);
        if((bag==inventory&&position==slot)||bag==254||!item||!item->bCount){reply(1,bag);return false;}
        if(!item->source)throw std::runtime_error("Refine material requires its original projection");
        const auto material_chart=ItemChart(sql,*item);
        auto material_grade=grade;
        if(material_chart.get<int>("bType")==14)++refine_count; // IT_REFINE
        else if(!material_chart.get<int>("bRefineMax")){reply(3,inventory);return false;}
        else material_grade=RefineGrade(NativeItemPowerLevel(sql,*item,material_chart));
        const auto gap=std::min(grade,material_grade);
        if(grade-gap>25){reply(5,bag);return false;}
        const auto add_grade=static_cast<std::uint8_t>(20+gap-grade);
        if(std::any_of(materials.begin(),materials.end(),[&](const auto& m){return m.bInvenID==bag&&m.bItemID==position;})) {
            reply(1,bag);return false;
        }
        sum=static_cast<std::uint8_t>(sum+add_grade);
        durability+=static_cast<std::uint32_t>(static_cast<std::uint32_t>(chart.get<int>("dwDuraMax"))*(8U+LegacyRandom15()%8U))/100U;
        materials.push_back(*item);
    }
    const int level=grade;int base_cost=0;
    sql << "SELECT \"dwRefineCost\" FROM character_compat.\"TLEVELCHART\" WHERE \"bLevel\"=:level",soci::use(level),soci::into(base_cost);
    if(!sql.got_data()) {
        sql << "SELECT \"dwRefineCost\" FROM character_compat.\"TLEVELCHART\" ORDER BY \"bLevel\" LIMIT 1",soci::into(base_cost);
        if(!sql.got_data()){reply(1,inventory);return false;}
    }
    const float product=float(static_cast<std::uint32_t>(base_cost))*static_cast<float>(chart.get<double>("fPrice"));
    if(!std::isfinite(product)||static_cast<double>(product)>=4294967296.0)
        throw std::runtime_error("Refine cost has an undefined source DWORD conversion");
    const auto cost=static_cast<std::uint32_t>(std::max(1.0f,product));
    const int npc_id=std::bit_cast<std::int16_t>(npc);int condition=0,rate=0,local=0,bonus=0;
    sql << "SELECT \"bCondition\",\"bDiscountRate\",\"wLocalID\",\"bAddProb\" FROM character_compat.\"TNPCCHART\" WHERE \"wID\"=:id",
        soci::use(npc_id),soci::into(condition),soci::into(rate),soci::into(local),soci::into(bonus);
    const bool npc_exists=sql.got_data();
    const auto discount=npc_exists?NpcDiscount(condition,rate,local):std::uint8_t{0};
    const auto discounted=cost-static_cast<std::uint32_t>(cost*discount)/100U;
    if(quote) {
        std::vector<std::byte> ack;wire::WritePOD(ack,cost);wire::WritePOD(ack,discount);
        packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_REFINECOST_ACK),std::move(ack));return false;
    }
    if(!UseNativeMoney(sql,c,s,discounted,false,packets)){reply(2,inventory);return false;}
    const bool title_check=discounted!=0;
    if(call_inventory!=252&&call_slot!=255) {
        if(s.wMapID!=0&&s.wMapID!=8){reply(8,call_inventory);return title_check;}
        const auto* caller=FindItem(s,call_inventory,call_slot);
        const auto error_bag=materials.back().bInvenID; // original last bAddInven
        if(!caller||caller->bKind!=55||!caller->bCount){reply(7,error_bag);return title_check;}
        // Reject a last-item caller alias instead of reproducing source UAF.
        if(caller->bCount==1&&(caller->dlID==target.dlID||std::any_of(materials.begin(),materials.end(),
            [&](const auto& item){return item.dlID==caller->dlID;}))) {reply(7,error_bag);return title_check;}
        DebitNativeItem(sql,c,s,call_inventory,call_slot,caller->dlID,1,packets);
    }
    if(!UseNativeMoney(sql,c,s,discounted,true,packets))throw std::runtime_error("Refine purse changed during its transaction");
    packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_MONEY_ACK),EncodeMoneyAck(s.dwGold,s.dwSilver,s.dwCooper));
    std::map<std::uint8_t,Magic> magic_chart;
    const auto magic_for=[&](std::uint8_t code)->const Magic& {
        if(const auto it=magic_chart.find(code);it!=magic_chart.end())return it->second;
        const int id=code;int kind=0,option=0,exclusive=0;
        sql << "SELECT \"dwKind\",\"bOptionKind\",\"bExclIndex\" FROM actor_compat.\"TITEMMAGICCHART\" WHERE \"bMagic\"=:id",
            soci::use(id),soci::into(kind),soci::into(option),soci::into(exclusive);
        if(!sql.got_data())throw std::runtime_error("Refine magic source template disappeared");
        return magic_chart.emplace(code,Magic{code,static_cast<std::uint8_t>(exclusive),static_cast<std::uint8_t>(option),
            static_cast<std::uint32_t>(kind)}).first->second;
    };
    std::vector<transfer::Magic> candidates;
    for(const auto& material:materials) {
        auto options=material.source->magic;
        std::sort(options.begin(),options.end(),[](const auto& a,const auto& b){return a.id<b.id;});
        for(const auto& option:options)if(chart.get<int>("bCanMagic")) {
            const auto& def=magic_for(option.id);const int kind=chart.get<int>("bKind");
            if(kind<1||kind>32)throw std::runtime_error("Refine target has an undefined source kind bit shift");
            if((def.kinds&(std::uint32_t{1}<<(kind-1)))&&(!target.bLevel||!def.option))candidates.push_back(option);
        }
        // Source deletes the ENTIRE material stack. Reacquire after optional
        // NPC-call consumption so a valid shared stack is not restored or
        // charged from a stale count/fingerprint.
        const auto* current=FindItem(s,material.bInvenID,material.bItemID);
        if(!current||current->dlID!=material.dlID)throw std::runtime_error("Refine material identity changed");
        DebitNativeItem(sql,c,s,current->bInvenID,current->bItemID,current->dlID,current->bCount,packets);
    }
    const auto roll=LegacyRandom15()%100U;
    const auto npc_bonus=npc_exists?NpcProbabilityBonus(condition,bonus,local):std::uint8_t{0};
    const auto probability=npc_exists?ItemMutationProbability(sql,c,s,false,
        static_cast<std::uint8_t>(20+unsigned(sum)*3U/count),npc_bonus,facts,packets):std::uint8_t{0};
    if(roll>=probability){reply(6,inventory);return true;}
    const auto* current=FindItem(s,inventory,slot);
    if(!current||current->dlID!=target.dlID||!current->source)throw std::runtime_error("Refine target identity changed");
    const auto original=*current;auto raw=*current->source;
    raw.durability_max+=durability;raw.durability=raw.durability_max;++raw.refine;
    std::uint8_t trans_probability=0;
    if(!candidates.empty())trans_probability=ItemMutationProbability(sql,c,s,true,
        static_cast<std::uint8_t>(8+6U*refine_count/count),npc_bonus,facts,packets);
    std::map<std::uint8_t,std::uint16_t> magic;
    for(const auto& option:raw.magic)magic.emplace(option.id,option.value);
    while(!candidates.empty()) {
        const auto option=candidates.back();candidates.pop_back();
        if(magic.size()>=6)continue; // even replacement is skipped; no random draw
        const auto& def=magic_for(option.id);bool collision=false;
        for(const auto& [id,value]:magic)if(def.exclusive&&id!=option.id&&magic_for(id).exclusive==def.exclusive) {
            collision=true;break;
        }
        const auto magic_roll=LegacyRandom15()%100U; // drawn even for a collision
        if(collision||magic_roll>=trans_probability)continue;
        const auto existing=magic.find(option.id);
        if(existing!=magic.end())existing->second=std::max(existing->second,option.value);
        else if(option.value)magic.emplace(option.id,option.value);
    }
    raw.magic.clear();for(const auto& [id,value]:magic)raw.magic.push_back({id,value});
    const auto hash=s.payload->transfer_state?GraphItemFingerprint(raw):StoreRefinedItem(sql,c,original,raw);
    auto replacement=ProjectNativeItem(sql,raw);replacement.durable_hash=hash;replacement.alarm_time=original.alarm_time;
    auto payload=std::make_shared<CharacterPayload>(*s.payload);s.payload=payload;
    bool replaced=false;
    for(auto& bag:payload->bags)if(bag.bag.bInvenID==inventory)
        for(auto& item:bag.items)if(item.bItemID==slot&&item.dlID==raw.id){item=replacement;replaced=true;}
    if(!replaced)throw std::runtime_error("Refine target disappeared from its inventory");
    reply(0,inventory,&replacement);
    // No GETITEM/USEITEM quest hook, equipment broadcast or extra UPDATEITEM
    // is present in OnCS_REFINE_REQ. The complete descriptor is in this ACK.
    return true;
}
}
