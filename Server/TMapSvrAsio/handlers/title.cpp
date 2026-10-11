#include "handlers.h"
#include "config.h"
#include "services/character_titles.h"
#include "services/char_state_store.h"
#include "services/channel_presence.h"
#include "services/main_transfer_runtime.h"
#include "services/player_service.h"
#include "services/session_registry.h"
#include "services/title_relay_queue.h"
#include "services/world_client.h"
#include "fourstory/db/co_offload.h"
#include "MessageId.h"
#include <cmath>

namespace tmapsvr {
boost::asio::awaitable<void> OnTitleListReq(std::shared_ptr<tnetlib::AsioSession> client,
    std::vector<std::byte> body,const HandlerContext& ctx) {
    if(ctx.mode==Mode::Bow)co_return;
    if(!body.empty())throw std::runtime_error("Invalid TITLELIST request length");
    const auto identity=ctx.session_reg?ctx.session_reg->Identity(client.get()):std::nullopt;
    if(!identity||identity->phase!=SessionPhase::Ready||identity->role!=MapSessionRole::Primary||!ctx.char_state)co_return;
    const auto state=ctx.char_state->Get(identity->char_id);
    if(!state||!state->payload||state->persistence_uncertain||state->bStartAct==2||(state->bStartAct==1&&state->wMapID==2010))co_return;
    const auto packet=EncodeTitleList(state->payload->titles);
    co_await client->SendPacket(static_cast<std::uint16_t>(tnetlib::protocol::MessageId::CS_TITLELIST_ACK),packet);
}
boost::asio::awaitable<void> OnChangeTitleReq(std::shared_ptr<tnetlib::AsioSession> client,
    std::vector<std::byte> body,const HandlerContext& ctx) {
    if(ctx.mode==Mode::Bow)co_return;
    DecodeTitleChange(body);
    const auto identity=ctx.session_reg?ctx.session_reg->Identity(client.get()):std::nullopt;
    if(!identity||identity->phase!=SessionPhase::Ready||identity->role!=MapSessionRole::Primary)co_return;
    if(!ctx.char_state||!ctx.player_service||!ctx.skill_cooldown||!ctx.title_relays||!ctx.world_client||!ctx.world_client->IsRegistered())
        throw std::runtime_error("Title selection dependencies unavailable");
    const auto cid=identity->char_id;const auto original=ctx.char_state->Freeze(cid,[](auto&){});
    if(!original)co_return;
    TitleChangeCommit committed;
    try {
        const auto before=transfer::PersistenceSnapshot(*original,identity->key,*ctx.skill_cooldown,SkillClockMs());
        auto* service=ctx.player_service;
        committed=co_await fourstory::db::CoOffloadIf(ctx.db_pool,[service,claim=identity->Claim(ctx.expected_group),&body,&before]{
            return service->SelectTitle(claim,body,before);
        });
        if(!committed.snapshot)throw std::runtime_error("Missing title commit state");
        ctx.char_state->Store(cid,*committed.snapshot);
    }catch(...) {
        auto uncertain=*original;uncertain.persistence_uncertain=true;ctx.char_state->Store(cid,uncertain);client->Close();throw;
    }
    if(!committed.changed)co_return;
    const auto& after=*committed.snapshot;
    auto relay=EncodeTitleRelay(cid,identity->key,after.payload->selected_title,after.szNAME);
    ctx.title_relays->Add(cid,client,relay,identity->authority_epoch);
    if(!co_await ctx.world_client->SendPacket(static_cast<std::uint16_t>(tnetlib::protocol::MessageId::MW_CHANGECHARBASE_ACK),std::move(relay)))client->Close();
}
boost::asio::awaitable<void> OnWorldTitleChanged(std::vector<std::byte> body,const HandlerContext& ctx) {
    if(!ctx.session_reg||!ctx.char_state||!ctx.world_client||!ctx.world_client->IsRegistered())co_return;
    wire::Reader r(body);std::uint32_t cid{},key{};std::uint8_t kind{},value{};std::uint16_t title{};std::string name;
    if(!r.Read(cid)||!r.Read(key)||!r.Read(kind)||!r.Read(value)||!r.Read(title)||!r.ReadString(name)||!r.Eof()||kind!=TitleItemKind||value)co_return;
    const auto client=ctx.session_reg->Find(cid);
    const auto identity=client?ctx.session_reg->Identity(client.get()):std::nullopt;
    if(!identity||identity->key!=key)co_return;
    if(identity->phase!=SessionPhase::Ready||identity->role==MapSessionRole::Replica)
        if(ctx.title_relays)ctx.title_relays->Discard(cid,client);
    if(identity->phase!=SessionPhase::Ready)co_return;
    if(identity->role==MapSessionRole::Replica) {
        // Replicas never persist or broadcast this source primary notification.
        ctx.char_state->Update(cid,[&](auto& state){if(state.payload){auto p=std::make_shared<CharacterPayload>(*state.payload);
            p->selected_title=title;for(auto& t:p->titles)t.selected=t.id==title;state.payload=std::move(p);}});
        co_return;
    }
    if(!ctx.title_relays||!ctx.title_relays->Take(cid,client,body,identity->authority_epoch))co_return;
    const auto state=ctx.char_state->Get(cid);if(!state||state->persistence_uncertain)co_return;
    const auto packet=EncodeTitleChanged(cid,title,name);
    std::vector<std::shared_ptr<tnetlib::AsioSession>> neighbors{client};
    const auto cell=[](float v)->int{return std::isfinite(v)&&v>=0&&v<65536?static_cast<std::uint16_t>(v)/64:-10000;};
    if(ctx.presence)ctx.presence->ForEachInChannel(identity->channel,cid,[&](const ChannelPresenceEntry& e,auto other){
        if(e.map_id==state->wMapID&&std::abs(cell(e.pos.x)-cell(state->fPosX))<=1&&std::abs(cell(e.pos.z)-cell(state->fPosZ))<=1)
            neighbors.push_back(std::move(other));
    });
    // Do not reapply a delayed echo to the primary: a later selection or skill
    // purchase may already be committed. Only notify in original World order.
    for(auto& neighbor:neighbors)co_await neighbor->SendPacket(static_cast<std::uint16_t>(tnetlib::protocol::MessageId::CS_CHANGECHARBASE_ACK),packet);
}
}
