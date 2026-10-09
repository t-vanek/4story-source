#include "postgresql_map_service.h"
#include "postgresql_map_owner.h"
#include "domain/main_transfer.h"
#include "services/skill_cooldown.h"
#include "services/skill_timing.h"
#include "services/character_statistics.h"
#include "services/postgresql_skill_targets.h"
#include "services/posture_effects.h"
#include <set>
#include <soci/soci.h>
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <map>
#include <stdexcept>

namespace tmapsvr {
namespace {
// PostgreSQL SMALLINT/INT preserve historical signed storage; narrowing to the
// corresponding wire unsigned type preserves those bits, including sentinels.
long long Number(const soci::row& r,const std::string& name) {
    if(r.get_indicator(name)==soci::i_null)return 0;
    switch(r.get_properties(name).get_data_type()) {
    case soci::dt_integer:return r.get<int>(name);
    case soci::dt_long_long:return r.get<long long>(name);
    default:throw std::runtime_error("Unexpected native character numeric type");
    }
}
std::uint32_t U32(const soci::row& r,const std::string& n){return static_cast<std::uint32_t>(Number(r,n));}
std::uint16_t U16(const soci::row& r,const std::string& n){return static_cast<std::uint16_t>(Number(r,n));}
std::uint8_t U8(const soci::row& r,const std::string& n){return static_cast<std::uint8_t>(Number(r,n));}
std::string Epoch(const char* name,const char* alias="epoch") {
    return "CASE WHEN \""+std::string(name)+"\"<TIMESTAMP '2000-01-01' THEN 0 ELSE "
        "extract(epoch FROM \""+name+"\" AT TIME ZONE 'UTC')::bigint END AS "+alias;
}
void ByteCount(std::size_t n){if(n>255)throw std::runtime_error("Character list exceeds client count width");}
std::uint32_t Truncate(float f) {
    if(!std::isfinite(f)||f<0||static_cast<double>(f)>std::numeric_limits<std::uint32_t>::max())
        throw std::runtime_error("Character stat outside original DWORD range");
    return static_cast<std::uint32_t>(f);
}
struct Formula {std::uint32_t initial{};float rate{},rate_y{};};
Formula ReadFormula(soci::session& sql,int id){
    soci::row r;sql<<"SELECT * FROM character_compat.\"TFORMULACHART\" WHERE \"bID\"=:id",soci::use(id),soci::into(r);
    if(!sql.got_data())throw std::runtime_error("Required character formula is missing");
    return {U32(r,"dwinit"),static_cast<float>(r.get<double>("fRateX")),static_cast<float>(r.get<double>("fRateY"))};
}
using Passive=AbilityEffect;
AbilityEffect ReadAbility(const soci::row& data,const SkillTemplate& skill,std::uint8_t rank) {
    int value=U16(data,"wValue"),increment=U16(data,"wValueInc");
    switch(U8(data,"bCalc")) {
    case 0:break;
    case 1:value+=(rank-1)*increment;break;
    case 2:{const int exponent=rank?skill.bStartLevel+(rank-1)*skill.bNextLevel:0;
        const double scaled=value*std::pow(static_cast<double>(skill.f1stRateX),exponent)/100;
        if(!std::isfinite(scaled)||scaled<std::numeric_limits<int>::min()||scaled>std::numeric_limits<int>::max())
            throw std::runtime_error("Skill ability outside original INT range");
        value=static_cast<int>(scaled);break;}
    case 3:value-=(rank-1)*increment;break;
    default:throw std::runtime_error("Unsupported skill ability calculation");
    }
    return {U8(data,"bExec"),U8(data,"bInc"),value};
}
void ReadPostures(soci::session& sql,CharacterPayload& p) {
    for(int id=131;id<=132;++id) {
        soci::row chart;sql<<"SELECT * FROM character_compat.\"TSKILLCHART\" WHERE \"wID\"=:id",soci::use(id),soci::into(chart);
        if(!sql.got_data()||U32(chart,"dwDuration")||U32(chart,"dwDurationInc")||
           !U8(chart,"bStatic")||U8(chart,"bPositive")%2!=1||U8(chart,"bORadius")||U16(chart,"wPosture"))
            throw std::runtime_error("Pinned automatic posture requires additional effect semantics");
        auto& def=p.posture_templates[id-131];def={static_cast<std::uint16_t>(id),U32(chart,"dwWeaponID"),{}};
        bool posture=false;
        soci::rowset<soci::row> data=(sql.prepare<<"SELECT * FROM character_compat.\"TSKILLDATA\" WHERE \"wSkillID\"=:id",soci::use(id));
        for(const auto& d:data) {
            const auto type=U8(d,"bType"),exec=U8(d,"bExec");
            if(U8(d,"bAction")!=3)throw std::runtime_error("Automatic posture has an unsupported action");
            if(type==6&&(exec==7||exec==26||exec==27)){posture=true;continue;}
            if(type!=1||(exec!=7&&exec!=8&&exec!=9&&exec!=16)||U8(d,"bCalc")>1||U8(d,"bInc")>5)
                throw std::runtime_error("Automatic posture has unsupported stat effects");
            // Automatic/recovered postures are rank 1: original Calc 0/1 both
            // evaluate to wValue. Values always come from the pinned backup.
            def.abilities.push_back({exec,U8(d,"bInc"),U16(d,"wValue")});
        }
        if(!posture)throw std::runtime_error("Automatic skill is not a source posture");
    }
}
void DeriveStats(soci::session& sql,CharSnapshot& s,CharacterPayload& p) {
    for(auto& bag:p.bags) {
        const int item=std::bit_cast<std::int16_t>(bag.bag.wItemID);int slots=0;
        sql<<"SELECT \"bSlotCount\" FROM character_compat.\"TITEMCHART\" WHERE \"wItemID\"=:i",soci::use(item),soci::into(slots);
        if(!sql.got_data())throw std::runtime_error("Inventory lacks pinned bag template");
        bag.slot_count=static_cast<std::uint8_t>(slots);
    }
    const auto growth=ReadFormula(sql,34),hp=ReadFormula(sql,8),mp=ReadFormula(sql,19);
    const int klass=s.bClass,race=s.bRace; soci::row cl,ra;
    sql<<"SELECT * FROM character_compat.\"TCLASSCHART\" WHERE \"bClassID\"=:id",soci::use(klass),soci::into(cl);
    if(!sql.got_data())throw std::runtime_error("Missing character class");
    sql<<"SELECT * FROM character_compat.\"TRACECHART\" WHERE \"bRaceID\"=:id",soci::use(race),soci::into(ra);
    if(!sql.got_data())throw std::runtime_error("Missing character race");
    std::map<int,std::uint32_t> equipment;
    character_statistics::ItemAttributes attributes;
    std::int64_t short_weapon_delay=0,long_weapon_delay=0;
    struct EquippedSkillItem {std::uint8_t kind,consumable_kind,consumable_count;bool powered;};
    std::vector<EquippedSkillItem> skill_equipment;
    for(const auto& bag:p.bags)if(bag.bag.bInvenID==254)
        for(const auto& item:bag.items) {
            const int id=std::bit_cast<std::int16_t>(item.wItemID);soci::row chart;
            sql<<"SELECT \"dwSpeedInc\",\"bKind\",\"bUseItemKind\",\"bUseItemCount\",\"bType\",\"wAttrID\" FROM character_compat.\"TITEMCHART\" WHERE \"wItemID\"=:id",soci::use(id),soci::into(chart);
            if(!sql.got_data())throw std::runtime_error("Equipped item lacks source template");
            const bool powered=!item.dwDuraMax||item.dwDuraCur;
            skill_equipment.push_back({U8(chart,"bKind"),U8(chart,"bUseItemKind"),U8(chart,"bUseItemCount"),powered});
            if(!powered)continue;
            for(const auto& [magic,value]:item.magic)equipment[magic]+=value;
            const int level=item.bLevel;int grade=0;
            sql<<"SELECT \"bGrade\" FROM actor_compat.\"TITEMGRADECHART\" WHERE \"bLevel\"=:l",soci::use(level),soci::into(grade);
            // Original grade array is zero-initialized for absent levels. The
            // attribute key narrows to WORD via MAPTITEMATTR::find.
            const int attr=std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(U16(chart,"wAttrID")+grade+item.bGem));
            soci::row a;
            sql<<"SELECT * FROM actor_compat.\"TITEMATTRCHART\" WHERE \"wID\"=:a",soci::use(attr),soci::into(a);
            if(!sql.got_data()) {
                // SetItemAttr deliberately falls back to the first unsigned
                // WORD map key. Keep that evidenced source rule, not an estimate.
                sql<<"SELECT * FROM actor_compat.\"TITEMATTRCHART\" ORDER BY (\"wID\"::integer & 65535) LIMIT 1",soci::into(a);
                if(!sql.got_data())throw std::runtime_error("Missing source item attribute fallback");
            }
            attributes.Add(U8(chart,"bType"),U16(a,"wMinAP"),U16(a,"wMaxAP"),U16(a,"wMinMAP"),U16(a,"wMaxMAP"),U16(a,"wDP"),U16(a,"wMDP"));
            // Original slots 0/1 supply physical AND magic, slot 2 ranged speed.
            if(item.bItemID<=2) {
                const auto increment=std::bit_cast<std::int32_t>(U32(chart,"dwSpeedInc"));
                (item.bItemID==2?long_weapon_delay:short_weapon_delay)+=increment;
            }
        }
    std::vector<Passive> passives;
    struct Instance {std::uint8_t attack_type{};bool long_attack{};std::vector<AbilityEffect> abilities;};
    std::map<std::uint16_t,Instance> instances;
    p.skill_templates.clear();
    p.equipment_kinds.fill(false);
    p.skill_points.fill(0);
    p.statistics.reset();
    ReadPostures(sql,p);
    for(const auto& skill:p.skills) {
        const int id=std::bit_cast<std::int16_t>(skill.wSkillID);soci::row chart;
        sql<<"SELECT * FROM character_compat.\"TSKILLCHART\" WHERE \"wID\"=:id",soci::use(id),soci::into(chart);
        if(!sql.got_data())throw std::runtime_error("Learned skill has no source template");
        {soci::rowset<int> kinds=(sql.prepare<<"SELECT \"bExec\" FROM character_compat.\"TSKILLDATA\" WHERE \"wSkillID\"=:id AND \"bType\"=0",soci::use(id));
         for(int kind:kinds)if(kind>=0&&kind<256)p.equipment_kinds[kind]=true;}
        SkillTemplate definition;
        definition.wID=skill.wSkillID;definition.dwReuseDelay=U32(chart,"dwReuseDelay");
        definition.nReuseDelayInc=std::bit_cast<std::int32_t>(U32(chart,"nReuseDelayInc"));
        definition.dwKindDelay=U32(chart,"dwKindDelay");definition.bKind=U8(chart,"bKind");
        definition.bSpeedApply=U8(chart,"bSpeedApply");
        definition.dwLoopDelay=U32(chart,"dwLoopDelay");
        definition.wTargetActiveID=U16(chart,"wTargetActiveID");
        definition.wPrevActiveID=U16(chart,"wPrevActiveID");
        definition.wMapID=U16(chart,"wMapID");
        definition.wUseItem=U16(chart,"wItemID");
        const auto weapon=U32(chart,"dwWeaponID");
        // Cash templates 25020-25022 are absent from the pinned backup. Ordinary
        // ammo is derived from the first compatible powered weapon.
        if(!definition.wUseItem) {
            definition.items=weapon?SkillItemGate::Unsuitable:SkillItemGate::Allowed;
            if(weapon)for(const auto& item:skill_equipment) {
                if(!item.kind||item.kind>32) {definition.items=SkillItemGate::Unsupported;break;}
                if(!(weapon&(std::uint32_t{1}<<(item.kind-1))))continue;
                definition.items=!item.powered?SkillItemGate::Unsuitable:
                    item.consumable_kind?SkillItemGate::Unsupported:SkillItemGate::Allowed;
                if(item.powered&&item.consumable_kind&&item.consumable_count==1) {
                    definition.items=SkillItemGate::Ammunition;definition.bAmmoKind=item.consumable_kind;
                }
                break; // source checks the first matching equipped item
            }
        }
        if(definition.wUseItem&&!weapon)definition.items=SkillItemGate::Reagent;
        definition.bUseMPType=U8(chart,"bUseMPType");definition.dwUseMP=U32(chart,"dwUseMP");
        definition.bUseHPType=U8(chart,"bUseHPType");definition.dwUseHP=U32(chart,"dwUseHP");
        definition.bStartLevel=U8(chart,"bLevel");definition.bNextLevel=U8(chart,"bNextLevel");
        definition.bMaxLevel=U8(chart,"bMaxLevel");definition.f1stRateX=growth.rate;
        definition.multi_attack=ReadNativeMultiAttack(sql,definition,skill.bLevel,U8(chart,"bTargetHit"));
        auto& instance=instances[skill.wSkillID];bool final_type=false;
        // The original query has no ORDER BY. Use the recovered source key
        // explicitly instead of relying on PostgreSQL's selected access path.
        soci::rowset<soci::row> all_data=(sql.prepare<<"SELECT * FROM character_compat.\"TSKILLDATA\" WHERE \"wSkillID\"=:id "
            "ORDER BY \"bAction\",\"bType\",\"bAttr\",\"bExec\"",soci::use(id));
        for(const auto& data:all_data) {
            const auto attr=U8(data,"bAttr"),type=U8(data,"bType"),exec=U8(data,"bExec");
            // CTSkillTemp::GetAttackType: physical/long and MAGICNO return
            // immediately; elemental magic only sets the fallback result.
            if(!final_type) {
                if(attr==1||attr==2){instance.attack_type=1;final_type=true;}
                else if(attr==3){instance.attack_type=3;final_type=true;}
                else if(attr>=4&&attr<=9)instance.attack_type=3;
            }
            if(type==1) {
                instance.long_attack|=exec==9||attr==2;
                instance.abilities.push_back(ReadAbility(data,definition,skill.bLevel));
            }
        }
        p.skill_templates.push_back(definition);
        const int kind=U8(chart,"bKind");const int group=kind==0?0:kind-klass*3;
        if(group>=0&&group<4) {
            int used=0;const int rank=skill.bLevel;
            sql<<"SELECT COALESCE(sum(\"bSkillPoint\"),0)::integer FROM actor_compat.\"TSKILLPOINTCHART\" "
                 "WHERE \"wID\"=:id AND \"bLevel\">=1 AND \"bLevel\"<=:rank",soci::use(id,"id"),soci::use(rank,"rank"),soci::into(used);
            p.skill_points[group]=static_cast<std::uint16_t>(p.skill_points[group]+used);
        }
        // Original IsRemainType only adds skills having a SA_CONTINUE row.
        soci::rowset<soci::row> data=(sql.prepare<<"SELECT d.* FROM character_compat.\"TSKILLDATA\" d WHERE d.\"wSkillID\"=:id "
            "AND d.\"bAction\" IN (1,4) AND d.\"bType\"=1 AND d.\"bExec\" IN (1,2,3,4,5,6,7,8,9,11,12,13,16,17,19,20,21,50,51,54,55,56,86,87) "
            "AND EXISTS (SELECT 1 FROM character_compat.\"TSKILLDATA\" a WHERE a.\"wSkillID\"=:id AND a.\"bAction\"=1)",soci::use(id,"id"));
        for(const auto& d:data) {
            passives.push_back(ReadAbility(d,definition,skill.bLevel));
        }
    }
    std::vector<AbilityEffect> active;
    const bool supported_effects=SupportedPostures(p);
    if(supported_effects)for(const auto& effect:*p.effects) {
        const auto& modifiers=p.posture_templates.at(effect.skill-131).abilities;
        active.insert(active.end(),modifiers.begin(),modifiers.end());
    }
    const auto changes=[&](const auto& effects,std::uint32_t base,int target){
        long long total=0;
        for(const auto& effect:effects)if(effect.target==target){
            const long long value=effect.value;
            switch(effect.increase){
            case 0:break;
            case 1:total+=value;break;
            case 2:total-=value;break;
            case 3:total+=static_cast<long long>(base)*value-base;break;
            case 4:total+=base/std::max(1LL,value)-base;break;
            case 5:total+=static_cast<long long>(static_cast<double>(base)*value/100.0)-base;break;
            default:throw std::runtime_error("Unsupported passive increment");
            }
        }
        if(total<std::numeric_limits<int>::min()||total>std::numeric_limits<int>::max())
            throw std::runtime_error("Passive total outside original INT range");
        return static_cast<int>(total);
    };
    std::vector<AbilityEffect> instance_abilities;
    const auto delta=[&](std::uint32_t base,int target) {
        // CalcAbilityValue clamps the active subtotal before adding passives;
        // both groups calculate against the original base, not each other.
        const long long active_delta=static_cast<long long>(changes(active,base,target))+changes(instance_abilities,base,target);
        const auto passive_delta=changes(passives,base,target);
        const long long value=std::max(0LL,static_cast<long long>(base)+active_delta)+passive_delta;
        const long long result=std::max(0LL,value)-base;
        if(result<std::numeric_limits<int>::min()||result>std::numeric_limits<int>::max())throw std::runtime_error("Effect total overflow");
        return static_cast<int>(result);
    };
    const auto stat=[&](const char* column,int type,std::uint16_t minimum=0){
        const auto base=static_cast<std::uint16_t>(1+U16(cl,column)+U16(ra,column));
        float value=static_cast<float>(std::max(base,minimum)*std::pow(static_cast<double>(growth.rate),int(s.bLevel)-1));
        if(s.bLevel>=10){const float reduction=static_cast<float>(std::min<int>(s.bAftermath,100)*0.3);
            value-=value*reduction/100;}
        value+=equipment[type];value+=delta(Truncate(value),type);return value;
    };
    const auto maximum=[&](const Formula& formula,float stat_value,int type){
        const std::uint32_t base=formula.initial+Truncate(stat_value*formula.rate)+equipment[type];
        const long long value=static_cast<long long>(base)+delta(base,type);
        if(value>std::numeric_limits<std::uint32_t>::max())throw std::runtime_error("Maximum stat overflow");
        return static_cast<std::uint32_t>(std::max(0LL,value));
    };
    s.dwMaxHP=maximum(hp,stat("wCON",3),50);s.dwMaxMP=maximum(mp,stat("wMEN",6),51);
    s.dwHP=std::min(s.dwHP,s.dwMaxHP);s.dwMP=std::min(s.dwMP,s.dwMaxMP);s.bDead=s.dwHP==0;
    p.skill_attack_timing.reset();
    // Active buffs can change rates, suppress equipment or disguise the caster.
    // Keep those graphs intact, but refuse speed-dependent casts until their
    // authoritative effect/expiry simulation is implemented.
    if(supported_effects) {
        const auto physical=ReadFormula(sql,4),magic=ReadFormula(sql,16);
        const auto base=[&](const Formula& f,const char* stat){
            return skill_timing::BaseAttackDelay(f.initial,f.rate,f.rate_y,U16(ra,stat),U16(cl,stat));
        };
        const auto normal=base(physical,"wDEX"),magical=base(magic,"wWIS");
        p.skill_attack_timing=std::array<SkillAttackTiming,3>{
            skill_timing::AttackTiming(normal,short_weapon_delay,delta(100,54),equipment[54]),
            skill_timing::AttackTiming(normal,long_weapon_delay,delta(100,55),equipment[55]),
            skill_timing::AttackTiming(magical,short_weapon_delay,delta(100,56),equipment[56])};
        if(!s.cluster.guild&&(!p.transfer_state||(p.transfer_state->companions.empty()&&p.transfer_state->recalls.empty()&&!p.transfer_state->local_id))) {
            static constexpr std::array columns{"wSTR","wDEX","wCON","wINT","wWIS","wMEN"};
            std::array<std::uint32_t,6> unscaled{};
            for(unsigned i=0;i<6;++i)unscaled[i]=1U+U16(cl,columns[i])+U16(ra,columns[i]);
            std::map<unsigned,FormulaRow> formulas;
            const auto build=[&]{return character_statistics::Build(s.bLevel,s.bAftermath,unscaled,attributes,*p.skill_attack_timing,
                [&](unsigned id){auto it=formulas.find(id);if(it==formulas.end()){const auto f=ReadFormula(sql,id);it=formulas.emplace(id,FormulaRow{f.initial,f.rate,f.rate_y}).first;}return it->second;},
                [&](unsigned i,std::uint16_t minimum){return stat(columns[i],i+1,minimum);},delta,
                [&](unsigned type){return equipment[type];});};
            p.statistics=build();
            for(std::size_t n=0;n<p.skill_templates.size();++n) {
                auto& definition=p.skill_templates[n];const auto& instance=instances.at(definition.wID);
                instance_abilities=instance.abilities;
                const auto cast=build();
                definition.attack_profile=SkillAttackProfile{s.bLevel,s.bAftermath,p.skills[n].bLevel,instance.attack_type,
                    instance.attack_type==3?cast.magic_critical:cast.physical_critical,instance.long_attack,
                    cast.attack_level,cast.magic_attack_level,
                    instance.long_attack?cast.min_ranged:cast.min_physical,instance.long_attack?cast.max_ranged:cast.max_physical,
                    cast.min_magic,cast.max_magic};
            }
        }
    }
}
ItemInstance ProjectItem(soci::session& sql,const transfer::Item& raw) {
    if(!raw.id)throw std::runtime_error("Character item has invalid identity");
    ItemInstance item;
    item.dlID=raw.id;item.bItemID=raw.slot;item.bInvenID=static_cast<std::uint8_t>(raw.storage_id);
    item.wItemID=raw.item;item.bLevel=raw.level;item.bGem=raw.gem;item.wMoggItemID=raw.appearance;
    item.wCompanion=static_cast<std::uint16_t>(raw.companion);item.bCount=raw.count;item.bGLevel=raw.grade;
    item.dwDuraMax=raw.durability_max;item.dwDuraCur=raw.durability;item.bRefineCur=raw.refine;
    item.dEndTime=raw.expires;item.bGradeEffect=raw.grade_effect;item.bELD=static_cast<std::uint8_t>(raw.eld);
    item.bWrap=static_cast<std::uint8_t>(raw.wrap);item.wColor=static_cast<std::uint16_t>(raw.color);
    item.dwGuildBound=raw.guild;item.wCustomTex=static_cast<std::uint16_t>(raw.texture);
    item.source=std::make_shared<const transfer::Item>(raw);
    const int template_id=std::bit_cast<std::int16_t>(raw.item);soci::row chart;
    sql<<"SELECT * FROM character_compat.\"TITEMCHART\" WHERE \"wItemID\"=:id",soci::use(template_id),soci::into(chart);
    if(!sql.got_data())throw std::runtime_error("Character item has no source template");
    item.bRefineMax=U8(chart,"bRefineMax");item.bKind=U8(chart,"bKind");item.stack_limit=U8(chart,"bStack");
    item.equipment=EquipmentRules{U32(chart,"dwSlotID"),U32(chart,"dwClassID"),U8(chart,"bPrmSlotID"),
        U8(chart,"bSubSlotID"),U8(chart,"bLevel"),U8(chart,"bEquipSkill")};
    std::set<std::uint8_t> ids;
    for(const auto& m:raw.magic){
        const int id=m.id;const auto value=m.value;
        if(!id||!value||!ids.insert(m.id).second)throw std::runtime_error("Invalid raw item magic");
        soci::row magic;sql<<"SELECT * FROM actor_compat.\"TITEMMAGICCHART\" WHERE \"bMagic\"=:id",soci::use(id),soci::into(magic);
        if(!sql.got_data())throw std::runtime_error("Item magic has no source template");
        const char* revisions[]={"fRevision","fMRevision","fAtRate","fMAtRate"};
        const auto type=U8(magic,"bRvType");if(type>4)throw std::runtime_error("Invalid item revision type");
        const float revision=type?static_cast<float>(chart.get<double>(revisions[type-1])):1.0f;
        const auto truncated=Truncate(revision*value*U16(magic,"wMaxValue"));
        const auto derived=static_cast<std::uint16_t>(std::max<unsigned>(static_cast<std::uint16_t>(truncated)/100,1));
        item.magic.emplace_back(m.id,derived);
    }
    return item;
}

}
void PostgreSQLMapService::RefreshEquipment(soci::session& sql,CharSnapshot& s,bool derive) {
    auto p=std::make_shared<CharacterPayload>(*s.payload);
    if(!p->statistics)throw std::runtime_error("Equipment requires supported source-derived statistics");
    if(!p->recalls.empty()||s.cluster.party)
        throw std::runtime_error("Equipment active recall or party dependencies are not implemented");
    // TPETTABLE is account-owned inactive pet inventory, not an active stat
    // effect. Active companion/recall graphs already lack a supported sheet.
    for(auto& bag:p->bags)for(auto& item:bag.items) {
        if(!item.source)throw std::runtime_error("Equipment item lacks original values");
        auto projected=ProjectItem(sql,*item.source);projected.durable_hash=item.durable_hash;item=std::move(projected);
    }
    p->equipment_kinds.fill(false);
    for(const auto& skill:p->skills) {
        const int id=std::bit_cast<std::int16_t>(skill.wSkillID);
        soci::rowset<int> kinds=(sql.prepare<<"SELECT \"bExec\" FROM character_compat.\"TSKILLDATA\" WHERE \"wSkillID\"=:id AND \"bType\"=0",soci::use(id));
        for(int kind:kinds)if(kind>=0&&kind<256)p->equipment_kinds[kind]=true;
    }
    if(derive)DeriveStats(sql,s,*p);
    s.payload=std::move(p);
}
std::optional<CharSnapshot> PostgreSQLMapService::LoadAuthorized(const MapSessionClaim& claim) {
    auto lease=m_pool.Acquire();auto& sql=*lease;
    auto tx=BeginMapTransaction(sql,m_config.world,m_config.server,m_config.owner_token,true);
    std::string phase;
    if(!LockAccount(sql,claim)||!LockClaim(sql,claim,phase)||phase!="claimed")return {};
    CheckCatalogs(sql);
    if(auto restored=RestoreTransferCheckpoint(sql,claim)) {
        const long long key=claim.key;
        sql<<"UPDATE app_world.map_sessions SET phase='loaded',updated_at=clock_timestamp() WHERE session_key=:k",soci::use(key);
        tx->commit();return restored;
    }
    const int world=claim.group;const long long character=claim.char_id,user=claim.user_id,key=claim.key;
    soci::row row;
    sql<<"SELECT c.* FROM app_world.\"TCHARTABLE\" c JOIN app_global.\"TALLCHARTABLE\" d "
         "ON d.\"bWorldID\"=c.\"bWorldID\" AND d.\"dwCharID\"=c.\"dwCharID\" "
         "WHERE c.\"bWorldID\"=:w AND c.\"dwCharID\"=:c AND c.\"dwUserID\"=:u AND c.\"bDelete\"=0 "
         "AND d.\"dwUserID\"=:u AND d.\"bDelete\"=0 FOR UPDATE OF c",
        soci::use(world,"w"),soci::use(character,"c"),soci::use(user,"u"),soci::into(row);
    if(!sql.got_data())return {};
    CharSnapshot s;
    s.dwCharID=U32(row,"dwCharID");
    s.bStartAct=U8(row,"bStartAct");
    s.bRealSex=U8(row,"bRealSex");
    s.bClass=U8(row,"bClass");
    s.bLevel=U8(row,"bLevel");
    s.bRace=U8(row,"bRace");
    s.bCountry=U8(row,"bCountry");
    s.bOriCountry=U8(row,"bOriCountry");
    s.bSex=U8(row,"bSex");
    s.bHair=U8(row,"bHair");
    s.bFace=U8(row,"bFace");
    s.bBody=U8(row,"bBody");
    s.bPants=U8(row,"bPants");
    s.bHand=U8(row,"bHand");
    s.bFoot=U8(row,"bFoot");
    s.bHelmetHide=U8(row,"bHelmetHide");
    s.dwGold=U32(row,"dwGold");
    s.dwSilver=U32(row,"dwSilver");
    s.dwCooper=U32(row,"dwCooper");
    s.dwEXP=U32(row,"dwEXP");
    s.dwHP=U32(row,"dwHP");
    s.dwMP=U32(row,"dwMP");
    s.wSkillPoint=U16(row,"wSkillPoint");
    s.dwRegion=U32(row,"dwRegion");
    s.bGuildLeave=U8(row,"bGuildLeave");
    s.dwGuildLeaveTime=U32(row,"dwGuildLeaveTime");
    s.wMapID=U16(row,"wMapID");
    s.wSpawnID=U16(row,"wSpawnID");
    s.wLastSpawnID=U16(row,"wLastSpawnID");
    s.dwLastDestination=U32(row,"dwLastDestination");
    s.wTemptedMon=U16(row,"wTemptedMon");
    s.bAftermath=U8(row,"bAftermath");
    s.fPosX=static_cast<float>(row.get<double>("fPosX"));
    s.fPosY=static_cast<float>(row.get<double>("fPosY"));
    s.fPosZ=static_cast<float>(row.get<double>("fPosZ"));
    s.wDIR=U16(row,"wDIR");
    s.bStatLevel=U8(row,"bStatLevel");
    s.bStatPoint=U8(row,"bStatPoint");
    s.dwStatExp=U32(row,"dwStatExp");
    s.szNAME=row.get<std::string>("szNAME");
    if(!std::isfinite(s.fPosX)||!std::isfinite(s.fPosY)||!std::isfinite(s.fPosZ))throw std::runtime_error("Invalid character position");
    auto payload=std::make_shared<CharacterPayload>();auto& p=*payload;
    p.effects.emplace();
    {soci::rowset<soci::row> effects=(sql.prepare<<"SELECT * FROM app_world.map_maintained_effects WHERE world_id=:w AND char_id=:c ORDER BY ordinal",
        soci::use(world,"w"),soci::use(character,"c"));
     for(const auto& r:effects) {
        if(U16(r,"ordinal")!=p.effects->size())throw std::runtime_error("Maintained effect order has a gap");
        MaintainedEffect e;e.skill=U16(r,"skill_id");e.level=U8(r,"skill_level");e.remaining=U32(r,"remaining");
        e.attack_type=U8(r,"attack_type");e.attack_id=U32(r,"attack_id");e.host_type=U8(r,"host_type");
        e.host_id=U32(r,"host_id");e.attack_country=U8(r,"attack_country");p.effects->push_back(e);
     }}
    if(!SupportedPostures(p))throw std::runtime_error("Fresh maintained effect needs unimplemented native simulation");
    p.rank_point=U32(row,"dwRankPoint");
    int lucky=0;sql<<"SELECT \"bLuckyNumber\" FROM app_global.\"TCURRENTUSER\" WHERE \"dwKEY\"=:k",soci::use(key),soci::into(lucky);
    p.lucky_number=static_cast<std::uint8_t>(lucky);
    // Guild reconstruction is a separate contract. Refuse unsupported persisted
    // membership rather than announcing a guild member as guildless.
    int guild=0;sql<<"SELECT count(*) FROM app_world.guild_membership WHERE world_id=:w AND char_id=:c",
        soci::use(world,"w"),soci::use(character,"c"),soci::into(guild);
    if(guild)throw std::runtime_error("Native Map guild hydration is not implemented");
    const auto children=[&](const char* table,const char* owner="dwCharID",std::string extra="",std::string order=""){
        return soci::rowset<soci::row>((sql.prepare<<"SELECT *"+extra+" FROM app_world.\""+table+
            "\" WHERE \"bWorldID\"=:w AND \""+owner+"\"=:c"+order,soci::use(world,"w"),soci::use(character,"c")));
    };
    for(const auto& r:children("TINVENTABLE","dwCharID",","+Epoch("dEndTime")," ORDER BY \"bInvenID\"")) {
        p.bags.push_back({{U8(r,"bInvenID"),U16(r,"wItemID"),Number(r,"epoch"),U8(r,"bELD")},{}});
    }
    for(const auto& r:children("TITEMTABLE","dwOwnerID",","+Epoch("dEndTime")+",app_world.item_fingerprint(\"TITEMTABLE\") AS durable_hash"," ORDER BY \"dwStorageID\",\"bItemID\"")) {
        const auto bag_id=Number(r,"dwStorageID");
        auto bag=std::find_if(p.bags.begin(),p.bags.end(),[&](const auto& b){return b.bag.bInvenID==bag_id;});
        if(bag==p.bags.end())throw std::runtime_error("Character item has no inventory bag");
        transfer::Item raw;
        raw.storage=U8(r,"bStorageType");raw.storage_id=U32(r,"dwStorageID");
        raw.owner_type=U8(r,"bOwnerType");raw.owner_id=U32(r,"dwOwnerID");
        raw.id=Number(r,"dlID");raw.slot=U8(r,"bItemID");raw.item=U16(r,"wItemID");
        raw.level=U8(r,"bLevel");raw.gem=U8(r,"bGem");raw.appearance=U16(r,"wMoggItemID");
        raw.companion=U32(r,"dwTime5");raw.count=U8(r,"bCount");raw.grade=U8(r,"bGLevel");
        raw.durability_max=U32(r,"dwDuraMax");raw.durability=U32(r,"dwDuraCur");
        raw.refine=U8(r,"bRefineCur");raw.expires=Number(r,"epoch");raw.grade_effect=U8(r,"bGradeEffect");
        raw.eld=U32(r,"dwTime1");raw.wrap=U32(r,"dwTime2");raw.color=U32(r,"dwTime3");
        raw.guild=U32(r,"dwTime4");raw.texture=U32(r,"dwTime6");
        std::map<int,std::uint16_t> magic;
        for(int i=1;i<=6;++i){const auto suffix=std::to_string(i);const auto id=U8(r,"bMagic"+suffix),value_id=id;
            const auto value=U16(r,"wValue"+suffix);if(value_id&&value)magic[id]=value;}
        for(const auto& [id,value]:magic)raw.magic.push_back({static_cast<std::uint8_t>(id),value});
        bag->items.push_back(ProjectItem(sql,raw));
        bag->items.back().durable_hash=r.get<std::string>("durable_hash");
    }
    for(const auto& r:children("TSKILLTABLE","dwCharID",""," ORDER BY (\"wSkillID\"::integer & 65535)"))
        p.skills.push_back({U16(r,"wSkillID"),U8(r,"bLevel"),U32(r,"dwRemainTick")});
    for(const auto& r:children("THOTKEYTABLE","dwCharID",""," ORDER BY \"bInvenID\"")) {
        CharacterHotkeys h;h.inventory=U8(r,"bInvenID");
        for(int i=0;i<12;++i){auto suffix=std::to_string(i+1);h.keys[i]={U8(r,"bType"+suffix),U16(r,"wID"+suffix)};}
        p.hotkeys.push_back(h);
    }
    for(const auto& r:children("TTITLETABLE")){
        CharacterTitle t{U16(r,"wTitleID"),U8(r,"bSelected")!=0};
        if(t.selected){if(p.selected_title)throw std::runtime_error("Multiple selected character titles");p.selected_title=t.id;}
        p.titles.push_back(t);
    }
    for(const auto& r:children("TCABINETTABLE"))p.cabinets.push_back({U8(r,"bCabinetID"),U8(r,"bUse")});
    for(const auto& r:children("TPOSTTABLE","dwCharID",","+Epoch("timeRecv"))) {
        CharacterPost post;post.id=U32(r,"dwPostID");post.sender_id=U32(r,"dwSendID");
        post.type=U8(r,"bType");post.read=U8(r,"bRead");post.received_at=Number(r,"epoch");
        post.recipient=r.get<std::string>("szRecvName");post.sender=r.get<std::string>("szSender");
        post.title=r.get<std::string>("szTitle");post.message=r.get<std::string>("szMessage");
        post.gold=U32(r,"dwGold");post.silver=U32(r,"dwSilver");post.cooper=U32(r,"dwCooper");p.posts.push_back(std::move(post));
    }
    for(const auto& r:children("TRECALLMONTABLE","dwOwnerID")) {
        p.recalls.push_back({U32(r,"dwID"),U32(r,"dwATTR"),U32(r,"dwHP"),U32(r,"dwMP"),U32(r,"dwTime"),
            U16(r,"wMonID"),U16(r,"wPetID"),U16(r,"wPosX"),U16(r,"wPosY"),U16(r,"wPosZ"),U8(r,"bLevel"),U8(r,"bSkillLevel"),U8(r,"bEffect")});
    }
    {soci::rowset<soci::row> pets=(sql.prepare<<"SELECT *,"+Epoch("timeUse")+" FROM app_world.\"TPETTABLE\" WHERE \"bWorldID\"=:w AND \"dwUserID\"=:u",
        soci::use(world,"w"),soci::use(user,"u"));
     for(const auto& r:pets)p.pets.push_back({U16(r,"wPetID"),r.get<std::string>("szName"),Number(r,"epoch"),U8(r,"bEffect")});}
    // Active recalls need their own admission/spawn contract before use. Rows
    // remain intact when admission is refused; never quietly lose companions.
    if(!p.recalls.empty())throw std::runtime_error("Native recall admission is not implemented");
    for(int i=0;i<2;++i){const int level=static_cast<int>(s.bLevel)-1+i;int experience=0;
        sql<<"SELECT \"dwEXP\" FROM character_compat.\"TLEVELCHART\" WHERE \"bLevel\"=:level",soci::use(level),soci::into(experience);
        if(!sql.got_data())throw std::runtime_error("Character level has no source experience threshold");
        (i?p.next_exp:p.prev_exp)=static_cast<std::uint32_t>(experience);}
    ByteCount(p.bags.size());ByteCount(p.skills.size());ByteCount(p.hotkeys.size());
    for(const auto& bag:p.bags)ByteCount(bag.items.size());
    DeriveStats(sql,s,p);s.payload=std::move(payload);
    sql<<"UPDATE app_world.map_sessions SET phase='loaded',updated_at=clock_timestamp() WHERE session_key=:k",soci::use(key);
    tx->commit();return s;
}

