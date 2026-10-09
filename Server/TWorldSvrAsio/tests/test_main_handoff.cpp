// Real TCP + WorldServer::Drive/dispatch, synthetic registered Map peers and
// in-memory character registry. This is not native secondary Map persistence.
#include "../world_server.h"
#include "../services/char_registry.h"
#include "../services/peer_registry.h"
#include "../wire_codec.h"
#include "MessageId.h"
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <deque>
#include <cstdio>
#include <stdexcept>

namespace asio=boost::asio;
using tcp=asio::ip::tcp;
using namespace std::chrono_literals;
using tnetlib::protocol::MessageId;
using Bytes=std::vector<std::byte>;
static int checks=0;
static void Check(bool ok,const char* label) {if(!ok)throw std::runtime_error(label);++checks;}
static asio::awaitable<void> Pause() {
    asio::steady_timer timer(co_await asio::this_coro::executor);timer.expires_after(2ms);co_await timer.async_wait(asio::use_awaitable);
}
template<class F> static asio::awaitable<void> Until(F ready) {
    for(int n=0;!ready();++n) {if(n==2500)throw std::runtime_error("handoff protocol deadline");co_await Pause();}
}
struct Client {
    std::shared_ptr<tworldsvr::WorldSession> wire;
    std::deque<tworldsvr::DecodedPacket> frames;
};
static Bytes Identity(std::uint32_t key=0x1234) {
    Bytes b;tworldsvr::wire::WritePOD<std::uint32_t>(b,42);tworldsvr::wire::WritePOD(b,key);return b;
}
static Bytes Add(std::uint16_t port,std::uint32_t key=0x1234,std::uint32_t user=51,std::uint32_t ip=0x0100007f) {
    auto b=Identity(key);tworldsvr::wire::WritePOD(b,ip);tworldsvr::wire::WritePOD(b,port);tworldsvr::wire::WritePOD(b,user);return b;
}
static asio::awaitable<void> Send(const std::shared_ptr<Client>& c,MessageId id,Bytes b) {
    co_await c->wire->SendPacket(static_cast<std::uint16_t>(id),std::move(b));
}
static asio::awaitable<void> Expect(const std::shared_ptr<Client>& c,MessageId id,Bytes expected) {
    co_await Until([&]{return !c->frames.empty();});
    auto packet=std::move(c->frames.front());c->frames.pop_front();
    Check(packet.wId==static_cast<std::uint16_t>(id) && packet.body==expected,"exact expected peer opcode/body");
}
static Bytes Position() {
    auto b=Identity();tworldsvr::wire::WritePOD<std::uint8_t>(b,1);
    tworldsvr::wire::WritePOD<std::uint16_t>(b,7);
    for(float value:{80.f,90.f,100.f})tworldsvr::wire::WritePOD(b,value);
    return b;
}
static Bytes Enter(std::uint8_t result=0) {
    using namespace tworldsvr::wire;
    auto b=Identity();WriteString(b,"TransferHero");
    for(std::uint8_t value:{10,0,1,0,0,0,0,0,0,3})WritePOD(b,value);
    WritePOD<std::uint32_t>(b,0);WritePOD<std::uint8_t>(b,1);WritePOD<std::uint16_t>(b,7);
    for(float value:{80.f,90.f,100.f})WritePOD(b,value);
    WritePOD<std::uint8_t>(b,0);WritePOD<std::uint8_t>(b,0);WritePOD(b,result);
    WritePOD<std::uint16_t>(b,0);WritePOD<std::uint32_t>(b,0);WritePOD<std::uint32_t>(b,0);
    return b;
}
static Bytes Released() {
    Bytes b{std::byte{0}};auto id=Identity();b.insert(b.end(),id.begin(),id.end());
    b.push_back(std::byte{0});tworldsvr::wire::WriteString(b,"TransferHero");
    // Deliberately opaque transport fixture, not a claim of a complete Map
    // state codec. Every byte must survive World forwarding without a reload.
    for(unsigned n=0;n<32000;++n)b.push_back(static_cast<std::byte>(n*71));
    return b;
}
static asio::awaitable<void> ExpectOpcode(const std::shared_ptr<Client>& c,MessageId id) {
    co_await Until([&]{return !c->frames.empty();});
    Check(c->frames.front().wId==static_cast<std::uint16_t>(id),"expected source exchange opcode");c->frames.pop_front();
}
static asio::awaitable<void> Barrier(const std::shared_ptr<Client>& c,std::uint8_t sid) {
    co_await Send(c,MessageId::MW_ADDCHAR_ACK,Add(4000+sid));
    auto invalid=Identity();invalid.push_back(std::byte{0});
    co_await Expect(c,MessageId::MW_INVALIDCHAR_REQ,invalid);
}
static asio::awaitable<void> Populate(const std::vector<std::shared_ptr<Client>>& clients,bool fresh) {
    using namespace tworldsvr::wire;
    auto main=clients[0];
    if(fresh) {
        co_await Send(main,MessageId::MW_ADDCHAR_ACK,Add(4001));
        Bytes enter{std::byte{1}};auto id=Identity();enter.insert(enter.end(),id.begin(),id.end());
        co_await Expect(main,MessageId::MW_ENTERSVR_REQ,enter);
    }
    co_await Send(main,MessageId::MW_ENTERSVR_ACK,Enter());
    co_await ExpectOpcode(main,MessageId::MW_CHARINFO_REQ);
    co_await Expect(main,MessageId::MW_ROUTE_REQ,Position());
    co_await ExpectOpcode(main,MessageId::MW_FRIENDLIST_REQ);
    auto routes=Identity();WritePOD<std::uint8_t>(routes,2);
    for(std::uint8_t sid=2;sid<=3;++sid) {WritePOD<std::uint32_t>(routes,0x0100007f);WritePOD<std::uint16_t>(routes,4000+sid);WritePOD(routes,sid);}
    co_await Send(main,MessageId::MW_ROUTE_ACK,routes);
    co_await Expect(main,MessageId::MW_ADDCONNECT_REQ,routes);
    for(std::uint8_t sid=2;sid<=3;++sid)co_await Send(clients[sid-1],MessageId::MW_ADDCHAR_ACK,Add(4000+sid));
    co_await Expect(main,MessageId::MW_CHARDATA_REQ,Identity());
    auto data=Identity();WritePOD<std::uint8_t>(data,0);WritePOD<std::uint8_t>(data,10);
    for(std::uint32_t value:{100,90,80,70})WritePOD(data,value);
    for(std::uint8_t value:{0,0,0})WritePOD(data,value);
    WriteString(data,"");
    co_await Send(main,MessageId::MW_CHARDATA_ACK,data);
    for(auto& client:clients) {co_await ExpectOpcode(client,MessageId::MW_ENTERCHAR_REQ);co_await Send(client,MessageId::MW_ENTERCHAR_ACK,Identity());}
    for(auto& client:clients)co_await Expect(client,MessageId::MW_CHECKMAIN_REQ,Position());
}
static asio::awaitable<void> Round(const std::vector<std::shared_ptr<Client>>& clients,unsigned main) {
    auto b=Position();b.push_back(std::byte{0});co_await Send(clients[main-1],MessageId::MW_CHECKCONNECT_ACK,b);
    for(auto& client:clients)co_await Expect(client,MessageId::MW_CHECKMAIN_REQ,Position());
}
static Bytes Success() {auto b=Identity();for(unsigned v:{0,3,1,2,3})b.push_back(static_cast<std::byte>(v));return b;}
int main() {
    try {
        asio::io_context io;tworldsvr::CharRegistry chars;tworldsvr::PeerRegistry peers;
        tworldsvr::WorldServerConfig cfg;cfg.port=0;cfg.ctx.io=&io;cfg.ctx.chars=&chars;cfg.ctx.peers=&peers;cfg.ctx.main_handoff_timeout_ms=1000;
        tworldsvr::WorldServer world(io,cfg);
        std::exception_ptr failure;bool done=false;
        auto report=[&](std::exception_ptr e){if(e){failure=e;io.stop();}};
        std::vector<std::shared_ptr<Client>> clients;
        for(unsigned id=1;id<=3;++id) {
            tcp::acceptor accept(io,{tcp::v4(),0});tcp::socket socket(io);
            socket.connect({asio::ip::address_v4::loopback(),accept.local_endpoint().port()});
            auto wire=std::make_shared<tworldsvr::WorldSession>(accept.accept());
            auto peer=std::make_shared<tworldsvr::PeerSession>(wire);peer->SetWid(0x0400|id);
            Check(peers.Register(peer),"synthetic Map endpoint registered");
            asio::co_spawn(io,world.Drive(peer),report);
            auto client=std::make_shared<Client>();client->wire=std::make_shared<tworldsvr::WorldSession>(std::move(socket));
            clients.push_back(client);
            asio::co_spawn(io,client->wire->Run([client](auto,tworldsvr::DecodedPacket packet)->asio::awaitable<void>{
                client->frames.push_back(std::move(packet));co_return;
            }),report);
        }
        asio::co_spawn(io,[&]() -> asio::awaitable<void> {
            auto main=clients[0],second=clients[1],third=clients[2];
            co_await Send(main,MessageId::MW_ADDCHAR_ACK,Add(4001));
            auto enter=Bytes{std::byte{1}};auto id=Identity();enter.insert(enter.end(),id.begin(),id.end());
            co_await Expect(main,MessageId::MW_ENTERSVR_REQ,enter);
            auto invalid=Identity();invalid.push_back(std::byte{0});
            co_await Send(second,MessageId::MW_ADDCHAR_ACK,Add(4002));
            co_await Expect(second,MessageId::MW_INVALIDCHAR_REQ,invalid);
            auto ch=chars.Find(42);Check(ch && ch->cons.size()==1 && ch->main_server_id==1,"unsolicited connect preserves the valid main");
            auto retire=Identity();retire.push_back(std::byte{1});retire.push_back(std::byte{0});
            co_await Send(second,MessageId::MW_CLOSECHAR_ACK,Identity());
            co_await Expect(second,MessageId::MW_DELCHAR_REQ,retire);
            Check(chars.Find(42)==ch && chars.ActiveUserCount()==1,"unaccepted Map close cannot evict primary character/account");
            co_await Send(third,MessageId::MW_CHECKMAIN_ACK,Identity());
            // An ordered duplicate registration is a barrier proving CHECKMAIN
            // finished on this peer before inspecting the original primary.
            co_await Send(third,MessageId::MW_ADDCHAR_ACK,Add(4003));
            co_await Expect(third,MessageId::MW_INVALIDCHAR_REQ,invalid);
            Check(ch->main_server_id==1 && main->frames.empty(),"unrequested CHECKMAIN from an unaccepted Map cannot transfer primary");
            co_await Populate(clients,false);
            co_await Send(main,MessageId::MW_CHECKMAIN_ACK,Identity());
            co_await Expect(main,MessageId::MW_CONRESULT_REQ,Success());
            Check(!ch->db_loading&&!ch->main_handoff&&ch->main_checks.empty(),"fresh load and main-check responses consumed");
            co_await Send(second,MessageId::MW_ENTERSVR_ACK,Enter());co_await Barrier(second,2);
            Check(ch->main_server_id==1&&second->frames.empty(),"secondary cannot forge a fresh primary load");
            co_await Round(clients,1);
            auto malformed=Identity();malformed.push_back(std::byte{0});
            co_await Send(third,MessageId::MW_CHECKMAIN_ACK,malformed);co_await Barrier(third,3);
            Check(ch->main_server_id==1&&!ch->main_handoff,"trailing CHECKMAIN body cannot consume round");
            co_await Send(second,MessageId::MW_CHECKMAIN_ACK,Identity());
            co_await Expect(main,MessageId::MW_RELEASEMAIN_REQ,Position());
            Check(ch->main_server_id==2&&ch->main_handoff&&ch->main_handoff->phase==tworldsvr::MainHandoffPhase::Release,"expected target starts one release stage");
            co_await Send(third,MessageId::MW_CHECKMAIN_ACK,Identity());co_await Barrier(third,3);
            Check(ch->main_server_id==2&&main->frames.empty(),"competing or delayed main answer cannot replace pending target");
            co_await Send(second,MessageId::MW_ENTERSVR_ACK,Enter());co_await Barrier(second,2);
            Check(ch->main_handoff->phase==tworldsvr::MainHandoffPhase::Release,"target ENTER before release cannot complete handoff");
            co_await Send(third,MessageId::MW_RELEASEMAIN_ACK,Released());co_await Barrier(third,3);
            Check(second->frames.empty(),"unrelated accepted Map cannot inject released state");
            auto wrong=Released();wrong[0]=std::byte{2};
            co_await Send(main,MessageId::MW_RELEASEMAIN_ACK,wrong);co_await Barrier(main,1);
            Check(ch->main_handoff->phase==tworldsvr::MainHandoffPhase::Release,"nonboolean release flag refused");
            {
                tcp::acceptor accept(io,{tcp::v4(),0});tcp::socket socket(io);
                socket.connect({asio::ip::address_v4::loopback(),accept.local_endpoint().port()});
                auto wire=std::make_shared<tworldsvr::WorldSession>(accept.accept());
                auto stale=std::make_shared<tworldsvr::PeerSession>(wire);stale->SetWid(0x0401);
                Check(!peers.Register(stale),"duplicate server ID cannot replace current source peer");
                asio::co_spawn(io,world.Drive(stale),report);
                auto copy=std::make_shared<Client>();copy->wire=std::make_shared<tworldsvr::WorldSession>(std::move(socket));
                asio::co_spawn(io,copy->wire->Run([copy](auto,tworldsvr::DecodedPacket packet)->asio::awaitable<void>{
                    copy->frames.push_back(std::move(packet));co_return;
                }),report);
                co_await Send(copy,MessageId::MW_RELEASEMAIN_ACK,Released());co_await Barrier(copy,1);
                Check(second->frames.empty()&&ch->main_handoff->phase==tworldsvr::MainHandoffPhase::Release,"same server ID on another socket cannot release the primary");
                copy->wire->Close();
            }
            co_await Send(main,MessageId::MW_RELEASEMAIN_ACK,Released());
            co_await Expect(second,MessageId::MW_ENTERSVR_REQ,Released());
            co_await Send(main,MessageId::MW_RELEASEMAIN_ACK,Released());co_await Barrier(main,1);
            Check(second->frames.empty()&&ch->main_handoff->phase==tworldsvr::MainHandoffPhase::Enter,"release accepted once and opaque graph preserved");
            co_await Send(third,MessageId::MW_ENTERSVR_ACK,Enter());co_await Barrier(third,3);
            Check(ch->main_handoff->phase==tworldsvr::MainHandoffPhase::Enter,"only expected target can acknowledge load");
            co_await Send(second,MessageId::MW_ENTERSVR_ACK,Enter());
            co_await Expect(second,MessageId::MW_MAPSVRLIST_REQ,Position());
            Check(ch->main_handoff->phase==tworldsvr::MainHandoffPhase::Confirm&&ch->chg_main_id==0,"loaded target awaits final confirmation");
            co_await Send(second,MessageId::MW_ENTERSVR_ACK,Enter());co_await Barrier(second,2);
            Check(second->frames.empty(),"duplicate target load cannot reinitialize client or restart routing");
            auto list=Identity();for(unsigned value:{2,1,3})list.push_back(static_cast<std::byte>(value));
            co_await Send(second,MessageId::MW_MAPSVRLIST_ACK,list);
            for(auto& c:clients)co_await Expect(c,MessageId::MW_CHECKMAIN_REQ,Position());
            co_await Send(main,MessageId::MW_CHECKMAIN_ACK,Identity());co_await Barrier(main,1);
            Check(ch->main_server_id==2&&ch->main_handoff,"old primary cannot reverse pending transfer confirmation");
            co_await Send(second,MessageId::MW_CHECKMAIN_ACK,Identity());
            co_await Expect(second,MessageId::MW_CONRESULT_REQ,Success());
            Check(!ch->main_handoff&&ch->main_checks.empty()&&chars.ActiveUserCount()==1,"one target confirmation completes World handoff");
            co_await Send(main,MessageId::MW_RELEASEMAIN_ACK,Released());co_await Barrier(main,1);
            Check(second->frames.empty(),"late release cannot replay after completion");
            // Deadline stays armed until confirmation, not merely until ENTER.
            co_await Round(clients,2);co_await Send(third,MessageId::MW_CHECKMAIN_ACK,Identity());
            co_await Expect(second,MessageId::MW_RELEASEMAIN_REQ,Position());
            co_await Send(second,MessageId::MW_RELEASEMAIN_ACK,Released());
            co_await Expect(third,MessageId::MW_ENTERSVR_REQ,Released());
            co_await Until([&]{return !chars.Find(42);});
            auto invalid_release=Identity();invalid_release.push_back(std::byte{1});
            co_await Expect(second,MessageId::MW_INVALIDCHAR_REQ,invalid_release);
            auto del=Identity();del.push_back(std::byte{0});del.push_back(std::byte{0});
            for(auto& c:clients)co_await Expect(c,MessageId::MW_DELCHAR_REQ,del);
            Check(chars.ActiveUserCount()==0,"stalled target retires every World connection and active user");
            for(bool after_enter:{false,true}) {
                co_await Populate(clients,true);
                co_await Send(main,MessageId::MW_CHECKMAIN_ACK,Identity());
                co_await Expect(main,MessageId::MW_CONRESULT_REQ,Success());
                co_await Round(clients,1);co_await Send(second,MessageId::MW_CHECKMAIN_ACK,Identity());
                co_await Expect(main,MessageId::MW_RELEASEMAIN_REQ,Position());
                if(after_enter) {
                    co_await Send(main,MessageId::MW_RELEASEMAIN_ACK,Released());
                    co_await Expect(second,MessageId::MW_ENTERSVR_REQ,Released());
                    co_await Send(second,MessageId::MW_ENTERSVR_ACK,Enter());
                    co_await Expect(second,MessageId::MW_MAPSVRLIST_REQ,Position());
                }
                co_await Until([&]{return !chars.Find(42);});
                if(!after_enter)co_await Expect(main,MessageId::MW_INVALIDCHAR_REQ,invalid_release);
                for(auto& c:clients)co_await Expect(c,MessageId::MW_DELCHAR_REQ,del);
                Check(chars.ActiveUserCount()==0,after_enter?"confirmation stage shares bounded handoff deadline":"source release stage shares bounded handoff deadline");
            }
            for(auto& c:clients)c->wire->Close();
            done=true;
        },report);
        io.run();if(failure)std::rethrow_exception(failure);Check(done,"handoff flow completes");
        std::printf("%d World handoff checks passed\n",checks);return 0;
    }catch(const std::exception& e){std::fprintf(stderr,"World handoff regression: %s\n",e.what());return 1;}
}
