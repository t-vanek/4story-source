#include "postgresql_map_service.h"
#include "native_item_effects.h"
#include "native_npc_discount.h"
#include "legacy_random.h"
#include "posture_effects.h"
#include "secure_code.h"
#include "domain/main_transfer.h"
#include "MessageId.h"
#include <algorithm>
#include <bit>
#include <map>
#include <optional>

namespace tmapsvr {
namespace {
using tnetlib::protocol::MessageId;
enum Kind { Upgrade=29, Downgrade=30, MagicGrade=31, RareGrade=32, ClearMagic=77, Wrap=79,
    Eld=80, ClearRefine=82, ChangeEffect=83, Color=85, Gem=102, DeleteEffect=104, CloakEffect=105, Accessory=205 };
enum Result : std::uint8_t { Success, NoItem, NoGrade, Fail, Money, NpcError, InvalidPosition,
    GemSuccess, GemFail, NoGem, MagicFail, ClearMagicSuccess, WrapSuccess, EldSuccess, CannotWrap,
    MaxEld, NoRefine, NoGradeEffect, ClearRefineSuccess, EffectSuccess, SameColor, ColorSuccess,
    GradeDown, AppearanceSuccess, WrongKind, SameItem, CannotColor, Undye };
bool AppearanceKind(int kind) {return (kind>=1&&kind<=9)||(kind>=12&&kind<=19);}
const ItemInstance* Find(const CharSnapshot& s,std::uint8_t bag,std::uint8_t slot) {
    for(const auto& inventory:s.payload->bags)if(inventory.bag.bInvenID==bag)
        for(const auto& item:inventory.items)if(item.bItemID==slot)return &item;
    return nullptr;
}
soci::row Chart(soci::session& sql,std::uint16_t id) {
    const int item=std::bit_cast<std::int16_t>(id);soci::row row;
    sql << "SELECT * FROM character_compat.\"TITEMCHART\" WHERE \"wItemID\"=:id",soci::use(item),soci::into(row);
    if(!sql.got_data())throw std::runtime_error("Item upgrade source template disappeared");
    return row;
}
void RequireUpgradeCatalog(soci::session& sql) {
    int found=0;
    sql << "SELECT count(*) FROM game_compat.item_upgrade_release r "
        "JOIN game_compat.catalog_release m ON m.manifest_sha256=r.map_manifest AND m.status='verified' "
        "JOIN character_compat.catalog_release c ON c.manifest_sha256=r.character_manifest AND c.status='verified' "
        "JOIN actor_compat.catalog_release a ON a.manifest_sha256=r.actor_manifest AND a.status='verified' WHERE r.status='verified'",
        soci::into(found);
    if(found!=1)throw std::runtime_error("Item upgrades require complete published source charts");
}
struct Grade {std::uint8_t probability{};std::uint32_t money{};};
Grade ReadGrade(soci::session& sql,std::uint8_t value) {
    const int level=value;int probability=0,money=0;
    sql << "SELECT \"bProb\",\"dwMoney\" FROM actor_compat.\"TITEMGRADECHART\" WHERE \"bLevel\"=:level",
        soci::use(level),soci::into(probability),soci::into(money);
    // m_itemgrade is explicitly zeroed in the original module constructor.
    if(!sql.got_data())return {};
    return {static_cast<std::uint8_t>(probability),static_cast<std::uint32_t>(money)};
}
bool HasOptionKind(soci::session& sql,const transfer::Item& item) {
    for(const auto& magic:item.magic) {
        const int id=magic.id;int kind=0;
        sql << "SELECT \"bOptionKind\" FROM actor_compat.\"TITEMMAGICCHART\" WHERE \"bMagic\"=:id",soci::use(id),soci::into(kind);
        if(!sql.got_data())throw std::runtime_error("Upgrade option template missing");
        if(kind)return true;
    }
    return false;
}
std::uint16_t HaveBuff(soci::session& sql,const CharSnapshot& s,int status) {
    if(!SupportedPostures(*s.payload))throw std::runtime_error("Item upgrade requires supported maintained effects");
    for(const auto& effect:*s.payload->effects)if(const auto value=NativeItemBuffValue(sql,effect.skill,status);value&&*value)return *value;
    return 0;
}
std::uint8_t EquipLevel(const transfer::Item& item,const soci::row& chart) {
    auto level=static_cast<std::uint8_t>(chart.get<int>("bLevel"));
    if(level>item.eld)level=static_cast<std::uint8_t>(level-static_cast<std::uint8_t>(item.eld));
    return level;
}
// Return false only for the source's intentional empty accessory catalog no-op
// AFTER scroll consumption. Bad references/divisors/cycles abort the transaction.
bool AddAccessoryMagic(soci::session& sql,transfer::Item& raw) {
    struct Option {std::uint8_t magic{};std::uint16_t maximum{};};
    std::vector<Option> options;
    soci::rowset<soci::row> rows=(sql.prepare << "SELECT \"MagicID\",\"MaxValue\" FROM game_compat.\"ACCESSORYMAGICTABLE\" ORDER BY \"_source_row_number\"");
    for(const auto& row:rows)options.push_back({static_cast<std::uint8_t>(row.get<int>(0)),static_cast<std::uint16_t>(row.get<int>(1))});
    if(options.empty())return false;
    std::map<std::uint8_t,std::uint8_t> groups;
    soci::rowset<soci::row> magic=(sql.prepare << "SELECT \"bMagic\",\"bExclIndex\" FROM actor_compat.\"TITEMMAGICCHART\" ORDER BY \"bMagic\"");
    for(const auto& row:magic)groups.emplace(static_cast<std::uint8_t>(row.get<int>(0)),static_cast<std::uint8_t>(row.get<int>(1)));
    auto existing=raw.magic;
    std::sort(existing.begin(),existing.end(),[](const auto& a,const auto& b){return a.id<b.id;});
    const auto eligible=[&](const Option& option)->std::optional<std::uint8_t> {
        if(!groups.contains(option.magic))throw std::runtime_error("Accessory magic source reference missing");
        if(std::any_of(existing.begin(),existing.end(),[&](const auto& old){return old.id==option.magic;}))return std::nullopt;
        for(const auto& old:existing) {
            if(!groups.contains(old.id))throw std::runtime_error("Accessory existing magic reference missing");
            if(groups.at(old.id)==groups.at(option.magic)&&old.id!=50&&old.id!=51&&option.magic!=50&&option.magic!=51)return std::nullopt;
            if((old.id==50&&option.magic==51)||(old.id==51&&option.magic==50))return option.magic;
        }
        return option.magic;
    };
    // Source narrows the index to BYTE after rand()%size; only the reachable
    // prefix participates. Check for an infinite rejection loop before drawing.
    const auto reachable=std::min<std::size_t>(options.size(),256);
    bool possible=false;
    for(std::size_t i=0;i<reachable;++i)if(eligible(options[i]))possible=true;
    if(!possible)throw std::runtime_error("Accessory source selection has no terminating candidate");
    for(;;) {
        const auto& option=options[static_cast<std::uint8_t>(LegacyRandom15()%options.size())];
        const auto id=eligible(option);if(!id)continue;
        if(!*id||!option.maximum)throw std::runtime_error("Accessory magic source ID or divisor is zero");
        // ((rand()%MaxValue)-MinValue)+MinValue: MinValue cancels, including
        // negative intermediate values. Zero is a valid live source value.
        const auto value=static_cast<std::uint16_t>(LegacyRandom15()%option.maximum);
        raw.magic.push_back({*id,value});++raw.refine;
        std::sort(raw.magic.begin(),raw.magic.end(),[](const auto& a,const auto& b){return a.id<b.id;});
        return true;
    }
}
}

std::uint16_t PostgreSQLMapService::UpgradeItemProbability(soci::session& sql,const MapSessionClaim& c,
    CharSnapshot& s,int kind,std::uint8_t base,std::uint8_t bonus,const QuestMapFacts& facts,QuestPackets& packets) {
    const bool guard=kind==0;
    const int status=guard?64:kind==Upgrade?42:60; // ITEMGUARD / ITEMUPGRADE / MAGICPROB
    const auto buff=HaveBuff(sql,s,status);
    std::uint16_t probability=guard?buff:static_cast<std::uint16_t>(base+bonus);
    if(!guard) {
        if(!facts.world_registered||!facts.world_events||!facts.world_events->connection)
            throw std::runtime_error("Upgrade probability requires current World events");
        const unsigned event=kind==Upgrade?7:kind==MagicGrade?8:9;
        probability=static_cast<std::uint16_t>(probability+unsigned(base)*facts.world_events->values[event]/100U);
        if(buff)probability=static_cast<std::uint16_t>(probability+unsigned(base)*buff/100U);
        // Existing native admission rejects nonzero PCBang entitlement. Do not
        // turn its missing producer into an implicit bonus for admitted users.
        if(kind==Upgrade&&s.payload->transfer_state&&s.payload->transfer_state->pc_bang)
            throw std::runtime_error("Upgrade PCBang authority is not integrated");
    }
    if(guard||buff)for(std::size_t i=0;i<s.payload->effects->size();++i)
        if(NativeItemBuffValue(sql,s.payload->effects->at(i).skill,status)) {
            EraseProgressEffect(sql,c,s,i,packets);break; // first matching row, even when its value is zero
        }
    return std::min<std::uint16_t>(probability,100);
}

bool PostgreSQLMapService::UpgradeNativeItem(soci::session& sql,const MapSessionClaim& c,CharSnapshot& s,
    std::span<const std::byte> body,const QuestMapFacts& facts,QuestPackets& packets) {
    if(s.payload->security.Blocks()) {
        const auto prompt=SecureUnlockPrompt(s.payload->security);packets.emplace_back(prompt.first,prompt.second);return false;
    }
    if(s.payload->deal_status||s.payload->personal_store)return false;
    wire::Reader reader(body.data(),body.size());
    std::uint8_t bag=0,slot=0,grade_bag=0,grade_slot=0,call_bag=0,call_slot=0;std::uint16_t npc=0,color=0;
    if(!reader.Read(bag)||!reader.Read(slot)||!reader.Read(grade_bag)||!reader.Read(grade_slot)||!reader.Read(npc)||
       !reader.Read(call_bag)||!reader.Read(call_slot)||!reader.Read(color)||!reader.Eof())throw std::runtime_error("Invalid ITEMUPGRADE request");
    const auto ack=[&](Result result,const transfer::Item* item=nullptr,std::optional<std::uint8_t> level={},std::uint16_t dye=0) {
        std::vector<std::byte> out;wire::WritePOD(out,static_cast<std::uint8_t>(result));
        wire::WritePOD(out,item?bag:std::uint8_t{0});wire::WritePOD(out,item?slot:std::uint8_t{0});
        wire::WritePOD(out,item?level.value_or(item->level):std::uint8_t{0});wire::WritePOD(out,item?item->gem:std::uint8_t{0});
        wire::WritePOD(out,item?item->grade_effect:std::uint8_t{0});wire::WritePOD(out,dye);
        wire::WritePOD(out,item?item->appearance:std::uint16_t{0});
        packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_ITEMUPGRADE_ACK),std::move(out));
    };
    const auto magic_ack=[&](Result result,const ItemInstance* item=nullptr) {
        std::vector<std::byte> out;wire::WritePOD(out,static_cast<std::uint8_t>(result));
        wire::WritePOD(out,item?bag:std::uint8_t{0});wire::WritePOD(out,item?slot:std::uint8_t{0});
        wire::WritePOD(out,static_cast<std::uint8_t>(item?item->magic.size():0));
        if(item)for(const auto& [id,value]:item->magic){wire::WritePOD(out,id);wire::WritePOD(out,value);}
        packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_ITEMMAGICGRADE_ACK),std::move(out));
    };
    for(auto id:{bag,grade_bag})if(std::none_of(s.payload->bags.begin(),s.payload->bags.end(),[&](const auto& b){return b.bag.bInvenID==id;}))return false;
    const auto* target=Find(s,bag,slot);const auto* material=Find(s,grade_bag,grade_slot);
    if(!target||!material||target->bGLevel||!target->bCount||!material->bCount){ack(NoItem);return false;}
    const auto original=*target,scroll=*material;
    if(!original.source||!scroll.source)throw std::runtime_error("Upgrade requires raw target and material");
    auto raw=*original.source;const auto item=Chart(sql,raw.item),grade_item=Chart(sql,scroll.wItemID);
    const auto type=grade_item.get<int>("bType");
    if((type!=9&&type!=1&&type!=2&&type!=4&&type!=6)||raw.wrap||scroll.source->wrap){ack(NoItem);return false;}
    if(original.dlID==scroll.dlID){ack(NoItem);return false;} // source material deletion can free the target
    const int npc_id=std::bit_cast<std::int16_t>(npc);int condition=0,rate=0,local=0,bonus=0;
    sql << "SELECT \"bCondition\",\"bDiscountRate\",\"wLocalID\",\"bAddProb\" FROM character_compat.\"TNPCCHART\" WHERE \"wID\"=:id",
        soci::use(npc_id),soci::into(condition),soci::into(rate),soci::into(local),soci::into(bonus);
    if(!sql.got_data()){ack(NpcError);return false;}
    const auto discount=NpcDiscount(condition,rate,local);
    const auto kind=grade_item.get<int>("bKind");
    const auto use=static_cast<std::uint16_t>(grade_item.get<int>("wUseValue"));
    const auto material_grade=static_cast<std::uint8_t>(grade_item.get<int>("bGrade"));
    std::uint32_t money=0;Grade level{};
    if(kind==Gem||kind==Upgrade||kind==Downgrade) {
        if(kind==Gem&&raw.gem==5){ack(NoGrade);return false;}
        if(!item.get<int>("bCanGrade")||HasOptionKind(sql,raw)){ack(NoGrade);return false;}
        if(kind!=Gem) {
            level=ReadGrade(sql,raw.level);
            if(!level.probability){ack(NoGrade);return false;}
        }
        if(kind==Upgrade) {
            // Source uses signed __int64 multiplication here, unlike REFINE's
            // DWORD product. Rates requiring territory ownership remain gated.
            const auto deducted=static_cast<std::uint32_t>(std::uint64_t{level.money}*discount/100U);
            if(deducted>level.money)throw std::runtime_error("Upgrade discount yields unsupported negative price");
            money=level.money-deducted;
            if(!UseNativeMoney(sql,c,s,money,false,packets)){ack(Money);return false;}
        }
    }else if(AppearanceKind(kind)) {
        if(scroll.bLevel){ack(SameItem);return false;}
        const int target_kind=item.get<int>("bKind"),sub=item.get<int>("bSubSlotID"),grade_sub=grade_item.get<int>("bSubSlotID");
        const auto slots=static_cast<std::uint32_t>(item.get<int>("dwSlotID"));
        const auto grade_slots=static_cast<std::uint32_t>(grade_item.get<int>("dwSlotID"));
        const bool same_body=sub==grade_sub&&(sub==3||sub==5||sub==6||sub==7||sub==8);
        const bool exception=(target_kind==11&&kind==12)||(target_kind==8&&kind==1)||(target_kind==5&&kind==3)||(target_kind==3&&kind==5);
        if(!(same_body||(exception&&sub==grade_sub))&&
           (((kind!=target_kind||grade_slots!=slots)&&(grade_slots==0||grade_slots==1||grade_slots==2))||grade_slots!=slots)) {
            ack(WrongKind);return false;
        }
        if(scroll.wItemID==raw.item){ack(SameItem);return false;}
    }else switch(kind) {
    case MagicGrade:if(!item.get<int>("bCanMagic")||raw.level||!raw.magic.empty()){ack(NoGrade);return false;}break;
    case RareGrade:if(!item.get<int>("bCanRare")||raw.level||raw.magic.empty()||raw.magic.size()>2){ack(NoGrade);return false;}break;
    case ClearMagic:if(raw.magic.empty()){ack(NoGrade);return false;}break;
    case Wrap:if(!item.get<int>("bCanWrap")||raw.wrap){ack(CannotWrap);return false;}break;
    case Eld:if(raw.eld>=9||EquipLevel(raw,item)==1){ack(MaxEld);return false;}break;
    case ClearRefine:if(!raw.refine){ack(NoRefine);return false;}break;
    case ChangeEffect:
        if(item.get<int>("bKind")==90||item.get<int>("bKind")==19){ack(CannotColor);return false;}
        if((raw.level<17&&item.get<int>("bType")!=17)||item.get<int>("bKind")==19){ack(NoGradeEffect);return false;}break;
    case Color:if(!item.get<int>("bCanColor")||raw.color==color){ack(SameColor);return false;}break;
    case DeleteEffect:if(!raw.grade_effect){ack(SameColor);return false;}break;
    case CloakEffect:if(item.get<int>("bKind")!=19){ack(SameColor);return false;}break;
    case Accessory:if(item.get<int>("bType")!=17||raw.magic.size()>=4){ack(NoGrade);return false;}break;
    default:ack(NoGrade);return false;
    }
    if(call_bag!=252&&call_slot!=255) {
        if(s.wMapID!=0&&s.wMapID!=8){ack(InvalidPosition);return money!=0;}
        const auto* caller=Find(s,call_bag,call_slot);
        if(!caller||caller->bKind!=55||!caller->bCount||caller->dlID==original.dlID||caller->dlID==scroll.dlID){ack(NpcError);return money!=0;}
        DebitNativeItem(sql,c,s,call_bag,call_slot,caller->dlID,1,packets);
    }
    if(money&&!UseNativeMoney(sql,c,s,money,true,packets))throw std::runtime_error("Upgrade purse changed during transaction");
    // All source material fields are owned copies above. Source GEM/MOGG/
    // ACCESSORY/CHGGRADEEFFECT dereference pGradeItem after deleting its last unit.
    DebitNativeItem(sql,c,s,grade_bag,grade_slot,scroll.dlID,1,packets);
    bool removed=false,modified=false;
    const auto save=[&] {
        const auto* current=Find(s,bag,slot);
        if(!current||current->dlID!=original.dlID)throw std::runtime_error("Upgrade target identity changed");
        const bool rearm=kind==Upgrade||kind==Downgrade||kind==Gem||kind==Accessory||AppearanceKind(kind);
        UpgradeNativeItemAttributes(sql,c,s,*current,raw,rearm);modified=true;
    };
    const auto erase=[&] {
        DebitNativeItem(sql,c,s,bag,slot,original.dlID,original.bCount,packets);removed=true;
    };
    const auto probability=[&](int branch,std::uint8_t base) {
        return UpgradeItemProbability(sql,c,s,branch,base,NpcProbabilityBonus(condition,bonus,local),facts,packets);
    };
    if(AppearanceKind(kind)) {
        raw.appearance=scroll.wItemID;save();ack(AppearanceSuccess,&raw);
    }else switch(kind) {
    case Upgrade: {
        const auto roll=LegacyRandom15()%100U;
        if(roll<probability(kind,level.probability)) {
            if(material_grade==3)(void)LegacyRandom15(); // bGrade rand()%3+1 is drawn but not used
            raw.level=24; // literal assignment in the authoritative source, not a placeholder
            if(!raw.grade_effect&&item.get<int>("bKind")!=19)raw.grade_effect=static_cast<std::uint8_t>(LegacyRandom15()%39U+1U);
            save();ack(Success,&raw);
        }else if((s.bStartAct==1&&s.wMapID==2010)||s.bStartAct==2||probability(0,0)) {
            const auto roll_down=raw.level<=11?0U:LegacyRandom15()%100U;
            auto down=roll_down<20?0U:roll_down<55?1U:2U;
            if(raw.level>11&&int(raw.level)-int(down)<11)down=raw.level-11;
            if(down) {raw.level=raw.level>down?raw.level-down:0;if(raw.level<17)raw.grade_effect=0;save();}
            ack(GradeDown,&raw);
        }else {erase();ack(Fail);}
        break;
    }
    case Downgrade:
        if(raw.level){raw.level=static_cast<std::uint8_t>(raw.level-material_grade);if(raw.level<17)raw.grade_effect=0;save();ack(Success,&raw);}
        else ack(Fail);
        break;
    case Gem: {
        RequireUpgradeCatalog(sql);
        const auto roll=LegacyRandom15()%100U+1U;const int gem=raw.gem;int base=0;
        if(gem>4)throw std::runtime_error("Gem grade exceeds the source array");
        sql << "SELECT \"bProb\" FROM game_compat.\"TGEMGRADECHART\" WHERE \"bGem\"=:gem",soci::use(gem),soci::into(base);
        if(!sql.got_data())throw std::runtime_error("Gem grade has no source array value");
        const auto multiplier=static_cast<std::uint8_t>(unsigned(use)*2U/100U);
        const bool won=(roll<=unsigned(base)*multiplier&&(raw.gem!=2||multiplier!=1))||use==1337;
        if(won)++raw.gem;
        else if(scroll.wItemID!=18191)raw.gem=0;
        else if(raw.gem>=3)--raw.gem;
        save();ack(won?GemSuccess:GemFail,&raw);break;
    }
    case MagicGrade:case RareGrade: {
        RequireUpgradeCatalog(sql);
        const auto buff=HaveBuff(sql,s,60);const auto chance=probability(kind,material_grade);
        if(LegacyRandom15()%100U<chance&&MakeNativeScrollMagic(sql,s,raw,kind==RareGrade,buff!=0)) {
            save();magic_ack(Success,Find(s,bag,slot));
        }else {if(kind==RareGrade)erase();magic_ack(MagicFail);}
        break;
    }
    case Accessory:
        RequireUpgradeCatalog(sql);
        if(AddAccessoryMagic(sql,raw)){save();magic_ack(Success,Find(s,bag,slot));}
        break;
    case ClearMagic:raw.magic.clear();save();magic_ack(ClearMagicSuccess,Find(s,bag,slot));break;
    case Wrap:raw.wrap=1;save();ack(WrapSuccess,&raw,static_cast<std::uint8_t>(raw.wrap));break;
    case Eld: {
        const auto amount=static_cast<std::uint8_t>(LegacyRandom15()%std::max<unsigned>(use,1));
        raw.eld=std::min<std::uint32_t>(raw.eld+std::max<unsigned>(amount,1),9);
        const auto base=static_cast<std::uint8_t>(item.get<int>("bLevel"));
        if(base<=raw.eld)raw.eld=static_cast<std::uint32_t>(int(base)-1);
        save();ack(EldSuccess,&raw,static_cast<std::uint8_t>(raw.eld));break;
    }
    case ClearRefine: {
        raw.durability=raw.durability_max=static_cast<std::uint32_t>(item.get<int>("dwDuraMax"));raw.refine=0;raw.magic.clear();save();
        auto out=EncodeItemDescriptor(*Find(s,bag,slot),c.char_id,true);out.insert(out.begin(),static_cast<std::byte>(bag));
        packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_CHANGEITEMATTR_ACK),std::move(out));
        ack(ClearRefineSuccess,&raw);break;
    }
    case ChangeEffect:case CloakEffect: {
        const auto effect=static_cast<std::uint8_t>(LegacyRandom15()%39U+1U);
        if(kind==ChangeEffect&&scroll.wItemID>=24501)raw.grade_effect=static_cast<std::uint8_t>(scroll.wItemID-24501+1);
        else raw.grade_effect=effect==raw.grade_effect?static_cast<std::uint8_t>((raw.grade_effect+1)%39+1):effect;
        if(kind==CloakEffect)raw.color=0;
        save();ack(kind==CloakEffect?ColorSuccess:EffectSuccess,&raw,{},kind==CloakEffect?color:0);break;
    }
    case Color:raw.color=color;raw.grade_effect=0;save();ack(ColorSuccess,&raw,{},color);break;
    case DeleteEffect:raw.grade_effect=0;save();ack(Undye,&raw,{},color);break;
    default:throw std::runtime_error("Validated item upgrade branch is not dispatched");
    }
    // Source has no additional MONEY/UPDATEITEM/quest-use/equipment broadcasts.
    // Derive cached stats to mirror its live getters after target/material loss.
    if(((modified||removed)&&bag==254)||grade_bag==254||call_bag==254)RefreshEquipment(sql,s,true);
    return true;
}
}
