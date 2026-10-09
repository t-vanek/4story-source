// Synthetic World endpoint: registration gate, malformed/missing ACK, bounded
// reconnect ordering and concurrent composed writes over real TCP. No database.
#include "services/world_client.h"
#include "world_session.h"
#include "wire_codec.h"
#include "MessageId.h"
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <cstdio>
#include <stdexcept>

namespace asio = boost::asio;
using namespace std::chrono_literals;
using tnetlib::protocol::MessageId;
using Bytes = std::vector<std::byte>;
static unsigned checks = 0;
static void Check(bool ok, const char* label) {
    if (!ok) throw std::runtime_error(label);
    ++checks;
}
static asio::awaitable<void> Pause(std::chrono::milliseconds delay) {
    asio::steady_timer timer(co_await asio::this_coro::executor);
    timer.expires_after(delay); co_await timer.async_wait(asio::use_awaitable);
}
template<class Predicate> static asio::awaitable<void> Until(Predicate ready) {
    for (int n=0; !ready(); ++n) {
        if (n==500) throw std::runtime_error("condition deadline");
        co_await Pause(2ms);
    }
}
static Bytes Ack() {
    Bytes b;
    tworldsvr::wire::WritePOD<std::uint8_t>(b, 0);
    tworldsvr::wire::WritePOD<std::uint16_t>(b, 1);
    tworldsvr::wire::WritePOD<std::uint32_t>(b, 45);
    tworldsvr::wire::WritePOD<std::uint16_t>(b, 1);
    tworldsvr::wire::WriteString(b, "notice");
    tworldsvr::wire::WriteString(b, "synthetic");
    return b;
}
static void Run(int scenario) {
    asio::io_context io;
    asio::ip::tcp::acceptor listener(io, {asio::ip::tcp::v4(),0});
    int accepts=0, losses=0, received=0, completed=0;
    bool drain_done=false, finished=false;
    std::exception_ptr failure;
    auto report=[&](std::exception_ptr e) { if(e) { failure=e; io.stop(); } };
    tmapsvr::AsioWorldClient client(io,"127.0.0.1",listener.local_endpoint().port(),
        nullptr,10ms,20ms,100ms);
    client.SetRelayWid(0x0401);
    client.SetDisconnectHandler([&]() -> asio::awaitable<bool> {
        ++losses;
        Check(!client.IsConnected() && !client.IsRegistered(),"drain starts with closed transport and admission");
        Check(!co_await client.SendPacket(0x1234,{}),"disconnected traffic is not buffered");
        co_await Pause(60ms);
        Check(accepts==losses,"replacement cannot connect while teardown is suspended");
        drain_done=true;
        co_return scenario==4 && losses==1;
    });
    asio::co_spawn(io,client.Run(),[&](std::exception_ptr e) {
        report(e); finished=true;
    });
    asio::co_spawn(io,[&]() -> asio::awaitable<void> {
        for (int cycle=0; cycle<(scenario==4?2:1); ++cycle) {
            auto sock=co_await listener.async_accept(asio::use_awaitable);
            ++accepts;
            if(cycle) Check(drain_done,"old teardown completed before reconnect");
            auto peer=std::make_shared<tworldsvr::WorldSession>(std::move(sock));
            asio::co_spawn(io,peer->Run([&,peer](auto, tworldsvr::DecodedPacket packet) -> asio::awaitable<void> {
                if(packet.wId==static_cast<std::uint16_t>(MessageId::RW_RELAYSVR_REQ)) {
                    Check(packet.body==Bytes{std::byte{1},std::byte{4}},"exact Map identity on every registration");
                    Check(client.IsConnected() && !client.IsRegistered(),"TCP connect alone cannot admit players");
                    if(scenario==0) co_return; // Missing ACK closes at deadline.
                    auto body=Ack();
                    if(scenario==1) body.resize(2);
                    if(scenario==3) body.push_back(std::byte{0});
                    const auto opcode=scenario==2?0x1234:static_cast<std::uint16_t>(MessageId::RW_RELAYSVR_ACK);
                    co_await peer->SendPacket(opcode,std::move(body));
                } else {
                    Check(packet.wId==0x1234 && packet.body==Bytes(60000,std::byte{0x5a}),"concurrent large frames remain intact");
                    ++received;
                }
            }),[peer,report](std::exception_ptr e) { peer->Close(); report(e); });
            if(scenario!=4) {
                co_await Until([&]{return !peer->IsOpen();});
            } else {
                co_await Until([&]{return client.IsRegistered();});
                Check(client.IsRegistered(),"complete acknowledgment opens admission");
                if(cycle==0) {
                    for(int n=0;n<12;++n) asio::co_spawn(io,[&]() -> asio::awaitable<void> {
                        Check(co_await client.SendPacket(0x1234,Bytes(60000,std::byte{0x5a})),"complete frame send succeeds");
                        ++completed;
                    },report);
                    co_await Until([&]{return received==12 && completed==12;});
                    // Past the registration deadline: cancellation must preserve
                    // this acknowledged connection, including non-empty lists.
                    co_await Pause(130ms);
                    Check(client.IsRegistered(),"registration deadline is cancelled after valid ACK");
                }
                peer->Close();
            }
        }
    },report);
    asio::co_spawn(io,[&]() -> asio::awaitable<void> {
        co_await Until([&]{return finished;});
        Check(losses==(scenario==4?2:1),"one teardown callback per lost connection");
        io.stop();
    },report);
    io.run();
    if(failure) std::rethrow_exception(failure);
    Check(finished,"reconnect loop stops on teardown shutdown verdict");
}
int main() {
    try {
        for(int n=0;n<5;++n) Run(n);
        std::printf("%u World link checks passed\n",checks); return 0;
    } catch(const std::exception& e) {
        std::fprintf(stderr,"World link check failed: %s\n",e.what()); return 1;
    }
}
