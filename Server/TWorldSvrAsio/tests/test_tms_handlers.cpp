// W4-11 wire test: the TMS conference-channel lifecycle over a
// two-peer loopback session.
//
// Alice 42/peer1 0x42, Bob 200/peer2 0x43.
//   1. INVITE      Alice invites Bob → both maps get TMSINVITE_REQ
//                  (roster {Alice,Bob}); the conference is opened.
//   2. SEND        Alice posts "hello" → both get TMSRECV_REQ.
//   3. OUT         Bob leaves → both get TMSOUT_REQ; conf goes solo.
//   4. SEND(solo)  Alice posts "hey" → Bob's map gets a re-pair
//                  TMSINVITEASK_REQ (no message fan-out).
//   5. INVITEASK   Accept → roster {Alice,Bob} restored (TMSINVITE_REQ
//                  to both) then the pending "hey" via TMSRECV_REQ.

#include "../handlers/handlers.h"
#include "../services/char_registry.h"
#include "../services/guild_registry.h"
#include "../services/peer_registry.h"
#include "../services/tms_registry.h"
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

// Parse a MW_TMSINVITE_REQ body: char_id, key, inviter, tms, count,
// then count × {id, name, klass, level}. Returns the roster ids.
struct InviteHdr {
    std::uint32_t char_id = 0, key = 0, inviter = 0, tms = 0;
    std::vector<std::uint32_t> ids;
};
InviteHdr ParseInvite(const std::vector<std::byte>& b)
{
    InviteHdr h;
    tworldsvr::wire::Reader r(b);
    std::uint8_t count = 0;
    r.Read(h.char_id); r.Read(h.key); r.Read(h.inviter); r.Read(h.tms);
    r.Read(count);
    for (std::uint8_t i = 0; i < count; ++i)
    {
        std::uint32_t id = 0; std::string name; std::uint8_t k = 0, l = 0;
        r.Read(id); r.ReadString(name); r.Read(k); r.Read(l);
        h.ids.push_back(id);
    }
    return h;
}

