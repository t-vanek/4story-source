#pragma once
#include "services/title_relay_queue.h"
void TitleAudienceRouting() {
    struct Link final : tmapsvr::IWorldClient {
        bool IsConnected() const override{return true;}
        asio::awaitable<bool> SendPacket(std::uint16_t,Bytes) override{co_return true;}
    } world;
    asio::io_context io;tcp::acceptor acceptor(io,{asio::ip::address_v4::loopback(),0});
    tmapsvr::InMemorySessionRegistry registry;tmapsvr::InMemoryCharStateStore state;
    tmapsvr::InMemoryChannelPresence presence;tmapsvr::TitleRelayQueue queue;
    tmapsvr::HandlerContext ctx;ctx.session_reg=&registry;ctx.char_state=&state;ctx.presence=&presence;
    ctx.title_relays=&queue;ctx.world_client=&world;
    std::vector<std::shared_ptr<Client>> clients;std::vector<std::shared_ptr<tnetlib::AsioSession>> peers;
    for(unsigned i=0;i<7;++i) {
        clients.push_back(Dial(io,acceptor.local_endpoint().port()));tcp::socket socket(io);acceptor.accept(socket);
        auto peer=std::make_shared<tnetlib::AsioSession>(std::move(socket),tnetlib::PeerType::Client);
        peers.push_back(peer);presence.Bind(kChar+i,i==6?2:1,peer);
        const float x[]={128,129,192,250,256,129,129};
        presence.UpdatePosition(kChar+i,i==5?1:0,{x[i],0,i==3?250.f:128.f});
    }
    tmapsvr::SessionIdentity identity;identity.char_id=kChar;identity.user_id=kUser;identity.key=kKey;
    identity.channel=1;identity.phase=tmapsvr::SessionPhase::Ready;
    Check(registry.TryBind(identity,peers[0]),"title audience ready primary bound");
    tmapsvr::CharSnapshot character;character.dwCharID=kChar;character.fPosX=128;character.fPosZ=128;character.szNAME="A";
    auto payload=std::make_shared<tmapsvr::CharacterPayload>();payload->titles={{84,false},{85,true}};payload->selected_title=85;character.payload=payload;state.Store(kChar,character);
    const auto first=tmapsvr::EncodeTitleRelay(kChar,kKey,84,"A"),second=tmapsvr::EncodeTitleRelay(kChar,kKey,85,"A");
    queue.Add(kChar,peers[0],first);queue.Add(kChar,peers[0],second);
    std::exception_ptr error;
    asio::co_spawn(io,[&]() -> asio::awaitable<void> {
        co_await tmapsvr::OnWorldTitleChanged(tmapsvr::EncodeTitleRelay(kChar,kKey+1,84,"A"),ctx);
        co_await tmapsvr::OnWorldTitleChanged(second,ctx); // out of order, not consumed
        co_await tmapsvr::OnWorldTitleChanged(first,ctx);
        Check(state.Get(kChar)->payload->selected_title==85,"delayed World echo cannot revert newer committed primary state");
        co_await tmapsvr::OnWorldTitleChanged(second,ctx);
        co_await tmapsvr::OnWorldTitleChanged(second,ctx); // duplicate, not broadcast
        for(auto& peer:peers)co_await peer->SendPacket(0x7777,{});
        co_await Until([&]{return std::all_of(clients.begin(),clients.end(),[](const auto& c){return !c->packets.empty()&&c->packets.back().first==0x7777;});},"title audience barriers arrive");
        for(unsigned i=0;i<clients.size();++i) {
            const auto& packets=clients[i]->packets;
            Check(packets.size()==(i<4?3:1),"title change uses source three-by-three cells and map/channel boundaries");
            if(i<4)Check(packets[0].first==0x544a&&packets[0].second==tmapsvr::EncodeTitleChanged(kChar,84,"A")&&
                packets[1].first==0x544a&&packets[1].second==tmapsvr::EncodeTitleChanged(kChar,85,"A"),"title changes retain FIFO echo order with no duplicate success");
        }
        queue.Add(kChar,peers[0],first,3);queue.Add(kChar,peers[0],second,4);
        Check(queue.Take(kChar,peers[0],second,4),"same-socket successor tenure discards predecessor pending title echoes");
        Check(!queue.Take(kChar,peers[0],first,4),"old title echo cannot consume successor tenure");
        queue.Add(kChar,peers[0],first,4);queue.Discard(kChar,peers[0]);
        Check(!queue.Take(kChar,peers[0],first,4),"non-primary title relay clears abandoned notification queue");
        identity.char_id=kChar+1;identity.role=tmapsvr::MapSessionRole::Replica;
        Check(registry.TryBind(identity,peers[1]),"title audience replica bound");state.Store(kChar+1,character);
        co_await tmapsvr::OnWorldTitleChanged(tmapsvr::EncodeTitleRelay(kChar+1,kKey,84,"A"),ctx);
        Check(state.Get(kChar+1)->payload->selected_title==84,"World relay refreshes replica without durable primary writes");
        co_await peers[1]->SendPacket(0x7778,{});
        co_await Until([&]{return clients[1]->packets.back().first==0x7778;},"replica title barrier arrives");
        Check(clients[1]->packets.size()==4,"replica does not duplicate primary title broadcast");
        for(auto& c:clients)c->wire->Close();for(auto& p:peers)p->Close();
    },[&](std::exception_ptr e){error=e;io.stop();});
    io.run();if(error)std::rethrow_exception(error);
}
