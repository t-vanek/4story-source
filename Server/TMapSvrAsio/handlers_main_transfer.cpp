#include "handlers_world.h"
#include "services/channel_presence.h"
#include "services/char_state_store.h"
#include "services/session_registry.h"
#include "services/player_service.h"
#include "services/native_cell_runtime.h"
#include "services/main_transfer_runtime.h"
#include "services/main_transfer_codec.h"
#include "services/maintained_effects.h"
#include "services/client_senders.h"
#include "services/world_client.h"
#include "services/world_senders.h"
#include "fourstory/db/co_offload.h"
#include "wire_codec.h"
#include "MessageId.h"
#include <cmath>
#include <chrono>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>

namespace tmapsvr {
boost::asio::awaitable<void> OnMWReleaseMainReq(std::vector<std::byte> body,const HandlerContext& ctx) {
    wire::Reader r(body.data(),body.size());std::uint32_t cid{},key{};std::uint8_t channel{};std::uint16_t map{};float x{},y{},z{};
    if(!r.Read(cid)||!r.Read(key)||!r.Read(channel)||!r.Read(map)||!r.Read(x)||!r.Read(y)||!r.Read(z)||!r.Eof()||
       !std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z))co_return;
    auto client=ctx.session_reg?ctx.session_reg->Find(cid,key):nullptr;
    if(!client||!ctx.char_state||!ctx.player_service||!ctx.skill_cooldown||!ctx.world_client)co_return;
    auto id=ctx.session_reg->Identity(client.get());auto before=ctx.char_state->Get(cid);
    if(!id||id->channel!=channel||!before||!before->payload||before->wMapID!=map||
       !ctx.session_reg->BeginTransfer(client.get(),MapSessionRole::Primary))co_return;
    SessionOperation operation(*ctx.session_reg,client.get());
    boost::asio::steady_timer drain(co_await boost::asio::this_coro::executor);
    const auto drain_deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
    while(ctx.session_reg->Operations(client.get())>1){
        // A gameplay handler may be waiting on another client's full send
        // queue. It must not indefinitely block the World receive loop.
        if(!client->IsOpen()||std::chrono::steady_clock::now()>=drain_deadline){client->Close();co_return;}
        drain.expires_after(std::chrono::milliseconds(5));co_await drain.async_wait(boost::asio::use_awaitable);
    }
    if(ctx.presence)ctx.presence->UnbindIfMatches(client.get());
    std::optional<CharSnapshot> frozen;
    try {frozen=ctx.char_state->Freeze(cid,[&](CharSnapshot& s){
        s.fPosX=x;s.fPosY=y;s.fPosZ=z;
        const auto now=SkillClockMs();
        s=transfer::PersistenceSnapshot(s,key,*ctx.skill_cooldown,now,true);
        auto p=std::make_shared<CharacterPayload>(*s.payload);
        auto graph=std::make_shared<transfer::State>(transfer::Capture(s,key,*ctx.skill_cooldown,now));
        p->skills=graph->skills;p->transfer_state=std::move(graph);p->transfer_received_ms=now;s.payload=std::move(p);
    });}catch(...){client->Close();co_return;}
    if(!frozen){client->Close();co_return;}
    std::vector<std::byte> encoded;
    bool prepared=false;
    try {
        encoded=transfer::Encode(*frozen->payload->transfer_state);
        auto* players=ctx.player_service;
        prepared=co_await fourstory::db::CoOffloadIf(ctx.db_pool,[players,claim=id->Claim(ctx.expected_group),&frozen,&encoded]{
            return players->PrepareTransfer(claim,*frozen,encoded);
        });
    }catch(...){client->Close();co_return;}
    operation.Finish();
    if(!prepared){client->Close();co_return;}
    if(!client->IsOpen())co_return;
    // RELEASEMAIN samples and omits expired effects, then broadcasts their
    // original SKILLEND notification. It does not grant automatic EXP or run
    // the periodic 903 entitlement renewal merely because a transfer occurs.
    const auto& sample=*frozen->payload->effect_sample;
    std::vector<std::shared_ptr<tnetlib::AsioSession>> neighbors{client};
    const auto cell=[](float value)->int{return std::isfinite(value)&&value>=0&&value<65536?static_cast<std::uint16_t>(value)/64:-10000;};
    if(ctx.presence)ctx.presence->ForEachInChannel(channel,cid,[&](const ChannelPresenceEntry& entry,auto session){
        if(entry.map_id==map&&std::abs(cell(entry.pos.x)-cell(x))<=1&&std::abs(cell(entry.pos.z)-cell(z))<=1)
            neighbors.push_back(std::move(session));
    });
    for(const auto& e:sample.before)if(e.remaining&&sample.at_ms-e.sampled_ms>=e.remaining) {
        const auto packet=EncodeSkillEnd(cid,e.skill);
        for(auto& session:neighbors)co_await session->SendPacket(static_cast<std::uint16_t>(tnetlib::protocol::MessageId::CS_SKILLEND_ACK),packet);
    }
    if(!co_await ctx.world_client->SendPacket(static_cast<std::uint16_t>(tnetlib::protocol::MessageId::MW_RELEASEMAIN_ACK),std::move(encoded)))client->Close();
}

