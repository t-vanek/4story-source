// W3a-1 wire test: drives DM_GUILDLOAD_ACK over a loopback session
// and asserts the resulting GuildRegistry / CharRegistry state.
//
// Scenarios:
//   1. Char registered → DM_GUILDLOAD_ACK with a valid body →
//      guild inserted, chief member appended.
//   2. Same guild_id sent twice → second is dropped silently
//      (idempotent insert, matches legacy SSHandler.cpp:8958).
//   3. Char NOT in registry (or wrong key) → guild NOT inserted.
//   4. Truncated body → guild NOT inserted, framer keeps the
//      session open for the next packet.
//
// The handler at present skips cabinet items (W3a-2 will parse
// them), so the test sends wCabinetCount=0 in every scenario.

#include "../config.h"
#include "../handlers/handlers.h"
#include "../services/char_registry.h"
#include "../services/guild_registry.h"
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

// Build a body matching the wire layout documented in
// handlers/handlers.h::OnGuildLoadAck. Returns a (body, wId) pair.
std::vector<std::byte> BuildLoadBody(std::uint32_t char_id,
                                      std::uint32_t key,
                                      std::uint32_t guild_id,
                                      const std::string& name,
                                      std::uint16_t cabinet_count = 0)
{
    std::vector<std::byte> b;
    using namespace tworldsvr::wire;
    WritePOD(b, char_id);
    WritePOD(b, key);
    WritePOD(b, guild_id);
    WriteString(b, name);
    WritePOD<std::uint32_t>(b, 1234);  // dwFame
    WritePOD<std::uint32_t>(b, 0xFF8800u); // dwFameColor
    WritePOD<std::uint8_t>(b, 10);     // bMaxCabinet
    WritePOD<std::uint8_t>(b, 5);      // bGPoint
    WritePOD<std::uint8_t>(b, 3);      // bLevel
    WritePOD<std::uint32_t>(b, char_id); // dwChief (re-asserted)
    WritePOD<std::uint32_t>(b, 50000); // dwExp
    WritePOD<std::uint32_t>(b, 100);   // dwGI
    WritePOD<std::uint8_t>(b, 1);      // bStatus
    WritePOD<std::uint32_t>(b, 9999);  // dwGold
    WritePOD<std::uint32_t>(b, 8888);  // dwSilver
    WritePOD<std::uint32_t>(b, 7777);  // dwCooper
    WritePOD<std::uint8_t>(b, 0);      // bDisorg
    WritePOD<std::uint32_t>(b, 0);     // dwTime
    WritePOD<std::int64_t>(b, 1700000000); // timeEstablish (epoch sec)
    WritePOD<std::uint32_t>(b, 4242);  // dwPvPTotalPoint
    WritePOD<std::uint32_t>(b, 100);   // dwPvPUseablePoint
    WritePOD<std::uint16_t>(b, cabinet_count); // wCount
    return b;
}

std::vector<std::byte> BuildLoadBodyShort(std::uint32_t char_id,
                                           std::uint32_t key,
                                           std::uint32_t guild_id)
{
    // Just the (char_id, key, guild_id, name) prefix — no fame /
    // level / etc. — to exercise the truncated-body branch.
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD(b, char_id);
    tworldsvr::wire::WritePOD(b, key);
    tworldsvr::wire::WritePOD(b, guild_id);
    tworldsvr::wire::WriteString(b, std::string{"Truncated"});
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

std::vector<std::byte> BuildAddCharBody(std::uint32_t char_id,
                                         std::uint32_t key,
                                         std::uint32_t user_id)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD(b, char_id);
    tworldsvr::wire::WritePOD(b, key);
    tworldsvr::wire::WritePOD<std::uint32_t>(b, 0x7f000001);
    tworldsvr::wire::WritePOD<std::uint16_t>(b, 33500);
    tworldsvr::wire::WritePOD(b, user_id);
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
    tworldsvr::HandlerContext ctx{};
    ctx.io     = &io;
    ctx.chars  = &chars;
    ctx.guilds = &guilds;

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

    // --- Pre: register the char (char_id=42, key=0xCAFEBABE) -------
    SendFramed(sock, ToUint16(MessageId::MW_ADDCHAR_ACK),
        BuildAddCharBody(42, 0xCAFEBABE, 100));
    std::this_thread::sleep_for(40ms);
    EXPECT(chars.Find(42) != nullptr);

    // --- Scenario 1: valid guild load --------------------------------
    SendFramed(sock, ToUint16(MessageId::DM_GUILDLOAD_ACK),
        BuildLoadBody(42, 0xCAFEBABE, 7, "AlphaGuild"));
    std::this_thread::sleep_for(40ms);
    {
        auto g = guilds.Find(7);
        EXPECT(g != nullptr);
        if (g)
        {
            EXPECT(g->id == 7);
            EXPECT(g->name == "AlphaGuild");
            EXPECT(g->level == 3);
            EXPECT(g->fame == 1234);
            EXPECT(g->gold == 9999);
            EXPECT(g->establish_time == 1700000000);
            EXPECT(g->members.size() == 1);
            if (!g->members.empty())
            {
                EXPECT(g->members[0].char_id == 42);
                EXPECT(g->members[0].duty == 2); // GUILD_DUTY_CHIEF (NetCode.h:1983)
            }
        }
        EXPECT(guilds.Size() == 1);
    }

    // --- Scenario 2: duplicate guild_id silently dropped ------------
    SendFramed(sock, ToUint16(MessageId::DM_GUILDLOAD_ACK),
        BuildLoadBody(42, 0xCAFEBABE, 7, "ShouldNotOverwrite"));
    std::this_thread::sleep_for(40ms);
    {
        auto g = guilds.Find(7);
        EXPECT(g != nullptr);
        if (g) EXPECT(g->name == "AlphaGuild"); // unchanged
        EXPECT(guilds.Size() == 1);
    }

    // --- Scenario 3: wrong key → not inserted ------------------------
    SendFramed(sock, ToUint16(MessageId::DM_GUILDLOAD_ACK),
        BuildLoadBody(42, 0xDEAD0000, 8, "BetaGuild"));
    std::this_thread::sleep_for(40ms);
    EXPECT(guilds.Find(8) == nullptr);
    EXPECT(guilds.Size() == 1);

    // --- Scenario 4: truncated body → not inserted, session alive ---
    SendFramed(sock, ToUint16(MessageId::DM_GUILDLOAD_ACK),
        BuildLoadBodyShort(42, 0xCAFEBABE, 9));
    std::this_thread::sleep_for(40ms);
    EXPECT(guilds.Find(9) == nullptr);
    EXPECT(guilds.Size() == 1);

    // Final sanity: session is still alive — the framer survived
    // the truncated-body drop. Send one more valid load.
    SendFramed(sock, ToUint16(MessageId::DM_GUILDLOAD_ACK),
        BuildLoadBody(42, 0xCAFEBABE, 10, "GammaGuild"));
    std::this_thread::sleep_for(40ms);
    EXPECT(guilds.Find(10) != nullptr);
    EXPECT(guilds.Size() == 2);

    boost::system::error_code ec;
    sock.shutdown(tcp::socket::shutdown_both, ec);
    sock.close(ec);

    std::this_thread::sleep_for(40ms);
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_guild_handlers (4 scenarios + alive)\n");
    else
        std::printf("FAIL test_tworldsvr_asio_guild_handlers (%d failure%s)\n",
            g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
