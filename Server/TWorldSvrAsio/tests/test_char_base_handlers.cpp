// W3a-3 wire test: drives MW_CHANGECHARBASE_ACK over a loopback
// session and asserts the per-bType mutation lands on TChar (and
// the name index where applicable). Mirrors the W2/W3a-1 wire
// test pattern: real WorldServer + TCP, no fakes.
//
// Scenarios:
//   1. IK_FACE / IK_HAIR / IK_RACE / IK_SEX update the simple
//      byte fields.
//   2. IK_COUNTRY / IK_AIDCOUNTRY update the country bytes.
//   3. IK_NAME renames the char + populates the secondary index.
//   4. IK_NAME refused on collision (second char already owns it).
//   5. Unknown bType is logged and dropped without side-effect.

#include "../handlers/handlers.h"
#include "../services/char_registry.h"
#include "../services/guild_registry.h"
#include "../services/peer_registry.h"
#include "../wire_codec.h"
#include "../world_server.h"
#include "../world_session.h"

#include "MessageId.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/read.hpp>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace {

int g_fails = 0;
#define EXPECT(cond) do { \
    if (!(cond)) { \
        std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); \
        ++g_fails; \
    } \
} while (0)

std::vector<std::byte> BuildAddCharBody(std::uint32_t char_id,
                                         std::uint32_t key)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD(b, char_id);
    tworldsvr::wire::WritePOD(b, key);
    tworldsvr::wire::WritePOD<std::uint32_t>(b, 0x7f000001);
    tworldsvr::wire::WritePOD<std::uint16_t>(b, 33500);
    tworldsvr::wire::WritePOD<std::uint32_t>(b, /*user_id=*/100);
    return b;
}

std::vector<std::byte> BuildBaseBody(std::uint32_t char_id,
                                      std::uint32_t key,
                                      std::uint8_t  type,
                                      std::uint8_t  value,
                                      const std::string& name = "")
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD(b, char_id);
    tworldsvr::wire::WritePOD(b, key);
    tworldsvr::wire::WritePOD(b, type);
    tworldsvr::wire::WritePOD(b, value);
    tworldsvr::wire::WritePOD<std::uint16_t>(b, /*title*/ 0);
    tworldsvr::wire::WriteString(b, name);
    return b;
}

void SendFramed(boost::asio::ip::tcp::socket& sock,
                std::uint16_t wId,
                const std::vector<std::byte>& body)
{
    tworldsvr::PacketHeader hdr{};
    hdr.wSize    = static_cast<std::uint16_t>(
        tworldsvr::kPacketHeaderSize + body.size());
    hdr.wID      = wId;
    // Fresh source Plain16 metadata is zero (value-initialized header).
    std::vector<std::byte> buf(hdr.wSize);
    std::memcpy(buf.data(), &hdr, sizeof(hdr));
    if (!body.empty())
        std::memcpy(buf.data() + sizeof(hdr), body.data(), body.size());
    boost::asio::write(sock, boost::asio::buffer(buf.data(), buf.size()));
}

std::pair<std::uint16_t,std::vector<std::byte>> ReadTitleFrame(boost::asio::ip::tcp::socket& sock) {
    const auto read=[&](void* target,std::size_t size){
        auto* bytes=static_cast<std::byte*>(target);std::size_t done=0;
        const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(3);
        sock.non_blocking(true);
        while(done<size){boost::system::error_code ec;auto n=sock.read_some(boost::asio::buffer(bytes+done,size-done),ec);
            if(ec&&ec!=boost::asio::error::would_block&&ec!=boost::asio::error::try_again)throw boost::system::system_error(ec);
            done+=n;if(done<size){if(std::chrono::steady_clock::now()>until)throw std::runtime_error("title relay timeout");std::this_thread::sleep_for(std::chrono::milliseconds(1));}}
        sock.non_blocking(false);
    };
    tworldsvr::PacketHeader h{};read(&h,sizeof(h));
    if(h.wSize<sizeof(h))throw std::runtime_error("bad title relay frame");
    std::vector<std::byte> b(h.wSize-sizeof(h));read(b.data(),b.size());
    EXPECT(h.dwNumber==0 && h.llChecksum==0);return {h.wID,b};
}

} // namespace

