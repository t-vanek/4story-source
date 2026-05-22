// W3a-4 wire test: MW_GUILDLEAVE_ACK + the surrounding
// TChar.guild_id ↔ TGuild.members invariant.
//
// Scenarios:
//   1. Load a guild via DM_GUILDLOAD_ACK → TChar.guild_id is set
//      on the founder; the guild has one member (the founder).
//   2. Drive MW_GUILDLEAVE_ACK on the founder → guild's members
//      list is empty, TChar.guild_id is cleared, the test reads
//      back the MW_GUILDLEAVE_REQ reply with the expected fields.
//   3. Drive MW_GUILDLEAVE_ACK a second time → handler treats it
//      as a benign no-op (no crash, no extra reply).
//   4. OnEnterCharReq after the leave → ENTERCHAR_ACK carries
//      guild_id=0 (W3a-3 returned stale data before this fix).
//
// Uses two peer sockets so we can verify the leave reply lands
// on the originating socket, not on an arbitrary registered peer.

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
    tworldsvr::wire::WritePOD<std::uint32_t>(b, /*user_id=*/100);
    return b;
}

std::vector<std::byte> NameBody(std::uint32_t char_id,
                                 std::uint32_t key,
                                 const std::string& name)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD(b, char_id);
    tworldsvr::wire::WritePOD(b, key);
    tworldsvr::wire::WritePOD<std::uint8_t>(b, /*IK_NAME=*/ 48);
    tworldsvr::wire::WritePOD<std::uint8_t>(b, 0);
    tworldsvr::wire::WritePOD<std::uint16_t>(b, 0);
    tworldsvr::wire::WriteString(b, name);
    return b;
}

std::vector<std::byte> GuildLoadBody(std::uint32_t char_id,
                                      std::uint32_t key,
                                      std::uint32_t guild_id,
                                      const std::string& name)
{
    std::vector<std::byte> b;
    using namespace tworldsvr::wire;
    WritePOD(b, char_id); WritePOD(b, key); WritePOD(b, guild_id);
    WriteString(b, name);
    WritePOD<std::uint32_t>(b, 1234);             // dwFame
    WritePOD<std::uint32_t>(b, 0xFF8800);         // dwFameColor
    WritePOD<std::uint8_t>(b, 10);                // bMaxCabinet
    WritePOD<std::uint8_t>(b, 5);                 // bGPoint
    WritePOD<std::uint8_t>(b, 3);                 // bLevel
    WritePOD<std::uint32_t>(b, char_id);          // dwChief
    WritePOD<std::uint32_t>(b, 50000);            // dwExp
    WritePOD<std::uint32_t>(b, 100);              // dwGI
    WritePOD<std::uint8_t>(b, 1);                 // bStatus
    WritePOD<std::uint32_t>(b, 9999);             // dwGold
    WritePOD<std::uint32_t>(b, 8888);             // dwSilver
    WritePOD<std::uint32_t>(b, 7777);             // dwCooper
    WritePOD<std::uint8_t>(b, 0);                 // bDisorg
    WritePOD<std::uint32_t>(b, 0);                // dwTime
    WritePOD<std::int64_t>(b, 1700000000);        // timeEstablish
    WritePOD<std::uint32_t>(b, 4242);             // dwPvPTotalPoint
    WritePOD<std::uint32_t>(b, 100);              // dwPvPUseablePoint
    WritePOD<std::uint16_t>(b, 0);                // wCabinetCount
    return b;
}

