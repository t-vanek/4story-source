// W2 wire test: drives MW_ADDCHAR_ACK + MW_CLOSECHAR_ACK frames
// through WorldServer over a loopback socket and asserts the
// CharRegistry mutation each handler is supposed to perform.
//
// Scenarios:
//   1. ADDCHAR_ACK { dwCharID=42, dwKEY=0xCAFEBABE, ip, port,
//      dwUserID=100 } → registry has the entry, user 100 marked
//      active.
//   2. Second ADDCHAR_ACK for the same char_id with the same key
//      and a different (ip:port) → "additional connection" branch
//      pushes a second TCharCon onto the existing TChar's cons[].
//   3. ADDCHAR_ACK for char_id=42 with a different key → dropped
//      (W3 will fire MW_INVALIDCHAR_REQ; W2 just logs).
//   4. CLOSECHAR_ACK for char_id=42 → registry empties, user 100
//      becomes inactive.
//   5. CLOSECHAR_ACK for an unknown char_id → no crash, no
//      mutation (stale-close race tolerance).
//
// All scenarios share one loopback session so the framer's
// continue-after-each-packet behavior gets exercised too.

#include "../config.h"
#include "../handlers/handlers.h"
#include "../services/char_registry.h"
#include "../wire_codec.h"
#include "../world_server.h"
#include "../world_session.h"

#include "MessageId.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/write.hpp>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
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
                                         std::uint32_t key,
                                         std::uint32_t ip,
                                         std::uint16_t port,
                                         std::uint32_t user_id)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD(b, char_id);
    tworldsvr::wire::WritePOD(b, key);
    tworldsvr::wire::WritePOD(b, ip);
    tworldsvr::wire::WritePOD(b, port);
    tworldsvr::wire::WritePOD(b, user_id);
    return b;
}

std::vector<std::byte> BuildCloseCharBody(std::uint32_t char_id,
                                           std::uint32_t key)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD(b, char_id);
    tworldsvr::wire::WritePOD(b, key);
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
    hdr.dwChkSum = tworldsvr::ComputeChecksum(body.data(), body.size());

    std::vector<std::byte> buf(hdr.wSize);
    std::memcpy(buf.data(), &hdr, sizeof(hdr));
    if (!body.empty())
        std::memcpy(buf.data() + sizeof(hdr), body.data(), body.size());
    boost::asio::write(sock, boost::asio::buffer(buf.data(), buf.size()));
}

} // namespace

int main()
{
    using boost::asio::ip::tcp;
    using namespace std::chrono_literals;
    using tnetlib::protocol::MessageId;
    using tnetlib::protocol::ToUint16;

    boost::asio::io_context io;

    tworldsvr::CharRegistry chars;
    tworldsvr::HandlerContext ctx{};
    ctx.io    = &io;
    ctx.chars = &chars;

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

    // --- Scenario 1: ADDCHAR_ACK inserts ----------------------------
    SendFramed(sock, ToUint16(MessageId::MW_ADDCHAR_ACK),
        BuildAddCharBody(42, 0xCAFEBABE, 0x0100007F /*127.0.0.1*/,
                         33500, 100));
    std::this_thread::sleep_for(40ms);
    {
        auto ch = chars.Find(42);
        EXPECT(ch != nullptr);
        if (ch)
        {
            EXPECT(ch->char_id == 42);
            EXPECT(ch->key == 0xCAFEBABE);
            EXPECT(ch->user_id == 100);
            EXPECT(ch->cons.size() == 1);
            if (!ch->cons.empty())
            {
                EXPECT(ch->cons[0].ip_addr == 0x0100007F);
                EXPECT(ch->cons[0].port == 33500);
            }
        }
        EXPECT(chars.IsUserActive(100));
        EXPECT(chars.Size() == 1);
    }

    // --- Scenario 2: additional connection appends TCharCon ---------
    SendFramed(sock, ToUint16(MessageId::MW_ADDCHAR_ACK),
        BuildAddCharBody(42, 0xCAFEBABE, 0x0200007F /*127.0.0.2*/,
                         33501, 100));
    std::this_thread::sleep_for(40ms);
    {
        auto ch = chars.Find(42);
        EXPECT(ch != nullptr);
        if (ch)
        {
            EXPECT(ch->cons.size() == 2);
            if (ch->cons.size() == 2)
                EXPECT(ch->cons[1].port == 33501);
        }
    }

    // --- Scenario 3: wrong key for known char_id is dropped ---------
    SendFramed(sock, ToUint16(MessageId::MW_ADDCHAR_ACK),
        BuildAddCharBody(42, 0xDEADC0DE, 0x0300007F, 33502, 100));
    std::this_thread::sleep_for(40ms);
    {
        auto ch = chars.Find(42);
        EXPECT(ch != nullptr);
        if (ch)
        {
            // No new connection was appended; the wrong-key frame
            // was dropped with a warning.
            EXPECT(ch->cons.size() == 2);
            EXPECT(ch->key == 0xCAFEBABE); // unchanged
        }
    }

    // --- Scenario 4: CLOSECHAR_ACK removes and deactivates user -----
    SendFramed(sock, ToUint16(MessageId::MW_CLOSECHAR_ACK),
        BuildCloseCharBody(42, 0xCAFEBABE));
    std::this_thread::sleep_for(40ms);
    {
        EXPECT(chars.Find(42) == nullptr);
        EXPECT(chars.Size() == 0);
        EXPECT(!chars.IsUserActive(100));
    }

    // --- Scenario 5: CLOSECHAR_ACK for unknown char is benign -------
    SendFramed(sock, ToUint16(MessageId::MW_CLOSECHAR_ACK),
        BuildCloseCharBody(999, 0xDEAD0001));
    std::this_thread::sleep_for(40ms);
    EXPECT(chars.Size() == 0); // still empty, no crash

    boost::system::error_code ec;
    sock.shutdown(tcp::socket::shutdown_both, ec);
    sock.close(ec);

    std::this_thread::sleep_for(40ms);
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_char_handlers (5 scenarios)\n");
    else
        std::printf("FAIL test_tworldsvr_asio_char_handlers (%d failure%s)\n",
            g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
