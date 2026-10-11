#include "postgresql_map_service.h"
#include "native_npc_discount.h"
#include "native_item_power.h"
#include "legacy_random.h"
#include "domain/main_transfer.h"
#include "MessageId.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <optional>

namespace tmapsvr {
namespace {
using tnetlib::protocol::MessageId;
enum Result : std::uint8_t { Success, InvalidItem, NeedMoney, Fail, ReplaceFail, Full, InvalidPosition };

const ItemInstance* FindItem(const CharSnapshot& s,std::uint8_t bag,std::uint8_t slot) {
    for(const auto& inventory:s.payload->bags)if(inventory.bag.bInvenID==bag)
        for(const auto& item:inventory.items)if(item.bItemID==slot)return &item;
    return nullptr;
}
bool HasBlank(const CharacterPayload& p) {
    for(const auto& bag:p.bags)if(bag.bag.bInvenID!=254)
        for(unsigned slot=0;slot<bag.slot_count;++slot)
            if(std::none_of(bag.items.begin(),bag.items.end(),[&](const auto& item){return item.bItemID==slot;}))return true;
    return false;
}
std::optional<soci::row> ItemChart(soci::session& sql,std::uint16_t item) {
    const int id=std::bit_cast<std::int16_t>(item);soci::row row;
    sql << "SELECT * FROM character_compat.\"TITEMCHART\" WHERE \"wItemID\"=:id",soci::use(id),soci::into(row);
    if(!sql.got_data())return std::nullopt;
    return row;
}
struct Npc {bool exists{};int condition{},discount{},local{},bonus{};};
Npc ReadNpc(soci::session& sql,std::uint16_t npc) {
    Npc out;const int id=std::bit_cast<std::int16_t>(npc);
    sql << "SELECT \"bCondition\",\"bDiscountRate\",\"wLocalID\",\"bAddProb\" FROM character_compat.\"TNPCCHART\" WHERE \"wID\"=:id",
        soci::use(id),soci::into(out.condition),soci::into(out.discount),soci::into(out.local),soci::into(out.bonus);
    out.exists=sql.got_data();return out;
}
std::optional<std::pair<std::uint32_t,std::uint32_t>> LevelCosts(soci::session& sql,std::uint8_t grade) {
    const int level=grade;int gamble=0,repair=0;
    sql << "SELECT \"dwGambleCost\",\"dwRepCost\" FROM character_compat.\"TLEVELCHART\" WHERE \"bLevel\"=:level",
        soci::use(level),soci::into(gamble),soci::into(repair);
    if(!sql.got_data()) {
        sql << "SELECT \"dwGambleCost\",\"dwRepCost\" FROM character_compat.\"TLEVELCHART\" ORDER BY \"bLevel\" LIMIT 1",
            soci::into(gamble),soci::into(repair);
        if(!sql.got_data())return std::nullopt;
    }
    return std::pair{static_cast<std::uint32_t>(gamble),static_cast<std::uint32_t>(repair)};
}
std::optional<std::uint32_t> BaseCost(const soci::row& chart,const transfer::Item& raw,
    const std::pair<std::uint32_t,std::uint32_t>& costs,bool quote) {
    if(chart.get<int>("bType")==13&&!chart.get<int>("bCanGamble"))return costs.first; // IT_GAMBLE
    if(chart.get<int>("bCanGamble")&&raw.grade)return costs.second;
    if(quote&&(raw.wrap||raw.appearance))return 0;
    return std::nullopt;
}
std::uint32_t Discounted(std::uint32_t cost,std::uint8_t rate) {
    return cost-static_cast<std::uint32_t>(cost*rate)/100U; // source DWORD product wraps before /100
}
void RequireCatalog(soci::session& sql) {
    int available=0;
    sql << "SELECT count(*) FROM game_compat.item_gambling_release r "
        "JOIN game_compat.catalog_release m ON m.manifest_sha256=r.map_manifest AND m.status='verified' "
        "JOIN character_compat.catalog_release c ON c.manifest_sha256=r.character_manifest AND c.status='verified' "
        "JOIN actor_compat.catalog_release a ON a.manifest_sha256=r.actor_manifest AND a.status='verified' WHERE r.status='verified'",
        soci::into(available);
    if(available!=1)throw std::runtime_error("Item gambling requires the complete published source catalog");
}
struct Magic {
    std::uint8_t id{},exclusive{};
    std::uint32_t kinds{};
    std::uint16_t bound{};
    bool magic{},rare{},refine{};
};
std::vector<Magic> MagicChart(soci::session& sql,bool upgrades=false) {
    std::vector<Magic> result;
    // CTBLItemMagicChart has no ORDER BY. Explicit archive extract order is a
    // documented native ordering policy; do not invent sorting by magic ID for
    // the source per-kind vectors. Catalyzer independently uses the sorted map.
    const auto query=std::string{"SELECT * FROM game_compat."}+
        (upgrades?"item_upgrade_magic":"item_gambling_magic")+" ORDER BY \"_source_row_number\"";
    soci::rowset<soci::row> rows=(sql.prepare << query);
    for(const auto& row:rows)result.push_back({static_cast<std::uint8_t>(row.get<int>("bMagic")),
        static_cast<std::uint8_t>(row.get<int>("bExclIndex")),static_cast<std::uint32_t>(row.get<int>("dwKind")),
        static_cast<std::uint16_t>(row.get<int>("wRareBound")),row.get<int>("bIsMagic")!=0,row.get<int>("bIsRare")!=0,row.get<int>("bRefine")!=0});
    return result;
}
std::uint16_t MagicValue(soci::session& sql,int base,std::uint16_t bound) {
    if(!bound)return 0;
    const auto r1=LegacyRandom15(),r2=LegacyRandom15(),r3=LegacyRandom15();
    double stored=0;
    sql << "SELECT \"fRateX\" FROM character_compat.\"TFORMULACHART\" WHERE \"bID\"=34",soci::into(stored);
    if(!sql.got_data())throw std::runtime_error("Item magic source formula missing");
    const auto rate=static_cast<float>(stored);
    const auto level=std::min(80,std::max(1,base-int(r1%10U))+std::max(0,int(r2%1000U)-989));
    const double range=bound*(97*std::pow(double(rate),level)/std::pow(double(rate),80)+3)/100;
    // The original WORD conversion/modulo is undefined outside this domain.
    // Never silently replace a zero or invalid divisor with a guessed value.
    if(!std::isfinite(range)||range<1||range>=65536)
        throw std::runtime_error("Item magic source divisor is undefined");
    return static_cast<std::uint16_t>(1U+r3%static_cast<std::uint16_t>(range));
}
bool MakeSpecialMagic(soci::session& sql,const CharSnapshot& s,transfer::Item& raw,const soci::row& item,
    bool rare=false,bool buff=false,int base=0,bool upgrades=false) {
    // OnCS_GAMBLEOPEN_REQ passes IMT_GAMBLE as argument FOUR (wMagicBuff),
    // leaving argument FIVE bMakeType=IMT_NONE. Thus SetMagicOpt uses base=0,
    // not the sealed item's grade. Preserve the actual source call contract.
    const auto old_count=raw.magic.size();
    if(raw.level||(!rare&&old_count)||(rare&&(!old_count||old_count>2)))return false;
    const unsigned minimum=rare?3:1,maximum=rare?5:2;
    const auto count_limit=std::clamp<unsigned>(static_cast<std::uint8_t>(s.bLevel*0.1),minimum,maximum);
    auto count=std::max<unsigned>(LegacyRandom15()%count_limit+1U,minimum);
    if(rare)count=buff?5-old_count:count>old_count?count-old_count:0;
    const int kind=item.get<int>("bKind");
    const auto chart=MagicChart(sql,upgrades);
    for(unsigned n=0;n<count;++n) {
        if(!item.get<int>("bCanMagic")||(rare&&!item.get<int>("bCanRare"))||kind<1||kind>=26)continue; // source vectors only IK1..25
        std::vector<const Magic*> candidates;
        for(const auto& option:chart)if((rare?option.rare:option.magic)&&(option.kinds&(std::uint32_t{1}<<(kind-1)))&&
            std::none_of(raw.magic.begin(),raw.magic.end(),[&](const auto& old){return old.id==option.id;}))candidates.push_back(&option);
        // Source new TITEMMAGIC() zero-initialises bMinLevel and LoadData never
        // copies the query's bMinLevel. The archival value is retained, unused.
        if(candidates.size()>255)throw std::runtime_error("Item magic exceeds source BYTE loop");
        const Magic* selected=nullptr;
        while(!candidates.empty()) {
            const auto index=static_cast<std::uint8_t>(LegacyRandom15()%candidates.size());
            selected=candidates[index];bool collision=false;
            if(selected->exclusive)for(const auto& old:raw.magic) {
                const auto def=std::find_if(chart.begin(),chart.end(),[&](const auto& row){return row.id==old.id;});
                if(def==chart.end())throw std::runtime_error("Item magic collision template missing");
                if(def->exclusive==selected->exclusive){collision=true;break;}
            }
            if(!collision)break;
            candidates.erase(candidates.begin()+index);selected=nullptr;
        }
        if(!selected)continue;
        const auto value=MagicValue(sql,base,selected->bound);
        if(value) {
            if(!selected->id)throw std::runtime_error("Source magic selected reserved ID zero");
            raw.magic.push_back({selected->id,value});
        }
    }
    std::sort(raw.magic.begin(),raw.magic.end(),[](const auto& a,const auto& b){return a.id<b.id;});
    return raw.magic.size()>old_count;
}
void Catalyzer(soci::session& sql,transfer::Item& raw) {
    auto chart=MagicChart(sql);
    std::erase_if(chart,[](const auto& magic){return !magic.id||!magic.refine;});
    std::sort(chart.begin(),chart.end(),[](const auto& a,const auto& b){return a.id<b.id;});
    constexpr std::array<unsigned,6> probabilities{100,50,20,10,5,1};
    for(const auto probability:probabilities)if(LegacyRandom15()%100U<probability) {
        if(chart.empty())throw std::runtime_error("Catalyzer source candidate list is empty");
        const auto index=static_cast<std::uint8_t>(LegacyRandom15()%chart.size());
        const auto selected=chart[index];const auto value=MagicValue(sql,70,selected.bound);
        if(value) {
            // std::map::insert in the source leaves an existing option intact,
            // but still removes the selected candidate after a nonzero draw.
            if(std::none_of(raw.magic.begin(),raw.magic.end(),[&](const auto& old){return old.id==selected.id;}))
                raw.magic.push_back({selected.id,value});
            chart.erase(chart.begin()+index);
        }
    }
    if(raw.magic.size()>6)throw std::runtime_error("Catalyzer exceeds durable source magic capacity");
    std::sort(raw.magic.begin(),raw.magic.end(),[](const auto& a,const auto& b){return a.id<b.id;});
}
struct Replacement {std::uint16_t item{},probability{};std::uint8_t count{};};
std::optional<Replacement> DrawReplacement(soci::session& sql,int type,int kind) {
    std::vector<Replacement> choices;std::uint16_t total=0;
    soci::rowset<soci::row> rows=(sql.prepare << "SELECT \"wReplaceID\",\"wProb\",\"bCountMax\" "
        "FROM game_compat.\"TGAMBLECHART\" WHERE \"bType\"=:type AND \"bKind\"=:kind ORDER BY \"_source_row_number\"",
        soci::use(type),soci::use(kind));
    for(const auto& row:rows) {
        choices.push_back({static_cast<std::uint16_t>(row.get<int>(0)),static_cast<std::uint16_t>(row.get<int>(1)),
            static_cast<std::uint8_t>(row.get<int>(2))});
        total=static_cast<std::uint16_t>(total+choices.back().probability);
    }
    if(choices.empty())return std::nullopt; // actual published empty group, not missing content
    if(!total)throw std::runtime_error("Gamble source probability divisor is zero");
    const auto selected=LegacyRandom15()%total;total=0;
    for(auto choice:choices) {
        total=static_cast<std::uint16_t>(total+choice.probability);
        if(total>=selected) { // original >= includes zero at the first boundary
            if(!choice.count)throw std::runtime_error("Gamble source count divisor is zero");
            choice.count=static_cast<std::uint8_t>(LegacyRandom15()%choice.count+1U);return choice;
        }
    }
    throw std::runtime_error("Gamble cumulative probability has no source result");
}
}

bool PostgreSQLMapService::MakeNativeScrollMagic(soci::session& sql,const CharSnapshot& s,
    transfer::Item& raw,bool rare,bool buff) {
    const auto item=ItemChart(sql,raw.item);
    if(!item)throw std::runtime_error("Scroll item source template disappeared");
    const auto projected=ProjectNativeItem(sql,raw);
    auto equip=static_cast<std::uint8_t>(item->get<int>("bLevel"));
    if(equip>raw.eld)equip=static_cast<std::uint8_t>(equip-static_cast<std::uint8_t>(raw.eld));
    const int base=std::min(70,int(s.bLevel))-std::max(0,34-int(std::max(equip,NativeItemPowerLevel(sql,projected,*item))));
    return MakeSpecialMagic(sql,s,raw,*item,rare,buff,base,true);
}

bool PostgreSQLMapService::GambleNativeItem(soci::session& sql,const MapSessionClaim& c,CharSnapshot& s,
    bool quote,std::span<const std::byte> body,const QuestMapFacts& facts,const QuestEvent& event,QuestPackets& packets) {
    wire::Reader reader(body.data(),body.size());
    std::uint8_t inventory=0,slot=0,call_inventory=252,call_slot=255;std::uint16_t npc_id=0;
    if(!reader.Read(inventory)||!reader.Read(slot)||!reader.Read(npc_id)||
       (!quote&&(!reader.Read(call_inventory)||!reader.Read(call_slot)))||!reader.Eof())
        throw std::runtime_error("Invalid GAMBLE request");
    if(s.payload->personal_store||s.payload->deal_status>=2)return false;
    const auto reply=[&](Result result,std::uint8_t bag,std::uint8_t position,const ItemInstance* item=nullptr,
        std::uint32_t cost=0,std::uint8_t discount=0) {
        std::vector<std::byte> ack;wire::WritePOD(ack,static_cast<std::uint8_t>(result));
        if(quote) {
            // Errors use SendCS_GAMBLECHECK_ACK's default zero fields.
            wire::WritePOD(ack,result==Success?bag:std::uint8_t{0});
            wire::WritePOD(ack,result==Success?position:std::uint8_t{0});
            wire::WritePOD(ack,cost);wire::WritePOD(ack,discount);
        } else {
            wire::WritePOD(ack,s.dwGold);wire::WritePOD(ack,s.dwSilver);wire::WritePOD(ack,s.dwCooper);
            wire::WritePOD(ack,bag);wire::WritePOD(ack,position);
            if(item) {auto descriptor=EncodeItemDescriptor(*item,c.char_id,false);ack.insert(ack.end(),descriptor.begin(),descriptor.end());}
        }
        packets.emplace_back(static_cast<std::uint16_t>(quote?MessageId::CS_GAMBLECHECK_ACK:MessageId::CS_GAMBLEOPEN_ACK),std::move(ack));
    };
    const auto* found=FindItem(s,inventory,slot);
    if(!found||!found->bCount){reply(InvalidItem,inventory,slot);return false;}
    const auto original=*found;
    if(!original.source)throw std::runtime_error("Gamble requires authoritative source item");
    const auto chart=ItemChart(sql,original.wItemID);
    if(!chart)throw std::runtime_error("Gamble source item template disappeared");
    auto raw=*original.source;
    if(quote) {
        const auto costs=LevelCosts(sql,raw.grade);
        if(!costs){reply(InvalidItem,inventory,slot);return false;}
        const auto npc=ReadNpc(sql,npc_id);
        const auto discount=npc.exists?NpcDiscount(npc.condition,npc.discount,npc.local):std::uint8_t{0};
        const auto cost=BaseCost(*chart,raw,*costs,true);
        if(!cost){reply(InvalidItem,inventory,slot);return false;}
        const auto discounted=Discounted(*cost,discount);
        if(!UseNativeMoney(sql,c,s,discounted,false,packets)){reply(NeedMoney,inventory,slot);return false;}
        reply(Success,inventory,slot,nullptr,*cost,discount);return discounted!=0; // preflight title side effect
    }
    std::optional<ItemInstance> caller;
    if(call_inventory!=252&&call_slot!=255) {
        if(s.wMapID!=0&&s.wMapID!=8){reply(InvalidPosition,call_inventory,call_slot);return false;}
        const auto* candidate=FindItem(s,call_inventory,call_slot);
        if(!candidate||candidate->bKind!=55||!candidate->bCount)return false; // original silent invalid caller
        if(candidate->dlID==original.dlID) {
            // Source pNpcItem may dangle after replacement/debit. Reject the
            // alias before any draw or cost instead of copying that UAF.
            reply(InvalidItem,inventory,slot);return false;
        }
        caller=*candidate;
    }
    std::uint32_t money=0;Result result=Success;bool changed=false;std::uint16_t new_item=0;std::uint8_t new_count=0;
    if(raw.wrap) {
        raw.wrap=0;ReplaceNativeItem(sql,c,s,original,raw);
    }else if(raw.appearance) {
        raw.appearance=0;ReplaceNativeItem(sql,c,s,original,raw);
    }else {
        const auto costs=LevelCosts(sql,raw.grade);
        if(!costs){reply(InvalidItem,inventory,slot);return false;}
        const auto npc=ReadNpc(sql,npc_id);
        const auto discount=npc.exists?NpcDiscount(npc.condition,npc.discount,npc.local):std::uint8_t{0};
        const auto cost=BaseCost(*chart,raw,*costs,false);
        if(!cost){reply(InvalidItem,inventory,slot);return false;}
        money=Discounted(*cost,discount);
        if(!UseNativeMoney(sql,c,s,money,false,packets)){reply(NeedMoney,inventory,slot);return false;}
        RequireCatalog(sql);
        raw.id=AllocateNativeItem(sql,c);raw.count=1;
        std::uint16_t probability=0;
        if(npc.exists) {
            if(!facts.world_registered||!facts.world_events||!facts.world_events->connection)
                throw std::runtime_error("Gamble probability requires a live World event projection");
            const auto base=static_cast<std::uint8_t>(chart->get<int>("bGambleProb"));
            probability=static_cast<std::uint16_t>(base+NpcProbabilityBonus(npc.condition,npc.bonus,npc.local));
            probability=static_cast<std::uint16_t>(probability+unsigned(base)*facts.world_events->values[10]/100U);
            probability=std::min<std::uint16_t>(probability,100); // CalcProb(PROB_GAMBLE), no buff consumption
        }
        if(probability>LegacyRandom15()%100U) {
            const auto selected=DrawReplacement(sql,chart->get<int>("bType"),chart->get<int>("bKind"));
            if(!selected)result=Fail;
            else {
                raw.count=selected->count;
                const auto replacement=ItemChart(sql,selected->item);
                if(!replacement)result=Fail;
                else {
                    raw.item=selected->item;
                    if(replacement->get<int>("bType")==14)Catalyzer(sql,raw);
                    else MakeSpecialMagic(sql,s,raw,*replacement); // return deliberately ignored by the source
                    raw.expires=NativeItemExpiry(sql,0,replacement->get<int>("bUseType"),replacement->get<int>("wUseTime"));
                    changed=true;
                }
            }
        }else if(!MakeSpecialMagic(sql,s,raw,*chart))
            result=static_cast<std::uint8_t>(chart->get<int>("bDestroyProb"))>LegacyRandom15()%100U?Fail:ReplaceFail;
        raw.grade=0;new_item=raw.item;new_count=raw.count;
        if(result==Fail)DebitNativeItem(sql,c,s,inventory,slot,original.dlID,1,packets);
        else if(original.bCount>1) {
            const std::vector<ItemInstance> items{ProjectNativeItem(sql,raw)};
            // Both source checks happen BEFORE debit, even if the old item
            // could free a slot. Full inventory still keeps preflight titles.
            if(!HasBlank(*s.payload)||!CanGrantQuestItems(*s.payload,items)) {
                result=Full;changed=false;
            }else {
                DebitNativeItem(sql,c,s,inventory,slot,original.dlID,1,packets);
                if(!GrantQuestItems(sql,c,s,items,packets))throw std::runtime_error("Gamble placement changed after debit");
            }
        }else ReplaceNativeItem(sql,c,s,original,raw);
    }
    if(result!=Full) {
        if(caller)DebitNativeItem(sql,c,s,call_inventory,call_slot,caller->dlID,1,packets);
        if(!UseNativeMoney(sql,c,s,money,true,packets))throw std::runtime_error("Gamble purse changed during transaction");
        packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_MONEY_ACK),EncodeMoneyAck(s.dwGold,s.dwSilver,s.dwCooper));
        if(inventory==254||call_inventory==254)RefreshEquipment(sql,s,true);
    }
    const auto* target=result==Fail?nullptr:FindItem(s,inventory,slot);
    if(result!=Fail&&!target)throw std::runtime_error("Gamble result lost its original slot");
    reply(result,inventory,slot,target);
    if(changed&&new_item)event(new_item,QTT_GETITEM,TT_GETITEM,new_count);
    // Source has no secure-code/dead/riding gate, cooldown or USEITEM hook.
    return result!=Full||money!=0;
}
}
