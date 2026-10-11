#include "postgresql_map_service.h"
#include "MessageId.h"
#include "posture_effects.h"
#include <soci/soci.h>
#include <bit>
#include <cmath>
#include <map>

namespace tmapsvr {
void PostgreSQLMapService::AdvanceProgressEffects(soci::session& sql,const MapSessionClaim& c,CharSnapshot& s,
    std::uint64_t now,const QuestEvent& event,QuestPackets& packets) {
    using tnetlib::protocol::MessageId;
    if(!s.payload||!SupportedPostures(*s.payload))throw std::runtime_error("Effect timer requires supported hydrated source effects");
    // CTObjBase::CheckMaintainSkill: expiry precedes automatic EXP. Delayed
    // ticks grant one charge, not a catch-up loop. Offline time is not charged.
    bool exp_expired=false;
    for(std::size_t i=0;i<s.payload->effects->size();) {
        if(IsNativePosture(s.payload->effects->at(i).skill)){++i;continue;}
        auto p=std::make_shared<CharacterPayload>(*s.payload);s.payload=p;
        auto& effect=p->effects->at(i);
        const auto definition=std::find_if(p->progress_effect_templates.begin(),p->progress_effect_templates.end(),
            [&](const auto& def){return def.skill==effect.skill;});
        if(definition==p->progress_effect_templates.end()||!effect.sampled_ms||now<effect.sampled_ms||now<effect.charge_ms)
            throw std::runtime_error("Progression effect has no valid local clock/template");
        const auto elapsed=now-effect.sampled_ms;
        if(effect.remaining&&elapsed>=effect.remaining) {
            exp_expired=exp_expired||effect.skill==903;
            EraseProgressEffect(sql,c,s,i,packets);
            continue;
        }
        if(effect.remaining)effect.remaining-=static_cast<std::uint32_t>(elapsed);
        effect.sampled_ms=now;
        const auto charge=now-effect.charge_ms;
        if(definition->status==67&&definition->value&&charge>=10000) {
            effect.charge_ms+=charge>=20000?charge:10000;
            int nation=0,compatible=0;
            sql << "SELECT count(*) FROM game_compat.quest_reward_release r JOIN game_compat.catalog_release m "
                "ON m.manifest_sha256=r.map_manifest AND m.status='verified' "
                "JOIN character_compat.catalog_release c ON c.manifest_sha256=r.character_manifest AND c.status='verified' "
                "JOIN actor_compat.catalog_release a ON a.manifest_sha256=r.actor_manifest AND a.status='verified' WHERE r.status='verified'",
                soci::into(compatible);
            if(compatible!=1)throw std::runtime_error("Automatic EXP requires the published progression catalog");
            sql << "SELECT \"bNation\" FROM game_compat.\"TLIMITEDLEVELCHART\"",soci::into(nation);
            if(!sql.got_data()||!nation)throw std::runtime_error("Source TGetNation is unavailable");
            const auto base=static_cast<int>(30.0f*std::log10(double(s.bLevel+1))+s.bLevel*2);
            const auto scaled=static_cast<std::uint32_t>((s.bLevel>79?1.85f:1.0f)*base);
            auto gain=static_cast<std::uint32_t>(definition->value)*scaled;
            const auto rate=nation==1?100U:64U-std::min<unsigned>(39,s.bLevel)*9U/39U;
            gain=static_cast<std::uint32_t>(gain*rate)/100U;
            if(gain)GainQuestExperience(sql,c,s,gain,event,packets);
        }
        ++i;
    }
    if(exp_expired)HangExpBenefit(sql,c,s,packets);
}
void PostgreSQLMapService::GainQuestExperience(soci::session& sql,const MapSessionClaim& c,CharSnapshot& s,
    std::uint32_t gain,const QuestEvent& event,QuestPackets& packets) {
    using tnetlib::protocol::MessageId;
    // CTPlayer::GainExp(gain,FALSE), ordinary Map build. BOW's distinct
    // compile-time no-op is not selected by this ordinary native runtime.
    if(!gain||s.bDead)return;
    struct Level {std::uint32_t exp{};std::uint8_t points{};};
    std::map<std::uint8_t,Level> levels;
    {soci::rowset<soci::row> rows=(sql.prepare << "SELECT \"bLevel\",\"dwEXP\",\"bSkillPoint\" FROM character_compat.\"TLEVELCHART\"");
     for(const auto& row:rows)if(!levels.emplace(static_cast<std::uint8_t>(row.get<int>(0)),
        Level{static_cast<std::uint32_t>(row.get<int>(1)),static_cast<std::uint8_t>(row.get<int>(2))}).second)
        throw std::runtime_error("Ambiguous source level definition");}
    int maximum=0;
    sql << "SELECT \"bMaxLevel\" FROM game_compat.\"TLIMITEDLEVELCHART\"",soci::into(maximum);
    if(!sql.got_data())throw std::runtime_error("Original TGetLimitedLevel content unavailable");
    auto p=std::make_shared<CharacterPayload>(*s.payload);s.payload=p;
    const auto current=levels.find(s.bLevel),previous=levels.find(p->previous_level);
    if(current==levels.end()||previous==levels.end())throw std::runtime_error("Experience needs original current and previous level rows");
    const auto interval=static_cast<std::uint32_t>(current->second.exp-previous->second.exp);
    const auto new_soul=static_cast<std::uint32_t>(p->soul_exp+gain);
    if(interval<=new_soul) {
        if(!p->soul_ready) {
            p->soul_ready=true;p->soul_exp=interval;
            std::vector<std::byte> ack;wire::WritePOD(ack,c.char_id);
            packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_SOULUP_ACK),std::move(ack),QuestAudience::Neighbors);
        }
    }else p->soul_exp=new_soul;
    if(s.bLevel>=maximum)return; // source soul update precedes the level cap
    s.dwEXP+=gain;bool level_up=false;
    if(current->second.exp<=s.dwEXP) {
        auto next=levels.find(static_cast<std::uint8_t>(s.bLevel+1));
        if(next!=levels.end()&&next->second.exp<s.dwEXP) {
            auto found=levels.rbegin();
            while(found!=levels.rend()&&found->second.exp>s.dwEXP)++found;
            // Original decrements the reverse iterator then dereferences it.
            // Reject its undefined endpoint cases, never guess a level or XP cap.
            if(found==levels.rbegin()||found==levels.rend())throw std::runtime_error("Source large experience jump has no representable next level");
            --found;next=levels.find(found->first);
        }
        if(next==levels.end())return; // source keeps changed EXP without an ACK
        if(next->first>=maximum)s.dwEXP=current->second.exp;
        if(!interval)throw std::runtime_error("Source soul experience interval is zero");
        const float soul_fraction=static_cast<float>(p->soul_exp)/static_cast<float>(interval);
        s.bLevel=next->first;p->next_exp=next->second.exp;level_up=true;
        RefreshEquipment(sql,s,true);
        for(unsigned i=previous->first;i<s.bLevel;++i) {
            const auto row=levels.find(static_cast<std::uint8_t>(i));if(row==levels.end())continue;
            if(i!=s.payload->previous_level&&i%10==8) {
                const unsigned index=i/10;const int item=index>6?18184:index>3?18183:18182;int kind=0;
                sql << "SELECT \"bKind\" FROM character_compat.\"TITEMCHART\" WHERE \"wItemID\"=:id",soci::use(item),soci::into(kind);
                if(!sql.got_data())continue; // original also skips this level's quest/points
                if(kind!=56||!s.payload->during||s.payload->during->pending_entry||!s.payload->event_ms)
                    throw std::runtime_error("Level benefit requires the admitted original EXP item contract");
                p=std::make_shared<CharacterPayload>(*s.payload);s.payload=p;
                p->during->items={{static_cast<std::uint16_t>(item),1,1800,p->event_time+1800}};
                HangExpBenefit(sql,c,s,packets);
                ForceProgressEffect(sql,c,s,923,1800000,packets);
                if(index>=7)ForceProgressEffect(sql,c,s,918,10800000,packets);
            }
            event(i,0,TT_LEVELUP,1);
            p=std::make_shared<CharacterPayload>(*s.payload);s.payload=p;
            s.wSkillPoint=static_cast<std::uint16_t>(s.wSkillPoint+row->second.points);
        }
        p=std::make_shared<CharacterPayload>(*s.payload);s.payload=p;
        if(int(p->previous_level)-1==s.bLevel)p->previous_level=s.bLevel;
        else if(levels.contains(static_cast<std::uint8_t>(s.bLevel-1)))p->previous_level=static_cast<std::uint8_t>(s.bLevel-1);
        p->prev_exp=levels.at(p->previous_level).exp;p->next_exp=levels.at(s.bLevel).exp;
        const float soul=static_cast<float>(static_cast<std::uint32_t>(p->next_exp-p->prev_exp))*soul_fraction;
        if(!std::isfinite(soul)||soul<0||static_cast<double>(soul)>=4294967296.0)
            throw std::runtime_error("Source soul conversion is outside DWORD range");
        p->soul_exp=static_cast<std::uint32_t>(soul);
        RefreshEquipment(sql,s,true);s.dwHP=s.dwMaxHP;s.dwMP=s.dwMaxMP;
        std::vector<std::byte> world;wire::WritePOD(world,c.char_id);wire::WritePOD(world,c.key);wire::WritePOD(world,s.bLevel);
        packets.emplace_back(static_cast<std::uint16_t>(MessageId::MW_LEVELUP_ACK),std::move(world),QuestAudience::World);
        if(!s.payload->statistics)throw std::runtime_error("Level progression requires complete source statistics");
        packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_CHARSTATINFO_ACK),EncodeCharacterStatistics(s,*s.payload->statistics));
        std::vector<std::byte> level;wire::WritePOD(level,c.char_id);wire::WritePOD(level,s.bLevel);wire::WritePOD(level,std::uint8_t{1});
        packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_LEVEL_ACK),std::move(level),QuestAudience::Neighbors);
        packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_HPMP_ACK),EncodeHpMpAck(c.char_id,1,s.dwMaxHP,s.dwHP,s.dwMaxMP,s.dwMP),QuestAudience::Neighbors);
    }
    if(level_up&&s.wMapID&&s.dwLastDestination) {
        const int portal=std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(s.dwLastDestination));
        const int destination=std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(s.dwLastDestination>>16));
        soci::rowset<soci::row> rows=(sql.prepare <<
            "SELECT d.\"bCondition1\",d.\"dwConditionID1\",d.\"bCondition2\",d.\"dwConditionID2\",d.\"bCondition3\",d.\"dwConditionID3\" "
            "FROM game_compat.\"TDESTINATIONCHART\" d JOIN game_compat.\"TPORTALCHART\" p ON p.\"wPortalID\"=d.\"wPortalID\" "
            "WHERE d.\"wPortalID\"=:portal AND d.\"wDestID\"=:dest",soci::use(portal,"portal"),soci::use(destination,"dest"));
        bool kick=false;
        for(const auto& row:rows)for(unsigned i=0;i<3;++i)
            if(row.get<int>(2*i)==6&&static_cast<std::uint32_t>(row.get<int>(2*i+1))<s.bLevel)kick=true;
        if(kick)packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_KICKOUTMAP_ACK),std::vector<std::byte>{});
    }
    packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_EXP_ACK),EncodeExpAck(s.dwEXP,s.payload->prev_exp,s.payload->next_exp,s.payload->soul_exp));
}
}
