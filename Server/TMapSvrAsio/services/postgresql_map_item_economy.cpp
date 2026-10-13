#include "postgresql_map_service.h"
#include "secure_code.h"
#include "posture_effects.h"
#include "native_npc_discount.h"
#include "native_item_power.h"
#include "native_item_effects.h"
#include "domain/main_transfer.h"
#include "MessageId.h"
#include <soci/soci.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>

namespace tmapsvr {
namespace {
std::uint8_t ByteFromFloat(float value) {
    const auto integer=std::trunc(value);
    if(!std::isfinite(integer)||integer<0||integer>255)
        throw std::runtime_error("Item power has an undefined source float-to-BYTE conversion");
    return static_cast<std::uint8_t>(value);
}
std::pair<float,float> FormulaRates(soci::session& sql,int id) {
    double x=0,y=0;
    sql << "SELECT \"fRateX\",\"fRateY\" FROM character_compat.\"TFORMULACHART\" WHERE \"bID\"=:id",
        soci::use(id),soci::into(x),soci::into(y);
    if(!sql.got_data())throw std::runtime_error("Item price source formula missing");
    return {static_cast<float>(x),static_cast<float>(y)};
}
std::uint8_t PowerLevel(soci::session& sql,const ItemInstance& item,const soci::row& chart,const soci::row& attr) {
    const auto magic=[&](unsigned id)->std::uint32_t {
        for(const auto& [code,value]:item.magic)if(code==id)return value;
        return 0; // CTItem::GetMagicValue absent magic, not a missing catalog
    };
    // NetCode MAGIC_TYPE: PAP7/PDP8/LAP9/MDP16/MAP17. Only these
    // aggregate options enter the source power-level reconstruction branch.
    if(!magic(7)&&!magic(8)&&!magic(9)&&!magic(16)&&!magic(17))
        return static_cast<std::uint8_t>(attr.get<int>("bGrade"));
    const auto type=chart.get<int>("bType"),kind=chart.get<int>("bKind");
    const auto a=[&](const char* field){return std::uint32_t{static_cast<std::uint16_t>(attr.get<int>(field))};};
    if(type==1||type==4) { // IT_WEAPON / IT_LONG
        constexpr std::array<float,26> rates{0,.75f,.2f,1,1,1.1f,.8f,1,.8f,1};
        if(kind<0||kind>=int(rates.size())||!rates[kind])return 1;
        const auto level_constant=1.0f/std::log(FormulaRates(sql,34).first);
        const auto [minimum,maximum]=FormulaRates(sql,22);
        auto low=magic(type==4?9:7)+magic(type==4?63:61)+a("wMinAP");
        auto high=magic(type==4?9:7)+magic(type==4?64:62)+a("wMaxAP");
        low+=magic(17)+magic(65)+(type==1?a("wMinMAP"):0U);
        high+=magic(17)+magic(66)+(type==1?a("wMaxMAP"):0U);
        // Cast to BYTE precedes +1, then assignment narrows again. Keep the
        // source wrap at 255 instead of changing the price to a wider formula.
        const auto min_level=static_cast<std::uint8_t>(ByteFromFloat(std::log(float(low)/(rates[kind]*minimum))*level_constant)+1U);
        const auto max_level=static_cast<std::uint8_t>(ByteFromFloat(std::log(float(high)/(rates[kind]*maximum))*level_constant)+1U);
        return static_cast<std::uint8_t>(std::max((unsigned(min_level)+max_level)/2,1U));
    }
    if(type==2||type==6) { // IT_DEFENSIVE / IT_SHIELD
        constexpr std::array<float,26> rates{0,0,0,0,0,0,0,0,0,0,0,.8f,1,1.3f,1.3f,1.3f,1.3f,1.3f,1.3f};
        constexpr std::array<float,15> parts{0,.3f,0,.2f,0,.3f,.25f,.15f,.1f};
        const int slot=chart.get<int>("bPrmSlotID");
        if(kind<0||kind>=int(rates.size())||!rates[kind]||slot<0||slot>=int(parts.size())||!parts[slot])return 1;
        const auto level_constant=1.0f/std::log(FormulaRates(sql,34).first);
        const auto rate=FormulaRates(sql,21).first;
        const auto defense=magic(8)+magic(16)+a("wDP")+a("wMDP");
        const float power=std::log(float(defense)/(rates[kind]*parts[slot]*rate))*level_constant+1;
        return std::max<std::uint8_t>(ByteFromFloat(power),1);
    }
    return 1; // original non-weapon/armor enhanced item branch
}
soci::row ItemAttribute(soci::session& sql,const ItemInstance& item,const soci::row& chart) {
    const int upgrade=item.bLevel;int grade=0;
    sql << "SELECT \"bGrade\" FROM actor_compat.\"TITEMGRADECHART\" WHERE \"bLevel\"=:level",soci::use(upgrade),soci::into(grade);
    const auto base=static_cast<std::uint16_t>(chart.get<int>("wAttrID"));
    const int key=std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(base+grade+item.bGem));
    soci::row attr;
    sql << "SELECT * FROM actor_compat.\"TITEMATTRCHART\" WHERE \"wID\"=:id",soci::use(key),soci::into(attr);
    if(!sql.got_data()) {
        // CTMapSvrModule::SetItemAttr's first unsigned WORD fallback.
        sql << "SELECT * FROM actor_compat.\"TITEMATTRCHART\" ORDER BY (\"wID\"::integer & 65535) LIMIT 1",soci::into(attr);
        if(!sql.got_data())throw std::runtime_error("Item price has no source attribute fallback");
    }
    return attr;
}
std::uint32_t ItemPrice(soci::session& sql,const ItemInstance& item,const soci::row& chart) {
    const auto attr=ItemAttribute(sql,item,chart);
    const int level=chart.get<int>("wAttrID")?PowerLevel(sql,item,chart,attr):chart.get<int>("bLevel");
    int money=0;
    sql << "SELECT \"dwMoney\" FROM character_compat.\"TLEVELCHART\" WHERE \"bLevel\"=:level",soci::use(level),soci::into(money);
    if(!sql.got_data()) {
        sql << "SELECT \"dwMoney\" FROM character_compat.\"TLEVELCHART\" ORDER BY \"bLevel\" LIMIT 1",soci::into(money);
        if(!sql.got_data())throw std::runtime_error("Item price has no source level fallback");
    }
    // DWORD * FLOAT is evaluated before addition of the source double .99.
    const float product=static_cast<std::uint32_t>(money)*static_cast<float>(chart.get<double>("fPrice"));
    const double price=product+0.99;
    if(!std::isfinite(price)||std::trunc(price)<0||std::trunc(price)>4294967295.0)
        throw std::runtime_error("Item price has an undefined source DWORD conversion");
    return static_cast<std::uint32_t>(price);
}
std::uint32_t RepairCost(soci::session& sql,const ItemInstance& item,const soci::row& chart) {
    if(!item.source||!item.dwDuraMax||item.dwDuraCur>=item.dwDuraMax)
        throw std::runtime_error("Repair cost requires a damaged original item");
    const auto attr=ItemAttribute(sql,item,chart);
    const int level=PowerLevel(sql,item,chart,attr);int repair=0;
    sql << "SELECT \"dwRepairCost\" FROM character_compat.\"TLEVELCHART\" WHERE \"bLevel\"=:level",
        soci::use(level),soci::into(repair);
    if(!sql.got_data()) {
        sql << "SELECT \"dwRepairCost\" FROM character_compat.\"TLEVELCHART\" ORDER BY \"bLevel\" LIMIT 1",soci::into(repair);
        if(!sql.got_data())throw std::runtime_error("Repair cost has no source level fallback");
    }
    // CTItem::GetRepairCost multiplies DWORDs before converting to FLOAT.
    const auto damage=static_cast<std::uint32_t>(repair)*(item.dwDuraMax-item.dwDuraCur);
    const float calculated=damage*static_cast<float>(chart.get<double>("fPrice"))/item.dwDuraMax;
    if(!std::isfinite(calculated))throw std::runtime_error("Repair cost has a nonfinite source calculation");
    const float cost=std::max(1.0f,calculated);
    if(static_cast<double>(cost)>=4294967296.0)
        throw std::runtime_error("Repair cost has an undefined source DWORD conversion");
    auto result=static_cast<std::uint32_t>(cost);
    // This source branch reads the stored WORD percentage directly, not
    // GetMagicValue's template-scaled projection. MTYPE_REPCOST=112.
    for(const auto& magic:item.source->magic)if(magic.id==112) {
        result=static_cast<std::uint32_t>(result*magic.value)/100U;break;
    }
    return result;
}
}
std::uint8_t NativeItemPowerLevel(soci::session& sql,const ItemInstance& item,const soci::row& chart) {
    return PowerLevel(sql,item,chart,ItemAttribute(sql,item,chart));
}
bool PostgreSQLMapService::SellNativeItem(soci::session& sql,const MapSessionClaim& c,CharSnapshot& s,
    std::span<const std::byte> body,std::uint16_t& sold,QuestPackets& packets) {
    using tnetlib::protocol::MessageId;
    if(body.size()!=5)throw std::runtime_error("Invalid ITEMSELL request length");
    if(s.payload->security.Blocks()) {
        const auto prompt=SecureUnlockPrompt(s.payload->security);packets.emplace_back(prompt.first,prompt.second);return false;
    }
    const auto at=[&](unsigned i){return std::to_integer<std::uint8_t>(body[i]);};
    const auto inventory=at(0),slot=at(1),count=at(2),call_inventory=at(3),call_slot=at(4);
    const auto reply=[&](std::uint8_t result) {
        std::vector<std::byte> ack;wire::WritePOD(ack,result);wire::WritePOD(ack,s.dwGold);
        wire::WritePOD(ack,s.dwSilver);wire::WritePOD(ack,s.dwCooper);
        packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_ITEMSELL_ACK),std::move(ack));
    };
    if(s.payload->personal_store||inventory==254)return false;
    if(s.payload->deal_status>=2){reply(2);return false;}
    if(s.payload->tournament_player)return false;
    if(s.wMapID>=500&&s.wMapID<533)throw std::runtime_error("ITEMSELL requires native tournament participant authority");
    const auto find=[&](std::uint8_t bag_id,std::uint8_t position)->const ItemInstance* {
        for(const auto& bag:s.payload->bags)if(bag.bag.bInvenID==bag_id)
            for(const auto& item:bag.items)if(item.bItemID==position)return &item;
        return nullptr;
    };
    const auto item=find(inventory,slot);
    if(!item||!count||item->bCount<count){reply(1);return false;}
    if(!item->source)throw std::runtime_error("Sold item requires its original projection");
    const auto original=*item;const int id=std::bit_cast<std::int16_t>(item->wItemID);soci::row chart;
    sql << "SELECT * FROM character_compat.\"TITEMCHART\" WHERE \"wItemID\"=:id",soci::use(id),soci::into(chart);
    if(!sql.got_data())throw std::runtime_error("Sold item source template disappeared");
    if(!(chart.get<int>("bIsSell")&2)){reply(3);return false;}
    if(call_inventory!=252&&call_slot!=255) {
        if(s.wMapID!=0&&s.wMapID!=8){reply(5);return false;}
        const auto caller=find(call_inventory,call_slot);
        if(!caller||caller->bKind!=55||!caller->bCount){reply(4);return false;}
        // The original can delete pItem and then dereference it, or underflow
        // its count when selling the very last NPC-call item. Reject that case.
        if(caller->dlID==original.dlID&&caller->bCount<=count){reply(4);return false;}
        DebitNativeItem(sql,c,s,call_inventory,call_slot,caller->dlID,1,packets);
    }
    auto money=static_cast<std::uint32_t>((ItemPrice(sql,original,chart)/4U)*count);
    if(!SupportedPostures(*s.payload))throw std::runtime_error("Sale requires supported maintained effects");
    // HaveItemProbBuff(PRICEUP65) returns the first nonzero raw wValue, not a
    // recalculated skill magnitude. Other status types never alter sale price.
    for(const auto& effect:*s.payload->effects) {
        const auto value=NativeItemBuffValue(sql,effect.skill,65);
        if(value&&*value) {
            money+=static_cast<std::uint32_t>(money*(*value))/100U;break;
        }
    }
    EarnNativeMoney(sql,c,s,money,packets);
    DebitNativeItem(sql,c,s,inventory,slot,original.dlID,count,packets);
    reply(0);sold=original.wItemID;
    // No separate MONEY_ACK, USEITEM event, NPC identifier, range, dead or wrap
    // check exists in this original request. GETITEM progress follows this ACK.
    return true;
}
bool PostgreSQLMapService::RepairNativeItems(soci::session& sql,const MapSessionClaim& c,CharSnapshot& s,
    std::span<const std::byte> body,QuestPackets& packets) {
    using tnetlib::protocol::MessageId;
    wire::Reader reader(body.data(),body.size());
    std::uint8_t quote=0,type=0,inventory=0,slot=0,call_inventory=0,call_slot=0;std::uint16_t npc=0;
    if(!reader.Read(quote)||!reader.Read(type)||!reader.Read(inventory)||!reader.Read(slot)||!reader.Read(npc)||
       !reader.Read(call_inventory)||!reader.Read(call_slot)||!reader.Eof())
        throw std::runtime_error("Invalid DURATIONREP request");
    if(s.payload->security.Blocks()) {
        const auto prompt=SecureUnlockPrompt(s.payload->security);packets.emplace_back(prompt.first,prompt.second);return false;
    }
    const int npc_id=std::bit_cast<std::int16_t>(npc);int condition=0,rate=0,local=0;
    sql << "SELECT \"bCondition\",\"bDiscountRate\",\"wLocalID\" FROM character_compat.\"TNPCCHART\" WHERE \"wID\"=:id",
        soci::use(npc_id),soci::into(condition),soci::into(rate),soci::into(local);
    const auto discount=sql.got_data()?NpcDiscount(condition,rate,local):std::uint8_t{0};
    // Native admission currently refuses account premium/PCBang. Never infer
    // an absent PCBANG_REAL flag from an unhydrated or incompatible graph.
    if(!s.payload->during||(s.payload->transfer_state&&s.payload->transfer_state->pc_bang))
        throw std::runtime_error("Repair requires admitted native entitlement authority");
    std::vector<ItemInstance> targets;
    const auto reply=[&](std::uint8_t result) {
        if(targets.size()>255)throw std::runtime_error("Repair response exceeds the original BYTE item count");
        std::vector<std::byte> ack;wire::WritePOD(ack,result);wire::WritePOD(ack,static_cast<std::uint8_t>(targets.size()));
        // SendCS_DURATIONREP_ACK pops the original ascending map traversal.
        for(auto it=targets.rbegin();it!=targets.rend();++it) {
            wire::WritePOD(ack,it->bInvenID);wire::WritePOD(ack,it->bItemID);
            wire::WritePOD(ack,it->dwDuraMax);wire::WritePOD(ack,it->dwDuraCur);
        }
        packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_DURATIONREP_ACK),std::move(ack));
    };
    const auto bag=[&](std::uint8_t id)->const CharacterBag* {
        for(const auto& row:s.payload->bags)if(row.bag.bInvenID==id)return &row;
        return nullptr;
    };
    const auto find=[&](std::uint8_t id,std::uint8_t position)->const ItemInstance* {
        if(const auto* row=bag(id))for(const auto& item:row->items)if(item.bItemID==position)return &item;
        return nullptr;
    };
    const auto chart_for=[&](const ItemInstance& item) {
        const int id=std::bit_cast<std::int16_t>(item.wItemID);soci::row chart;
        sql << "SELECT * FROM character_compat.\"TITEMCHART\" WHERE \"wItemID\"=:id",soci::use(id),soci::into(chart);
        if(!sql.got_data())throw std::runtime_error("Repaired item source template disappeared");
        return chart;
    };
    std::uint32_t cost=0;
    if(type==0) { // RPT_NORMAL
        const auto* item=find(inventory,slot);
        if(!item||!item->bCount){reply(1);return false;}
        const auto chart=chart_for(*item);
        if(!chart.get<int>("bCanRepair")){reply(3);return false;}
        if(item->dwDuraCur>=item->dwDuraMax){reply(1);return false;}
        cost=RepairCost(sql,*item,chart);targets.push_back(*item);
    }else if(type==1||type==2) { // RPT_EQUIP / RPT_ALL
        if(type==1&&!bag(254)){reply(1);return false;}
        for(const auto& row:s.payload->bags)if(type==2||row.bag.bInvenID==254)
            for(const auto& item:row.items)if(item.dwDuraMax&&item.dwDuraCur<item.dwDuraMax) {
                const auto chart=chart_for(item);
                if(!chart.get<int>("bCanRepair"))continue;
                cost+=RepairCost(sql,item,chart);targets.push_back(item);
            }
        std::sort(targets.begin(),targets.end(),[](const auto& a,const auto& b){
            return a.bInvenID<b.bInvenID||(a.bInvenID==b.bInvenID&&a.bItemID<b.bItemID);
        });
    }
    if(targets.empty()){reply(1);return false;} // includes unknown source repair type
    if(targets.size()>255)throw std::runtime_error("Repair response exceeds the original BYTE item count");
    if(quote) {
        std::vector<std::byte> ack;wire::WritePOD(ack,cost);wire::WritePOD(ack,discount);
        packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_DURATIONREPCOST_ACK),std::move(ack));return false;
    }
    const auto discounted=cost-static_cast<std::uint32_t>(cost*discount)/100U;
    if(!UseNativeMoney(sql,c,s,discounted,false,packets)){reply(2);return false;}
    const bool title_check=discounted!=0;
    if(call_inventory!=252&&call_slot!=255) {
        if(s.wMapID!=0&&s.wMapID!=8){reply(8);return title_check;}
        const auto* caller=find(call_inventory,call_slot);
        if(!caller||caller->bKind!=55||!caller->bCount){reply(7);return title_check;}
        // Reject the source use-after-free if the only NPC-call item were also
        // in the repair list. Keep valid repairable stacks with a survivor.
        if(caller->bCount==1&&std::any_of(targets.begin(),targets.end(),[&](const auto& item){return item.dlID==caller->dlID;})) {
            reply(7);return title_check;
        }
        DebitNativeItem(sql,c,s,call_inventory,call_slot,caller->dlID,1,packets);
    }
    if(!UseNativeMoney(sql,c,s,discounted,true,packets))throw std::runtime_error("Repair purse changed during its transaction");
    packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_MONEY_ACK),EncodeMoneyAck(s.dwGold,s.dwSilver,s.dwCooper));
    auto p=std::make_shared<CharacterPayload>(*s.payload);s.payload=p;
    bool equipment_changed=false;
    for(auto& target:targets) {
        auto row=std::find_if(p->bags.begin(),p->bags.end(),[&](const auto& b){return b.bag.bInvenID==target.bInvenID;});
        if(row==p->bags.end())throw std::runtime_error("Repaired inventory disappeared");
        auto item=std::find_if(row->items.begin(),row->items.end(),[&](const auto& i){return i.bItemID==target.bItemID&&i.dlID==target.dlID;});
        if(item==row->items.end()||!item->source)throw std::runtime_error("Repaired item identity changed");
        auto raw=*item->source;raw.durability=raw.durability_max;std::string hash;
        if(!p->transfer_state) {
            const int world=c.group,owner=std::bit_cast<std::int32_t>(c.char_id),durability=std::bit_cast<std::int32_t>(raw.durability);
            const long long id=std::bit_cast<long long>(raw.id);
            sql << "UPDATE app_world.\"TITEMTABLE\" i SET \"dwDuraCur\"=:dura WHERE \"bWorldID\"=:w AND \"dwOwnerID\"=:c AND \"dlID\"=:id "
                "AND \"bOwnerType\"=0 AND \"bStorageType\"=0 AND app_world.item_fingerprint(i)=:hash RETURNING app_world.item_fingerprint(i)",
                soci::use(durability,"dura"),soci::use(world,"w"),soci::use(owner,"c"),soci::use(id,"id"),soci::use(item->durable_hash,"hash"),soci::into(hash);
            if(!sql.got_data())throw std::runtime_error("Repaired item ownership changed");
        }else hash=GraphItemFingerprint(raw);
        equipment_changed|=target.bInvenID==254&&!target.dwDuraCur;
        const auto alarm=item->alarm_time;
        *item=ProjectNativeItem(sql,raw);item->durable_hash=hash;item->alarm_time=alarm;target=*item;
    }
    // Source stat getters read IsPower() dynamically. Refresh the native cache
    // after restoring a broken equipped item without inventing extra packets.
    if(equipment_changed)RefreshEquipment(sql,s,true);
    reply(0);
    return true;
}
}
