#pragma once

// Source CTPlayer quest progress operations. The legacy in-memory quest_log
// remains for its old backend; native gameplay owns progress in the immutable
// character payload and commits it through PostgreSQLMapService.
#include "domain/character.h"
#include "domain/quest_def.h"
#include "domain/main_transfer.h"
#include "services/quest_chart.h"
#include "services/client_senders.h"
#include "services/world_event_state.h"
#include "wire_codec.h"
#include "MessageId.h"
#include <algorithm>
#include <map>
#include <functional>
#include <span>
#include <stdexcept>

namespace tmapsvr {
enum class QuestAudience : std::uint8_t { Owner, Neighbors, World };
struct QuestPacket {
    std::uint16_t opcode;
    std::vector<std::byte> body;
    QuestAudience audience;
    QuestPacket(std::uint16_t op,std::vector<std::byte> bytes,QuestAudience target=QuestAudience::Owner)
        :opcode(op),body(std::move(bytes)),audience(target){}
};
using QuestPackets = std::vector<QuestPacket>;
struct QuestMapFacts {
    std::uint16_t map{};
    std::uint8_t channel{};
    bool world_registered{};
    std::uint64_t sample_ms{}; // local scheduler observation, never a client field
    std::vector<std::pair<std::uint32_t,std::uint16_t>> monsters;
    std::optional<WorldEventSnapshot> world_events;
    std::uint8_t soulmate_min_level{200}; // source GetNeighbor result, never a client field
};
struct QuestCommit {
    std::shared_ptr<const CharSnapshot> snapshot;
    QuestPackets packets;
};
namespace quest_runtime {
inline void AlignGraphTimers(transfer::State& stored,const transfer::State& live) {
    if(stored.quests.size()!=live.quests.size())throw std::runtime_error("Quest graph membership changed");
    for(std::size_t i=0;i<stored.quests.size();++i) {
        auto& old=stored.quests[i];const auto& now=live.quests[i];
        if(old.id!=now.id||old.completed!=now.completed||old.triggered!=now.triggered||old.save!=now.save||now.remaining>old.remaining)
            throw std::runtime_error("Quest graph changed outside transaction");
        old.remaining=now.remaining;
    }
}
inline bool Running(const QuestProgressRow& q) { return q.bCompleteCount < q.bTriggerCount; }
inline auto& Rows(CharacterPayload& p) {
    if(!p.quests) throw std::runtime_error("Quest state was not hydrated");
    return *p.quests;
}
inline const auto& Rows(const CharacterPayload& p) {
    if(!p.quests) throw std::runtime_error("Quest state was not hydrated");
    return *p.quests;
}
inline QuestProgressRow* Find(std::vector<QuestProgressRow>& rows,std::uint32_t id) {
    const auto it=std::find_if(rows.begin(),rows.end(),[&](const auto& q){return q.dwQuestID==id;});
    return it==rows.end()?nullptr:&*it;
}
inline std::uint32_t LevelKey(const QuestDef& def) {
    for(const auto& c:def.conditions)if(c.type==16)return c.id;
    return 0;
}
inline void HydrateLevels(CharacterPayload& p,const IQuestChart& chart) {
    if(p.quest_levels)return;
    p.quest_levels.emplace();
    for(const auto& q:Rows(p))if(const auto* def=chart.Find(q.dwQuestID))
        if(const auto key=LevelKey(*def))p.quest_levels->insert(key);
}
inline void Sort(std::vector<QuestProgressRow>& rows) {
    std::sort(rows.begin(),rows.end(),[](const auto& a,const auto& b){return a.dwQuestID<b.dwQuestID;});
    for(std::size_t i=1;i<rows.size();++i)
        if(rows[i-1].dwQuestID==rows[i].dwQuestID)throw std::runtime_error("Duplicate quest identity");
}
inline std::string Json(const std::vector<QuestProgressRow>& input) {
    auto rows=input;Sort(rows);std::string out="[";
    for(auto& q:rows) {
        if(out.size()>1)out+=',';
        out+='['+std::to_string(q.dwQuestID)+','+std::to_string(q.dwTick)+','+
            std::to_string(q.bCompleteCount)+','+std::to_string(q.bTriggerCount)+",[";
        std::sort(q.terms.begin(),q.terms.end(),[](const auto& a,const auto& b){return a.dwTermID<b.dwTermID;});
        for(std::size_t i=0;i<q.terms.size();++i) {
            if(i)out+=',';
            const auto& t=q.terms[i];
            if(i&&t.dwTermID==q.terms[i-1].dwTermID)throw std::runtime_error("Duplicate source quest term key");
            out+='['+std::to_string(t.dwTermID)+','+std::to_string(t.bTermType)+','+std::to_string(t.bCount)+']';
        }
        out+="]]";
    }
    return out+']';
}
inline void Sample(CharacterPayload& p,std::uint64_t now) {
    if(!p.quests)return;
    const auto elapsed=now>=p.quest_sample_ms?now-p.quest_sample_ms:0;
    for(auto& q:*p.quests) {
        if(!Running(q)){q.dwTick=0;q.timer_active=false;q.timer_expired=false;}
        else if(q.timer_active) {
            if(elapsed>q.dwTick)q.timer_expired=true;
            q.dwTick=elapsed<q.dwTick?q.dwTick-static_cast<std::uint32_t>(elapsed):0;
        }
    }
    p.quest_sample_ms=now;
}
inline QuestTermRow* CountedTerm(QuestProgressRow& q,std::uint32_t id,std::uint8_t type) {
    for(auto& t:q.terms)if((t.dwTermID==id&&t.bTermType==type)||(type==21&&t.bTermType==21))return &t;
    if(type!=QTT_HUNT&&type!=QTT_TALK&&type!=QTT_USEITEM&&type!=QTT_SPAWNID_DEL)return nullptr;
    q.terms.push_back({id,type,0});return &q.terms.back();
}
inline std::uint16_t ItemCount(const CharacterPayload& p,std::uint32_t id) {
    std::uint16_t count=0;
    for(const auto& bag:p.bags)for(const auto& item:bag.items)
        if(item.wItemID==id)count=static_cast<std::uint16_t>(count+item.bCount);
    return count; // original WORD accumulation, including its bit-width wrap
}
inline std::uint8_t TermCount(const CharacterPayload& p,const QuestTermDef& term) {
    if(term.bTermType==QTT_GETITEM)return static_cast<std::uint8_t>(std::min<unsigned>(ItemCount(p,term.dwTermID),255));
    if(term.bTermType==QTT_QUESTCOMPLETED)
        for(const auto& q:Rows(p))if(q.dwQuestID==term.dwTermID)return q.bCompleteCount;
    return 0; // CTPlayer::GetTermCount's explicit default
}
inline std::uint8_t TermStatus(const CharSnapshot& s,QuestProgressRow& q,const QuestTermDef& term) {
    switch(term.bTermType) {
    case QTT_GETITEM:
        return !term.bGoalCount||ItemCount(*s.payload,term.dwTermID)<term.bGoalCount?QTS_RUN:QTS_SUCCESS;
    case QTT_TALK:case QTT_HUNT:case QTT_USEITEM:case QTT_SPAWNID_DEL: {
        const auto* row=CountedTerm(q,term.dwTermID,term.bTermType);
        return !row||row->bCount<term.bGoalCount?QTS_RUN:QTS_SUCCESS;
    }
    case QTT_TIMER:return q.timer_active&&q.timer_expired?QTS_FAILED:QTS_SUCCESS;
    case QTT_LEFT:return s.fPosX<term.dwTermID?QTS_RUN:QTS_SUCCESS;
    case QTT_TOP:return s.fPosZ>term.dwTermID?QTS_RUN:QTS_SUCCESS;
    case QTT_RIGHT:return s.fPosX>term.dwTermID?QTS_RUN:QTS_SUCCESS;
    case QTT_BOTTOM:return s.fPosZ<term.dwTermID?QTS_RUN:QTS_SUCCESS;
    case QTT_QUESTCOMPLETED:
        for(const auto& other:Rows(*s.payload))if(other.dwQuestID==term.dwTermID)
            return other.bCompleteCount<term.bGoalCount?QTS_RUN:QTS_SUCCESS;
        return QTS_RUN;
    default:return QTS_SUCCESS; // CQuest::CheckComplete's source default
    }
}
inline QuestPackets InitialPackets(const CharSnapshot& s,const IQuestChart& chart) {
    if(!s.payload)throw std::runtime_error("Quest admission snapshot missing");
    auto quests=Rows(*s.payload);Sort(quests);
    if(s.bStartAct==1&&s.wMapID==2010)quests.clear(); // source IsTutorial load exclusion
    std::vector<std::pair<QuestProgressRow*,const QuestDef*>> active;
    for(auto& q:quests) {
        const auto* def=chart.Find(q.dwQuestID);
        if(def&&def->bType&&Running(q))active.emplace_back(&q,def);
    }
    if(active.size()>255)throw std::runtime_error("Quest list exceeds client count width");
    std::vector<std::byte> list;wire::WritePOD(list,static_cast<std::uint8_t>(active.size()));
    for(auto [q,def]:active) {
        if(def->terms.size()>255)throw std::runtime_error("Quest terms exceed client count width");
        wire::WritePOD(list,q->dwQuestID);wire::WritePOD(list,def->bType);wire::WritePOD(list,def->bCountMax);
        wire::WritePOD(list,static_cast<std::uint8_t>(def->terms.size()));
        for(const auto& term:def->terms) {
            const auto* count=CountedTerm(*q,term.dwTermID,term.bTermType);
            const auto value=count?count->bCount:TermCount(*s.payload,term);
            const auto status=TermStatus(s,*q,term);
            wire::WritePOD(list,term.dwTermID);wire::WritePOD(list,term.bTermType);
            wire::WritePOD(list,term.bGoalCount);wire::WritePOD(list,value);wire::WritePOD(list,status);
        }
    }
    QuestPackets out;
    out.emplace_back(static_cast<std::uint16_t>(tnetlib::protocol::MessageId::CS_QUESTLIST_ACK),std::move(list));
    for(const auto& q:quests)if(q.dwTick) {
        const auto* def=chart.Find(q.dwQuestID);if(!def||!def->bType)continue;
        std::vector<std::byte> timer;wire::WritePOD(timer,q.dwQuestID);wire::WritePOD(timer,q.dwTick);
        out.emplace_back(static_cast<std::uint16_t>(tnetlib::protocol::MessageId::CS_QUESTSTARTTIMER_ACK),std::move(timer));
    }
    return out;
}
inline std::uint32_t Talk(CharacterPayload& p,const IQuestChart& chart,std::uint16_t npc,QuestPackets& packets) {
    auto& rows=Rows(p);Sort(rows);std::uint32_t last=0;
    for(auto& q:rows) {
        if(!Running(q))continue;
        const auto* def=chart.Find(q.dwQuestID);if(!def||!def->bType)continue;
        const auto t=std::find_if(def->terms.begin(),def->terms.end(),[&](const auto& d){return d.dwTermID==npc&&d.bTermType==QTT_TALK;});
        if(t==def->terms.end())continue;
        last=q.dwQuestID;auto* row=CountedTerm(q,npc,QTT_TALK);const auto previous=row->bCount;
        row->bCount=static_cast<std::uint8_t>(std::min<unsigned>(unsigned(previous)+1,t->bGoalCount));
        if(row->bCount!=previous) {
            q.save=1;
            packets.emplace_back(static_cast<std::uint16_t>(tnetlib::protocol::MessageId::CS_QUESTUPDATE_ACK),
                EncodeQuestUpdateAck(q.dwQuestID,npc,QTT_TALK,row->bCount,row->bCount<t->bGoalCount?QTS_RUN:QTS_SUCCESS));
        }
    }
    return last;
}
// CheckQuest for quest actions whose original dwTargetID is zero. Monster-kill
// hooks must supply their separate target/spawn-neighbour contract, not call
// this entry point and silently lose the type-21 second branch.
inline void UpdateWithoutTarget(CharSnapshot& s,CharacterPayload& p,const IQuestChart& chart,
    const QuestMapFacts& facts,std::uint32_t id,std::uint8_t type,int delta,QuestPackets& packets) {
    Sort(Rows(p));
    for(auto& q:Rows(p)) {
        const auto* def=chart.Find(q.dwQuestID);if(!Running(q)||!def)continue;
        const auto term=std::find_if(def->terms.begin(),def->terms.end(),[&](const auto& t){
            return (t.dwTermID==id&&t.bTermType==type)||(type==21&&t.bTermType==21);
        });
        if(term==def->terms.end())continue;
        auto* count=CountedTerm(q,id,type);const auto previous=count?count->bCount:0;
        if(type==21&&std::any_of(facts.monsters.begin(),facts.monsters.end(),[&](const auto& m){return m.first==id&&m.second==id;}))++delta;
        if(count)count->bCount=static_cast<std::uint8_t>(std::clamp(int(count->bCount)+delta,0,int(term->bGoalCount)));
        if(!count||count->bCount!=previous) {
            q.save=1;
            packets.emplace_back(static_cast<std::uint16_t>(tnetlib::protocol::MessageId::CS_QUESTUPDATE_ACK),
                EncodeQuestUpdateAck(q.dwQuestID,term->dwTermID,term->bTermType,
                    count?count->bCount:TermCount(p,{id,type,0}),TermStatus(s,q,*term)));
        }
    }
}
inline void Drop(CharacterPayload& p,const IQuestChart& chart,std::uint32_t id,QuestPackets& packets,std::uint8_t result=QR_DROP) {
    HydrateLevels(p,chart);
    if(Find(Rows(p),id))if(const auto* def=chart.Find(id))
        if(const auto key=LevelKey(*def))p.quest_levels->erase(key);
    if(auto* q=Find(Rows(p),id);q&&Running(*q)) {
        --q->bTriggerCount;q->dwTick=0;q->save=1;q->timer_active=false;q->timer_expired=false;
        // Source DropQuest retains history, type21, and non-counted terms.
        for(auto& t:q->terms)if(t.bTermType==QTT_HUNT||t.bTermType==QTT_TALK||t.bTermType==QTT_USEITEM)t.bCount=0;
    }
    packets.emplace_back(static_cast<std::uint16_t>(tnetlib::protocol::MessageId::CS_QUESTCOMPLETE_ACK),
        EncodeQuestCompleteAck(result,id,0,0,id));
}
inline void EndTimer(CharacterPayload& p,const IQuestChart& chart,std::uint32_t id,QuestPackets& packets) {
    auto* q=Find(Rows(p),id);const auto* def=chart.Find(id);
    if(!q||!def||!Running(*q))return;
    for(const auto& t:def->terms)if(t.bTermType==QTT_TIMER) {
        q->dwTick=0;q->save=1;q->timer_active=false;q->timer_expired=false;
        packets.emplace_back(static_cast<std::uint16_t>(tnetlib::protocol::MessageId::CS_QUESTUPDATE_ACK),
            EncodeQuestUpdateAck(id,t.dwTermID,QTT_TIMER,0,QTS_FAILED));
    }
}
}
}
