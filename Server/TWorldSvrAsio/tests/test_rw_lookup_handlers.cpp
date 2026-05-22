// W3a-3 wire test: OnRW_ENTERCHAR_REQ + OnRW_RELAYCONNECT_REQ
// drive the relay-side lookup path.
//
// Scenarios:
//   1. RELAYSVR + ADDCHAR + CHANGECHARBASE (name) → ENTERCHAR_REQ
//      with the right (id, name) → RW_ENTERCHAR_ACK bResult=1.
//   2. ENTERCHAR_REQ with a wrong name → bResult=0 (miss path).
//   3. RELAYCONNECT_REQ with a known char_id routes
//      MW_RELAYCONNECT_REQ to the peer matching main_server_id
//      (which equals LOBYTE(wID) per W3a-2 convention). Drive
//      two peer connections so the routing is non-trivial.
//   4. RELAYSVR_REQ broadcast — a second relay registering causes
//      a MW_RELAYCONNECT_REQ(char_id=0, relay_on=0) to land on
//      the first peer.

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
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>

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

void SendFramed(boost::asio::ip::tcp::socket& sock,
                std::uint16_t wId,
                const std::vector<std::byte>& body)
{
    tworldsvr::PacketHeader hdr{};
    hdr.wSize    = static_cast<std::uint16_t>(
        tworldsvr::kPacketHeaderSize + body.size());
    hdr.wID      = wId;
    hdr.dwChkSum = tworldsvr::ComputeChecksum(body.data(), body.size());
    std::vector<std::byte> buf(hdr.wSize);
    std::memcpy(buf.data(), &hdr, sizeof(hdr));
    if (!body.empty())
        std::memcpy(buf.data() + sizeof(hdr), body.data(), body.size());
    boost::asio::write(sock, boost::asio::buffer(buf.data(), buf.size()));
}

std::pair<std::uint16_t, std::vector<std::byte>>
ReadFramed(boost::asio::ip::tcp::socket& sock)
{
    tworldsvr::PacketHeader hdr{};
    boost::asio::read(sock, boost::asio::buffer(&hdr, sizeof(hdr)));
    const std::size_t body_size = hdr.wSize - tworldsvr::kPacketHeaderSize;
    std::vector<std::byte> body(body_size);
    if (body_size > 0)
        boost::asio::read(sock, boost::asio::buffer(body.data(), body_size));
    return {hdr.wID, std::move(body)};
}

// Drain any in-flight ACKs that prior steps wrote to the socket
// so the next ReadFramed lands on the response we care about.
void DrainPending(boost::asio::ip::tcp::socket& sock, int max_packets = 8)
{
    boost::system::error_code ec;
    sock.non_blocking(true);
    for (int i = 0; i < max_packets; ++i)
    {
        tworldsvr::PacketHeader hdr{};
        const auto n = boost::asio::read(sock,
            boost::asio::buffer(&hdr, sizeof(hdr)),
            boost::asio::transfer_exactly(sizeof(hdr)), ec);
        if (ec || n != sizeof(hdr)) break;
        const std::size_t body_size = hdr.wSize - tworldsvr::kPacketHeaderSize;
        if (body_size > 0)
        {
            std::vector<char> tmp(body_size);
            boost::asio::read(sock, boost::asio::buffer(tmp.data(), body_size),
                ec);
            if (ec) break;
        }
    }
    sock.non_blocking(false);
}

std::vector<std::byte> RelaysvrBody(std::uint16_t wid)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD<std::uint16_t>(b, wid);
    return b;
}

std::vector<std::byte> AddCharBody(std::uint32_t char_id,
                                    std::uint32_t key)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD(b, char_id);
    tworldsvr::wire::WritePOD(b, key);
    tworldsvr::wire::WritePOD<std::uint32_t>(b, 0x7f000001);
    tworldsvr::wire::WritePOD<std::uint16_t>(b, 33500);
    tworldsvr::wire::WritePOD<std::uint32_t>(b, 100);
    return b;
}

std::vector<std::byte> NameBody(std::uint32_t char_id,
                                 std::uint32_t key,
                                 const std::string& name)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD(b, char_id);
    tworldsvr::wire::WritePOD(b, key);
    tworldsvr::wire::WritePOD<std::uint8_t>(b, /*bType=IK_NAME*/ 48);
    tworldsvr::wire::WritePOD<std::uint8_t>(b, 0);
    tworldsvr::wire::WritePOD<std::uint16_t>(b, 0);
    tworldsvr::wire::WriteString(b, name);
    return b;
}

std::vector<std::byte> EntercharBody(std::uint32_t char_id,
                                      const std::string& name)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD(b, char_id);
    tworldsvr::wire::WriteString(b, name);
    return b;
}

