#include "handlers_world.h"
#include "wire_codec.h"
#include "services/session_registry.h"
#include "services/session_validator.h"
#include "services/char_state_store.h"
#include "services/client_senders.h"
#include "services/main_transfer_runtime.h"
#include "services/world_client.h"
#include "services/world_senders.h"
#include "fourstory/db/co_offload.h"
#include "MessageId.h"
#include <cmath>
namespace tmapsvr {
using tnetlib::protocol::MessageId;
// Fresh native admission. Durable guild/recall state is rejected by the loader
// until those repositories are supported; packet fields still retain their
// original widths and order, including the Windows BOOL in CHARINFO_REQ.
boost::asio::awaitable<void> OnMWCharInfoReq(std::vector<std::byte> body,const HandlerContext& ctx){
    wire::Reader r(body.data(),body.size());std::uint32_t cid{},key{};
    if(!r.Read(cid)||!r.Read(key))co_return;
    auto client=ctx.session_reg?ctx.session_reg->Find(cid,key):nullptr;
    auto snap=ctx.char_state?ctx.char_state->Get(cid):std::nullopt;
    if(!client||!snap||!snap->payload)co_return;
    const auto identity=ctx.session_reg->Identity(client.get());
    if(!identity||identity->role!=MapSessionRole::Primary||identity->phase!=SessionPhase::Loaded||snap->cluster.hydrated)co_return;
    if(!ctx.skill_cooldown){client->Close();co_return;}
    auto c=snap->cluster;std::uint16_t title{};std::uint32_t rank{},bow{};
    if(!r.Read(c.guild)||!r.Read(c.guild_country)||!r.ReadString(c.guild_name)||!r.Read(c.fame)||!r.Read(c.fame_color)||
       !r.Read(c.tactics)||!r.ReadString(c.tactics_name)||!r.Read(c.duty)||!r.Read(c.peer)||!r.Read(c.castle)||!r.Read(c.camp)||
       !r.Read(c.party)||!r.Read(c.party_type)||!r.Read(c.party_chief)||!r.Read(title)||!r.Read(rank)||!r.Read(bow)||!r.Eof()||bow){client->Close();co_return;}
    c.hydrated=true;
    auto payload=std::make_shared<CharacterPayload>(*snap->payload);payload->selected_title=title;payload->rank_point=rank;
    ctx.char_state->Update(cid,[&](CharSnapshot& s){s.cluster=c;s.payload=std::move(payload);});
    const auto hydrated=ctx.char_state->Get(cid);
    const std::array<std::byte,2> ack{std::byte{0},std::byte{1}};
    co_await client->SendPacket(static_cast<std::uint16_t>(MessageId::CS_CHGCHANNEL_ACK),ack);
    // Legacy OnMW_CHARINFO_REQ sends this before the route/CONRESULT exchange.
    // Client OnCS_CONNECT_ACK activates its frame and calls OnRegionChanged;
    // its map/character must already be hydrated at that point.
    // CSSender.cpp samples GetReuseRemainTick(dwTick) while constructing the
    // packet. Loading, World hydration and the preceding write can take time;
    // reuse the live snapshot path without changing or rearming loaded timers.
    if(!client->IsOpen())co_return;
    std::vector<std::byte> info;
    try {
        const auto current=transfer::PersistenceSnapshot(*hydrated,key,*ctx.skill_cooldown,SkillClockMs());
        info=EncodeCharInfoAck(current,FormatServerClock());
    }catch(...){client->Close();co_return;}
    co_await client->SendPacket(static_cast<std::uint16_t>(MessageId::CS_CHARINFO_ACK),std::move(info));
}
boost::asio::awaitable<void> OnMWNativeRouteReq(std::vector<std::byte> body,const HandlerContext& ctx,bool server_list){
    wire::Reader r(body.data(),body.size());std::uint32_t cid{},key{};std::uint8_t channel{};std::uint16_t map{};float x{},y{},z{};
    if(!r.Read(cid)||!r.Read(key)||!r.Read(channel)||!r.Read(map)||!r.Read(x)||!r.Read(y)||!r.Read(z)||!r.Eof())co_return;
    auto client=ctx.session_reg?ctx.session_reg->Find(cid,key):nullptr;
    auto snap=ctx.char_state?ctx.char_state->Get(cid):std::nullopt;
    if(!client||!snap||!snap->payload||!ctx.validator||!ctx.world_client)co_return;
    auto identity=ctx.session_reg->Identity(client.get());
    if(!identity||identity->channel!=channel||snap->wMapID!=map||!std::isfinite(y)){client->Close();co_return;}
    std::optional<std::vector<std::uint8_t>> neighbors;
    try{auto* validator=ctx.validator;
        neighbors=co_await fourstory::db::CoOffloadIf(ctx.db_pool,[validator,claim=identity->Claim(ctx.expected_group),map,x,z]{return validator->NeighborServers(claim,map,x,z);});
    }catch(...){client->Close();co_return;}
    if(!client->IsOpen())co_return;
    if(!neighbors){client->Close();co_return;}
    if(server_list){
        auto reply=EncodeEnterCharAck(cid,key);wire::WritePOD<std::uint8_t>(reply,static_cast<std::uint8_t>(neighbors->size()));
        for(auto sid:*neighbors)wire::WritePOD(reply,sid);
        co_await ctx.world_client->SendPacket(static_cast<std::uint16_t>(MessageId::MW_MAPSVRLIST_ACK),std::move(reply));
    }else {
        std::vector<ServerRoute> routes;
        if(!neighbors->empty()) {
            if(!ctx.route_resolver){client->Close();co_return;}
            try{auto* resolver=ctx.route_resolver;
                routes=co_await fourstory::db::CoOffloadIf(ctx.db_pool,[resolver,claim=identity->Claim(ctx.expected_group),ids=*neighbors]{return resolver->ResolveAuthorized(claim,ids);});
            }catch(...){client->Close();co_return;}
            if(routes.size()!=neighbors->size()){client->Close();co_return;}
        }
        if(client->IsOpen())co_await ctx.world_client->SendPacket(static_cast<std::uint16_t>(MessageId::MW_ROUTE_ACK),EncodeRouteAck(cid,key,routes));
    }
}
boost::asio::awaitable<void> OnMWCharDataReq(std::vector<std::byte> body,const HandlerContext& ctx){
    wire::Reader r(body.data(),body.size());std::uint32_t cid{},key{};
    if(!r.Read(cid)||!r.Read(key)||!r.Eof())co_return;
    auto client=ctx.session_reg?ctx.session_reg->Find(cid,key):nullptr;
    auto s=ctx.char_state?ctx.char_state->Get(cid):std::nullopt;
    if(!client||!s||!s->payload||!ctx.world_client||!s->cluster.hydrated)co_return;
    if(!s->payload->recalls.empty()){client->Close();co_return;}
    auto b=EncodeEnterCharAck(cid,key);
    wire::WritePOD(b,s->bStartAct);wire::WritePOD(b,s->bLevel);
    wire::WritePOD(b,s->dwMaxHP);wire::WritePOD(b,s->dwHP);wire::WritePOD(b,s->dwMaxMP);wire::WritePOD(b,s->dwMP);
    wire::WritePOD(b,s->bCountry);wire::WritePOD(b,s->cluster.mode);
    wire::WritePOD<std::uint8_t>(b,0);wire::WriteString(b,s->cluster.comment);
    co_await ctx.world_client->SendPacket(static_cast<std::uint16_t>(MessageId::MW_CHARDATA_ACK),std::move(b));
}
boost::asio::awaitable<void> OnMWNativeEnterCharReq(std::vector<std::byte> body,const HandlerContext& ctx){
    wire::Reader r(body.data(),body.size());std::uint32_t cid{},key{};if(!r.Read(cid)||!r.Read(key))co_return;
    auto client=ctx.session_reg?ctx.session_reg->Find(cid,key):nullptr;
    auto s=ctx.char_state?ctx.char_state->Get(cid):std::nullopt;
    if(!client||!ctx.char_state||!ctx.world_client)co_return;
    const auto identity=ctx.session_reg->Identity(client.get());if(!identity)co_return;
    const bool replica=identity->role==MapSessionRole::Replica;
    if(replica) {
        if(identity->phase!=SessionPhase::Pending||s||!ctx.validator)co_return;
        s=CharSnapshot{};s->dwCharID=cid;
        // The replica receives only the source cluster summary, not the core
        // graph. Unknown vitals/inventory must never become a client CHARINFO.
        s->dwHP=s->dwMP=s->dwMaxHP=s->dwMaxMP=0;
    } else if(!s||!s->payload)co_return;
    auto c=s->cluster;std::uint8_t start{},level{},helmet{},country{},aid{},klass{},recalls{};std::string name;std::uint16_t map{};float x{},y{},z{};
    if(!r.Read(start)||!r.ReadString(name)||!r.Read(map)||!r.Read(x)||!r.Read(y)||!r.Read(z)||
       !r.Read(c.guild)||!r.Read(c.fame)||!r.Read(c.fame_color)||!r.ReadString(c.guild_name)||!r.Read(c.duty)||!r.Read(c.peer)||
       !r.Read(c.castle)||!r.Read(c.camp)||!r.Read(c.tactics)||!r.ReadString(c.tactics_name)||!r.Read(c.party)||!r.Read(c.party_type)||
       !r.Read(c.party_chief)||!r.Read(c.commander)||!r.Read(level)||!r.Read(helmet)||!r.Read(country)||!r.Read(aid)||!r.Read(c.mode)||
       !r.Read(c.riding)||!r.Read(c.chat_ban_time)||!r.Read(c.soulmate)||!r.Read(c.soul_silence)||!r.ReadString(c.soulmate_name)||!r.Read(klass)||
       !r.Read(recalls)||recalls||!r.ReadString(c.comment)||!r.Eof()||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)){
        client->Close();co_return;
    }
    if(!replica&&(!c.hydrated||
       start!=s->bStartAct||name!=s->szNAME||map!=s->wMapID||x!=s->fPosX||y!=s->fPosY||z!=s->fPosZ||
       level!=s->bLevel||klass!=s->bClass||country!=s->bCountry||aid!=s->payload->aid_country||helmet!=s->bHelmetHide)){client->Close();co_return;}
    c.aid_country=aid;
    if(replica) {
        bool loaded=false;
        try{auto* validator=ctx.validator;
            loaded=co_await fourstory::db::CoOffloadIf(ctx.db_pool,[validator,claim=identity->Claim(ctx.expected_group),map,x,z]{return validator->LoadReplica(claim,map,x,z);});
        }catch(...){client->Close();co_return;}
        if(!loaded){client->Close();co_return;}
        if(!client->IsOpen())co_return;
        s->bStartAct=start;s->szNAME=std::move(name);s->wMapID=map;s->fPosX=x;s->fPosY=y;s->fPosZ=z;
        s->bLevel=level;s->bHelmetHide=helmet;s->bCountry=country;s->bClass=klass;c.hydrated=true;
        s->cluster=std::move(c);ctx.char_state->Store(cid,*s);
        // CONRESULT is sent only to the main Map. Its client then sends
        // CONREADY to all returned server IDs, including this replica.
        if(!ctx.session_reg->Transition(cid,key,SessionPhase::Pending,SessionPhase::Admitted)){client->Close();co_return;}
    }else ctx.char_state->Update(cid,[&](CharSnapshot& v){v.cluster=std::move(c);});
    co_await ctx.world_client->SendPacket(static_cast<std::uint16_t>(MessageId::MW_ENTERCHAR_ACK),EncodeEnterCharAck(cid,key));
}
}