CharSnapshot PostgreSQLMapService::HydrateTransfer(soci::session& sql,const transfer::State& t) const {
    CharSnapshot s=t.character;s.payload.reset();auto payload=std::make_shared<CharacterPayload>();auto& p=*payload;
    auto graph=std::make_shared<transfer::State>(t);graph->character.payload.reset();
    p.transfer_state=std::move(graph);p.transfer_received_ms=SkillClockMs();
    p.skills=t.skills;p.cabinets=t.cabinets;p.titles=t.titles;p.recalls=t.recalls;p.pets=t.pets;
    p.effects=t.buffs;
    p.aid_country=t.aid_country;p.lucky_number=t.lucky;p.rank_point=t.rank_points;
    for(const auto& h:t.hotkeys)p.hotkeys.push_back(h.row);
    std::set<std::uint8_t> bags;std::set<std::uint64_t> items;std::set<std::pair<std::uint32_t,std::uint8_t>> slots;
    for(const auto& bag:t.bags){if(!bags.insert(bag.bInvenID).second)throw std::runtime_error("Duplicate transferred bag");p.bags.push_back({bag,{}});}
    for(const auto& raw:t.items){
        if(!raw.id||!items.insert(raw.id).second)throw std::runtime_error("Duplicate transferred item");
        // All storage kinds remain in the typed server graph. Only inventory
        // items belong in the existing client inventory projection.
        if(raw.storage!=0)continue;
        if(raw.owner_type!=0||raw.owner_id!=s.dwCharID||!slots.insert({raw.storage_id,raw.slot}).second)
            throw std::runtime_error("Invalid transferred inventory owner or slot");
        auto bag=std::find_if(p.bags.begin(),p.bags.end(),[&](const auto& b){return b.bag.bInvenID==raw.storage_id;});
        if(bag==p.bags.end())throw std::runtime_error("Transferred item has no bag");
        bag->items.push_back(ProjectItem(sql,raw));
        bag->items.back().durable_hash=GraphItemFingerprint(raw);
    }
    std::set<std::uint16_t> skills;
    for(const auto& skill:p.skills)if(!skills.insert(skill.wSkillID).second)throw std::runtime_error("Duplicate transferred skill");
    for(const auto& title:p.titles)if(title.selected){if(p.selected_title)throw std::runtime_error("Multiple transferred titles");p.selected_title=title.id;}
    for(int i=0;i<2;++i){const int level=static_cast<int>(s.bLevel)-1+i;int experience=0;
        sql<<"SELECT \"dwEXP\" FROM character_compat.\"TLEVELCHART\" WHERE \"bLevel\"=:level",soci::use(level),soci::into(experience);
        if(!sql.got_data())throw std::runtime_error("Transferred level has no source threshold");
        (i?p.next_exp:p.prev_exp)=static_cast<std::uint32_t>(experience);}
    ByteCount(p.bags.size());ByteCount(p.skills.size());ByteCount(p.hotkeys.size());for(const auto& b:p.bags)ByteCount(b.items.size());
    DeriveStats(sql,s,p);
    // Never silently clamp unsaved transferred core to an incomplete derived
    // stat model. Active-effect stat derivation remains a gameplay dependency.
    if(s.dwHP!=t.character.dwHP||s.dwMP!=t.character.dwMP)throw std::runtime_error("Transferred vitals exceed derived state");
    s.payload=std::move(payload);return s;
}

}
