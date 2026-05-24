// W6-21 wire test: teleport confirm (MW_TELEPORT_ACK).
//
// Three map peers (svr 0x42 main, 0x43, 0x44). Three branches:
//
//   * Happy path — char A's TELEPORT_ACK names a known dest (0x43)
//     → TELEPORT_REQ(TPR_SUCCESS) back to the responder + CONLIST_REQ
//     to the destination. Verifies the response carries the char's
//     current channel/map/pos and that party_waiter is cleared.
//   * NODESTINATION — char B's TELEPORT_ACK names an unknown dest
//     (0x99) → TELEPORT_REQ(TPR_NODESTINATION) back to the responder,
//     then CloseChar fans DELCHAR to the char's cons and drops it
//     from the registry.
//   * Unknown char → DELCHAR(logout=1,save=0) on the reporter.

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
#include <mutex>
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

void SendFramed(boost::asio::ip::tcp::socket& sock, std::uint16_t wId,
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

std::vector<std::byte> RelaysvrBody(std::uint16_t wid)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD<std::uint16_t>(b, wid);
    return b;
}

std::vector<std::byte> AddCharBody(std::uint32_t char_id, std::uint32_t key)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD(b, char_id);
    tworldsvr::wire::WritePOD(b, key);
    tworldsvr::wire::WritePOD<std::uint32_t>(b, 0x7f000001);
    tworldsvr::wire::WritePOD<std::uint16_t>(b, 33500);
    tworldsvr::wire::WritePOD<std::uint32_t>(b, /*user_id=*/100);
    return b;
}

// MW_TELEPORT_ACK: DWORD char_id, key, BYTE dest_server_id.
std::vector<std::byte> TeleportBody(std::uint32_t char_id, std::uint32_t key,
                                    std::uint8_t dest_server_id)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD(b, char_id);
    tworldsvr::wire::WritePOD(b, key);
    tworldsvr::wire::WritePOD(b, dest_server_id);
    return b;
}

} // namespace

