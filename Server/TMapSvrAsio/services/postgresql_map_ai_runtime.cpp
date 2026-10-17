#include "postgresql_map_service.h"
#include "postgresql_map_owner.h"
#include "legacy_random.h"
#include <soci/soci.h>
#include <iterator>
#include <utility>

namespace tmapsvr {
NativeMonsterAiExecutor PostgreSQLMapService::MakeNativeMonsterAiExecutor(std::uint32_t tick) {
    return NativeMonsterAiExecutor(m_monster_catalog,m_monster_ai,m_battles,
        [this,tick](NativeMonsterMap& map,std::uint32_t id) {
            StageNativeCellNotices(EnterNativeMonster(map,id,m_monster_catalog,m_monster_ai,tick));
        },
        [this](NativeMonsterMap& map,std::uint32_t id,bool exit_map) {
            StageNativeCellNotices(LeaveNativeMonster(map,id,exit_map));
        },
        [this](const NativeAiInvocation& call){m_monster_ai_queue.Publish(call);},
        [this](const NativeAiInvocation& call){m_monster_ai_queue.CancelObject(call);},LegacyRandom15,tick);
}
void PostgreSQLMapService::StageNativeCellNotices(NativeCellNotices notices) {
    // Called during boot or while m_monster_ai_mutex is already held. Keep
    // source ADD/DEL order; overflow is an execution failure, never truncation.
    constexpr std::size_t max_notices=65536,max_bytes=64*1024*1024;
    if(notices.size()>max_notices-m_native_cell_notices.size())throw std::runtime_error("Native cell publication count limit");
    auto bytes=m_native_cell_notice_bytes;
    for(const auto& notice:notices) {
        const auto charge=sizeof(NativeCellNotice)+(notice.monster?notice.monster->WireSize():5);
        if(charge>max_bytes-bytes)throw std::runtime_error("Native cell publication byte limit");
        bytes+=charge;
    }
    m_native_cell_notices.insert(m_native_cell_notices.end(),std::make_move_iterator(notices.begin()),std::make_move_iterator(notices.end()));
    m_native_cell_notice_bytes=bytes;
}
NativeCellNotices PostgreSQLMapService::TakeNativeCellNotices() {
    std::lock_guard lock(m_monster_ai_mutex);
    if(m_native_ai_retired){m_native_cell_notices.clear();m_native_cell_notice_bytes=0;return {};}
    m_native_cell_notice_bytes=0;return std::exchange(m_native_cell_notices,{});
}
void PostgreSQLMapService::RequireNativeActorAdmission() const {
    if(m_native_ai_actor_barrier)
        throw std::runtime_error("Native source monster actors require full ADDMON/client host/awake lifecycle and player coexistence");
    if(m_native_ai_retired)throw std::runtime_error("Native monster AI authority retired; restart required");
}
void PostgreSQLMapService::CheckNativeMonsterAiAuthority() {
    if(m_native_ai_retired)throw std::runtime_error("Native monster AI authority retired");
    auto lease=m_pool.Acquire();
    auto tx=BeginMapTransaction(*lease,m_config.world,m_config.server,m_config.owner_token,true);
    CheckCatalogs(*lease);CheckNativeBattleRelease(*lease);
    tx->commit();
}
bool PostgreSQLMapService::NativeMonsterAiPending() const {
    std::lock_guard lock(m_monster_ai_mutex);return m_monster_ai_queue.Pending()!=0;
}
bool PostgreSQLMapService::NativeMonsterAiImmediateWork() const {
    std::lock_guard lock(m_monster_ai_mutex);return m_monster_ai_queue.ImmediateWork();
}
void PostgreSQLMapService::AdvanceNativeMonsterAi(std::uint32_t tick) {
    std::lock_guard lock(m_monster_ai_mutex);
    if(m_native_ai_retired)return;
    try {m_monster_ai_queue.Turn(tick);}
    catch(...) {m_native_ai_retired=true;m_monster_ai_queue.Stop();throw;}
}
void PostgreSQLMapService::DispatchNativeMonsterAi() {
    std::lock_guard lock(m_monster_ai_mutex);
    if(m_native_ai_retired)return;
    try {
        auto call=m_monster_ai_queue.TakeAcknowledgement();
        if(!call)return;
        // Permanent admission barrier is the proof for empty-host/visibility
        // branches, including during delayed execution after LoadData.
        if(!m_native_ai_actor_barrier)throw std::runtime_error("Native AI ACK without actor-isolation authority");
        const auto key=std::uint32_t{call->map}|(std::uint32_t{call->channel}<<16);
        const auto environment=m_battles.maps.find(key);
        if(environment==m_battles.maps.end()||environment->second.instance_template)return;
        // Source physical channel -> FindTMap(party,map). Ordinary maps ignore
        // party; no real party-instance map exists yet for instance templates.
        const auto map=m_monster_maps.find(key);
        if(map==m_monster_maps.end())return;
        if(map->second.map_id!=call->map||map->second.physical_channel!=call->channel)
            throw std::runtime_error("Native AI physical map identity differs from index");
        auto executor=MakeNativeMonsterAiExecutor(m_monster_ai_queue.Tick());
        executor.Acknowledge(map->second,*call);
    }catch(...) {m_native_ai_retired=true;m_monster_ai_queue.Stop();throw;}
}
void PostgreSQLMapService::StopNativeMonsterAi() {
    m_native_ai_retired=true;
    std::lock_guard lock(m_monster_ai_mutex);m_monster_ai_queue.Stop();
    m_native_cell_notices.clear();m_native_cell_notice_bytes=0;
}
} // namespace tmapsvr
