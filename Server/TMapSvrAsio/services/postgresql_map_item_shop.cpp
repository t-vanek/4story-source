#include "postgresql_map_service.h"
#include "native_skill_state.h"
#include "native_npc_discount.h"
#include "native_quest_magic_item.h"
#include "quest_conditions.h"
#include "secure_code.h"
#include "MessageId.h"
#include <openssl/rand.h>
#include <algorithm>
#include <bit>
#include <cmath>
#include <map>
#include <set>

namespace tmapsvr {
namespace {
using tnetlib::protocol::MessageId;
struct Npc {std::uint16_t map{},local{};std::uint8_t type{},country{},condition{},discount{};};
std::optional<Npc> ReadNpc(soci::session& sql,std::uint16_t npc) {
    const int id=std::bit_cast<std::int16_t>(npc);soci::row row;
    sql << "SELECT \"wMapID\",\"wLocalID\",\"bType\",\"bCountryID\",\"bCondition\",\"bDiscountRate\" "
        "FROM character_compat.\"TNPCCHART\" WHERE \"wID\"=:id",soci::use(id),soci::into(row);
    if(!sql.got_data())return {};
    return Npc{static_cast<std::uint16_t>(row.get<int>(0)),static_cast<std::uint16_t>(row.get<int>(1)),
        static_cast<std::uint8_t>(row.get<int>(2)),static_cast<std::uint8_t>(row.get<int>(3)),
        static_cast<std::uint8_t>(row.get<int>(4)),static_cast<std::uint8_t>(row.get<int>(5))};
}
void RequireMerchandise(soci::session& sql,bool magic) {
    int compatible=0;
    sql << "SELECT count(*) FROM character_compat.trainer_release r JOIN character_compat.catalog_release c "
        "ON c.manifest_sha256=r.character_manifest AND c.status='verified' JOIN actor_compat.catalog_release a "
        "ON a.manifest_sha256=r.actor_manifest AND a.status='verified' WHERE r.status='verified'",soci::into(compatible);
    if(compatible!=1)throw std::runtime_error("NPC merchandise requires complete compatible source publication");
    if(magic) {
        sql << "SELECT count(*) FROM game_compat.quest_reward_release r JOIN game_compat.catalog_release m "
            "ON m.manifest_sha256=r.map_manifest AND m.status='verified' JOIN character_compat.catalog_release c "
            "ON c.manifest_sha256=r.character_manifest AND c.status='verified' JOIN actor_compat.catalog_release a "
            "ON a.manifest_sha256=r.actor_manifest AND a.status='verified' WHERE r.status='verified'",soci::into(compatible);
        if(compatible!=1)throw std::runtime_error("Magic shop requires published source quest-item templates");
    }
}
std::set<std::uint32_t> Merchandise(soci::session& sql,std::uint16_t npc,bool magic) {
    const int id=std::bit_cast<std::int16_t>(npc);std::set<std::uint32_t> out;
    soci::rowset<int> rows=(sql.prepare << "SELECT \"dwItemID\" FROM character_compat.\"TNPCITEMCHART\" WHERE \"wNpcID\"=:id",soci::use(id));
    for(int value:rows)out.insert(magic?static_cast<std::uint32_t>(value):static_cast<std::uint16_t>(value));
    return out; // source maps deduplicate and order unsigned WORD/DWORD keys
}
std::uint32_t LevelMoney(soci::session& sql,int level,bool pvp) {
    const std::string field=pvp?"dwPvPMoney":"dwMoney";int money=0;
    sql << "SELECT \""+field+"\" FROM character_compat.\"TLEVELCHART\" WHERE \"bLevel\"=:level",soci::use(level),soci::into(money);
    if(!sql.got_data()) {
        sql << "SELECT \""+field+"\" FROM character_compat.\"TLEVELCHART\" ORDER BY \"bLevel\" LIMIT 1",soci::into(money);
        if(!sql.got_data())throw std::runtime_error("NPC price has no source level fallback");
    }
    return static_cast<std::uint32_t>(money);
}
std::uint32_t Price(float product,bool round) {
    const double value=static_cast<double>(product)+(round?.99:0.0);
    if(!std::isfinite(value)||std::trunc(value)<0||std::trunc(value)>4294967295.0)
        throw std::runtime_error("NPC price has undefined source DWORD conversion");
    return static_cast<std::uint32_t>(value);
}
std::uint32_t ItemPrice(soci::session& sql,const soci::row& item,bool pvp) {
    int level=0;const int attr=item.get<int>("wAttrID");
    if(attr) {
        sql << "SELECT \"bGrade\" FROM actor_compat.\"TITEMATTRCHART\" WHERE \"wID\"=:id",soci::use(attr),soci::into(level);
        // GetItemPrice deliberately uses grade0 when the base attribute is absent.
    }else level=item.get<int>("bLevel");
    return Price(static_cast<float>(LevelMoney(sql,level,pvp))*static_cast<float>(item.get<double>(pvp?"fPvPrice":"fPrice")),true);
}
std::optional<soci::row> ReadItem(soci::session& sql,std::uint16_t item) {
    const int id=std::bit_cast<std::int16_t>(item);soci::row row;
    sql << "SELECT * FROM character_compat.\"TITEMCHART\" WHERE \"wItemID\"=:id",soci::use(id),soci::into(row);
    if(!sql.got_data())return {};
    return row;
}
bool Listed(const CharSnapshot& s,const soci::row& item,const Npc& npc) {
    if(s.bClass>=32)throw std::runtime_error("NPC merchandise class shift is outside source range");
    return (static_cast<std::uint32_t>(item.get<int>("dwClassID"))&(std::uint32_t{1}<<s.bClass))&&
        (item.get<int>("bItemCountry")==3||npc.condition==4||item.get<int>("bItemCountry")==s.bCountry);
}
std::vector<std::pair<std::uint16_t,std::uint32_t>> TrainerOffers(soci::session& sql,const CharSnapshot& s,const std::set<std::uint32_t>& ids) {
    std::map<std::pair<std::uint16_t,std::uint8_t>,std::pair<std::uint8_t,std::uint8_t>> points;
    {soci::rowset<soci::row> rows=(sql.prepare << "SELECT \"wID\",\"bLevel\",\"bSkillPoint\",\"bGroupPoint\" FROM actor_compat.\"TSKILLPOINTCHART\"");
     for(const auto& row:rows)points.emplace(std::make_pair(static_cast<std::uint16_t>(row.get<int>(0)),static_cast<std::uint8_t>(row.get<int>(1))),
         std::make_pair(static_cast<std::uint8_t>(row.get<int>(2)),static_cast<std::uint8_t>(row.get<int>(3))));}
    std::map<std::uint8_t,std::uint16_t> used;
    for(const auto& skill:s.payload->skills) {
        const auto def=std::find_if(s.payload->skill_templates.begin(),s.payload->skill_templates.end(),[&](const auto& t){return t.wID==skill.wSkillID;});
        if(def==s.payload->skill_templates.end())throw std::runtime_error("Trainer lost learned skill definition");
        for(unsigned rank=1;rank<=skill.bLevel;++rank) {
            const auto i=points.find({skill.wSkillID,static_cast<std::uint8_t>(rank)});
            if(i!=points.end())used[def->bKind]=static_cast<std::uint16_t>(used[def->bKind]+i->second.first);
        }
    }
    std::vector<std::pair<std::uint16_t,std::uint32_t>> out;
    for(auto value:ids) {
        const auto id=static_cast<std::uint16_t>(value);
        if(!NativePlayerSkillAvailable(*s.payload,id))continue;
        const int key=std::bit_cast<std::int16_t>(id);soci::row row;
        sql << "SELECT \"wParentSkillID\",\"bLevel\",\"bNextLevel\",\"bMaxLevel\",\"bKind\",\"dwClassID\",\"fPrice\" "
            "FROM character_compat.\"TSKILLCHART\" WHERE \"wID\"=:id",soci::use(key),soci::into(row);
        if(!sql.got_data())throw std::runtime_error("Available trainer skill missing from pinned character catalog");
        const auto mine=std::find_if(s.payload->skills.begin(),s.payload->skills.end(),[&](const auto& skill){return skill.wSkillID==id;});
        const bool learned=mine!=s.payload->skills.end();const auto rank=static_cast<std::uint8_t>(learned?mine->bLevel+1:1);
        const auto need=points.find({id,rank});const auto cost=need==points.end()?std::pair<std::uint8_t,std::uint8_t>{}:need->second;
        if(s.wSkillPoint<cost.first||used[static_cast<std::uint8_t>(row.get<int>("bKind"))]<cost.second)continue;
        const auto level=static_cast<std::uint8_t>(row.get<int>("bLevel")+(learned?mine->bLevel*row.get<int>("bNextLevel"):0));
        if(learned) {if(row.get<int>("bMaxLevel")<=mine->bLevel||s.bLevel<level)continue;}
        else {
            if(s.bClass>=32)throw std::runtime_error("Trainer class shift out of range");
            const auto parent=static_cast<std::uint16_t>(row.get<int>("wParentSkillID"));
            if(!(static_cast<std::uint32_t>(row.get<int>("dwClassID"))&(std::uint32_t{1}<<s.bClass))||
               (parent&&std::none_of(s.payload->skills.begin(),s.payload->skills.end(),[&](const auto& skill){return skill.wSkillID==parent;})))continue;
            // Source listing does not require start-level or parent rank for a new skill.
        }
        out.emplace_back(id,Price(static_cast<float>(LevelMoney(sql,level,false))*static_cast<float>(row.get<double>("fPrice")),false));
    }
    return out;
}
}

void PostgreSQLMapService::ListNativeNpcItems(soci::session& sql,const CharSnapshot& s,std::uint16_t id,QuestPackets& packets) const {
    const auto npc=ReadNpc(sql,id);if(!npc||!NativeNpcCanTalk(sql,s,npc->country,npc->local))return;
    if(npc->map==3000) {
        if(s.wMapID!=3000)return;
        throw std::runtime_error("BOW shop requires the BOW compile-mode currency contract");
    }
    const auto discount=NpcDiscount(npc->condition,npc->discount,npc->local);
    std::vector<std::byte> ack;wire::WritePOD(ack,id);wire::WritePOD(ack,npc->type);wire::WritePOD(ack,discount);
    if(npc->type==1||npc->type==19||npc->type==2||npc->type==21||npc->type==25) {
        RequireMerchandise(sql,npc->type==25);const auto ids=Merchandise(sql,id,npc->type==25);
        const auto count_pos=ack.size();wire::WritePOD(ack,std::uint8_t{});unsigned count=0;
        if(npc->type==1||npc->type==19) {
            for(const auto& [skill,price]:TrainerOffers(sql,s,ids)){wire::WritePOD(ack,skill);wire::WritePOD(ack,price);++count;}
        }else for(auto value:ids) {
            if(npc->type==25) {
                auto magic=ReadQuestMagicItem(sql,value);if(!magic)continue;
                const auto item=ReadItem(sql,magic->item.item);if(!item||!Listed(s,*item,*npc))continue;
                wire::WritePOD(ack,value);wire::WritePOD(ack,magic->price);
                // Source temporarily adds days +59 minutes only for display.
                long long now=0;sql << "SELECT floor(extract(epoch FROM clock_timestamp()))::bigint",soci::into(now);
                magic->item.expires=now+std::int64_t{magic->days}*86400+3540;
                const auto descriptor=EncodeItemDescriptor(ProjectNativeItem(sql,magic->item,true),s.dwCharID,false);
                ack.insert(ack.end(),descriptor.begin(),descriptor.end());++count;
            }else {
                const auto item=ReadItem(sql,static_cast<std::uint16_t>(value));if(!item||!Listed(s,*item,*npc))continue;
                wire::WritePOD(ack,static_cast<std::uint16_t>(value));wire::WritePOD(ack,ItemPrice(sql,*item,npc->type==21));++count;
            }
        }
        if(count>255)throw std::runtime_error("NPC listing exceeds source BYTE count");
        ack[count_pos]=static_cast<std::byte>(count);
    }else if(npc->type==8||npc->type==22)throw std::runtime_error("NPC portal/monster shop requires its remaining runtime contract");
    // Other source NPC types deliberately emit only this four-byte header.
    packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_NPCITEMLIST_ACK),std::move(ack));
}

