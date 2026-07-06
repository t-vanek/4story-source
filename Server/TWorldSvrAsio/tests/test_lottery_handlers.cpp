// W6-47 wire tests: LOTTERY / GIFTTIME reward runs.
//
//   EVENT_LOTTERY (state=1, map=0, winners=2 over a 2-char pool) →
//     two WPT_LOTITEM mails on the FIRST map peer (title|message
//     split checked), a winner board broadcast on every map (both
//     names, order-independent), NO registry store, NO event
//     broadcast.
//   EVENT_LOTTERY with map_type=1 → only the char inside the
//     tournament band (500..532) can win.
//   EVENT_GIFTTIME → every char inside the [HI,LO] level band gets
//     the first row mailed with use_time=winner; no board.
//   state=0 → completely inert (sentinel).

#include "../handlers/handlers.h"
#include "../services/char_registry.h"
#include "../services/ctrl_svr_slot.h"
#include "../services/event_registry.h"
#include "../services/guild_registry.h"
#include "../services/peer_registry.h"
#include "../wire_codec.h"
#include "../world_server.h"
#include "../world_session.h"

#include "MessageId.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <utility>
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

// CT_EVENTUPDATE_REQ body carrying a full EVENTINFO with lottery rows.
std::vector<std::byte>
EventBody(std::uint8_t blob_id, std::uint8_t state, std::uint16_t value,
          std::uint16_t map_id, const std::string& title,
          const std::string& lot_msg,
          const std::vector<std::tuple<std::uint16_t, std::uint8_t,
                                       std::uint16_t>>& lots)
{
    namespace wire = tworldsvr::wire;
    std::vector<std::byte> b;
    wire::WritePOD<std::uint8_t>(b, blob_id);      // outer event_id
    wire::WritePOD<std::uint16_t>(b, value);       // outer wValue
    wire::WritePOD<std::uint32_t>(b, 900);         // dwIndex
    wire::WritePOD<std::uint8_t>(b, blob_id);      // m_bID
    wire::WritePOD<std::uint8_t>(b, state);        // m_bState
    wire::WritePOD<std::uint8_t>(b, 0);            // group
    wire::WritePOD<std::uint8_t>(b, 0);            // svr_type
    wire::WritePOD<std::uint8_t>(b, 0);            // svr_id
    wire::WritePOD<std::int64_t>(b, 0);            // start
    wire::WritePOD<std::int64_t>(b, 0);            // end
    wire::WritePOD<std::uint16_t>(b, value);       // m_wValue
    wire::WritePOD<std::uint16_t>(b, map_id);      // m_wMapID
    wire::WritePOD<std::uint32_t>(b, 0);           // start_alarm
    wire::WritePOD<std::uint32_t>(b, 0);           // end_alarm
    wire::WriteString(b, "");                      // start_msg
    wire::WriteString(b, "");                      // end_msg
    wire::WriteString(b, title);                   // title
    wire::WritePOD<std::uint8_t>(b, 0);            // part_time
    wire::WriteString(b, lot_msg);                 // lot_msg
    wire::WritePOD<std::uint16_t>(b, 0);           // cash rows
    wire::WritePOD<std::uint8_t>(b, 0);            // mon start
    wire::WritePOD<std::uint8_t>(b, 0);            // mon end
    wire::WritePOD<std::uint16_t>(b, 0);           // spawn rows
    wire::WritePOD<std::uint16_t>(b, 0);           // regen rows
    wire::WritePOD<std::uint16_t>(b,
        static_cast<std::uint16_t>(lots.size()));  // lottery rows
    for (const auto& [item, num, winner] : lots)
    {
        wire::WritePOD<std::uint16_t>(b, item);
        wire::WritePOD<std::uint8_t>(b, num);
        wire::WritePOD<std::uint16_t>(b, winner);
    }
    return b;
}

struct PostMail
{
    std::uint8_t  type = 0;
    std::uint32_t recv_id = 0;
    std::string   recver, title, message;
    std::uint16_t item_id = 0;
    std::uint8_t  num = 0;
    std::uint16_t use_time = 0;
};