int main()
{
    using boost::asio::ip::tcp;
    using namespace std::chrono_literals;
    using tnetlib::protocol::MessageId;
    using tnetlib::protocol::ToUint16;

    boost::asio::io_context io;

    tworldsvr::CharRegistry  chars;
    tworldsvr::GuildRegistry guilds;
    tworldsvr::PeerRegistry  peers;
    tworldsvr::HandlerContext ctx{};
    ctx.io     = &io;
    ctx.chars  = &chars;
    ctx.guilds = &guilds;
    ctx.peers  = &peers;
    ctx.nation = 0;

    tworldsvr::WorldServerConfig svr_cfg{};
    svr_cfg.port            = 0;
    svr_cfg.max_connections = 4;
    svr_cfg.ctx             = ctx;
    tworldsvr::WorldServer server(io, svr_cfg);
    const std::uint16_t port = server.Port();
    boost::asio::co_spawn(io, server.Run(), boost::asio::detached);
    std::thread io_thread([&io] { io.run(); });
    std::this_thread::sleep_for(20ms);

    boost::asio::io_context client_io;
    tcp::socket sock(client_io);
    sock.connect(tcp::endpoint(
        boost::asio::ip::make_address_v4("127.0.0.1"), port));
    std::this_thread::sleep_for(20ms);

    // Pre: register char 42 + char 50.
    SendFramed(sock, ToUint16(MessageId::MW_ADDCHAR_ACK),
        BuildAddCharBody(42, 0xCAFEBABE));
    SendFramed(sock, ToUint16(MessageId::MW_ADDCHAR_ACK),
        BuildAddCharBody(50, 0xDEADBEEF));
    std::this_thread::sleep_for(40ms);
    EXPECT(chars.Find(42) != nullptr);
    EXPECT(chars.Find(50) != nullptr);

    constexpr std::uint8_t kIkFace = 45, kIkHair = 46, kIkRace = 47,
                           kIkName = 48, kIkSex = 49,
                           kIkCountry = 96, kIkAidCountry = 97;

    // --- 1: simple byte fields ---------------------------------------
    SendFramed(sock, ToUint16(MessageId::MW_CHANGECHARBASE_ACK),
        BuildBaseBody(42, 0xCAFEBABE, kIkFace, 3));
    SendFramed(sock, ToUint16(MessageId::MW_CHANGECHARBASE_ACK),
        BuildBaseBody(42, 0xCAFEBABE, kIkHair, 5));
    SendFramed(sock, ToUint16(MessageId::MW_CHANGECHARBASE_ACK),
        BuildBaseBody(42, 0xCAFEBABE, kIkRace, 2));
    SendFramed(sock, ToUint16(MessageId::MW_CHANGECHARBASE_ACK),
        BuildBaseBody(42, 0xCAFEBABE, kIkSex, 1));
    std::this_thread::sleep_for(80ms);
    {
        auto c = chars.Find(42);
        EXPECT(c != nullptr);
        if (c) {
            std::lock_guard g(c->lock);
            EXPECT(c->face == 3);
            EXPECT(c->hair == 5);
            EXPECT(c->race == 2);
            EXPECT(c->sex == 1);
        }
    }

    // --- 2: country fields -------------------------------------------
    SendFramed(sock, ToUint16(MessageId::MW_CHANGECHARBASE_ACK),
        BuildBaseBody(42, 0xCAFEBABE, kIkCountry, 1));
    SendFramed(sock, ToUint16(MessageId::MW_CHANGECHARBASE_ACK),
        BuildBaseBody(42, 0xCAFEBABE, kIkAidCountry, 2));
    std::this_thread::sleep_for(40ms);
    {
        auto c = chars.Find(42);
        EXPECT(c != nullptr);
        if (c) {
            std::lock_guard g(c->lock);
            EXPECT(c->country == 1);
            EXPECT(c->aid_country == 2);
        }
    }

    // --- 3: IK_NAME renames + indexes --------------------------------
    SendFramed(sock, ToUint16(MessageId::MW_CHANGECHARBASE_ACK),
        BuildBaseBody(42, 0xCAFEBABE, kIkName, 0, "Alice"));
    std::this_thread::sleep_for(40ms);
    EXPECT(chars.FindByName("Alice") != nullptr);
    EXPECT(chars.FindByName("ALICE") != nullptr);  // case-insensitive

    // --- 4: collision refused ----------------------------------------
    SendFramed(sock, ToUint16(MessageId::MW_CHANGECHARBASE_ACK),
        BuildBaseBody(50, 0xDEADBEEF, kIkName, 0, "Alice"));
    std::this_thread::sleep_for(40ms);
    // Char 50's name index entry was not created (collision); the
    // first char retains the name.
    auto alice = chars.FindByName("Alice");
    EXPECT(alice && alice->char_id == 42);

    // --- 5: unknown bType silently drops -----------------------------
    SendFramed(sock, ToUint16(MessageId::MW_CHANGECHARBASE_ACK),
        BuildBaseBody(42, 0xCAFEBABE, /*bType=*/ 200, /*value*/ 9));
    std::this_thread::sleep_for(40ms);
    // Session is still alive (framer didn't tear down); send a
    // valid follow-up to confirm.
    SendFramed(sock, ToUint16(MessageId::MW_CHANGECHARBASE_ACK),
        BuildBaseBody(42, 0xCAFEBABE, kIkFace, 9));
    std::this_thread::sleep_for(40ms);
    {
        auto c = chars.Find(42);
        if (c) {
            std::lock_guard g(c->lock);
            EXPECT(c->face == 9);
        }
    }

    // Title source is the registered, ready main Map; original all-Map fan-out.
    tcp::socket main_map(client_io),replica_map(client_io),anonymous(client_io);
    const auto endpoint=tcp::endpoint(boost::asio::ip::make_address_v4("127.0.0.1"),port);
    main_map.connect(endpoint);replica_map.connect(endpoint);anonymous.connect(endpoint);
    for(auto [socket,id]:std::vector<std::pair<tcp::socket*,std::uint16_t>>{{&main_map,0x0401},{&replica_map,0x0402}}){
        std::vector<std::byte> register_body;tworldsvr::wire::WritePOD(register_body,id);
        SendFramed(*socket,ToUint16(MessageId::RW_RELAYSVR_REQ),register_body);
        EXPECT(ReadTitleFrame(*socket).first==ToUint16(MessageId::RW_RELAYSVR_ACK));
    }
    EXPECT(ReadTitleFrame(main_map).first==ToUint16(MessageId::MW_RELAYCONNECT_REQ));
    {auto c=chars.Find(42);std::lock_guard g(c->lock);c->main_server_id=1;c->cons={{1,0,0,true,true},{2,0,0,true,true}};}
    auto title=BuildBaseBody(42,0xCAFEBABE,103,0,"A");title[10]=std::byte{87};
    auto wrong=title;wrong[4]^=std::byte{1};SendFramed(main_map,ToUint16(MessageId::MW_CHANGECHARBASE_ACK),wrong);
    wrong=title;wrong.push_back(std::byte{});SendFramed(main_map,ToUint16(MessageId::MW_CHANGECHARBASE_ACK),wrong);
    SendFramed(replica_map,ToUint16(MessageId::MW_CHANGECHARBASE_ACK),title);
    SendFramed(anonymous,ToUint16(MessageId::MW_CHANGECHARBASE_ACK),title);
    std::this_thread::sleep_for(40ms);
    {auto c=chars.Find(42);std::lock_guard g(c->lock);EXPECT(c->title==0);}
    EXPECT(main_map.available()==0&&replica_map.available()==0);
    SendFramed(main_map,ToUint16(MessageId::MW_CHANGECHARBASE_ACK),title);
    const auto one=ReadTitleFrame(main_map),two=ReadTitleFrame(replica_map);
    EXPECT(one.first==ToUint16(MessageId::MW_CHANGECHARBASE_REQ)&&one.second==title);
    EXPECT(two==one);
    {auto c=chars.Find(42);std::lock_guard g(c->lock);EXPECT(c->title==87);}
    main_map.close();replica_map.close();anonymous.close();

    boost::system::error_code ec;
    sock.shutdown(tcp::socket::shutdown_both, ec);
    sock.close(ec);

    std::this_thread::sleep_for(40ms);
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_char_base_handlers (6 scenarios)\n");
    else
        std::printf("FAIL test_tworldsvr_asio_char_base_handlers (%d failure%s)\n",
            g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
