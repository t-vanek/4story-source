#include "postgresql_map_service.h"
#include "domain/main_transfer.h"
#include "MessageId.h"
#include <soci/soci.h>
#include <bit>

namespace tmapsvr {
PvpBalance PostgreSQLMapService::ReadPvpBalance(soci::session& sql,const MapSessionClaim& c) {
    const int world=c.group,character=std::bit_cast<std::int32_t>(c.char_id);
    long long total=0,available=0,month=0;
    sql << "SELECT total,available,month FROM app_world.pvp_balances WHERE world_id=:w AND char_id=:c",
        soci::use(world,"w"),soci::use(character,"c"),soci::into(total),soci::into(available),soci::into(month);
    // Both original procedures explicitly initialize missing-row outputs to0.
    return {static_cast<std::uint32_t>(total),static_cast<std::uint32_t>(available),static_cast<std::uint32_t>(month)};
}
void PostgreSQLMapService::HydratePvpBalance(soci::session& sql,const MapSessionClaim& c,CharacterPayload& p) {
    if(p.transfer_state) {
        const auto& t=*p.transfer_state;p.pvp=PvpBalance{t.pvp_total,t.pvp_available,t.month_points};
    } else p.pvp=ReadPvpBalance(sql,c);
}
void PostgreSQLMapService::CheckPvpBalance(soci::session& sql,const MapSessionClaim& c,const CharSnapshot& s) {
    if(!s.payload||!s.payload->pvp)throw std::runtime_error("PvP balance was not hydrated");
    // Graph identity and the operation head are checked by the caller. Never
    // restore a stale normalized balance after a primary transfer.
    if(!s.payload->transfer_state&&ReadPvpBalance(sql,c)!=*s.payload->pvp)
        throw std::runtime_error("Snapshot predates PvP balance");
}
void PostgreSQLMapService::SpendNativePvp(soci::session& sql,const MapSessionClaim& c,CharSnapshot& s,
    std::uint32_t amount,QuestPackets& packets) {
    CheckPvpBalance(sql,c,s);
    if(!amount)return; // CTPlayer::UsePvPoint sends no packet for zero.
    auto p=std::make_shared<CharacterPayload>(*s.payload);s.payload=p;
    const auto before=*p->pvp;
    if(amount>before.available)throw std::runtime_error("PvP purchase exceeds its validated balance");
    p->pvp->available-=amount;
    if(!p->transfer_state) {
        const int world=c.group,character=std::bit_cast<std::int32_t>(c.char_id);int changed=0;
        const long long total=before.total,old=before.available,month=before.month,left=p->pvp->available;
        sql << "UPDATE app_world.pvp_balances SET available=:left WHERE world_id=:w AND char_id=:c "
            "AND total=:total AND available=:old AND month=:month RETURNING 1",
            soci::use(left,"left"),soci::use(world,"w"),soci::use(character,"c"),soci::use(total,"total"),
            soci::use(old,"old"),soci::use(month,"month"),soci::into(changed);
        if(!sql.got_data()||changed!=1)throw std::runtime_error("PvP balance changed during purchase");
    }
    std::vector<std::byte> ack;wire::WritePOD(ack,p->pvp->total);wire::WritePOD(ack,p->pvp->available);
    wire::WritePOD(ack,std::uint8_t{9}); // PVPE_BUYITEM, PVP_USEABLE only; no rank notification.
    wire::WritePOD(ack,p->pvp->month);
    packets.emplace_back(static_cast<std::uint16_t>(tnetlib::protocol::MessageId::CS_PVPPOINT_ACK),std::move(ack));
}
}