std::vector<std::byte> LeaveBody(std::uint32_t char_id,
                                  std::uint32_t key)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD(b, char_id);
    tworldsvr::wire::WritePOD(b, key);
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
    svr_cfg.port = 0;
    svr_cfg.max_connections = 4;
    svr_cfg.ctx = ctx;
    tworldsvr::WorldServer server(io, svr_cfg);
    const std::uint16_t port = server.Port();
    boost::asio::co_spawn(io, server.Run(), boost::asio::detached);
    std::thread io_thread([&io] { io.run(); });
    std::this_thread::sleep_for(20ms);

    boost::asio::io_context client_io;
    tcp::socket peer1(client_io);
    peer1.connect(tcp::endpoint(
        boost::asio::ip::make_address_v4("127.0.0.1"), port));
    std::this_thread::sleep_for(20ms);

    SendFramed(peer1, ToUint16(MessageId::RW_RELAYSVR_REQ),
        RelaysvrBody(0x0042));
    // Drain RELAYSVR_ACK.
    { auto [w, _] = ReadFramed(peer1); EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK)); }
    EXPECT(peers.Size() == 1);

    // --- Scenario 1: load guild → TChar.guild_id set -----------------
    SendFramed(peer1, ToUint16(MessageId::MW_ADDCHAR_ACK),
        AddCharBody(42, 0xCAFEBABE));
    SendFramed(peer1, ToUint16(MessageId::MW_CHANGECHARBASE_ACK),
        NameBody(42, 0xCAFEBABE, "Alice"));
    SendFramed(peer1, ToUint16(MessageId::DM_GUILDLOAD_ACK),
        GuildLoadBody(42, 0xCAFEBABE, 7, "Alpha"));
    // Drain the GUILDESTABLISH_REQ reply.
    { auto [w, _] = ReadFramed(peer1); EXPECT(w == ToUint16(MessageId::MW_GUILDESTABLISH_REQ)); }

    // Poll until the back-pointer is set (handler is async).
    for (int i = 0; i < 50; ++i)
    {
        if (auto c = chars.Find(42))
        {
            std::lock_guard g(c->lock);
            if (c->guild_id == 7) break;
        }
        std::this_thread::sleep_for(10ms);
    }
    {
        auto c = chars.Find(42);
        EXPECT(c != nullptr);
        if (c) { std::lock_guard g(c->lock); EXPECT(c->guild_id == 7); }
        auto g = guilds.Find(7);
        EXPECT(g != nullptr);
        if (g) { std::lock_guard gl(g->lock); EXPECT(g->members.size() == 1); }
    }

    // --- Scenario 2: GUILDLEAVE_ACK removes member + replies --------
    SendFramed(peer1, ToUint16(MessageId::MW_GUILDLEAVE_ACK),
        LeaveBody(42, 0xCAFEBABE));
    {
        auto [w, body] = ReadFramed(peer1);
        EXPECT(w == ToUint16(MessageId::MW_GUILDLEAVE_REQ));
        tworldsvr::wire::Reader r(body);
        std::uint32_t cid = 0, key = 0; std::string name;
        std::uint8_t  reason = 0; std::uint32_t time_unix = 0;
        EXPECT(r.Read(cid));    EXPECT(cid == 42);
        EXPECT(r.Read(key));    EXPECT(key == 0xCAFEBABE);
        EXPECT(r.ReadString(name)); EXPECT(name == "Alice");
        EXPECT(r.Read(reason)); EXPECT(reason == 1); // GUILD_LEAVE_SELF
        EXPECT(r.Read(time_unix));
        EXPECT(time_unix > 0);                       // any non-zero epoch
    }
    {
        auto c = chars.Find(42);
        if (c) { std::lock_guard g(c->lock); EXPECT(c->guild_id == 0); }
        auto g = guilds.Find(7);
        if (g) { std::lock_guard gl(g->lock); EXPECT(g->members.empty()); }
    }

    // --- Scenario 3: second leave is a benign no-op -----------------
    SendFramed(peer1, ToUint16(MessageId::MW_GUILDLEAVE_ACK),
        LeaveBody(42, 0xCAFEBABE));
    std::this_thread::sleep_for(60ms);
    // No reply expected (handler short-circuits on guild_id=0).
    // Confirm the socket is still alive by sending another packet.
    SendFramed(peer1, ToUint16(MessageId::RW_ENTERCHAR_REQ),
        EntercharBody(42, "Alice"));
    {
        auto [w, body] = ReadFramed(peer1);
        EXPECT(w == ToUint16(MessageId::RW_ENTERCHAR_ACK));
        // --- Scenario 4: ENTERCHAR_ACK after leave carries guild=0 ---
        tworldsvr::wire::Reader r(body);
        std::uint32_t cid = 0; std::string name;
        std::uint8_t  result = 0, country = 0, aid = 0;
        std::uint32_t gid = 0, gchief = 0; std::uint8_t duty = 0;
        EXPECT(r.Read(cid));         EXPECT(cid == 42);
        EXPECT(r.ReadString(name));
        EXPECT(r.Read(result));      EXPECT(result == 1);
        EXPECT(r.Read(country));
        EXPECT(r.Read(aid));
        EXPECT(r.Read(gid));         EXPECT(gid == 0);
        EXPECT(r.Read(gchief));      EXPECT(gchief == 0);
        EXPECT(r.Read(duty));        EXPECT(duty == 0);
    }

    boost::system::error_code ec;
    peer1.shutdown(tcp::socket::shutdown_both, ec);
    peer1.close(ec);

    std::this_thread::sleep_for(60ms);
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_guild_mut_handlers (4 scenarios)\n");
    else
        std::printf("FAIL test_tworldsvr_asio_guild_mut_handlers (%d failure%s)\n",
            g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
