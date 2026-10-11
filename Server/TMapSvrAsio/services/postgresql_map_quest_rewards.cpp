#include "postgresql_map_service.h"
#include "character_titles.h"
#include "skill_learning.h"
#include "MessageId.h"
#include <soci/soci.h>
#include <algorithm>
#include <bit>
#include <map>

namespace tmapsvr {
namespace {
void AwardTitles(soci::session& sql,CharSnapshot& s,std::uint8_t kind,std::uint32_t value,QuestPackets& packets) {
    // TPlayer::GetTitle(QUEST_TITLE/GOLD_TITLE, value, TRUE). Retain the source
    // category replacement and ANY-owned early return, even for empty matches.
    std::map<std::uint16_t,std::uint32_t> catalog;
    const int category=kind;
    {soci::rowset<soci::row> rows=(sql.prepare <<
        "SELECT \"wTitleID\",\"dwRequirement\" FROM game_compat.\"TTITLECHART\" WHERE \"bKind\"=:kind",
        soci::use(category));
     for(const auto& row:rows)catalog.emplace(static_cast<std::uint16_t>(row.get<int>(0)),static_cast<std::uint32_t>(row.get<int>(1)));}
    auto p=std::make_shared<CharacterPayload>(*s.payload);s.payload=p;
    std::vector<std::uint16_t> eligible;
    for(const auto& [id,need]:catalog)if(kind==8?need<value:need==value)eligible.push_back(id);
    if(kind==8&&eligible.size()>1&&eligible[eligible.size()-2]==p->selected_title)p->selected_title=eligible.back();
    for(const auto id:eligible)if(std::any_of(p->titles.begin(),p->titles.end(),[&](const auto& t){return t.id==id;}))return;
    const bool selected=catalog.contains(p->selected_title);
    std::erase_if(p->titles,[&](const auto& t){return catalog.contains(t.id);});
    for(const auto id:eligible) {
        p->titles.push_back({id,selected});p->titles=OrderedTitles(std::move(p->titles));
        auto packet=EncodeTitleList(p->titles);wire::WritePOD(packet,id);wire::WritePOD(packet,std::uint8_t{1});
        packets.emplace_back(static_cast<std::uint16_t>(tnetlib::protocol::MessageId::CS_TITLEGAIN_ACK),std::move(packet));
    }
}
void CheckRewardTitles(soci::session& sql,const MapSessionClaim& c,const CharacterPayload& p) {
    if(p.transfer_state)return;
    const int world=c.group,character=std::bit_cast<std::int32_t>(c.char_id);
    std::vector<CharacterTitle> rows;
    soci::rowset<soci::row> stored=(sql.prepare << "SELECT \"wTitleID\",\"bSelected\" FROM app_world.\"TTITLETABLE\" "
        "WHERE \"bWorldID\"=:w AND \"dwCharID\"=:c FOR UPDATE",soci::use(world,"w"),soci::use(character,"c"));
    for(const auto& row:stored)rows.push_back({static_cast<std::uint16_t>(row.get<int>(0)),row.get<int>(1)!=0});
    if(TitleJson(rows)!=TitleJson(p.titles))throw std::runtime_error("Quest reward title state changed");
}
void SaveRewardTitles(soci::session& sql,const MapSessionClaim& c,const CharacterPayload& p) {
    if(p.transfer_state)return;
    const int world=c.group,character=std::bit_cast<std::int32_t>(c.char_id);
    sql << "DELETE FROM app_world.\"TTITLETABLE\" WHERE \"bWorldID\"=:w AND \"dwCharID\"=:c",
        soci::use(world,"w"),soci::use(character,"c");
    for(const auto& title:OrderedTitles(p.titles)) {
        const int id=std::bit_cast<std::int16_t>(title.id),selected=title.selected;
        sql << "INSERT INTO app_world.\"TTITLETABLE\"(\"bWorldID\",\"dwCharID\",\"wTitleID\",\"bSelected\") VALUES(:w,:c,:id,:selected)",
            soci::use(world,"w"),soci::use(character,"c"),soci::use(id,"id"),soci::use(selected,"selected");
    }
}
}
std::uint8_t PostgreSQLMapService::CompleteQuestRewards(soci::session& sql,const MapSessionClaim& c,CharSnapshot& s,
    const QuestDef& def,std::uint8_t chosen_type,std::uint32_t chosen_id,
    const std::function<std::uint8_t()>& probability,const QuestEvent& event,QuestPackets& packets) {
    using tnetlib::protocol::MessageId;
    CheckRewardTitles(sql,c,*s.payload);
    const auto roll=probability();std::uint8_t sum=0;
    std::vector<ItemInstance> items;
    for(const auto& reward:def.rewards)if(reward.bRewardType==RT_ITEM||reward.bRewardType==RT_MAGICITEM) {
        bool take=false;
        switch(reward.bTakeMethod) {
        case RM_SELECT:take=reward.bRewardType==chosen_type&&reward.dwRewardID==chosen_id;break;
        case RM_PROB:take=probability()<reward.bTakeData;break;
        case RM_RANDOM:sum=static_cast<std::uint8_t>(sum+reward.bTakeData);take=sum>roll;break;
        case RM_DEFAULT:take=true;break;
        }
        if(!take)continue;
        auto created=CreateQuestRewardItems(sql,c,s,reward);
        items.insert(items.end(),std::make_move_iterator(created.begin()),std::make_move_iterator(created.end()));
        // Source breaks after a taken row whenever the accumulated BYTE is
        // nonzero, even if this row is not RANDOM or its template is missing.
        if(sum)break;
    }
    // Deliberately before GETITEM quest consumption: the source requires room
    // for the reward while the turn-in items still occupy the inventory.
    if(!GrantQuestItems(sql,c,s,items,packets))return QR_INVENTORYFULL;
    for(const auto& item:items)event(item.wItemID,QTT_GETITEM,TT_GETITEM,item.bCount);
    for(const auto& reward:def.rewards)if(reward.bRewardType!=RT_ITEM&&reward.bRewardType!=RT_MAGICITEM) {
        bool take=false;
        switch(reward.bTakeMethod) {
        case RM_SELECT:take=reward.bRewardType==chosen_type&&reward.dwRewardID==chosen_id;break;
        case RM_PROB:take=probability()<reward.bTakeData;break;
        case RM_DEFAULT:take=true;break;
        // Source does not implement RM_RANDOM for non-item rewards.
        }
        if(!take)continue;
        switch(reward.bRewardType) {
        case RT_GOLD: {
            if(reward.dwRewardID) {
                // NetCode::CalcMoney adds copper/silver as DWORD before widening.
                const auto total=static_cast<std::uint32_t>(s.dwCooper+s.dwSilver*1000U)+
                    std::uint64_t{s.dwGold}*1000000U+reward.dwRewardID;
                s.dwGold=static_cast<std::uint32_t>(total/1000000U);s.dwSilver=static_cast<std::uint32_t>(total/1000U%1000U);
                s.dwCooper=static_cast<std::uint32_t>(total%1000U);
                AwardTitles(sql,s,8,s.dwGold,packets);
                // Child completion can run before this method returns. Keep the
                // locked normalized projection current within this transaction.
                SaveRewardTitles(sql,c,*s.payload);
            }
            packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_MONEY_ACK),EncodeMoneyAck(s.dwGold,s.dwSilver,s.dwCooper));
            break;
        }
        case RT_SKILL:case RT_SKILLUP: {
            auto p=std::make_shared<CharacterPayload>(*s.payload);s.payload=p;
            const auto skill=static_cast<std::uint16_t>(reward.dwRewardID);const int id=std::bit_cast<std::int16_t>(skill);
            auto found=std::find_if(p->skills.begin(),p->skills.end(),[&](const auto& row){return row.wSkillID==skill;});
            const bool upgrade=reward.bRewardType==RT_SKILLUP;
            if(upgrade&&found==p->skills.end())break;
            if(!upgrade) {
                int classes=0;
                sql << "SELECT \"dwClassID\" FROM character_compat.\"TSKILLCHART\" WHERE \"wID\"=:id",soci::use(id),soci::into(classes);
                if(!sql.got_data())break;
                if(s.bClass>=32)throw std::runtime_error("Quest skill class shift out of range");
                if(!(static_cast<std::uint32_t>(classes)&(std::uint32_t{1}<<s.bClass))||found!=p->skills.end())break;
                p->skills.push_back({skill,1,0});found=std::prev(p->skills.end());
            } else found->bLevel=static_cast<std::uint8_t>(found->bLevel+1); // source BYTE wrap, no trainer cap
            const int rank=found->bLevel;
            if(!p->transfer_state) {
                const int world=c.group,character=std::bit_cast<std::int32_t>(c.char_id);
                if(upgrade) {
                    int changed=0;
                    sql << "UPDATE app_world.\"TSKILLTABLE\" SET \"bLevel\"=:r WHERE \"bWorldID\"=:w AND \"dwCharID\"=:c AND \"wSkillID\"=:id RETURNING 1",
                        soci::use(rank,"r"),soci::use(world,"w"),soci::use(character,"c"),soci::use(id,"id"),soci::into(changed);
                    if(!sql.got_data())throw std::runtime_error("Quest learned skill disappeared");
                } else sql << "INSERT INTO app_world.\"TSKILLTABLE\"(\"bWorldID\",\"dwCharID\",\"wSkillID\",\"bLevel\",\"dwRemainTick\") VALUES(:w,:c,:id,:r,0)",
                    soci::use(world,"w"),soci::use(character,"c"),soci::use(id,"id"),soci::use(rank,"r");
            }
            std::sort(p->skills.begin(),p->skills.end(),[](const auto& a,const auto& b){return a.wSkillID<b.wSkillID;});
            RefreshEquipment(sql,s,true);
            LearningPlan learned;learned.after=s;learned.result=0;learned.rank=static_cast<std::uint8_t>(rank);
            packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_SKILLBUY_ACK),EncodeSkillPurchaseAck({0,skill},learned));
            break;
        }
        case RT_EXP: {
            if(!s.payload->effects)throw std::runtime_error("Quest experience requires hydrated maintained skills");
            std::uint16_t bonus=0;
            for(const auto& effect:*s.payload->effects) {
                const int id=std::bit_cast<std::int16_t>(effect.skill);int value=0;
                sql << "SELECT \"wValue\" FROM character_compat.\"TSKILLDATA\" WHERE \"wSkillID\"=:id AND \"bType\"=6 AND \"bExec\"=63 "
                    "ORDER BY \"bAction\",\"bType\",\"bAttr\",\"bExec\" LIMIT 1",soci::use(id),soci::into(value);
                if(sql.got_data()&&(bonus=static_cast<std::uint16_t>(value)))break;
            }
            const std::uint32_t gain=reward.dwRewardID+static_cast<std::uint32_t>(reward.dwRewardID*bonus)/100U;
            GainQuestExperience(sql,c,s,gain,event,packets);break;
        }
        // RT_CHGCLASS is explicitly empty in OnQuestComplete. TITLE/SOUL/POINT
        // have no switch cases there; quest titles are awarded separately below.
        default:break;
        }
    }
    ConsumeQuestItems(sql,c,s,def,packets);
    AwardTitles(sql,s,15,def.dwQuestID,packets);
    SaveRewardTitles(sql,c,*s.payload);
    return QR_SUCCESS;
}
}