bool HasId(const std::vector<std::uint32_t>& v, std::uint32_t id)
{
    return std::find(v.begin(), v.end(), id) != v.end();
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
    tworldsvr::TmsRegistry    tms;
    tworldsvr::HandlerContext ctx{};
    ctx.io = &io; ctx.chars = &chars; ctx.guilds = &guilds;
    ctx.peers = &peers; ctx.tms = &tms; ctx.nation = 0;

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

    const std::uint32_t kAliceKey = 0xA1, kBobKey = 0xB0;
    SendFramed(p1, ToUint16(MessageId::MW_ADDCHAR_ACK),
               AddCharBody(42, kAliceKey));
    SendFramed(p2, ToUint16(MessageId::MW_ADDCHAR_ACK),
               AddCharBody(200, kBobKey));
    for (int i = 0; i < 100; ++i)
    { if (chars.Find(42) && chars.Find(200)) break;
      std::this_thread::sleep_for(10ms); }
    EXPECT(chars.Find(42) && chars.Find(200));
    chars.Rename(42, "Alice");
    chars.Rename(200, "Bob");

    std::uint32_t tms_id = 0;

    // --- Step 1: Alice invites Bob → conference opened -------------
    {
        std::vector<std::byte> b;
        tworldsvr::wire::WritePOD<std::uint32_t>(b, 42);        // char
        tworldsvr::wire::WritePOD<std::uint32_t>(b, kAliceKey); // key
        tworldsvr::wire::WritePOD<std::uint32_t>(b, 0);         // tms (new)
        tworldsvr::wire::WritePOD<std::uint8_t>(b, 1);          // count
        tworldsvr::wire::WritePOD<std::uint32_t>(b, 200);       // target
        SendFramed(p1, ToUint16(MessageId::MW_TMSINVITE_ACK), b);
    }
    {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_TMSINVITE_REQ));
        auto h = ParseInvite(b);
        EXPECT(h.char_id == 42); EXPECT(h.inviter == 42);
        EXPECT(h.ids.size() == 2);
        EXPECT(HasId(h.ids, 42) && HasId(h.ids, 200));
        tms_id = h.tms;
        EXPECT(tms_id != 0);
    }
    {
        auto [w, b] = ReadFramed(p2);
        EXPECT(w == ToUint16(MessageId::MW_TMSINVITE_REQ));
        auto h = ParseInvite(b);
        EXPECT(h.char_id == 200); EXPECT(h.inviter == 42);
        EXPECT(h.tms == tms_id); EXPECT(h.ids.size() == 2);
    }

    // --- Step 2: Alice posts a message → fan-out to both ----------
    {
        std::vector<std::byte> b;
        tworldsvr::wire::WritePOD<std::uint32_t>(b, 42);
        tworldsvr::wire::WritePOD<std::uint32_t>(b, kAliceKey);
        tworldsvr::wire::WritePOD<std::uint32_t>(b, tms_id);
        tworldsvr::wire::WriteString(b, "hello");
        SendFramed(p1, ToUint16(MessageId::MW_TMSSEND_ACK), b);
    }
    {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_TMSRECV_REQ));
        tworldsvr::wire::Reader r(b);
        std::uint32_t cid = 0, key = 0, t = 0; std::string sender, msg;
        r.Read(cid); r.Read(key); r.Read(t);
        r.ReadString(sender); r.ReadString(msg);
        EXPECT(cid == 42); EXPECT(t == tms_id);
        EXPECT(sender == "Alice"); EXPECT(msg == "hello");
    }
    {
        auto [w, b] = ReadFramed(p2);
        EXPECT(w == ToUint16(MessageId::MW_TMSRECV_REQ));
        tworldsvr::wire::Reader r(b);
        std::uint32_t cid = 0, key = 0, t = 0; std::string sender, msg;
        r.Read(cid); r.Read(key); r.Read(t);
        r.ReadString(sender); r.ReadString(msg);
        EXPECT(cid == 200); EXPECT(msg == "hello");
    }

    // --- Step 3: Bob leaves → both told, conference goes solo -----
    {
        std::vector<std::byte> b;
        tworldsvr::wire::WritePOD<std::uint32_t>(b, 200);
        tworldsvr::wire::WritePOD<std::uint32_t>(b, kBobKey);
        tworldsvr::wire::WritePOD<std::uint32_t>(b, tms_id);
        SendFramed(p2, ToUint16(MessageId::MW_TMSOUT_ACK), b);
    }
    {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_TMSOUT_REQ));
        tworldsvr::wire::Reader r(b);
        std::uint32_t cid = 0, key = 0, t = 0; std::string target;
        r.Read(cid); r.Read(key); r.Read(t); r.ReadString(target);
        EXPECT(cid == 42); EXPECT(t == tms_id); EXPECT(target == "Bob");
    }
    {
        auto [w, b] = ReadFramed(p2);
        EXPECT(w == ToUint16(MessageId::MW_TMSOUT_REQ));
        tworldsvr::wire::Reader r(b);
        std::uint32_t cid = 0, key = 0, t = 0; std::string target;
        r.Read(cid); r.Read(key); r.Read(t); r.ReadString(target);
        EXPECT(cid == 200); EXPECT(target == "Bob");
    }

    // --- Step 4: solo SEND re-pairs the last member (Bob) ---------
    {
        std::vector<std::byte> b;
        tworldsvr::wire::WritePOD<std::uint32_t>(b, 42);
        tworldsvr::wire::WritePOD<std::uint32_t>(b, kAliceKey);
        tworldsvr::wire::WritePOD<std::uint32_t>(b, tms_id);
        tworldsvr::wire::WriteString(b, "hey");
        SendFramed(p1, ToUint16(MessageId::MW_TMSSEND_ACK), b);
    }
    {
        auto [w, b] = ReadFramed(p2);   // re-pair dialog on Bob's map
        EXPECT(w == ToUint16(MessageId::MW_TMSINVITEASK_REQ));
        tworldsvr::wire::Reader r(b);
        std::uint32_t cid = 0, key = 0, tid = 0, tkey = 0, t = 0;
        std::string msg;
        r.Read(cid); r.Read(key); r.Read(tid); r.Read(tkey);
        r.Read(t); r.ReadString(msg);
        EXPECT(cid == 200); EXPECT(tid == 42);
        EXPECT(t == tms_id); EXPECT(msg == "hey");
    }

    // --- Step 5: Bob accepts → roster restored + pending message --
    {
        std::vector<std::byte> b;
        tworldsvr::wire::WritePOD<std::uint32_t>(b, 42);        // member
        tworldsvr::wire::WritePOD<std::uint32_t>(b, kAliceKey);
        tworldsvr::wire::WritePOD<std::uint32_t>(b, 200);       // joiner
        tworldsvr::wire::WritePOD<std::uint32_t>(b, kBobKey);
        tworldsvr::wire::WritePOD<std::uint8_t>(b, 1);          // accept
        tworldsvr::wire::WritePOD<std::uint32_t>(b, tms_id);
        tworldsvr::wire::WriteString(b, "hey");
        SendFramed(p2, ToUint16(MessageId::MW_TMSINVITEASK_ACK), b);
    }
    {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_TMSINVITE_REQ));
        auto h = ParseInvite(b);
        EXPECT(h.tms == tms_id); EXPECT(h.ids.size() == 2);
        EXPECT(HasId(h.ids, 42) && HasId(h.ids, 200));
    }
    {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_TMSRECV_REQ));
        tworldsvr::wire::Reader r(b);
        std::uint32_t cid = 0, key = 0, t = 0; std::string sender, msg;
        r.Read(cid); r.Read(key); r.Read(t);
        r.ReadString(sender); r.ReadString(msg);
        EXPECT(cid == 42); EXPECT(msg == "hey");
    }
    {
        auto [w, b] = ReadFramed(p2);
        EXPECT(w == ToUint16(MessageId::MW_TMSINVITE_REQ));
        auto h = ParseInvite(b);
        EXPECT(h.tms == tms_id); EXPECT(h.ids.size() == 2);
    }
    {
        auto [w, b] = ReadFramed(p2);
        EXPECT(w == ToUint16(MessageId::MW_TMSRECV_REQ));
        tworldsvr::wire::Reader r(b);
        std::uint32_t cid = 0, key = 0, t = 0; std::string sender, msg;
        r.Read(cid); r.Read(key); r.Read(t);
        r.ReadString(sender); r.ReadString(msg);
        EXPECT(cid == 200); EXPECT(msg == "hey");
    }

    p1.close(); p2.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_tms_handlers\n");
    else
        std::printf("FAIL test_tworldsvr_asio_tms_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
