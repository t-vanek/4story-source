// W4-6 wire test: soulmate SEARCH / REG / END over a three-peer
// loopback session.
//
// Alice 42/peer1 0x42 (lvl20, real_sex0, sex0, region11),
// Bob 200/peer2 0x43 (lvl22, real_sex1, sex1, region22),
// Carol 400/peer3 0x44 (lvl22, real_sex0, sex0, region33).
//
// Scenarios: SEARCH matches Bob (Carol filtered by the real-sex
// tiebreak) + mutual pairing; REG preview (bReg=0, no pairing);
// REG register (bReg=1, mutual pairing); REG cross-country FAIL;
// END dissolves both sides; END with no soulmate FAIL.

#include "../handlers/handlers.h"
#include "../services/char_registry.h"
#include "../services/guild_registry.h"
#include "../services/peer_registry.h"
#include "../services/soulmate_constants.h"
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

namespace sm = tworldsvr::soulmate;

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

std::vector<std::byte> SearchBody(std::uint32_t cid, std::uint32_t key,
                                   std::uint8_t min_level)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD<std::uint32_t>(b, cid);
    tworldsvr::wire::WritePOD<std::uint32_t>(b, key);
    tworldsvr::wire::WritePOD<std::uint8_t>(b, min_level);
    tworldsvr::wire::WritePOD<std::uint8_t>(b, /*npc_inven=*/3);
    tworldsvr::wire::WritePOD<std::uint8_t>(b, /*npc_item=*/7);
    return b;
}

std::vector<std::byte> RegBody(std::uint32_t cid, std::uint32_t key,
                                const std::string& name, std::uint8_t reg)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD<std::uint32_t>(b, cid);
    tworldsvr::wire::WritePOD<std::uint32_t>(b, key);
    tworldsvr::wire::WriteString(b, name);
    tworldsvr::wire::WritePOD<std::uint8_t>(b, reg);
    tworldsvr::wire::WritePOD<std::uint8_t>(b, /*npc_inven=*/3);
    tworldsvr::wire::WritePOD<std::uint8_t>(b, /*npc_item=*/7);
    return b;
}

std::vector<std::byte> EndBody(std::uint32_t cid, std::uint32_t key)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD<std::uint32_t>(b, cid);
    tworldsvr::wire::WritePOD<std::uint32_t>(b, key);
    return b;
}

struct Search { std::uint32_t char_id = 0, key = 0, soul = 0, region = 0;
                std::uint8_t result = 0, npc_inven = 0, npc_item = 0;
                std::string name; };
Search DecodeSearch(const std::vector<std::byte>& b)
{
    Search s{};
    tworldsvr::wire::Reader r(b);
    r.Read(s.char_id); r.Read(s.key); r.Read(s.result); r.Read(s.soul);
    r.ReadString(s.name); r.Read(s.region); r.Read(s.npc_inven);
    r.Read(s.npc_item);
    return s;
}

struct Reg { std::uint32_t char_id = 0, key = 0, soul = 0, region = 0;
             std::uint8_t result = 0, reg = 0, ni = 0, it = 0;
             std::string name; };
Reg DecodeReg(const std::vector<std::byte>& b)
{
    Reg s{};
    tworldsvr::wire::Reader r(b);
    r.Read(s.char_id); r.Read(s.key); r.Read(s.result); r.Read(s.reg);
    r.Read(s.ni); r.Read(s.it); r.Read(s.soul); r.ReadString(s.name);
    r.Read(s.region);
    return s;
}

struct End { std::uint32_t char_id = 0, key = 0, time = 0;
             std::uint8_t result = 0; };
