// W3a-2 wire test: drives RW_RELAYSVR_REQ over a loopback session
// and asserts:
//   1. PeerRegistry gains an entry under the sent wID.
//   2. The RW_RELAYSVR_ACK reply lands back on the same socket
//      with the configured nation byte + empty op/msg lists.
//   3. A second RW_RELAYSVR_REQ with wID=0 is rejected (sentinel).
//   4. A second RW_RELAYSVR_REQ from a NEW socket with the same
//      wID is rejected and PeerRegistry retains the original.
//   5. Closing the original socket removes the registry entry
//      (HandleConnection's exit path).

#include "../config.h"
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

// Read one framed packet off the socket. Returns (wId, body) or
// throws on framing/checksum violation. Used to assert the
// RW_RELAYSVR_ACK reply landed correctly.
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
    ctx.nation = 7;   // arbitrary marker value to verify the reply

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

    // --- Scenario 1+2: RELAYSVR_REQ wID=11 → registered + ACK ----
    {
        std::vector<std::byte> body;
        tworldsvr::wire::WritePOD<std::uint16_t>(body, 11);
        SendFramed(sock, ToUint16(MessageId::RW_RELAYSVR_REQ), body);

        // Read the ACK from the same socket. Should arrive
        // synchronously since the handler co_returns after the
        // SendPacket awaitable completes.
        auto [ack_id, ack_body] = ReadFramed(sock);
        EXPECT(ack_id == ToUint16(MessageId::RW_RELAYSVR_ACK));

        tworldsvr::wire::Reader r(ack_body);
        std::uint8_t  nation  = 0;
        std::uint16_t op_cnt  = 0;
        std::uint16_t msg_cnt = 0;
        EXPECT(r.Read(nation));
        EXPECT(nation == 7);
        EXPECT(r.Read(op_cnt));
        EXPECT(op_cnt == 0);
        EXPECT(r.Read(msg_cnt));
        EXPECT(msg_cnt == 0);

        std::this_thread::sleep_for(20ms);
        EXPECT(peers.Size() == 1);
        EXPECT(peers.Find(11) != nullptr);
    }

    // --- Scenario 3: wID=0 is rejected (sentinel) ----------------
    {
        std::vector<std::byte> body;
        tworldsvr::wire::WritePOD<std::uint16_t>(body, 0);
        SendFramed(sock, ToUint16(MessageId::RW_RELAYSVR_REQ), body);
        std::this_thread::sleep_for(30ms);
        // Registry is unchanged; no ACK is written for this
        // rejection path (legacy + W3a-2 parity).
        EXPECT(peers.Size() == 1);
    }

    // --- Scenario 4: second socket reusing wID=11 → keep original
    {
        boost::asio::io_context client_io2;
        tcp::socket sock2(client_io2);
        sock2.connect(tcp::endpoint(
            boost::asio::ip::make_address_v4("127.0.0.1"), port));
        std::this_thread::sleep_for(20ms);

        std::vector<std::byte> body;
        tworldsvr::wire::WritePOD<std::uint16_t>(body, 11);
        SendFramed(sock2, ToUint16(MessageId::RW_RELAYSVR_REQ), body);
        std::this_thread::sleep_for(30ms);

        // Original entry still owns wID=11; the second registration
        // didn't overwrite. Registry size is still 1.
        EXPECT(peers.Size() == 1);

        // Close the duplicate socket — the cleanup path runs but
        // SHOULD NOT remove the original's entry because the
        // duplicate's PeerSession wasn't in the registry.
        boost::system::error_code ec;
        sock2.shutdown(tcp::socket::shutdown_both, ec);
        sock2.close(ec);
        std::this_thread::sleep_for(40ms);
        EXPECT(peers.Size() == 1);
        EXPECT(peers.Find(11) != nullptr);
    }

    // --- Scenario 5: closing original socket removes entry --------
    {
        boost::system::error_code ec;
        sock.shutdown(tcp::socket::shutdown_both, ec);
        sock.close(ec);
        std::this_thread::sleep_for(60ms);
        EXPECT(peers.Size() == 0);
        EXPECT(peers.Find(11) == nullptr);
    }

    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_relay_handlers (5 scenarios)\n");
    else
        std::printf("FAIL test_tworldsvr_asio_relay_handlers (%d failure%s)\n",
            g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
