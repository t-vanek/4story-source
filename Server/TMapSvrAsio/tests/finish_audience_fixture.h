#pragma once
// Real transport/handler with a controlled commit result. PostgreSQL and source
// response bytes are independently exercised by the native finish fixture.
void FinishAudienceRouting() {
    struct Service final : tmapsvr::IPlayerService {
        std::optional<tmapsvr::CharSnapshot> LoadChar(std::uint32_t) override{return {};}
        void SaveChar(const tmapsvr::CharSnapshot&) override{}
        tmapsvr::FinishSkillCommit FinishSkill(const tmapsvr::MapSessionClaim&,std::span<const std::byte>,
            const tmapsvr::CharSnapshot& before) override {
            using A=tmapsvr::FinishAudience;
            tmapsvr::FinishSkillCommit out;out.snapshot=std::make_shared<const tmapsvr::CharSnapshot>(before);
            out.packets={{0x52b7,Bytes(7)},{0x5324,Bytes(87)},{0x52a1,Bytes(84)},{0x52a2,Bytes(21)}};
            out.audiences={A::Neighbors,A::Owner,A::Nearby,A::Nearby};return out;
        }
    } service;
    asio::io_context io;tcp::acceptor acceptor(io,{asio::ip::address_v4::loopback(),0});
    tmapsvr::InMemorySessionRegistry registry;tmapsvr::InMemoryCharStateStore state;
    tmapsvr::InMemoryChannelPresence presence;tmapsvr::SkillCooldownTracker timers;
    tmapsvr::HandlerContext ctx;ctx.player_service=&service;ctx.session_reg=&registry;
    ctx.char_state=&state;ctx.presence=&presence;ctx.skill_cooldown=&timers;
    std::vector<std::shared_ptr<Client>> clients;std::vector<std::shared_ptr<tnetlib::AsioSession>> peers;
    for(unsigned i=0;i<7;++i) {
        clients.push_back(Dial(io,acceptor.local_endpoint().port()));tcp::socket socket(io);acceptor.accept(socket);
        auto peer=std::make_shared<tnetlib::AsioSession>(std::move(socket),tnetlib::PeerType::Client);
        peers.push_back(peer);presence.Bind(kChar+i,i==6?2:1,peer);
        // Owner, inside radius, exactly on radius, cell neighbor outside radius,
        // outside cell neighborhood, other map, other channel.
        const float x[]={128,129,192,250,256,129,129};
        presence.UpdatePosition(kChar+i,i==5?1:0,{x[i],0,i==3?250.f:128.f});
    }
    tmapsvr::SessionIdentity identity;identity.char_id=kChar;identity.user_id=kUser;identity.key=kKey;identity.channel=1;
    Check(registry.TryBind(identity,peers[0]),"finish audience owner is bound");
    tmapsvr::CharSnapshot character;character.dwCharID=kChar;character.fPosX=128;character.fPosZ=128;state.Store(kChar,character);
    Bytes request;WritePOD(request,kChar);WritePOD(request,kChar);WritePOD(request,std::uint8_t{1});
    WritePOD(request,128.f);WritePOD(request,0.f);WritePOD(request,128.f);WritePOD(request,std::uint16_t{509});
    WritePOD(request,std::uint32_t{0});WritePOD(request,std::uint32_t{0});WritePOD(request,std::uint16_t{0});WritePOD(request,std::uint8_t{0});
    std::exception_ptr error;
    asio::co_spawn(io,[&]() -> asio::awaitable<void> {
        co_await tmapsvr::OnFinishSkill(peers[0],request,ctx);
        for(auto& peer:peers)co_await peer->SendPacket(0x7777,{});
        co_await Until([&]{return std::all_of(clients.begin(),clients.end(),[](const auto& c){return !c->packets.empty()&&c->packets.back().first==0x7777;});},"finish audience barriers arrive on every actual socket");
        const std::vector<std::vector<std::uint16_t>> expected={
            {0x52b7,0x5324,0x52a1,0x52a2,0x7777},
            {0x52b7,0x52a1,0x52a2,0x7777},{0x52b7,0x52a1,0x52a2,0x7777},
            {0x52b7,0x7777},{0x7777},{0x7777},{0x7777}};
        for(unsigned i=0;i<clients.size();++i) {
            std::vector<std::uint16_t> ids;for(const auto& p:clients[i]->packets)ids.push_back(p.first);
            Check(ids==expected[i],"finish preserves source recipient scope/order and never broadcasts private statistics");
        }
        for(auto& c:clients)c->wire->Close();for(auto& p:peers)p->Close();
    },[&](std::exception_ptr e){error=e;io.stop();});
    io.run();if(error)std::rethrow_exception(error);
}