bool PostgreSQLMapService::BuyNativeItem(soci::session& sql,const MapSessionClaim& c,CharSnapshot& s,bool magic,
    std::span<const std::byte> body,const IQuestChart& chart,const QuestEvent& event,QuestPackets& packets) const {
    wire::Reader reader(body.data(),body.size());std::uint16_t npc_id=0,item_id=0;std::uint32_t quest_id=0;
    std::uint8_t count=0,caller_bag=0,caller_slot=0;
    if(!reader.Read(npc_id)||!reader.Read(quest_id)||(!magic&&!reader.Read(item_id))||!reader.Read(count)||
       !reader.Read(caller_bag)||!reader.Read(caller_slot)||!reader.Eof())throw std::runtime_error("Invalid NPC purchase request");
    if(s.payload->security.Blocks()) {const auto prompt=SecureUnlockPrompt(s.payload->security);packets.emplace_back(prompt.first,prompt.second);return false;}
    const auto reply=[&](std::uint8_t result,bool caller_error=false) {
        std::vector<std::byte> ack;wire::WritePOD(ack,result);if(!magic)wire::WritePOD(ack,caller_error?std::uint16_t{}:item_id);
        wire::WritePOD(ack,s.dwGold);wire::WritePOD(ack,s.dwSilver);wire::WritePOD(ack,s.dwCooper);
        packets.emplace_back(static_cast<std::uint16_t>(magic?MessageId::CS_MAGICITEMBUY_ACK:MessageId::CS_ITEMBUY_ACK),std::move(ack));
    };
    if(s.payload->deal_status>=2){reply(4);return false;}
    if(s.payload->tournament_player)return false;
    if(s.wMapID>=500&&s.wMapID<533)throw std::runtime_error("NPC purchase requires native tournament participant authority");
    const auto npc=ReadNpc(sql,npc_id);if(!npc||!count){reply(1);return false;}
    if(npc->map==3000) {
        // A client on an ordinary map must not select a remote BOW NPC to
        // activate the non-BOW source's no-payment branch. The actual BOW
        // process spends bonus points and still needs that native authority.
        if(s.wMapID!=3000){reply(1);return false;}
        throw std::runtime_error("BOW shop requires the BOW compile-mode currency contract");
    }
    // MAGICITEMBUY intentionally has no CanTalk call in the original.
    if(!magic&&!NativeNpcCanTalk(sql,s,npc->country,npc->local))return false;
    if(magic||!quest_id)RequireMerchandise(sql,magic);
    std::optional<QuestMagicItem> template_item;
    if(magic) {
        if(npc->type==25&&Merchandise(sql,npc_id,true).contains(quest_id))template_item=ReadQuestMagicItem(sql,quest_id);
        if(!template_item){reply(1);return false;}item_id=template_item->item.item;
    }else if(quest_id) {
        const auto* quest=chart.Find(quest_id);
        // Reject the source free-purchase exploit: an unknown nonzero quest
        // falls back to NPC merchandise but still suppresses payment there.
        if(!quest){reply(1);return false;}
        auto p=std::make_shared<CharacterPayload>(*s.payload);s.payload=p;
        quest_runtime::ConditionInputs inputs;inputs.probability=[] {
            unsigned char bytes[2];if(RAND_bytes(bytes,2)!=1)throw std::runtime_error("Quest probability unavailable");
            return static_cast<std::uint8_t>((((unsigned(bytes[0])<<8)|bytes[1])&32767U)%100U);
        };
        std::uint8_t level=0;
        if(quest_runtime::CanRun(s,*p,*quest,chart,inputs,level)||std::none_of(quest->terms.begin(),quest->terms.end(),[&](const auto& term){
            return term.bTermType==QTT_ITEMID&&term.dwTermID==item_id;})){reply(1);return false;}
    }else if((npc->type!=2&&npc->type!=21)||!Merchandise(sql,npc_id,false).contains(item_id)){reply(1);return false;}
    const auto item=ReadItem(sql,item_id);if(!item){reply(1);return false;}
    count=std::min(count,static_cast<std::uint8_t>(item->get<int>("bStack")));
    if(!count)throw std::runtime_error("NPC purchase has zero source stack capacity");
    const bool pvp=!magic&&!quest_id&&npc->map!=3000&&npc->type==21;
    const bool pay=magic||(!quest_id&&npc->map!=3000);
    std::uint32_t price=0;
    if(pay) {
        price=(magic?template_item->price:ItemPrice(sql,*item,pvp))*count;
        price-=static_cast<std::uint32_t>(price*NpcDiscount(npc->condition,npc->discount,npc->local))/100U;
        if(pvp) {if(!s.payload->pvp)throw std::runtime_error("PvP wallet unavailable");if(s.payload->pvp->available<price){reply(2);return false;}}
        else if(!UseNativeMoney(sql,c,s,price,false,packets)){reply(2);return false;}
    }
    transfer::Item raw;
    if(magic) {
        raw=template_item->item;long long now=0;
        sql << "SELECT floor(extract(epoch FROM clock_timestamp()))::bigint",soci::into(now);
        raw.expires=now+std::int64_t{template_item->days}*86400;
        // Source Copy retains template count1, even though price uses request count.
    }else {
        raw.item=item_id;raw.count=count;
        raw.durability=raw.durability_max=static_cast<std::uint32_t>(item->get<int>("dwDuraMax"));
        raw.expires=NativeItemExpiry(sql,0,item->get<int>("bUseType"),item->get<int>("wUseTime"));
    }
    raw.id=AllocateNativeItem(sql,c);raw.owner_id=c.char_id;
    const std::vector<ItemInstance> items{ProjectNativeItem(sql,raw)};
    // Capacity is checked before the NPC-call item is consumed.
    if(!CanGrantQuestItems(*s.payload,items)){reply(3);return true;}
    bool equipped_caller=false;
    if(caller_bag!=252&&caller_slot!=255) {
        if(s.wMapID!=0&&s.wMapID!=8){reply(6,true);return true;}
        const ItemInstance* caller=nullptr;
        for(const auto& bag:s.payload->bags)if(bag.bag.bInvenID==caller_bag)
            for(const auto& entry:bag.items)if(entry.bItemID==caller_slot)caller=&entry;
        if(!caller||caller->bKind!=55||!caller->bCount){reply(5,true);return true;}
        const auto identity=caller->dlID;equipped_caller=caller_bag==254;
        DebitNativeItem(sql,c,s,caller_bag,caller_slot,identity,1,packets);
    }
    if(!GrantQuestItems(sql,c,s,items,packets))throw std::runtime_error("NPC purchase placement changed after validation");
    if(pay) {
        if(pvp)SpendNativePvp(sql,c,s,price,packets);
        else if(!UseNativeMoney(sql,c,s,price,true,packets))throw std::runtime_error("NPC purchase purse changed after validation");
    }
    if(equipped_caller)RefreshEquipment(sql,s,true);
    reply(0);
    if(!magic)event(item_id,QTT_GETITEM,TT_GETITEM,count);
    return true;
}
}
