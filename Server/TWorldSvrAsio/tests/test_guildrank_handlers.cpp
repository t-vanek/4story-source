// W6-11 wire test: day-change guild ranking.
//
// Three guilds with different PvP points; SM_CHANGEDAY recomputes
// each guild's total + month rank.

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
#include <memory>
#include <mutex>
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

std::shared_ptr<tworldsvr::TGuild>
MakeGuild(std::uint32_t id, std::uint32_t total, std::uint32_t month)
{
    auto g = std::make_shared<tworldsvr::TGuild>();
    g->id = id;
    g->pvp_total_point = total;
    g->pvp_month_point = month;
    return g;
}

std::uint32_t RankTotal(tworldsvr::GuildRegistry& gr, std::uint32_t id)
{
    auto g = gr.Find(id);
    std::lock_guard lk(g->lock);
    return g->rank_total;
}
std::uint32_t RankMonth(tworldsvr::GuildRegistry& gr, std::uint32_t id)
{
    auto g = gr.Find(id);
    std::lock_guard lk(g->lock);
    return g->rank_month;
}

} // namespace

int main()
{
    using boost::asio::ip::tcp;
    using namespace std::chrono_literals;
    using tnetlib::protocol::MessageId;
    using tnetlib::protocol::ToUint16;

    boost::asio::io_context io;
    tworldsvr::CharRegistry   chars;
    tworldsvr::GuildRegistry  guilds;
    tworldsvr::PeerRegistry   peers;
    tworldsvr::HandlerContext ctx{};
    ctx.io = &io; ctx.chars = &chars; ctx.guilds = &guilds;
    ctx.peers = &peers; ctx.nation = 0;

    // A: total 100 / month 50, B: total 200 / month 10, C: 0 / 0.
    guilds.Insert(MakeGuild(1, 100, 50));
    guilds.Insert(MakeGuild(2, 200, 10));
    guilds.Insert(MakeGuild(3, 0, 0));

    tworldsvr::WorldServerConfig svr_cfg{};
    svr_cfg.port = 0; svr_cfg.max_connections = 4; svr_cfg.ctx = ctx;
    tworldsvr::WorldServer server(io, svr_cfg);
    const std::uint16_t port = server.Port();
    boost::asio::co_spawn(io, server.Run(), boost::asio::detached);
    std::thread io_thread([&io] { io.run(); });
    std::this_thread::sleep_for(20ms);

    boost::asio::io_context client_io;
    tcp::socket p1(client_io);
    p1.connect(tcp::endpoint(
        boost::asio::ip::make_address_v4("127.0.0.1"), port));
    std::this_thread::sleep_for(20ms);

    SendFramed(p1, ToUint16(MessageId::RW_RELAYSVR_REQ), RelaysvrBody(0x0042));
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK)); }

    SendFramed(p1, ToUint16(MessageId::SM_CHANGEDAY_REQ), {});

    // Wait for the recompute (B should reach total-rank 1).
    for (int i = 0; i < 100; ++i)
    { if (RankTotal(guilds, 2) == 1) break; std::this_thread::sleep_for(10ms); }

    EXPECT(RankTotal(guilds, 1) == 2);  // A: one guild (B) has more total
    EXPECT(RankTotal(guilds, 2) == 1);  // B: top total
    EXPECT(RankTotal(guilds, 3) == 0);  // C: no points → unranked
    EXPECT(RankMonth(guilds, 1) == 1);  // A: top month (50)
    EXPECT(RankMonth(guilds, 2) == 2);  // B: A has more month
    EXPECT(RankMonth(guilds, 3) == 0);

    p1.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_guildrank_handlers\n");
    else
        std::printf("FAIL test_tworldsvr_asio_guildrank_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
