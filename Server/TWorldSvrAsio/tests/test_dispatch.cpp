// W1 wire test for TWorldSvrAsio.
//
// Stands WorldServer up on an ephemeral port (port=0), opens a
// loopback client socket, and exercises three scenarios against the
// dispatch stub:
//
//   1. Send a frame with a KNOWN wID (MW_CHECKCONNECT_REQ) and a
//      tiny body. Expect the server to dispatch (log + drop) and
//      keep the connection open.
//   2. Send a second frame with an UNKNOWN wID (0xFFFF, body=0).
//      Expect the same — dispatch logs the unknown ID, keeps going.
//   3. Send a frame with a CORRUPT checksum. Expect the server to
//      close the connection (framing violation).
//
// All three scenarios share one socket — proving the framing loop
// survives both legitimate and unknown packets but terminates on a
// corrupt one. No DB, no fakes, no FourStoryCommon dependencies
// beyond what links transitively through tworldsvr_asio_core.

#include "../config.h"
#include "../handlers/handlers.h"
#include "../world_server.h"
#include "../world_session.h"
#include "../wire_codec.h"

#include "MessageId.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>

#include <atomic>
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

// Frame a packet with a correct checksum and write it onto the
// loopback socket. Mirrors WorldSession::SendPacket on the client
// side so the test does not depend on the server's send path.
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

// Same as SendFramed but writes a deliberately wrong checksum so
// the server's framer rejects the frame and closes the socket.
void SendFramedBadChecksum(boost::asio::ip::tcp::socket& sock,
                            std::uint16_t wId,
                            const std::vector<std::byte>& body)
{
    tworldsvr::PacketHeader hdr{};
    hdr.wSize    = static_cast<std::uint16_t>(
        tworldsvr::kPacketHeaderSize + body.size());
    hdr.wID      = wId;
    hdr.dwChkSum = 0xDEADBEEFu;   // intentionally wrong

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

    boost::asio::io_context io;

    tworldsvr::HandlerContext ctx{};
    ctx.io = &io;

    tworldsvr::WorldServerConfig svr_cfg{};
    svr_cfg.port            = 0;        // ephemeral
    svr_cfg.max_connections = 8;
    svr_cfg.ctx             = ctx;

    tworldsvr::WorldServer server(io, svr_cfg);
    const std::uint16_t port = server.Port();
    boost::asio::co_spawn(io, server.Run(), boost::asio::detached);

    std::thread io_thread([&io] { io.run(); });

    // Give the acceptor a beat to settle before the client connects.
    std::this_thread::sleep_for(20ms);

    // --- Scenario 1 + 2: known + unknown wID on one socket --------
    {
        boost::asio::io_context client_io;
        tcp::socket sock(client_io);
        sock.connect(tcp::endpoint(
            boost::asio::ip::make_address_v4("127.0.0.1"), port));

        // 1. Known wID — pick MW_ENTERSVR_REQ because its name
        // shows up in the dispatch log (proves NameOf wiring).
        SendFramed(sock,
            tnetlib::protocol::ToUint16(
                tnetlib::protocol::MessageId::MW_ENTERSVR_REQ),
            {});

        // 2. Unknown wID — 0xFFFF is unallocated in the catalog.
        SendFramed(sock, 0xFFFFu, {});

        // Give the dispatcher time to consume both before we close.
        // Then check m_live still counts us — i.e. the server didn't
        // tear down the session on either packet.
        std::this_thread::sleep_for(50ms);
        EXPECT(server.LiveConnections() == 1);

        // Clean disconnect — the server-side coroutine should
        // observe EOF and decrement m_live.
        boost::system::error_code ec;
        sock.shutdown(tcp::socket::shutdown_both, ec);
        sock.close(ec);
    }
    std::this_thread::sleep_for(50ms);
    EXPECT(server.LiveConnections() == 0);

    // --- Scenario 3: bad checksum closes the connection -----------
    {
        boost::asio::io_context client_io;
        tcp::socket sock(client_io);
        sock.connect(tcp::endpoint(
            boost::asio::ip::make_address_v4("127.0.0.1"), port));

        std::this_thread::sleep_for(20ms);
        EXPECT(server.LiveConnections() == 1);

        SendFramedBadChecksum(sock, 0x1234u, {std::byte{0x11}, std::byte{0x22}});

        // The framer logs the checksum mismatch and closes. Our
        // read of the next byte should observe EOF.
        std::this_thread::sleep_for(50ms);
        EXPECT(server.LiveConnections() == 0);

        boost::system::error_code ec;
        sock.shutdown(tcp::socket::shutdown_both, ec);
        sock.close(ec);
    }

    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_dispatch (3 scenarios)\n");
    else
        std::printf("FAIL test_tworldsvr_asio_dispatch (%d failure%s)\n",
            g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