int main()
{
    using boost::asio::ip::tcp;
    using namespace std::chrono_literals;
    using tnetlib::protocol::MessageId;
    using tnetlib::protocol::ToUint16;
    namespace wire = tworldsvr::wire;

    boost::asio::io_context io;
    tworldsvr::CharRegistry   chars;
    tworldsvr::GuildRegistry  guilds;
    tworldsvr::PeerRegistry   peers;
    tworldsvr::HandlerContext ctx{};
    ctx.io = &io; ctx.chars = &chars; ctx.guilds = &guilds;
    ctx.peers = &peers; ctx.nation = 0;

    tworldsvr::WorldServerConfig svr_cfg{};
    svr_cfg.port = 0; svr_cfg.max_connections = 8; svr_cfg.ctx = ctx;
    tworldsvr::WorldServer server(io, svr_cfg);
    const std::uint16_t port = server.Port();
    boost::asio::co_spawn(io, server.Run(), boost::asio::detached);
    std::thread io_thread([&io] { io.run(); });
    std::this_thread::sleep_for(20ms);

    boost::asio::io_context client_io;
    tcp::socket p1(client_io), p2(client_io), p3(client_io);
    const auto ep = tcp::endpoint(
        boost::asio::ip::make_address_v4("127.0.0.1"), port);
    p1.connect(ep); p2.connect(ep); p3.connect(ep);
    std::this_thread::sleep_for(20ms);

    // p1=0x42 (main), p2=0x43, p3=0x44; drain the RELAYCONNECT fan-out.
    SendFramed(p1, ToUint16(MessageId::RW_RELAYSVR_REQ), RelaysvrBody(0x0042));
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK)); }
    SendFramed(p2, ToUint16(MessageId::RW_RELAYSVR_REQ), RelaysvrBody(0x0043));
    { auto [w, _] = ReadFramed(p2);
      EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK)); }
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::MW_RELAYCONNECT_REQ)); }
    SendFramed(p3, ToUint16(MessageId::RW_RELAYSVR_REQ), RelaysvrBody(0x0044));
    { auto [w, _] = ReadFramed(p3);
      EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK)); }
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::MW_RELAYCONNECT_REQ)); }
    { auto [w, _] = ReadFramed(p2);
      EXPECT(w == ToUint16(MessageId::MW_RELAYCONNECT_REQ)); }

    auto cons_size = [&](std::uint32_t id) -> std::size_t {
        auto c = chars.Find(id);
        if (!c) return 0;
        std::lock_guard g(c->lock);
        return c->cons.size();
    };

    // Char A (100) on p1 only (cons = {0x42 main}); set party_waiter +
    // pos so the happy-path reply carries something verifiable.
    SendFramed(p1, ToUint16(MessageId::MW_ADDCHAR_ACK),
               AddCharBody(/*char_id=*/100, /*key=*/0xA1));
    for (int i = 0; i < 200 && !chars.Find(100); ++i)
        std::this_thread::sleep_for(10ms);
    {
        auto a = chars.Find(100);
        EXPECT(a != nullptr);
        if (a)
        {
            std::lock_guard g(a->lock);
            a->channel = 7; a->map_id = 1234;
            a->pos_x = 100.5f; a->pos_y = 200.5f; a->pos_z = 300.5f;
            a->party_waiter = true;
        }
    }

    // Char B (200) on p1 main + p2; serialise the two ADDCHARs so p1
    // wins the main slot before p2 adds its con (same trick as
    // test_conn_handlers.cpp).
    SendFramed(p1, ToUint16(MessageId::MW_ADDCHAR_ACK),
               AddCharBody(200, 0xB0));
    for (int i = 0; i < 200 && !chars.Find(200); ++i)
        std::this_thread::sleep_for(10ms);
    SendFramed(p2, ToUint16(MessageId::MW_ADDCHAR_ACK),
               AddCharBody(200, 0xB0));
    for (int i = 0; i < 200 && cons_size(200) != 2; ++i)
        std::this_thread::sleep_for(10ms);
    EXPECT(cons_size(200) == 2);

    // --- Test A: happy path — TELEPORT_ACK with dest=0x43 -----------
    {
        SendFramed(p1, ToUint16(MessageId::MW_TELEPORT_ACK),
                   TeleportBody(100, 0xA1, /*dest=*/0x43));
        // TELEPORT_REQ(SUCCESS) back to the responder (p1).
        auto [w1, got1] = ReadFramed(p1);
        EXPECT(w1 == ToUint16(MessageId::MW_TELEPORT_REQ));
        {
            wire::Reader r(got1);
            std::uint32_t cid = 0, key = 0; std::uint8_t ch = 0;
            std::uint16_t map_id = 0; float x = 0, y = 0, z = 0;
            std::uint8_t result = 0xFF;
            r.Read(cid); r.Read(key); r.Read(ch); r.Read(map_id);
            r.Read(x); r.Read(y); r.Read(z); r.Read(result);
            EXPECT(cid == 100);
            EXPECT(key == 0xA1);
            EXPECT(ch == 7);
            EXPECT(map_id == 1234);
            EXPECT(x == 100.5f);
            EXPECT(y == 200.5f);
            EXPECT(z == 300.5f);
            EXPECT(result == 0);    // TPR_SUCCESS
        }
        // CONLIST_REQ on the destination map (p2 = 0x43).
        auto [w2, got2] = ReadFramed(p2);
        EXPECT(w2 == ToUint16(MessageId::MW_CONLIST_REQ));
        {
            wire::Reader r(got2);
            std::uint32_t cid = 0, key = 0; std::uint8_t ch = 0;
            std::uint16_t map_id = 0; float x = 0, y = 0, z = 0;
            r.Read(cid); r.Read(key); r.Read(ch); r.Read(map_id);
            r.Read(x); r.Read(y); r.Read(z);
            EXPECT(cid == 100);
            EXPECT(key == 0xA1);
            EXPECT(ch == 7);
            EXPECT(map_id == 1234);
        }
        // party_waiter cleared on the happy path.
        bool ok = false;
        for (int i = 0; i < 200; ++i)
        {
            auto a = chars.Find(100);
            std::lock_guard g(a->lock);
            if (!a->party_waiter) { ok = true; break; }
            std::this_thread::sleep_for(10ms);
        }
        EXPECT(ok);
    }

    // --- Test B: NODESTINATION — char B's TELEPORT_ACK with dest=0x99
    //             (no such peer) → TELEPORT_REQ(NODESTINATION) +
    //             CloseChar (DELCHAR fan-out across char B's cons,
    //             then drop from the registry).
    {
        SendFramed(p1, ToUint16(MessageId::MW_TELEPORT_ACK),
                   TeleportBody(200, 0xB0, /*dest=*/0x99));
        auto [w1, got1] = ReadFramed(p1);
        EXPECT(w1 == ToUint16(MessageId::MW_TELEPORT_REQ));
        {
            wire::Reader r(got1);
            std::uint32_t cid = 0, key = 0; std::uint8_t ch = 0;
            std::uint16_t map_id = 0; float x = 0, y = 0, z = 0;
            std::uint8_t result = 0;
            r.Read(cid); r.Read(key); r.Read(ch); r.Read(map_id);
            r.Read(x); r.Read(y); r.Read(z); r.Read(result);
            EXPECT(cid == 200);
            EXPECT(key == 0xB0);
            EXPECT(result == 3);    // TPR_NODESTINATION
        }
        // CloseChar DELCHARs the live cons (main 0x42 → p1 first,
        // then 0x43 → p2). Default char.logout/save = false/false
        // so both DELCHARs carry (0, 0).
        auto [w1b, got1b] = ReadFramed(p1);
        EXPECT(w1b == ToUint16(MessageId::MW_DELCHAR_REQ));
        {
            wire::Reader r(got1b);
            std::uint32_t cid = 0, key = 0;
            std::uint8_t logout = 0, save = 0;
            r.Read(cid); r.Read(key); r.Read(logout); r.Read(save);
            EXPECT(cid == 200);
            EXPECT(key == 0xB0);
        }
        auto [w2, _2] = ReadFramed(p2);
        EXPECT(w2 == ToUint16(MessageId::MW_DELCHAR_REQ));

        // Char B is gone from the registry.
        bool gone = false;
        for (int i = 0; i < 200; ++i)
        {
            if (!chars.Find(200)) { gone = true; break; }
            std::this_thread::sleep_for(10ms);
        }
        EXPECT(gone);
    }

    // --- Test C: unknown char → DELCHAR on the reporter --------------
    {
        SendFramed(p3, ToUint16(MessageId::MW_TELEPORT_ACK),
                   TeleportBody(/*char_id=*/999, /*key=*/0xCC,
                       /*dest=*/0x42));
        auto [w, got] = ReadFramed(p3);
        EXPECT(w == ToUint16(MessageId::MW_DELCHAR_REQ));
        wire::Reader r(got);
        std::uint32_t cid = 0, key = 0;
        std::uint8_t logout = 0, save = 1;
        r.Read(cid); r.Read(key); r.Read(logout); r.Read(save);
        EXPECT(cid == 999);
        EXPECT(logout == 1);
        EXPECT(save == 0);
    }

    p1.close(); p2.close(); p3.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_teleport_confirm_handlers\n");
    else
        std::printf("FAIL test_tworldsvr_asio_teleport_confirm_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
