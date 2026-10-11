#pragma once
#include "quest_runtime.h"

namespace tmapsvr::quest_runtime {
// CTPlayer::CheckLevelCondition, CheckQuestCondition and CanRunQuest. The
// result is the original QCT_* byte, not an invented boolean accept rule.
struct ConditionInputs {
    std::function<std::uint8_t()> probability;
    std::function<std::optional<std::uint8_t>(std::uint32_t)> switch_state;
};
inline const QuestTermDef* Incomplete(const CharSnapshot& s,QuestProgressRow& q,const QuestDef& def) {
    for(const auto& t:def.terms)if(TermStatus(s,q,t)!=QTS_SUCCESS)return &t;
    return nullptr;
}
inline bool HasItem(const CharacterPayload& p,std::uint32_t id) {
    for(const auto& bag:p.bags)for(const auto& item:bag.items)if(item.wItemID==id)return true;
    return false;
}
inline std::uint8_t CheckConditions(CharSnapshot& s,CharacterPayload& p,const QuestDef& def,
    const IQuestChart& chart,const ConditionInputs& inputs,std::uint8_t& level) {
    HydrateLevels(p,chart);
    std::uint8_t level_result=0,result=0;
    for(const auto& c:def.conditions) {
        if(c.type==1) {level=static_cast<std::uint8_t>(c.id);if(s.bLevel<c.id)level_result=1;}
        else if(c.type==2&&s.bLevel>c.id)level_result=2;
        if(level_result)break;
    }
    for(const auto& c:def.conditions) {
        result=c.type;
        const auto accept=[&](bool value){if(value)result=0;};
        switch(c.type) {
        case 1:case 2:result=0;break;
        case 7:accept(s.wMapID==c.id);break;
        case 8:accept(s.fPosX>=c.id);break;
        case 9:accept(s.fPosZ<=c.id);break;
        case 10:accept(s.fPosX<=c.id);break;
        case 11:accept(s.fPosZ>=c.id);break;
        case 12:
            if(!inputs.probability)throw std::runtime_error("Quest probability source missing");
            accept(inputs.probability()<c.id);break;
        case 5:
            if(s.bClass>=32)throw std::runtime_error("Quest class bit shift is out of source range");
            accept((c.id&(std::uint32_t{1}<<s.bClass))!=0);break;
        case 3: {
            auto* parent=Find(Rows(p),c.id);
            const auto* parent_def=chart.Find(c.id);
            if(!parent||!parent_def||!Running(*parent))break;
            if(def.bType==QT_DROPITEM) {
                result=0;
                for(const auto& term:parent_def->terms)for(const auto& target:def.terms)
                    if(term.dwTermID==target.dwTermID&&TermStatus(s,*parent,term)==QTS_SUCCESS)result=3;
            } else {
                const auto* term=Incomplete(s,*parent,*parent_def);
                accept(def.bType==QT_COMPLETE||(term&&term->bTermType!=QTT_TIMER));
            }
            break;
        }
        case 4:accept(HasItem(p,c.id));break;
        case 13:accept(!HasItem(p,c.id));break;
        case 14:accept(c.id==(p.aid_country!=3?p.aid_country:s.bCountry));break;
        case 15: {
            const auto* q=Find(Rows(p),c.id);accept(q&&q->bCompleteCount&&q->bCompleteCount>=c.count);break;
        }
        case 16:accept(!p.quest_levels->contains(c.id));break;
        case 17:accept(c.id==s.bSex);break;
        case 18: {const auto* q=Find(Rows(p),c.id);accept(!q||Running(*q));break;}
        case 19:
            if(!p.effects)throw std::runtime_error("Quest maintained effects not hydrated");
            accept(std::any_of(p.effects->begin(),p.effects->end(),[&](const auto& e){
                return e.skill==static_cast<std::uint16_t>(c.id);
            }));break;
        case 20: {
            if(!inputs.switch_state)throw std::runtime_error("Quest Map switch state not integrated");
            const auto state=inputs.switch_state(c.id);accept(state&&*state==c.count);break;
        }
        default:break; // Source leaves QCT_MONID and unknown nonzero codes failed.
        }
        if(result&&!def.condition_check)return result;
        if(!result&&def.condition_check&&!level_result)return 0;
    }
    return result?result:level_result;
}
inline std::uint8_t CanRun(CharSnapshot& s,CharacterPayload& p,const QuestDef& def,
    const IQuestChart& chart,const ConditionInputs& inputs,std::uint8_t& level) {
    const auto* q=Find(Rows(p),def.dwQuestID);
    if(q&&(def.bCountMax?q->bTriggerCount>=def.bCountMax:Running(*q)))return 21;
    if(def.dwParentID) {
        const auto* parent=Find(Rows(p),def.dwParentID);
        if(!parent)return 22;
        const auto* parent_def=chart.Find(def.dwParentID);
        if(!parent_def)throw std::runtime_error("Loaded parent quest has no source definition");
        if(def.bType==QT_COMPLETE&&(!Running(*parent)||(parent_def->bCountMax&&parent->bCompleteCount>=parent_def->bCountMax)))return 21;
        if(def.bType==QT_MISSION&&(!parent->bTriggerCount||Running(*parent)))return 21;
    }
    if(const auto result=CheckConditions(s,p,def,chart,inputs,level))return result;
    return q&&Running(*q)?21:0;
}
}