boost::asio::awaitable<void> OnMWTransferEnterReq(std::vector<std::byte> body,const HandlerContext& ctx) {
    auto decoded=transfer::Decode(body);
    if(!decoded||decoded->db_load)co_return;
    const auto cid=decoded->character.dwCharID,key=decoded->key;
    auto client=ctx.session_reg?ctx.session_reg->Find(cid,key):nullptr;
    if(!client||!ctx.char_state||!ctx.player_service||!ctx.skill_cooldown||!ctx.world_client)co_return;
    auto id=ctx.session_reg->Identity(client.get());auto replica=ctx.char_state->Get(cid);
    if(!id||!replica||!replica->cluster.hydrated||!ctx.session_reg->BeginTransfer(client.get(),MapSessionRole::Replica))co_return;
    SessionOperation operation(*ctx.session_reg,client.get());
    boost::asio::steady_timer drain(co_await boost::asio::this_coro::executor);
    const auto drain_deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
    while(ctx.session_reg->Operations(client.get())>1){
        if(!client->IsOpen()||std::chrono::steady_clock::now()>=drain_deadline){client->Close();co_return;}
        drain.expires_after(std::chrono::milliseconds(5));co_await drain.async_wait(boost::asio::use_awaitable);
    }
    // These clocks/mode belong to the existing target connection, not the
    // remote full DTO. Capture only after outstanding local operations drain.
    replica=ctx.char_state->Get(cid);
    if(!replica||!replica->recovery){client->Close();co_return;}
    if(ctx.presence)ctx.presence->UnbindIfMatches(client.get());
    std::optional<TransferredCharacter> incoming;
    auto* players=ctx.player_service;const auto claim=id->Claim(ctx.expected_group);
    // A write is attempted once. After an unknown result, the only permitted
    // follow-up is a read of the exact committed receipt. Never turn absence of
    // confirmation into another promotion attempt or an old replica publish.
    bool unknown=false;
    try {incoming=co_await fourstory::db::CoOffloadIf(ctx.db_pool,[players,claim,&body]{return players->AcceptTransfer(claim,body);});}
    catch(...){unknown=true;}
    if(unknown) {
        try {incoming=co_await fourstory::db::CoOffloadIf(ctx.db_pool,[players,claim,&body]{return players->ConfirmAcceptedTransfer(claim,body);});}
        catch(...){client->Close();co_return;}
    }
    if(!incoming){client->Close();co_return;}
    incoming->snapshot.cluster=replica->cluster;
    incoming->snapshot.recovery=replica->recovery;
    incoming->snapshot.movement=replica->movement; // receiver's CTPlayer survives the ownership change
    incoming->snapshot.rps=replica->rps;
    incoming->snapshot.rps.ClearPending(); // a result from an older ownership tenure is not replayed
    // Publish ownership even if the socket closed during PostgreSQL commit;
    // teardown waits for this operation and uses the promoted claim.
    ctx.char_state->Store(cid,incoming->snapshot);
    if(!ctx.session_reg->FinishTransfer(client.get(),MapSessionRole::Primary,incoming->authority_epoch)){client->Close();co_return;}
    if(ctx.player_service->HasNativeCellAuthority()) {
        try {
            const auto current=ctx.session_reg->Identity(client.get());
            if(!current)throw std::runtime_error("Native cell promotion lost its session");
            PublishNativeCellNotices(ctx,ctx.player_service->RebindNativePlayer(id->Claim(ctx.expected_group),
                current->Claim(ctx.expected_group),incoming->snapshot.wMapID,incoming->snapshot.fPosX,incoming->snapshot.fPosZ));
        } catch(...) {client->Close();co_return;}
    }
    try {ctx.skill_cooldown->Restore(cid,incoming->snapshot.payload->skills,SkillClockMs());}
    catch(...){client->Close();co_return;}
    operation.Finish();
    if(!client->IsOpen())co_return;
    const auto& p=*incoming->snapshot.payload;
    if(!co_await ctx.world_client->SendPacket(static_cast<std::uint16_t>(tnetlib::protocol::MessageId::MW_ENTERSVR_ACK),
        EncodeEnterSvrAck(incoming->snapshot,key,p.aid_country,id->channel,1,1,0,p.selected_title,p.rank_point,0)))client->Close();
}
}
