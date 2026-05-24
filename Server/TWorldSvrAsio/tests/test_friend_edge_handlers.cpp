// W4-17 wire test: friend-edge write-back (insert on accept, erase
// on remove).
//
// Bob (200/peer2) accepts Alice's (42/peer1) friend invite → world
// persists both directed edges. Alice then removes Bob → world
// deletes only Alice's forward edge (Bob still friends Alice). We
// read the (fake) repo back to confirm each write.

#include "../handlers/handlers.h"
#include "../services/char_registry.h"
#include "../services/fake_friend_repository.h"
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

#include <algorithm>
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

bool HasForward(tworldsvr::FakeFriendRepository& repo, std::uint32_t cid,
                std::uint32_t fid)
{
    auto fl = repo.LoadForChar(cid);
    for (const auto& f : fl.forward) if (f.id == fid) return true;
    return false;
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
    tworldsvr::CharRegistry         chars;
    tworldsvr::GuildRegistry        guilds;
    tworldsvr::PeerRegistry         peers;
    tworldsvr::FakeFriendRepository friend_repo;

    tworldsvr::HandlerContext ctx{};
    ctx.io = &io; ctx.chars = &chars; ctx.guilds = &guilds;
    ctx.peers = &peers; ctx.friend_repo = &friend_repo; ctx.nation = 0;

    tworldsvr::WorldServerConfig svr_cfg{};
    svr_cfg.port = 0; svr_cfg.max_connections = 4; svr_cfg.ctx = ctx;
    tworldsvr::WorldServer server(io, svr_cfg);
    const std::uint16_t port = server.Port();
    boost::asio::co_spawn(io, server.Run(), boost::asio::detached);
    std::thread io_thread([&io] { io.run(); });
    std::this_thread::sleep_for(20ms);

    boost::asio::io_context client_io;
    tcp::socket p1(client_io), p2(client_io);
    const auto ep = tcp::endpoint(
        boost::asio::ip::make_address_v4("127.0.0.1"), port);
    p1.connect(ep); p2.connect(ep);
    std::this_thread::sleep_for(20ms);

    SendFramed(p1, ToUint16(MessageId::RW_RELAYSVR_REQ), RelaysvrBody(0x0042));
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK)); }
    SendFramed(p2, ToUint16(MessageId::RW_RELAYSVR_REQ), RelaysvrBody(0x0043));
    { auto [w, _] = ReadFramed(p2);
      EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK)); }
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::MW_RELAYCONNECT_REQ)); }

    const std::uint32_t kA = 0xA1, kB = 0xB0;
    SendFramed(p1, ToUint16(MessageId::MW_ADDCHAR_ACK), AddCharBody(42, kA));
    SendFramed(p2, ToUint16(MessageId::MW_ADDCHAR_ACK), AddCharBody(200, kB));
    for (int i = 0; i < 100; ++i)
    { if (chars.Find(42) && chars.Find(200)) break;
      std::this_thread::sleep_for(10ms); }
    EXPECT(chars.Find(42) && chars.Find(200));
    chars.Rename(42, "Alice");
    chars.Rename(200, "Bob");

    // Bob accepts Alice's invite → both edges persisted.
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint32_t>(b, 200);   // answerer = Bob
        wire::WritePOD<std::uint32_t>(b, kB);
        wire::WriteString(b, "Alice");           // inviter name
        wire::WritePOD<std::uint8_t>(b, 0);      // ASK_YES
        SendFramed(p2, ToUint16(MessageId::MW_FRIENDREPLY_ACK), b);
    }
    // Accept replies to both maps.
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::MW_FRIENDADD_REQ)); }
    { auto [w, _] = ReadFramed(p2);
      EXPECT(w == ToUint16(MessageId::MW_FRIENDADD_REQ)); }

    std::this_thread::sleep_for(30ms);
    EXPECT(HasForward(friend_repo, 42, 200));   // Alice -> Bob
    EXPECT(HasForward(friend_repo, 200, 42));   // Bob -> Alice

    // Alice removes Bob → only Alice's forward edge is deleted.
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint32_t>(b, 42);
        wire::WritePOD<std::uint32_t>(b, kA);
        wire::WritePOD<std::uint32_t>(b, 200);   // target = Bob
        SendFramed(p1, ToUint16(MessageId::MW_FRIENDERASE_ACK), b);
    }
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::MW_FRIENDERASE_REQ)); }

    std::this_thread::sleep_for(30ms);
    EXPECT(!HasForward(friend_repo, 42, 200));  // Alice's edge gone
    EXPECT(HasForward(friend_repo, 200, 42));   // Bob still friends Alice

    p1.close(); p2.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_friend_edge_handlers\n");
    else
        std::printf("FAIL test_tworldsvr_asio_friend_edge_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
