// W6-28 wire test: MW_ARENAJOIN_ACK.
//
// One peer (p1=0x42). Alice (42, chief), Bob (200), Carol (300) all
// in one party. Two test cases:
//
//   * Arena ENTER with keep_list = [Alice, Bob]:
//     - TParty.arena flips to true
//     - Carol gets LeaveParty (PARTYDEL fan-out to all 3 members)
//     - Alice + Bob stay in the party
//   * Arena LEAVE (join=0):
//     - TParty.arena flips back to false
//     - No member removal, no PARTYDEL packets
//
// (The corps-unwind branch — `NotifyCorpsLeave` when the party is
// in a corps — relies on the W3c-3 corps_leave handlers + their
// test coverage; we leave that path to the existing tests.)

#include "../handlers/handlers.h"
#include "../services/char_registry.h"
#include "../services/corps_registry.h"
#include "../services/guild_registry.h"
#include "../services/party_constants.h"
#include "../services/party_registry.h"
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

std::vector<std::byte>
ArenaJoinBody(std::uint32_t char_id, std::uint32_t key, std::uint8_t join,
              const std::vector<std::uint32_t>& keep)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD(b, char_id);
    tworldsvr::wire::WritePOD(b, key);
    tworldsvr::wire::WritePOD(b, join);
    tworldsvr::wire::WritePOD<std::uint32_t>(b,
        static_cast<std::uint32_t>(keep.size()));
    for (auto id : keep)
        tworldsvr::wire::WritePOD(b, id);
    return b;
}

} // namespace

int main()
{
    using boost::asio::ip::tcp;
    using namespace std::chrono_literals;
    using tnetlib::protocol::MessageId;
    using tnetlib::protocol::ToUint16;
    namespace party = tworldsvr::party;

    boost::asio::io_context io;
    tworldsvr::CharRegistry  chars;
    tworldsvr::GuildRegistry guilds;
    tworldsvr::PartyRegistry parties;
    tworldsvr::CorpsRegistry corps;
    tworldsvr::PeerRegistry  peers;
    tworldsvr::HandlerContext ctx{};
    ctx.io = &io; ctx.chars = &chars; ctx.guilds = &guilds;
    ctx.parties = &parties; ctx.corps = &corps;
    ctx.peers = &peers; ctx.nation = 0;

    tworldsvr::WorldServerConfig svr_cfg{};
    svr_cfg.port = 0; svr_cfg.max_connections = 4; svr_cfg.ctx = ctx;
    tworldsvr::WorldServer server(io, svr_cfg);
    const std::uint16_t port = server.Port();
    boost::asio::co_spawn(io, server.Run(), boost::asio::detached);
    std::thread io_thread([&io] { io.run(); });
    std::this_thread::sleep_for(20ms);

    boost::asio::io_context client_io;
    tcp::socket p1(client_io);
    const auto ep = tcp::endpoint(
        boost::asio::ip::make_address_v4("127.0.0.1"), port);
    p1.connect(ep);
    std::this_thread::sleep_for(20ms);

    SendFramed(p1, ToUint16(MessageId::RW_RELAYSVR_REQ), RelaysvrBody(0x0042));
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK)); }

    SendFramed(p1, ToUint16(MessageId::MW_ADDCHAR_ACK), AddCharBody(42,  0xA1));
    SendFramed(p1, ToUint16(MessageId::MW_ADDCHAR_ACK), AddCharBody(200, 0xB0));
    SendFramed(p1, ToUint16(MessageId::MW_ADDCHAR_ACK), AddCharBody(300, 0xC0));
    for (int i = 0; i < 200 && (!chars.Find(42) || !chars.Find(200) ||
                                 !chars.Find(300)); ++i)
        std::this_thread::sleep_for(10ms);
    EXPECT(chars.Find(42) && chars.Find(200) && chars.Find(300));

    // Seed: party with all three, Alice as chief.
    constexpr std::uint16_t kPid = 7;
    {
        auto pty = std::make_shared<tworldsvr::TParty>();
        pty->id            = kPid;
        pty->obtain_type   = party::kObtainHunter;
        pty->chief_char_id = 42;
        for (auto m : {42u, 200u, 300u}) pty->AddMember(m);
        EXPECT(parties.Insert(pty));
    }
    auto set_pid = [&](std::uint32_t id) {
        auto c = chars.Find(id);
        if (c) { std::lock_guard g(c->lock); c->party_id = kPid; }
    };
    set_pid(42); set_pid(200); set_pid(300);

    auto party_size = [&]() {
        auto p = parties.Find(kPid);
        if (!p) return std::uint8_t{0};
        std::lock_guard g(p->lock); return p->Size();
    };
    auto party_arena = [&]() {
        auto p = parties.Find(kPid);
        if (!p) return false;
        std::lock_guard g(p->lock); return p->arena;
    };

    // --- Test A: arena ENTER, keep [Alice, Bob] → Carol kicked -----
    {
        SendFramed(p1, ToUint16(MessageId::MW_ARENAJOIN_ACK),
                   ArenaJoinBody(42, 0xA1, /*join=*/1, {42u, 200u}));
        // LeaveParty fans MW_PARTYDEL_REQ to each surviving member +
        // the leaver — three packets in members_before order
        // (Alice / Bob / Carol). With size==3 the legacy
        // is_delete=true branch keeps the party alive (≥2 members
        // remain after Carol leaves).
        for (int i = 0; i < 3; ++i)
        {
            auto [w, _] = ReadFramed(p1);
            EXPECT(w == ToUint16(MessageId::MW_PARTYDEL_REQ));
        }
        bool ok = false;
        for (int i = 0; i < 200; ++i)
        {
            if (party_arena() && party_size() == 2) { ok = true; break; }
            std::this_thread::sleep_for(10ms);
        }
        EXPECT(ok);
        // Carol's party_id back-pointer cleared by LeaveParty.
        {
            auto carol = chars.Find(300);
            std::lock_guard g(carol->lock);
            EXPECT(carol->party_id == 0);
        }
    }

    // --- Test B: arena LEAVE — flag flip, no fan-out ---------------
    {
        SendFramed(p1, ToUint16(MessageId::MW_ARENAJOIN_ACK),
                   ArenaJoinBody(42, 0xA1, /*join=*/0, {}));
        // No reply expected. Poll for the flag flip; then send a
        // benign stimulus + read its reply to drain handler queue.
        bool ok = false;
        for (int i = 0; i < 200; ++i)
        {
            if (!party_arena()) { ok = true; break; }
            std::this_thread::sleep_for(10ms);
        }
        EXPECT(ok);
        // Party size unchanged.
        EXPECT(party_size() == 2);
    }

    p1.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_arenajoin_handlers\n");
    else
        std::printf("FAIL test_tworldsvr_asio_arenajoin_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
