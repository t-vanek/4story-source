#include "handlers.h"
#include "services/char_state_store.h"
#include "services/client_senders.h"
#include "services/main_transfer_runtime.h"
#include "services/player_service.h"
#include "services/session_registry.h"
#include "services/channel_presence.h"
#include "fourstory/db/co_offload.h"
#include "wire_codec.h"
#include "MessageId.h"
#include <cmath>

namespace tmapsvr {
boost::asio::awaitable<void> OnSkillEndReq(std::shared_ptr<tnetlib::AsioSession> sess,
    std::vector<std::byte> body,const HandlerContext& ctx) {
    using tnetlib::protocol::MessageId;
    EffectEndRequest request;wire::Reader reader(body);
    if(!reader.Read(request.object)||!reader.Read(request.object_type)||!reader.Read(request.host)||
       !reader.Read(request.attacker)||!reader.Read(request.attack_type)||!reader.Read(request.skill)||
       !reader.Read(request.map)||!reader.Read(request.channel)||!reader.Eof())
        throw std::runtime_error("Invalid SKILLEND request length");
    if(!ctx.session_reg)throw std::runtime_error("Effect cancellation identity unavailable");
    const auto identity=ctx.session_reg->Identity(sess.get());
    if(!identity||request.object_type!=1||request.object!=identity->char_id)
        throw std::runtime_error("Effect cancellation requires own PC");
    const auto cid=identity->char_id;const auto acknowledgement=EncodeSkillEnd(cid,request.skill);
    // Original client says to all Map connections. The secondary has no PC
    // object to mutate and answers the requester without touching persistence.
    if(identity->role==MapSessionRole::Replica) {
        co_await sess->SendPacket(static_cast<std::uint16_t>(MessageId::CS_SKILLEND_ACK),acknowledgement);co_return;
    }
    if(!ctx.char_state||!ctx.player_service||!ctx.skill_cooldown)
        throw std::runtime_error("Effect transaction context missing");
    const auto original=ctx.char_state->Freeze(cid,[](auto&){});
    if(!original)co_return;
    CharSnapshot before;
    try {before=transfer::PersistenceSnapshot(*original,identity->key,*ctx.skill_cooldown,SkillClockMs());}
    catch(...){ctx.char_state->Store(cid,*original);throw;}
    EffectEndCommit committed;
    try {
        auto* service=ctx.player_service;
        committed=co_await fourstory::db::CoOffloadIf(ctx.db_pool,[service,claim=identity->Claim(ctx.expected_group),&request,&before]{
            return service->EndMaintainedEffect(claim,request,before);
        });
        if(!committed.snapshot)throw std::runtime_error("Missing effect commit state");
        ctx.char_state->Store(cid,*committed.snapshot);
    }catch(...) {
        auto uncertain=*original;uncertain.persistence_uncertain=true;
        ctx.char_state->Store(cid,uncertain);sess->Close();throw;
    }
    const auto& after=*committed.snapshot;
    std::vector<std::shared_ptr<tnetlib::AsioSession>> neighbors{sess};
    const auto cell=[](float value)->int{return std::isfinite(value)&&value>=0&&value<65536?static_cast<std::uint16_t>(value)/64:-10000;};
    if(ctx.presence)ctx.presence->ForEachInChannel(identity->channel,cid,[&](const ChannelPresenceEntry& entry,auto client){
        if(entry.map_id==after.wMapID&&std::abs(cell(entry.pos.x)-cell(after.fPosX))<=1&&
           std::abs(cell(entry.pos.z)-cell(after.fPosZ))<=1)neighbors.push_back(std::move(client));
    });
    for(auto& client:neighbors)co_await client->SendPacket(static_cast<std::uint16_t>(MessageId::CS_SKILLEND_ACK),acknowledgement);
    if(committed.removed) {
        if(!after.payload||!after.payload->statistics)throw std::runtime_error("Effect commit lacks statistics");
        if(before.dwMaxHP!=after.dwMaxHP||before.dwMaxMP!=after.dwMaxMP)
            for(auto& client:neighbors)co_await client->SendPacket(static_cast<std::uint16_t>(MessageId::CS_HPMP_ACK),
                EncodeHpMpAck(cid,1,after.dwMaxHP,after.dwHP,after.dwMaxMP,after.dwMP));
        co_await sess->SendPacket(static_cast<std::uint16_t>(MessageId::CS_CHARSTATINFO_ACK),
            EncodeCharacterStatistics(after,*after.payload->statistics));
    }
}
}
