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
    for(int n=0;!ready();++n) {if(n==500)throw std::runtime_error("secondary protocol deadline");co_await Pause();}
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
int main() {
    try {
        asio::io_context io;tworldsvr::CharRegistry chars;tworldsvr::PeerRegistry peers;
        tworldsvr::WorldServerConfig cfg;cfg.port=0;cfg.ctx.io=&io;cfg.ctx.chars=&chars;cfg.ctx.peers=&peers;
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
            auto routes=Identity();tworldsvr::wire::WritePOD<std::uint8_t>(routes,2);
            for(std::uint8_t server=2;server<=3;++server) {
                tworldsvr::wire::WritePOD<std::uint32_t>(routes,0x0100007f);
                tworldsvr::wire::WritePOD<std::uint16_t>(routes,4000+server);
                tworldsvr::wire::WritePOD(routes,server);
            }
            co_await Send(main,MessageId::MW_ROUTE_ACK,routes);
            co_await Expect(main,MessageId::MW_ADDCONNECT_REQ,routes);
            Check(ch->cons.size()==3 && !ch->cons[1].valid && !ch->cons[2].valid,"route installs expected pending connections");
            co_await Send(second,MessageId::MW_CLOSECHAR_ACK,Identity());
            co_await Expect(second,MessageId::MW_DELCHAR_REQ,retire);
            Check(chars.Find(42)==ch && !ch->cons[1].valid,"planned but unaccepted Map cannot close the character");
            for(int bad=0;bad<4;++bad) {
                auto key=bad==0?0x9999u:0x1234u;
                co_await Send(second,MessageId::MW_ADDCHAR_ACK,Add(bad==1?4999:4002,key,bad==2?99:51,bad==3?0x0200007f:0x0100007f));
                auto reject=Identity(key);reject.push_back(std::byte{0});
                co_await Expect(second,MessageId::MW_INVALIDCHAR_REQ,reject);
                Check(ch->cons.size()==3 && !ch->cons[1].valid && ch->user_id==51 && ch->key==0x1234,"bad identity/endpoint cannot mutate pending or main state");
            }
            co_await Send(second,MessageId::MW_ADDCHAR_ACK,Add(4002));
            co_await Until([&]{return ch->cons[1].valid;});
            Check(!ch->cons[1].ready && !ch->cons[2].valid && main->frames.empty(),"partial connection set waits without premature data synchronization");
            co_await Send(second,MessageId::MW_ADDCHAR_ACK,Add(4002));
            co_await Expect(second,MessageId::MW_INVALIDCHAR_REQ,invalid);
            Check(ch->cons.size()==3 && ch->cons[1].valid,"duplicate does not append a second connection");
            co_await Send(third,MessageId::MW_ADDCHAR_ACK,Add(4003));
            co_await Expect(main,MessageId::MW_CHARDATA_REQ,Identity());
            Check(ch->cons[2].valid && !ch->cons[2].ready && ch->main_server_id==1,"all connected peers trigger data request without transferring primary ownership");
            co_await Send(third,MessageId::MW_ADDCHAR_ACK,Add(4003));
            co_await Expect(third,MessageId::MW_INVALIDCHAR_REQ,invalid);
            Check(main->frames.empty() && chars.ActiveUserCount()==1,"duplicate completion cannot resynchronize or lose active user");
            auto malformed=Identity();malformed.push_back(std::byte{0});
            co_await Send(main,MessageId::MW_CLOSECHAR_ACK,malformed);
            // Ordered follow-up on the same peer proves the malformed close was
            // parsed before duplicate rejection without closing the character.
            co_await Send(main,MessageId::MW_ADDCHAR_ACK,Add(4001));
            co_await Expect(main,MessageId::MW_INVALIDCHAR_REQ,invalid);
            Check(chars.Find(42)==ch,"trailing CLOSECHAR bytes cannot retire a valid character");
            co_await Send(third,MessageId::MW_CLOSECHAR_ACK,Identity());
            co_await Until([&]{return !chars.Find(42);});
            Check(chars.ActiveUserCount()==0,"accepted Map still completes legitimate close-all");
            for(auto& c:clients)c->wire->Close();
            done=true;
        },report);
        io.run();if(failure)std::rethrow_exception(failure);Check(done,"secondary flow completes");
        std::printf("%d secondary World admission checks passed\n",checks);return 0;
    }catch(const std::exception& e){std::fprintf(stderr,"Secondary admission regression: %s\n",e.what());return 1;}
}
