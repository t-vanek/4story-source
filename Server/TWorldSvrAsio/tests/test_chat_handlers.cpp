// W4-5 wire test: MW_CHAT_ACK channel relay over a three-peer
// loopback session.
//
// Alice 42/peer1 0x42 (sender), Bob 200/peer2 0x43, Carol 400/peer3
// 0x44 (war-country 1 via aid_country). Guild 7 = {42,200}; party
// 10 = {42,400}.
//
// Scenarios: GUILD → both guild members; PARTY → both party members;
// WORLD → every peer (char_id 0); WHISPER → recipient + sender echo;
// WHISPER across war-countries → blocked (proven by a follow-up
// WORLD reaching everyone).

#include "../handlers/handlers.h"
#include "../services/char_registry.h"
#include "../services/chat_constants.h"
#include "../services/corps_registry.h"
#include "../services/guild_registry.h"
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
#include <string>
#include <thread>
#include <vector>

namespace chat = tworldsvr::chat;

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

std::vector<std::byte> ChatBody(std::uint32_t sender, std::uint32_t key,
                                 const std::string& sname, std::uint8_t group,
                                 std::uint32_t target, const std::string& name,
                                 const std::string& talk)
{
    std::vector<std::byte> b;
    using namespace tworldsvr::wire;
    WritePOD<std::uint8_t>(b, /*channel=*/1);
    WritePOD<std::uint32_t>(b, sender);
    WritePOD<std::uint32_t>(b, key);
    WriteString(b, sname);
    WritePOD<std::uint8_t>(b, /*type=*/chat::kTypeNormal);
    WritePOD<std::uint8_t>(b, group);
    WritePOD<std::uint32_t>(b, target);
    WriteString(b, name);
    WriteString(b, talk);
    return b;
}

struct ChatReq {
    std::uint32_t char_id = 0, key = 0, sender = 0, target = 0;
    std::uint8_t  channel = 0, country = 0, war = 0, type = 0, group = 0;
    std::string   sender_name, talk;
};
ChatReq DecodeChat(const std::vector<std::byte>& b)
{
    ChatReq c{};
    tworldsvr::wire::Reader r(b);
    r.Read(c.char_id); r.Read(c.key); r.Read(c.channel); r.Read(c.sender);
    r.ReadString(c.sender_name); r.Read(c.country); r.Read(c.war);
    r.Read(c.type); r.Read(c.group); r.Read(c.target); r.ReadString(c.talk);
    return c;
}

ChatReq ReadChat(boost::asio::ip::tcp::socket& s)
{
    auto [w, b] = ReadFramed(s);
    EXPECT(w == tnetlib::protocol::ToUint16(
        tnetlib::protocol::MessageId::MW_CHAT_REQ));
    return DecodeChat(b);
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
    tworldsvr::PartyRegistry  parties;
    tworldsvr::CorpsRegistry  corps_reg;
    tworldsvr::PeerRegistry   peers;
    tworldsvr::HandlerContext ctx{};
    ctx.io = &io; ctx.chars = &chars; ctx.guilds = &guilds;
    ctx.parties = &parties; ctx.corps = &corps_reg; ctx.peers = &peers;
    ctx.nation = 0;

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
    chars.Rename(42, "Alice"); chars.Rename(200, "Bob");
    chars.Rename(400, "Carol");
    // Carol's war-country differs (aid_country=1) for the whisper gate.
    { auto c = chars.Find(400); std::lock_guard g(c->lock);
      c->aid_country = 1; }

    {
        auto gd = std::make_shared<tworldsvr::TGuild>();
        gd->id = 7;
        tworldsvr::TGuildMember m1; m1.char_id = 42;  gd->members.push_back(m1);
        tworldsvr::TGuildMember m2; m2.char_id = 200; gd->members.push_back(m2);
        EXPECT(guilds.Insert(gd));
    }
    {
        auto pt = std::make_shared<tworldsvr::TParty>();
        pt->id = 10; pt->chief_char_id = 42;
        pt->AddMember(42); pt->AddMember(400);
        EXPECT(parties.Insert(pt));
    }

    // --- GUILD → both guild members ---------------------------------
    SendFramed(p1, ToUint16(MessageId::MW_CHAT_ACK),
        ChatBody(42, 0xA1, "Alice", chat::kGuild, 7, "", "hi guild"));
    { auto c = ReadChat(p1); EXPECT(c.char_id == 42); EXPECT(c.group == chat::kGuild);
      EXPECT(c.sender == 42); EXPECT(c.talk == "hi guild"); }
    { auto c = ReadChat(p2); EXPECT(c.char_id == 200);
      EXPECT(c.talk == "hi guild"); }

    // --- PARTY → both party members ---------------------------------
    SendFramed(p1, ToUint16(MessageId::MW_CHAT_ACK),
        ChatBody(42, 0xA1, "Alice", chat::kParty, 10, "", "hi party"));
    { auto c = ReadChat(p1); EXPECT(c.char_id == 42); EXPECT(c.group == chat::kParty); }
    { auto c = ReadChat(p3); EXPECT(c.char_id == 400); EXPECT(c.talk == "hi party"); }

    // --- WORLD → every peer (char_id 0) -----------------------------
    SendFramed(p1, ToUint16(MessageId::MW_CHAT_ACK),
        ChatBody(42, 0xA1, "Alice", chat::kWorld, 0, "", "hello world"));
    for (auto* s : {&p1, &p2, &p3})
    { auto c = ReadChat(*s); EXPECT(c.char_id == 0);
      EXPECT(c.group == chat::kWorld); EXPECT(c.sender == 42);
      EXPECT(c.talk == "hello world"); }

    // --- WHISPER to Bob → recipient + sender echo -------------------
    SendFramed(p1, ToUint16(MessageId::MW_CHAT_ACK),
        ChatBody(42, 0xA1, "Alice", chat::kWhisper, 200, "Bob", "psst"));
    { auto c = ReadChat(p2); EXPECT(c.char_id == 200); EXPECT(c.sender == 42);
      EXPECT(c.sender_name == "Alice"); EXPECT(c.talk == "psst"); }
    { auto c = ReadChat(p1); EXPECT(c.char_id == 42); EXPECT(c.sender == 42);
      EXPECT(c.sender_name == "Bob"); }   // echo names the recipient

    // --- WHISPER across war-countries → blocked ---------------------
    SendFramed(p1, ToUint16(MessageId::MW_CHAT_ACK),
        ChatBody(42, 0xA1, "Alice", chat::kWhisper, 400, "Carol", "enemy"));
    // (no packet) — verify via a follow-up WORLD reaching everyone.
    SendFramed(p1, ToUint16(MessageId::MW_CHAT_ACK),
        ChatBody(42, 0xA1, "Alice", chat::kWorld, 0, "", "after"));
    { auto c = ReadChat(p1); EXPECT(c.group == chat::kWorld);
      EXPECT(c.talk == "after"); }   // not the blocked whisper echo
    { auto c = ReadChat(p2); EXPECT(c.talk == "after"); }
    { auto c = ReadChat(p3); EXPECT(c.talk == "after"); }

    p1.close(); p2.close(); p3.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_chat_handlers (5 channels)\n");
    else
        std::printf("FAIL test_tworldsvr_asio_chat_handlers (%d failure%s)\n",
            g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
