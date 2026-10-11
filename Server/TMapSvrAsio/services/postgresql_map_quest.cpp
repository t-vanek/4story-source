#include "postgresql_map_service.h"
#include "domain/main_transfer.h"
#include "services/skill_cooldown.h"
#include <soci/soci.h>
#include <bit>
#include <map>

namespace tmapsvr {
std::vector<QuestProgressRow> PostgreSQLMapService::ReadQuestProgress(soci::session& sql,const MapSessionClaim& c) {
    const int world=c.group,character=std::bit_cast<std::int32_t>(c.char_id);
    std::vector<QuestProgressRow> quests;
    soci::rowset<soci::row> rows=(sql.prepare <<
        "SELECT \"dwQuestID\",\"dwTick\",\"bCompleteCount\",\"bTriggerCount\" FROM app_world.\"TQUESTTABLE\" "
        "WHERE \"bWorldID\"=:w AND \"dwCharID\"=:c ORDER BY (\"dwQuestID\"::bigint & 4294967295)",
        soci::use(world,"w"),soci::use(character,"c"));
    for(const auto& r:rows)quests.push_back({static_cast<std::uint32_t>(r.get<int>(0)),
        static_cast<std::uint32_t>(r.get<int>(1)),static_cast<std::uint8_t>(r.get<int>(2)),
        static_cast<std::uint8_t>(r.get<int>(3)),{}});
    soci::rowset<soci::row> terms=(sql.prepare <<
        "SELECT \"dwQuestID\",\"dwTermID\",\"bTermType\",\"bCount\" FROM app_world.\"TQUESTTERMTABLE\" "
        "WHERE \"bWorldID\"=:w AND \"dwCharID\"=:c ORDER BY (\"dwQuestID\"::bigint & 4294967295),(\"dwTermID\"::bigint & 4294967295)",
        soci::use(world,"w"),soci::use(character,"c"));
    for(const auto& r:terms) {
        auto* q=quest_runtime::Find(quests,static_cast<std::uint32_t>(r.get<int>(0)));
        if(!q)throw std::runtime_error("Orphan native quest term");
        q->terms.push_back({static_cast<std::uint32_t>(r.get<int>(1)),
            static_cast<std::uint8_t>(r.get<int>(2)),static_cast<std::uint8_t>(r.get<int>(3))});
    }
    return quests;
}
void PostgreSQLMapService::HydrateQuestProgress(soci::session& sql,const MapSessionClaim& c,CharacterPayload& p) {
    if(p.transfer_state) {
        std::vector<QuestProgressRow> rows;
        for(const auto& q:p.transfer_state->quests)
            rows.push_back({q.id,q.remaining,q.completed,q.triggered,{},q.save});
        quest_runtime::Sort(rows);
        for(const auto& t:p.transfer_state->quest_terms) {
            auto* q=quest_runtime::Find(rows,t.quest);
            if(!q)throw std::runtime_error("Orphan transferred quest term");
            q->terms.push_back({t.id,t.type,t.count});
        }
        // Never replace the graph's newer quests with its old normalized rows.
        p.quests=std::move(rows);
    } else p.quests=ReadQuestProgress(sql,c);
    quest_runtime::Json(*p.quests); // reject unrepresentable source key collisions
    for(auto& q:*p.quests)q.timer_active=q.dwTick!=0;
    p.quest_sample_ms=SkillClockMs();
    const int world=c.group,character=std::bit_cast<std::int32_t>(c.char_id);long long head=0;
    sql << "SELECT COALESCE(max(operation_id),0) FROM app_world.quest_operations WHERE world_id=:w AND char_id=:c",
        soci::use(world,"w"),soci::use(character,"c"),soci::into(head);
    p.last_quest_id=static_cast<std::uint64_t>(head);
}
void PostgreSQLMapService::CheckQuestProgress(soci::session& sql,const MapSessionClaim& c,const CharSnapshot& s) {
    if(!s.payload)throw std::runtime_error("Quest snapshot missing");
    const int world=c.group,character=std::bit_cast<std::int32_t>(c.char_id);long long head=0;
    sql << "SELECT COALESCE(max(operation_id),0) FROM app_world.quest_operations WHERE world_id=:w AND char_id=:c",
        soci::use(world,"w"),soci::use(character,"c"),soci::into(head);
    if(static_cast<std::uint64_t>(head)!=s.payload->last_quest_id)
        throw std::runtime_error("Snapshot predates quest mutation");
    auto live=quest_runtime::Rows(*s.payload);
    if(s.payload->transfer_state)return;
    const auto stored=ReadQuestProgress(sql,c);
    // A core checkpoint may decrease remaining time, but cannot change quest
    // membership, trigger/completion counters or term progress.
    for(const auto& q:stored) {
        auto* candidate=quest_runtime::Find(live,q.dwQuestID);
        if(!candidate||candidate->dwTick>q.dwTick)throw std::runtime_error("Quest timer or membership changed");
        candidate->dwTick=q.dwTick;
    }
    if(quest_runtime::Json(live)!=quest_runtime::Json(stored))throw std::runtime_error("Quest progress changed outside transaction");
}
void PostgreSQLMapService::WriteQuestProgress(soci::session& sql,const MapSessionClaim& c,const CharSnapshot& s) {
    if(s.payload->transfer_state)return;
    const int world=c.group,character=std::bit_cast<std::int32_t>(c.char_id);
    for(const auto& q:quest_runtime::Rows(*s.payload)) {
        const int quest=std::bit_cast<std::int32_t>(q.dwQuestID),tick=std::bit_cast<std::int32_t>(q.dwTick),
            completed=q.bCompleteCount,triggered=q.bTriggerCount;
        sql << "INSERT INTO app_world.\"TQUESTTABLE\"(\"bWorldID\",\"dwCharID\",\"dwQuestID\",\"dwTick\",\"bCompleteCount\",\"bTriggerCount\") "
            "VALUES(:w,:c,:q,:t,:complete,:trigger) ON CONFLICT(\"bWorldID\",\"dwCharID\",\"dwQuestID\") "
            "DO UPDATE SET \"dwTick\"=EXCLUDED.\"dwTick\",\"bCompleteCount\"=EXCLUDED.\"bCompleteCount\",\"bTriggerCount\"=EXCLUDED.\"bTriggerCount\"",
            soci::use(world,"w"),soci::use(character,"c"),soci::use(quest,"q"),soci::use(tick,"t"),
            soci::use(completed,"complete"),soci::use(triggered,"trigger");
        for(const auto& t:q.terms) {
            const int id=std::bit_cast<std::int32_t>(t.dwTermID),type=t.bTermType,count=t.bCount;
            sql << "INSERT INTO app_world.\"TQUESTTERMTABLE\"(\"bWorldID\",\"dwCharID\",\"dwQuestID\",\"dwTermID\",\"bTermType\",\"bCount\") "
                "VALUES(:w,:c,:q,:id,:type,:count) ON CONFLICT(\"bWorldID\",\"dwCharID\",\"dwQuestID\",\"dwTermID\") DO UPDATE SET \"bCount\"=EXCLUDED.\"bCount\"",
                soci::use(world,"w"),soci::use(character,"c"),soci::use(quest,"q"),soci::use(id,"id"),soci::use(type,"type"),soci::use(count,"count");
        }
    }
}
void PostgreSQLMapService::WriteQuestTimers(soci::session& sql,const MapSessionClaim& c,const CharSnapshot& s) {
    CheckQuestProgress(sql,c,s);
    if(s.payload->transfer_state)return;
    const int world=c.group,character=std::bit_cast<std::int32_t>(c.char_id);
    for(const auto& q:quest_runtime::Rows(*s.payload)) {
        const int quest=std::bit_cast<std::int32_t>(q.dwQuestID),tick=std::bit_cast<std::int32_t>(q.dwTick);
        sql << "UPDATE app_world.\"TQUESTTABLE\" SET \"dwTick\"=:t WHERE \"bWorldID\"=:w AND \"dwCharID\"=:c AND \"dwQuestID\"=:q",
            soci::use(tick,"t"),soci::use(world,"w"),soci::use(character,"c"),soci::use(quest,"q");
    }
}
}
