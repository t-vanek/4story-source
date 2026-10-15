#pragma once
#include "domain/party.h"
#include "handlers.h"
#include "channel_presence.h"
#include "char_state_store.h"
#include "player_service.h"
#include "session_registry.h"
#include "fourstory/db/co_offload.h"
#include <cmath>

namespace tmapsvr {
inline bool SameNativeIdentity(const SessionIdentity& a,const SessionIdentity& b) {
    return a.char_id==b.char_id&&a.user_id==b.user_id&&a.key==b.key&&a.channel==b.channel&&
        a.connection_id==b.connection_id&&a.authority_epoch==b.authority_epoch&&a.role==b.role&&b.phase==SessionPhase::Ready;
}
inline PartyProjection CaptureNativePartyProjection(const CharacterClusterState& s) {
    return {s.party,s.commander,s.party_chief,s.party_type,s.hydrated};
}
inline bool SameNativePartyProjection(const PartyProjection& a,const CharacterClusterState& b) {
    return a.hydrated&&b.hydrated&&a==CaptureNativePartyProjection(b);
}
// Caller already owns the actor operation. Never wait for another actor's
// checkpoint while holding it; busy/stale recipients may miss this projection.
inline boost::asio::awaitable<bool> DeliverNativeGridProjection(
    const std::shared_ptr<tnetlib::AsioSession>& actor,const SessionIdentity& expected,
    const std::shared_ptr<tnetlib::AsioSession>& recipient,std::uint16_t opcode,
    const std::vector<std::byte>& body,const HandlerContext& ctx,bool nearby=false,
    const PartyProjection* party=nullptr) {
    if(!ctx.session_reg||!ctx.char_state||!ctx.presence||!ctx.player_service)co_return false;
    const auto source=ctx.session_reg->Identity(actor.get()),id=ctx.session_reg->Identity(recipient.get());
    if(!actor->IsOpen()||!source||!SameNativeIdentity(expected,*source)||!recipient->IsOpen()||!id||id->phase!=SessionPhase::Ready)co_return false;
    if(party) {
        const auto state=ctx.char_state->Get(expected.char_id);
        if(!state||!SameNativePartyProjection(*party,state->cluster))co_return false;
    }
    std::optional<SessionOperation> operation;
    if(actor.get()!=recipient.get()) {
        if(!ctx.session_reg->BeginGameplay(recipient.get()))co_return false;
        operation.emplace(*ctx.session_reg,recipient.get());
    }
    const auto live=ctx.char_state->Get(id->char_id);bool authorized=false;
    if(live)try {
        auto* service=ctx.player_service;
        authorized=bool(co_await fourstory::db::CoOffloadIf(ctx.db_pool,
            [service,claim=id->Claim(ctx.expected_group),&live]{return service->AuthorizeParty(claim,*live);}));
    }catch(...){authorized=false;}
    if(!authorized){if(actor.get()==recipient.get())actor->Close();co_return false;}
    const auto current_source=ctx.session_reg->Identity(actor.get()),current=ctx.session_reg->Identity(recipient.get());
    const auto position=ctx.presence->FindEntry(expected.char_id),neighbor=ctx.presence->FindEntry(id->char_id);
    if(party) {
        const auto state=ctx.char_state->Get(expected.char_id);
        if(!state||!SameNativePartyProjection(*party,state->cluster))co_return false;
    }
    const auto cell=[](float v){return std::isfinite(v)&&v>=0&&v<65536?static_cast<std::uint16_t>(v)/64:-10000;};
    if(!actor->IsOpen()||!current_source||!SameNativeIdentity(expected,*current_source)||!recipient->IsOpen()||!current||!SameNativeIdentity(*id,*current)||
       !position||!neighbor||position->session.lock().get()!=actor.get()||neighbor->session.lock().get()!=recipient.get()||
       position->channel!=expected.channel||neighbor->channel!=expected.channel||id->channel!=expected.channel||position->map_id!=neighbor->map_id||
       std::abs(cell(position->pos.x)-cell(neighbor->pos.x))>1||std::abs(cell(position->pos.z)-cell(neighbor->pos.z))>1)co_return false;
    if(nearby&&std::hypot(position->pos.x-neighbor->pos.x,position->pos.z-neighbor->pos.z)>64.f)co_return false;
    co_await recipient->SendPacket(opcode,body);co_return true; // queue attempt, never an application ACK
}
}
