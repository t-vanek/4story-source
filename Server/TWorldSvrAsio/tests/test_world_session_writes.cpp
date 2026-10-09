// Actual WorldSession, independent 8-byte SS decoder, deliberately stalled TCP
// receiver. Exercises composed writes and teardown without database/test doubles.
#include "../world_session.h"
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <array>
#include <cstdio>
#include <set>
#include <stdexcept>

namespace asio=boost::asio;
using tcp=asio::ip::tcp;
using Bytes=std::vector<std::byte>;
using namespace std::chrono_literals;
static int checks=0;
static void Check(bool value,const char* label) {
    if(!value) throw std::runtime_error(label);
    ++checks;
}
static asio::awaitable<void> Pause(std::chrono::milliseconds delay) {
    asio::steady_timer timer(co_await asio::this_coro::executor);
    timer.expires_after(delay);co_await timer.async_wait(asio::use_awaitable);
}
template<class F> static asio::awaitable<void> Until(F predicate) {
    for(int n=0;!predicate();++n) {
        if(n==500)throw std::runtime_error("pending sends did not finish");
        co_await Pause(2ms);
    }
}
static std::uint32_t LE(const std::byte* b,unsigned n) {
    std::uint32_t v=0;for(unsigned i=0;i<n;++i)v|=std::to_integer<std::uint32_t>(b[i])<<(8*i);return v;
}
static asio::awaitable<std::pair<std::uint16_t,Bytes>> Read(tcp::socket& s) {
    std::array<std::byte,8> header;
    co_await asio::async_read(s,asio::buffer(header),asio::use_awaitable);
    const auto size=LE(header.data(),2);
    Check(size>=8,"frame length is valid");
    Bytes body(size-8);
    co_await asio::async_read(s,asio::buffer(body),asio::use_awaitable);
    std::uint32_t checksum=0;std::size_t at=0;
    for(;at+4<=body.size();at+=4)checksum^=LE(body.data()+at,4);
    for(;at<body.size();++at)checksum^=std::to_integer<std::uint8_t>(body[at]);
    Check(checksum==LE(header.data()+4,4),"independent checksum matches complete body");
    co_return std::pair{static_cast<std::uint16_t>(LE(header.data()+2,2)),std::move(body)};
}
static Bytes Body(unsigned n) {
    const std::size_t sizes[]={65527,0,1,317,60000,8193};
    Bytes b(sizes[n%6]);
    for(std::size_t i=0;i<b.size();++i)b[i]=std::byte((i*17+n*31)&255);
    return b;
}
static void Run(int scenario) {
    asio::io_context io;
    tcp::acceptor accept(io,{tcp::v4(),0});
    tcp::socket client(io);client.connect({asio::ip::address_v4::loopback(),accept.local_endpoint().port()});
    auto socket=accept.accept();
    socket.set_option(asio::socket_base::send_buffer_size(1024));
    auto session=std::make_shared<tworldsvr::WorldSession>(std::move(socket));
    std::exception_ptr failure;bool done=false;int completed=0;
    auto report=[&](std::exception_ptr e) { if(e) { failure=e;io.stop(); } };
    asio::steady_timer watchdog(io);watchdog.expires_after(8s);
    watchdog.async_wait([&](auto ec) { if(!ec) {failure=std::make_exception_ptr(std::runtime_error("wire test deadline"));io.stop();} });
    asio::co_spawn(io,[&]() -> asio::awaitable<void> {
        if(scenario==0) {
            for(unsigned n=0;n<32;++n) asio::co_spawn(io,[&,n]() -> asio::awaitable<void> {
                co_await session->SendPacket(static_cast<std::uint16_t>(100+n),Body(n));++completed;
            },report);
            co_await Pause(60ms);
            Check(completed<32,"slow receiver forces writes to overlap in time");
            std::set<std::uint16_t> ids;
            for(unsigned n=0;n<32;++n) {
                auto [id,body]=co_await Read(client);
                Check(id>=100 && id<132 && ids.insert(id).second,"every concurrent frame arrives exactly once");
                Check(body==Body(id-100),"per-frame payload remains unchanged across suspension");
            }
            co_await Until([&]{return completed==32;});
            co_await session->SendPacket(200,Bytes(65528,std::byte{1})); // oversize: no frame
            co_await session->SendPacket(201,{});
            auto [id,body]=co_await Read(client);
            Check(id==201 && body.empty() && session->IsOpen(),"oversize rejection preserves the following empty frame");
        } else if(scenario==1 || scenario==2) {
            for(unsigned n=0;n<24;++n) asio::co_spawn(io,[&,n]() -> asio::awaitable<void> {
                co_await session->SendPacket(static_cast<std::uint16_t>(100+n),Bytes(65527,std::byte(n)));++completed;
            },report);
            co_await Pause(60ms);Check(completed<24,"teardown test has unfinished writes");
            if(scenario==1)session->Close();
            else {client.set_option(asio::socket_base::linger(true,0));client.close();}
            co_await Until([&]{return completed==24;});
            Check(!session->IsOpen(),"close or peer reset retires the writer and releases waiters");
            co_await session->SendPacket(250,Bytes{std::byte{1}});
            Check(!session->IsOpen(),"late sends cannot reopen a retired session");
        } else if(scenario==3 || scenario==4) {
            const int count=scenario==3?300:80;
            for(int n=0;n<count;++n) asio::co_spawn(io,[&,n]() -> asio::awaitable<void> {
                co_await session->SendPacket(100,Bytes(scenario==3?0:65527,std::byte(n)));++completed;
            },report);
            co_await Until([&]{return completed==count;});
            Check(!session->IsOpen(),"stalled peer exceeding message or byte budget is closed");
        } else {
            client.close();
            co_await session->Run([](auto,auto) -> asio::awaitable<void> {co_return;});
            Check(!session->IsOpen(),"read-loop EOF closes transport without a server wrapper");
        }
        session->Close();done=true;watchdog.cancel();
    },report);
    io.run();
    if(failure)std::rethrow_exception(failure);
    Check(done,"wire scenario completes");
}
int main(int argc,char**) {
    try {
        for(int n=0;n<(argc>1?1:6);++n)Run(n);
        std::printf("%d World write checks passed\n",checks);return 0;
    }catch(const std::exception& e){std::fprintf(stderr,"World write regression: %s\n",e.what());return 1;}
}