PostMail ParseMail(const std::vector<std::byte>& b)
{
    tworldsvr::wire::Reader r(b);
    PostMail m{};
    r.Read(m.type); r.Read(m.recv_id); r.ReadString(m.recver);
    r.ReadString(m.title); r.ReadString(m.message);
    r.Read(m.item_id); r.Read(m.num); r.Read(m.use_time);
    return m;
}

void AddChar(boost::asio::ip::tcp::socket& s, std::uint32_t id,
             std::uint32_t key, std::uint32_t user)
{
    namespace wire = tworldsvr::wire;
    using tnetlib::protocol::MessageId;
    using tnetlib::protocol::ToUint16;
    std::vector<std::byte> b;
    wire::WritePOD<std::uint32_t>(b, id);
    wire::WritePOD<std::uint32_t>(b, key);
    wire::WritePOD<std::uint32_t>(b, 0x7f000001);
    wire::WritePOD<std::uint16_t>(b, 33500);
    wire::WritePOD<std::uint32_t>(b, user);
    SendFramed(s, ToUint16(MessageId::MW_ADDCHAR_ACK), b);
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
    tworldsvr::CharRegistry  chars;
    tworldsvr::GuildRegistry guilds;
    tworldsvr::PeerRegistry  peers;
    tworldsvr::CtrlSvrSlot   ctrl_svr;
    tworldsvr::EventRegistry events;

    tworldsvr::HandlerContext ctx{};
    ctx.io = &io; ctx.chars = &chars; ctx.guilds = &guilds;
    ctx.peers = &peers; ctx.ctrl_svr = &ctrl_svr;
    ctx.events = &events;
    ctx.nation = 0;

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

    // p1 registers FIRST → it is peers.front() → receives the mails.
    SendFramed(p1, ToUint16(MessageId::RW_RELAYSVR_REQ),
        RelaysvrBody(0x0442));
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK)); }
    SendFramed(p2, ToUint16(MessageId::RW_RELAYSVR_REQ),
        RelaysvrBody(0x0443));
    { auto [w, _] = ReadFramed(p2);
      EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK)); }
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::MW_RELAYCONNECT_REQ)); }

    // Alice lvl 50 map 10; Bob lvl 90 map 505 (tournament band).
    AddChar(p1, 200, 0xA1, 500);
    AddChar(p2, 300, 0xB2, 501);
    for (int i = 0; i < 1000 && (!chars.Find(200) || !chars.Find(300));
         ++i)
        std::this_thread::sleep_for(10ms);
    chars.Rename(200, "Alice");
    chars.Rename(300, "Bob");
    if (auto c = chars.Find(200))
    { std::lock_guard g(c->lock); c->level = 50; c->map_id = 10; }
    if (auto c = chars.Find(300))
    { std::lock_guard g(c->lock); c->level = 90; c->map_id = 505; }

    // --- W6-47a: LOTTERY over the whole population ------------------
    SendFramed(p1, ToUint16(MessageId::CT_EVENTUPDATE_REQ),
        EventBody(14, /*state=*/1, /*value=*/7, /*map=*/0,
                  "LotEvent", "Win|Grats", {{1001, 2, 2}}));
    {
        std::set<std::string> mailed;
        for (int k = 0; k < 2; ++k)
        {
            auto [w, b] = ReadFramed(p1);
            EXPECT(w == ToUint16(MessageId::MW_WORLDPOSTSEND_REQ));
            const auto m = ParseMail(b);
            EXPECT(m.type == 4);            // WPT_LOTITEM
            EXPECT(m.title == "Win");
            EXPECT(m.message == "Grats");
            EXPECT(m.item_id == 1001);
            EXPECT(m.num == 2);
            EXPECT(m.use_time == 0);
            mailed.insert(m.recver);
        }
        EXPECT(mailed.count("Alice") == 1);
        EXPECT(mailed.count("Bob") == 1);   // no-repeat draw
    }
    for (auto* s : {&p1, &p2})
    {
        auto [w, b] = ReadFramed(*s);
        EXPECT(w == ToUint16(MessageId::MW_EVENTMSGLOTTERY_REQ));
        wire::Reader r(b);
        std::string title;
        std::uint16_t items = 0;
        r.ReadString(title); r.Read(items);
        EXPECT(title == "LotEvent");
        EXPECT(items == 1);
        std::uint16_t item = 0, users = 0;
        std::uint8_t num = 0;
        r.Read(item); r.Read(num); r.Read(users);
        EXPECT(item == 1001 && num == 2);
        EXPECT(users == 2);
        std::set<std::string> names;
        for (int k = 0; k < 2; ++k)
        {
            std::string n; r.ReadString(n); names.insert(n);
        }
        EXPECT(names.count("Alice") == 1 && names.count("Bob") == 1);
    }
    EXPECT(events.Size() == 0);             // never stored

    // --- W6-47b: LOTTERY with the tournament map filter -------------
    SendFramed(p1, ToUint16(MessageId::CT_EVENTUPDATE_REQ),
        EventBody(14, 1, 7, /*map=*/1, "TnmtLot", "T|M",
                  {{2002, 1, 5}}));
    {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_WORLDPOSTSEND_REQ));
        const auto m = ParseMail(b);
        EXPECT(m.recver == "Bob");          // only 505 qualifies
        EXPECT(m.item_id == 2002);
    }
    for (auto* s : {&p1, &p2})
    {
        auto [w, b] = ReadFramed(*s);
        EXPECT(w == ToUint16(MessageId::MW_EVENTMSGLOTTERY_REQ));
        wire::Reader r(b);
        std::string title; std::uint16_t items = 0;
        r.ReadString(title); r.Read(items);
        EXPECT(items == 1);
        std::uint16_t item = 0, users = 0; std::uint8_t num = 0;
        r.Read(item); r.Read(num); r.Read(users);
        EXPECT(users == 1);
        std::string n; r.ReadString(n);
        EXPECT(n == "Bob");
    }

    // --- W6-47c: GIFTTIME level band --------------------------------
    // value = (60 << 8) | 255 → levels 60..255 → Bob only.
    SendFramed(p2, ToUint16(MessageId::CT_EVENTUPDATE_REQ),
        EventBody(15, 1, static_cast<std::uint16_t>((60 << 8) | 255),
                  0, "Gift", "GT|GM", {{3003, 4, 77}}));
    {
        auto [w, b] = ReadFramed(p1);       // still the FIRST peer
        EXPECT(w == ToUint16(MessageId::MW_WORLDPOSTSEND_REQ));
        const auto m = ParseMail(b);
        EXPECT(m.recver == "Bob");
        EXPECT(m.title == "GT" && m.message == "GM");
        EXPECT(m.item_id == 3003 && m.num == 4);
        EXPECT(m.use_time == 77);           // winner field as use_time
    }

    // --- W6-47d: state=0 is inert (sentinel) ------------------------
    SendFramed(p1, ToUint16(MessageId::CT_EVENTUPDATE_REQ),
        EventBody(14, /*state=*/0, 7, 0, "Off", "A|B", {{4004, 1, 9}}));
    std::this_thread::sleep_for(30ms);
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint8_t>(b, 66);
        SendFramed(p1, ToUint16(MessageId::CT_CASHSHOPSTOP_REQ), b);
    }
    for (auto* s : {&p1, &p2})
    {
        auto [w, b] = ReadFramed(*s);
        EXPECT(w == ToUint16(MessageId::MW_CASHSHOPSTOP_REQ));
        wire::Reader r(b);
        std::uint8_t type = 0; r.Read(type);
        EXPECT(type == 66);
    }

    p1.close(); p2.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_lottery_handlers\n");
    else
        std::printf("FAIL test_tworldsvr_asio_lottery_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
