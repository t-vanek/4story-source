#include "handlers.h"
#include "config.h"
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
boost::asio::awaitable<void> OnSkillBuyReq(std::shared_ptr<tnetlib::AsioSession> sess,
    std::vector<std::byte> body,const HandlerContext& ctx) {
    if(ctx.mode==Mode::Bow)co_return;
    const auto identity=ctx.session_reg?ctx.session_reg->Identity(sess.get()):std::nullopt;
    if(!identity||identity->role!=MapSessionRole::Primary||!ctx.char_state||!ctx.player_service||!ctx.skill_cooldown)
        throw std::runtime_error("SKILLBUY requires an authoritative primary");
    const auto cid=identity->char_id;const auto original=ctx.char_state->Freeze(cid,[](auto&){});
    if(!original)co_return;
    if(original->payload&&original->payload->security.Blocks()) {
        ctx.char_state->Store(cid,*original);const auto prompt=SecureUnlockPrompt(original->payload->security);
        co_await sess->SendPacket(prompt.first,prompt.second);co_return;
    }
    if(original->bStartAct==2||(original->bStartAct==1&&original->wMapID==2010)){
        ctx.char_state->Store(cid,*original);co_return;
    }
    try{DecodeSkillPurchase(body);}catch(...){ctx.char_state->Store(cid,*original);throw;}
    SkillPurchaseCommit committed;
    try {
        const auto before=transfer::PersistenceSnapshot(*original,identity->key,*ctx.skill_cooldown,SkillClockMs());
        auto* service=ctx.player_service;
        committed=co_await fourstory::db::CoOffloadIf(ctx.db_pool,[service,claim=identity->Claim(ctx.expected_group),&body,&before]{
            return service->PurchaseSkill(claim,body,before);
        });
        if(!committed.snapshot)throw std::runtime_error("Missing learning commit state");
        ctx.char_state->Store(cid,*committed.snapshot);
    }catch(...) {
        auto uncertain=*original;uncertain.persistence_uncertain=true;
        ctx.char_state->Store(cid,uncertain);sess->Close();throw;
    }
    for(const auto& [id,packet]:committed.packets)co_await sess->SendPacket(id,packet);
}
boost::asio::awaitable<void> OnNativeAction(std::shared_ptr<tnetlib::AsioSession> sess,
    std::vector<std::byte> body,const HandlerContext& ctx) {
    const auto request=DecodeSkillAction(body);
    const auto identity=ctx.session_reg?ctx.session_reg->Identity(sess.get()):std::nullopt;
    if(!identity||identity->role!=MapSessionRole::Primary||request.object!=identity->char_id||request.type!=1||
       !ctx.char_state||!ctx.player_service||!ctx.skill_cooldown)
        throw std::runtime_error("ACTION requires an authoritative own PC");
    const auto cid=identity->char_id;const auto original=ctx.char_state->Freeze(cid,[](auto&){});
    if(!original)co_return;
    SkillActionCommit committed;
    try {
        const auto before=transfer::PersistenceSnapshot(*original,identity->key,*ctx.skill_cooldown,SkillClockMs());
        auto* service=ctx.player_service;
        committed=co_await fourstory::db::CoOffloadIf(ctx.db_pool,[service,claim=identity->Claim(ctx.expected_group),&body,&before]{
            return service->CommitSkillAction(claim,body,before);
        });
        if(!committed.snapshot)throw std::runtime_error("Missing action commit state");
        ctx.char_state->Store(cid,*committed.snapshot);
    }catch(...) {
        auto uncertain=*original;uncertain.persistence_uncertain=true;
        ctx.char_state->Store(cid,uncertain);sess->Close();throw;
    }
    const auto& after=*committed.snapshot;
    std::vector<std::shared_ptr<tnetlib::AsioSession>> neighbors{sess};
    const auto cell=[](float value)->int{return std::isfinite(value)&&value>=0&&value<65536?static_cast<std::uint16_t>(value)/64:-10000;};
    if(ctx.presence)ctx.presence->ForEachInChannel(identity->channel,cid,[&](const ChannelPresenceEntry& entry,auto client){
        if(entry.map_id!=after.wMapID)return;
        const bool nearby=request.skill?std::hypot(entry.pos.x-after.fPosX,entry.pos.z-after.fPosZ)<=64.f:
            std::abs(cell(entry.pos.x)-cell(after.fPosX))<=1&&std::abs(cell(entry.pos.z)-cell(after.fPosZ))<=1;
        if(nearby)neighbors.push_back(std::move(client));
    });
    for(auto& client:neighbors)co_await client->SendPacket(static_cast<std::uint16_t>(tnetlib::protocol::MessageId::CS_ACTION_ACK),committed.acknowledgement);
}
boost::asio::awaitable<void> OnFinishSkill(std::shared_ptr<tnetlib::AsioSession> sess,
    std::vector<std::byte> body,const HandlerContext& ctx) {
    const auto request=DecodeFinishSkill(body);
    const auto identity=ctx.session_reg?ctx.session_reg->Identity(sess.get()):std::nullopt;
    if(!identity||identity->role!=MapSessionRole::Primary||request.host!=identity->char_id||
       request.attacker!=identity->char_id||request.type!=1||!ctx.char_state||!ctx.player_service||!ctx.skill_cooldown)
        throw std::runtime_error("FINISHSKILL requires an authoritative own PC");
    const auto cid=identity->char_id;
    const auto original=ctx.char_state->Freeze(cid,[](auto&){});
    if(!original)co_return;
    FinishSkillCommit committed;
    try {
        const auto before=transfer::PersistenceSnapshot(*original,identity->key,*ctx.skill_cooldown,SkillClockMs());
        auto* service=ctx.player_service;
        committed=co_await fourstory::db::CoOffloadIf(ctx.db_pool,[service,claim=identity->Claim(ctx.expected_group),&body,&before]{
            return service->FinishSkill(claim,body,before);
        });
        if(!committed.snapshot||committed.packets.size()!=committed.audiences.size())
            throw std::runtime_error("Missing finished skill state or audiences");
        ctx.char_state->Store(cid,*committed.snapshot);
    }catch(...) {
        auto uncertain=*original;uncertain.persistence_uncertain=true;
        ctx.char_state->Store(cid,uncertain);sess->Close();throw;
    }
    // EraseMaintainSkill uses neighboring cells and private statistics; Defend
    // uses nearby players. Preserve both recipient scope and packet order.
    const auto& after=*committed.snapshot;
    std::vector<std::shared_ptr<tnetlib::AsioSession>> owner{sess},neighbors{sess},nearby{sess};
    const auto cell=[](float v)->int{return std::isfinite(v)&&v>=0&&v<65536?static_cast<std::uint16_t>(v)/64:-10000;};
    if(ctx.presence)ctx.presence->ForEachInChannel(identity->channel,cid,[&](const ChannelPresenceEntry& entry,auto client){
        if(entry.map_id!=after.wMapID)return;
        if(std::hypot(entry.pos.x-after.fPosX,entry.pos.z-after.fPosZ)<=64.f)nearby.push_back(client);
        if(std::abs(cell(entry.pos.x)-cell(after.fPosX))<=1&&std::abs(cell(entry.pos.z)-cell(after.fPosZ))<=1)
            neighbors.push_back(client);
    });
    for(std::size_t i=0;i<committed.packets.size();++i) {
        const auto audience=committed.audiences[i];
        if(audience!=FinishAudience::Owner&&audience!=FinishAudience::Neighbors&&audience!=FinishAudience::Nearby)
            throw std::runtime_error("Invalid committed finish audience");
        const auto& clients=audience==FinishAudience::Owner?owner:audience==FinishAudience::Neighbors?neighbors:nearby;
        const auto& [id,packet]=committed.packets[i];
        for(auto& client:clients)co_await client->SendPacket(id,packet);
    }
}
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