End DecodeEnd(const std::vector<std::byte>& b)
{
    End s{};
    tworldsvr::wire::Reader r(b);
    r.Read(s.char_id); r.Read(s.key); r.Read(s.result); r.Read(s.time);
    return s;
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

    tworldsvr::WorldServerConfig svr_cfg{};
    svr_cfg.port = 0; svr_cfg.max_connections = 6; svr_cfg.ctx = ctx;
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

    auto reg = [&](tcp::socket& s, std::uint16_t wid) {
        SendFramed(s, ToUint16(MessageId::RW_RELAYSVR_REQ), RelaysvrBody(wid));
        auto [w, _] = ReadFramed(s);
        EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK));
    };
    auto drain = [&](tcp::socket& s) {
        auto [w, _] = ReadFramed(s);
        EXPECT(w == ToUint16(MessageId::MW_RELAYCONNECT_REQ));
    };
    reg(p1, 0x0042);
    reg(p2, 0x0043); drain(p1);
    reg(p3, 0x0044); drain(p1); drain(p2);

    SendFramed(p1, ToUint16(MessageId::MW_ADDCHAR_ACK), AddCharBody(42, 0xA1));
    SendFramed(p2, ToUint16(MessageId::MW_ADDCHAR_ACK), AddCharBody(200, 0xB0));
    SendFramed(p3, ToUint16(MessageId::MW_ADDCHAR_ACK), AddCharBody(400, 0xCA));
    for (int i = 0; i < 100; ++i)
    { if (chars.Find(42) && chars.Find(200) && chars.Find(400)) break;
      std::this_thread::sleep_for(10ms); }
    EXPECT(chars.Find(42) && chars.Find(200) && chars.Find(400));

    auto with = [&](std::uint32_t id, auto fn) {
        auto c = chars.Find(id);
        EXPECT(c != nullptr);
        if (c) { std::lock_guard g(c->lock); fn(*c); }
    };
    chars.Rename(42, "Alice"); chars.Rename(200, "Bob");
    chars.Rename(400, "Carol");
    with(42,  [](tworldsvr::TChar& c) { c.level = 20; c.real_sex = 0;
                                        c.sex = 0; c.region = 11; });
    with(200, [](tworldsvr::TChar& c) { c.level = 22; c.real_sex = 1;
                                        c.sex = 1; c.region = 22; });
    with(400, [](tworldsvr::TChar& c) { c.level = 22; c.real_sex = 0;
                                        c.sex = 0; c.region = 33; });
    auto mate = [&](std::uint32_t id) -> std::uint32_t {
        auto c = chars.Find(id); std::lock_guard g(c->lock);
        return c->soulmate.target;
    };
    auto poll = [&](auto pred) {
        for (int i = 0; i < 200; ++i)
        { if (pred()) return true; std::this_thread::sleep_for(5ms); }
        return pred();
    };
    auto clear = [&] {
        for (std::uint32_t id : {42u, 200u, 400u})
            with(id, [](tworldsvr::TChar& c) { c.soulmate = tworldsvr::TSoulmate{}; });
    };

    // --- SEARCH: Alice matches Bob (Carol filtered by real-sex) -----
    SendFramed(p1, ToUint16(MessageId::MW_SOULMATESEARCH_ACK),
        SearchBody(42, 0xA1, /*min_level=*/255));
    {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_SOULMATESEARCH_REQ));
        auto s = DecodeSearch(b);
        EXPECT(s.char_id == 42); EXPECT(s.result == sm::kSuccess);
        EXPECT(s.soul == 200); EXPECT(s.name == "Bob");
        EXPECT(s.region == 22); EXPECT(s.npc_inven == 3); EXPECT(s.npc_item == 7);
    }
    EXPECT(poll([&] { return mate(42) == 200 && mate(200) == 42; }));
    clear();

    // --- REG preview (bReg=0): success, no pairing ------------------
    SendFramed(p1, ToUint16(MessageId::MW_SOULMATEREG_ACK),
        RegBody(42, 0xA1, "Bob", /*reg=*/0));
    {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_SOULMATEREG_REQ));
        auto s = DecodeReg(b);
        EXPECT(s.result == sm::kSuccess); EXPECT(s.reg == 0);
        EXPECT(s.soul == 200); EXPECT(s.name == "Bob");
    }
    EXPECT(mate(42) == 0 && mate(200) == 0);   // preview made no pairing

    // --- REG register (bReg=1): mutual pairing ----------------------
    SendFramed(p1, ToUint16(MessageId::MW_SOULMATEREG_ACK),
        RegBody(42, 0xA1, "Bob", /*reg=*/1));
    {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_SOULMATEREG_REQ));
        auto s = DecodeReg(b);
        EXPECT(s.result == sm::kSuccess); EXPECT(s.reg == 1);
        EXPECT(s.soul == 200);
    }
    EXPECT(poll([&] { return mate(42) == 200 && mate(200) == 42; }));

    // --- REG cross-country → FAIL -----------------------------------
    with(400, [](tworldsvr::TChar& c) { c.country = 1; });
    SendFramed(p1, ToUint16(MessageId::MW_SOULMATEREG_ACK),
        RegBody(42, 0xA1, "Carol", /*reg=*/1));
    {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_SOULMATEREG_REQ));
        EXPECT(DecodeReg(b).result == sm::kFail);
    }

    // --- END: Alice dissolves (paired with Bob) → both cleared ------
    SendFramed(p1, ToUint16(MessageId::MW_SOULMATEEND_ACK), EndBody(42, 0xA1));
    {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_SOULMATEEND_REQ));
        auto s = DecodeEnd(b);
        EXPECT(s.char_id == 42); EXPECT(s.result == sm::kSuccess);
        EXPECT(s.time > 0);
    }
    EXPECT(poll([&] { return mate(42) == 0 && mate(200) == 0; }));

    // --- END with no soulmate → FAIL --------------------------------
    SendFramed(p3, ToUint16(MessageId::MW_SOULMATEEND_ACK), EndBody(400, 0xCA));
    {
        auto [w, b] = ReadFramed(p3);
        EXPECT(w == ToUint16(MessageId::MW_SOULMATEEND_REQ));
        EXPECT(DecodeEnd(b).result == sm::kFail);
    }

    p1.close(); p2.close(); p3.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_soulmate_handlers "
                    "(6 scenarios)\n");
    else
        std::printf("FAIL test_tworldsvr_asio_soulmate_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