std::vector<std::byte> RelayConnectBody(std::uint32_t char_id)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD(b, char_id);
    return b;
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
    ctx.nation = 3;

    tworldsvr::WorldServerConfig svr_cfg{};
    svr_cfg.port            = 0;
    svr_cfg.max_connections = 8;
    svr_cfg.ctx             = ctx;
    tworldsvr::WorldServer server(io, svr_cfg);
    const std::uint16_t port = server.Port();
    boost::asio::co_spawn(io, server.Run(), boost::asio::detached);
    std::thread io_thread([&io] { io.run(); });
    std::this_thread::sleep_for(20ms);

    // ---- Peer #1 (acts as the "main map" for chars) -----------------
    boost::asio::io_context client_io1;
    tcp::socket peer1(client_io1);
    peer1.connect(tcp::endpoint(
        boost::asio::ip::make_address_v4("127.0.0.1"), port));
    std::this_thread::sleep_for(20ms);

    // Register with wID=0x0011 (main_server_id will be LOBYTE = 0x11)
    SendFramed(peer1, ToUint16(MessageId::RW_RELAYSVR_REQ), RelaysvrBody(0x0011));
    // Drain the RELAYSVR_ACK.
    {
        auto [wid, _] = ReadFramed(peer1);
        EXPECT(wid == ToUint16(MessageId::RW_RELAYSVR_ACK));
    }
    EXPECT(peers.Size() == 1);

    // Register char 42 on peer1, then name it "Alice". Poll the
    // registry until both the char and the name index reflect the
    // change — the handler dispatch is async and inserting a fixed
    // sleep was flaky on slower CI hosts.
    SendFramed(peer1, ToUint16(MessageId::MW_ADDCHAR_ACK),
        AddCharBody(42, 0xCAFEBABE));
    SendFramed(peer1, ToUint16(MessageId::MW_CHANGECHARBASE_ACK),
        NameBody(42, 0xCAFEBABE, "Alice"));
    for (int i = 0; i < 50; ++i)
    {
        if (chars.Find(42) && chars.FindByName("Alice")) break;
        std::this_thread::sleep_for(10ms);
    }
    EXPECT(chars.Find(42) != nullptr);
    EXPECT(chars.FindByName("Alice") != nullptr);

    // ---- Scenario 1: ENTERCHAR_REQ hit ------------------------------
    SendFramed(peer1, ToUint16(MessageId::RW_ENTERCHAR_REQ),
        EntercharBody(42, "Alice"));
    {
        auto [wid, body] = ReadFramed(peer1);
        EXPECT(wid == ToUint16(MessageId::RW_ENTERCHAR_ACK));
        tworldsvr::wire::Reader r(body);
        std::uint32_t reply_char_id = 0; std::string reply_name;
        std::uint8_t result = 0, country = 0;
        EXPECT(r.Read(reply_char_id));
        EXPECT(r.ReadString(reply_name));
        EXPECT(r.Read(result));
        EXPECT(r.Read(country));
        EXPECT(reply_char_id == 42);
        EXPECT(reply_name == "Alice");
        EXPECT(result == 1);
    }

    // ---- Scenario 2: ENTERCHAR_REQ miss -----------------------------
    SendFramed(peer1, ToUint16(MessageId::RW_ENTERCHAR_REQ),
        EntercharBody(42, "Bogus"));
    {
        auto [wid, body] = ReadFramed(peer1);
        EXPECT(wid == ToUint16(MessageId::RW_ENTERCHAR_ACK));
        tworldsvr::wire::Reader r(body);
        std::uint32_t reply_char_id = 0; std::string reply_name;
        std::uint8_t result = 0;
        EXPECT(r.Read(reply_char_id));
        EXPECT(r.ReadString(reply_name));
        EXPECT(r.Read(result));
        EXPECT(result == 0);
    }

    // ---- Scenario 3: RELAYCONNECT routes to main_server_id ----------
    //
    // Peer1 sends RELAYCONNECT_REQ for char_id=42; char_42 has
    // main_server_id=0x11 (set at ADDCHAR time from LOBYTE of
    // peer1's wID). The handler should route MW_RELAYCONNECT_REQ
    // back to peer1 (the only peer matching that byte).
    SendFramed(peer1, ToUint16(MessageId::RW_RELAYCONNECT_REQ),
        RelayConnectBody(42));
    {
        auto [wid, body] = ReadFramed(peer1);
        EXPECT(wid == ToUint16(MessageId::MW_RELAYCONNECT_REQ));
        tworldsvr::wire::Reader r(body);
        std::uint32_t cid = 0; std::uint8_t on = 0;
        EXPECT(r.Read(cid));
        EXPECT(r.Read(on));
        EXPECT(cid == 42);
        EXPECT(on == 1);
    }

    // ---- Scenario 4: second relay triggers broadcast ----------------
    //
    // Peer2 connects and registers wID=0x0022. OnRelaysvrReq's
    // fan-out should send MW_RELAYCONNECT_REQ(0, 0) to peer1.
    DrainPending(peer1);
    boost::asio::io_context client_io2;
    tcp::socket peer2(client_io2);
    peer2.connect(tcp::endpoint(
        boost::asio::ip::make_address_v4("127.0.0.1"), port));
    std::this_thread::sleep_for(20ms);

    SendFramed(peer2, ToUint16(MessageId::RW_RELAYSVR_REQ), RelaysvrBody(0x0022));
    // Drain peer2's own RELAYSVR_ACK.
    {
        auto [wid, _] = ReadFramed(peer2);
        EXPECT(wid == ToUint16(MessageId::RW_RELAYSVR_ACK));
    }
    std::this_thread::sleep_for(40ms);

    // Peer1 should have received the broadcast notification.
    {
        auto [wid, body] = ReadFramed(peer1);
        EXPECT(wid == ToUint16(MessageId::MW_RELAYCONNECT_REQ));
        tworldsvr::wire::Reader r(body);
        std::uint32_t cid = 0; std::uint8_t on = 0;
        EXPECT(r.Read(cid));
        EXPECT(r.Read(on));
        EXPECT(cid == 0);
        EXPECT(on == 0);
    }
    EXPECT(peers.Size() == 2);

    boost::system::error_code ec;
    peer1.shutdown(tcp::socket::shutdown_both, ec);
    peer1.close(ec);
    peer2.shutdown(tcp::socket::shutdown_both, ec);
    peer2.close(ec);

    std::this_thread::sleep_for(60ms);
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_rw_lookup_handlers (4 scenarios)\n");
    else
        std::printf("FAIL test_tworldsvr_asio_rw_lookup_handlers (%d failure%s)\n",
            g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
