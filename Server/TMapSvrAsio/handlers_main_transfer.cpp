#include "handlers_world.h"
#include "services/channel_presence.h"
#include "services/char_state_store.h"
#include "services/session_registry.h"
#include "services/player_service.h"
#include "services/main_transfer_runtime.h"
#include "services/main_transfer_codec.h"
#include "services/world_client.h"
#include "services/world_senders.h"
#include "fourstory/db/co_offload.h"
#include "wire_codec.h"
#include "MessageId.h"
#include <cmath>
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
        auto p=std::make_shared<CharacterPayload>(*s.payload);
        auto graph=std::make_shared<transfer::State>(transfer::Capture(s,key,*ctx.skill_cooldown,SkillClockMs()));
        p->skills=graph->skills;p->transfer_state=std::move(graph);s.payload=std::move(p);
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
    if(ctx.presence)ctx.presence->UnbindIfMatches(client.get());
    std::optional<TransferredCharacter> incoming;
    auto* players=ctx.player_service;const auto claim=id->Claim(ctx.expected_group);
    // The backend's exact-body journal makes a lost COMMIT reply confirmable.
    // Never publish the old replica again after an uncertain primary commit.
    for(int attempt=0;attempt<2&&!incoming;++attempt) {
        try {incoming=co_await fourstory::db::CoOffloadIf(ctx.db_pool,[players,claim,&body]{return players->AcceptTransfer(claim,body);});}
        catch(...){if(attempt==1){client->Close();co_return;}}
    }
    if(!incoming){client->Close();co_return;}
    incoming->snapshot.cluster=replica->cluster;
    // Publish ownership even if the socket closed during PostgreSQL commit;
    // teardown waits for this operation and uses the promoted claim.
    ctx.char_state->Store(cid,incoming->snapshot);
    if(!ctx.session_reg->FinishTransfer(client.get(),MapSessionRole::Primary,incoming->authority_epoch)){client->Close();co_return;}
    try {ctx.skill_cooldown->Restore(cid,incoming->snapshot.payload->skills,SkillClockMs());}
    catch(...){client->Close();co_return;}
    operation.Finish();
    if(!client->IsOpen())co_return;
    const auto& p=*incoming->snapshot.payload;
    if(!co_await ctx.world_client->SendPacket(static_cast<std::uint16_t>(tnetlib::protocol::MessageId::MW_ENTERSVR_ACK),
        EncodeEnterSvrAck(incoming->snapshot,key,p.aid_country,id->channel,1,1,0,p.selected_title,p.rank_point,0)))client->Close();
}
}
