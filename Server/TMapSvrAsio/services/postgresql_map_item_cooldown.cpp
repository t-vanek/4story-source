#include "postgresql_map_service.h"
#include "domain/main_transfer.h"
#include "services/skill_cooldown.h"
#include <soci/soci.h>

namespace tmapsvr {
std::vector<ItemCooldown> PostgreSQLMapService::ReadItemCooldowns(soci::session& sql,const MapSessionClaim& c) {
    const int world=c.group,character=SqlInt32Identity(c.char_id);
    std::vector<ItemCooldown> out;
    soci::rowset<soci::row> rows=(sql.prepare <<
        "SELECT delay_group,remaining FROM app_world.item_cooldowns WHERE world_id=:w AND char_id=:c ORDER BY delay_group",
        soci::use(world,"w"),soci::use(character,"c"));
    for(const auto& row:rows)out.push_back({static_cast<std::uint16_t>(row.get<int>(0)),
        static_cast<std::uint32_t>(row.get<long long>(1))});
    return out;
}
void PostgreSQLMapService::HydrateItemCooldowns(soci::session& sql,const MapSessionClaim& c,
    const CharSnapshot& s,CharacterPayload& p) {
    ItemCooldownState state;state.sampled_ms=SkillClockMs();
    auto rows=p.transfer_state?p.transfer_state->item_cooldowns:ReadItemCooldowns(sql,c);
    OrderItemCooldowns(rows);
    // OnDM_LOADCHAR_ACK applies these gates both to database load and
    // RELEASEMAIN: tutorial ignores reuse; durations >= one hour are omitted.
    for(const auto& row:rows)
        if((s.bStartAct==1&&s.wMapID==2010)||row.remaining>=3600000)state.ignored_on_load.push_back(row);
        else state.timers.push_back(row);
    if(state.timers.size()>255)throw std::runtime_error("Loaded item timers exceed original CHARINFO count");
    for(const auto& row:state.timers)state.deadlines.emplace(row.group,state.sampled_ms+row.remaining);
    p.item_cooldowns=std::move(state);
}
void PostgreSQLMapService::CheckItemCooldowns(soci::session& sql,const MapSessionClaim& c,const CharSnapshot& s) {
    if(!s.payload||!s.payload->item_cooldowns)throw std::runtime_error("Item cooldown state was not hydrated");
    // The complete graph and mutation head fence transferred state. Its old
    // normalized rows must never replace more recent item use on another Map.
    if(s.payload->transfer_state)return;
    CheckItemCooldownProjection(ReadItemCooldowns(sql,c),*s.payload->item_cooldowns);
}
void PostgreSQLMapService::WriteItemCooldowns(soci::session& sql,const MapSessionClaim& c,const CharSnapshot& s) {
    if(!s.payload||!s.payload->item_cooldowns)throw std::runtime_error("Item cooldown state was not hydrated");
    if(s.payload->transfer_state)return;
    const int world=c.group,character=SqlInt32Identity(c.char_id);
    const auto state=ItemCooldownJson(ActiveItemCooldowns(*s.payload->item_cooldowns));
    // Only called after ownership, checkpoint and mutation-head validation.
    // Core saves may reduce timers; the native ITEMUSE transaction alone may
    // arm a group. Exact source SAVE semantics omit expired live entries.
    sql << "DELETE FROM app_world.item_cooldowns WHERE world_id=:w AND char_id=:c",
        soci::use(world,"w"),soci::use(character,"c");
    sql << "INSERT INTO app_world.item_cooldowns(world_id,char_id,delay_group,remaining) "
        "SELECT :w,:c,(e->>0)::integer,(e->>1)::bigint FROM jsonb_array_elements(CAST(:state AS jsonb)) e",
        soci::use(world,"w"),soci::use(character,"c"),soci::use(state,"state");
}
bool PostgreSQLMapService::ItemCooldownCheckpointMatches(soci::session& sql,const MapSessionClaim& c,const CharSnapshot& s) {
    return s.payload&&s.payload->item_cooldowns&&
        ItemCooldownJson(ReadItemCooldowns(sql,c))==ItemCooldownJson(ActiveItemCooldowns(*s.payload->item_cooldowns));
}
}
