// W6-36 wire tests: operator item-state tool.
//
// CT_ITEMSTATE_REQ(id, N x (item_id, init_state)):
//   - applies each row via IItemStateRepository::ChangeState, stopping
//     at the first failure (legacy SSHandler.cpp:10056 per-item break)
//   - broadcasts MW_ITEMSTATE_REQ(id, succeeded-prefix) to every
//     registered map peer — even when zero rows applied
//   - echoes CT_ITEMSTATE_ACK (same payload) to the W6-35 ctrl-svr
//     slot when identified; silently skipped before identification
//   - truncated body → silent drop
//
// The ctrl peer never does the RW_RELAYSVR handshake, which doubles as
// the "broadcast reaches map peers only" check.

#include "../handlers/handlers.h"
#include "../services/char_registry.h"
#include "../services/ctrl_svr_slot.h"
#include "../services/fake_item_state_repository.h"
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

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
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

// CT_ITEMSTATE_REQ body builder.
std::vector<std::byte>
StateBody(std::uint32_t id,
          const std::vector<std::pair<std::uint16_t, std::uint8_t>>& rows)
{
    namespace wire = tworldsvr::wire;
    std::vector<std::byte> b;
    wire::WritePOD<std::uint32_t>(b, id);
    wire::WritePOD<std::uint16_t>(b,
        static_cast<std::uint16_t>(rows.size()));
    for (auto& row : rows)
    {
        wire::WritePOD<std::uint16_t>(b, row.first);
        wire::WritePOD<std::uint8_t>(b, row.second);
    }
    return b;
}

struct StatePacket
{
    std::uint32_t id = 0;
    std::vector<std::pair<std::uint16_t, std::uint8_t>> rows;
};

StatePacket ParseState(const std::vector<std::byte>& body)
{
    namespace wire = tworldsvr::wire;
    wire::Reader r(body);
    StatePacket out{};
    std::uint16_t count = 0;
    r.Read(out.id); r.Read(count);
    out.rows.reserve(count);
    for (std::uint16_t i = 0; i < count; ++i)
    {
        std::uint16_t item = 0;
        std::uint8_t  st   = 0;
        r.Read(item); r.Read(st);
        out.rows.emplace_back(item, st);
    }
    return out;
}

} // namespace

int main()
{
    using boost::asio::ip::tcp;
    using namespace std::chrono_literals;
    using tnetlib::protocol::MessageId;
    using tnetlib::protocol::ToUint16;

    boost::asio::io_context io;
    tworldsvr::CharRegistry            chars;
    tworldsvr::GuildRegistry           guilds;
    tworldsvr::PeerRegistry            peers;
    tworldsvr::CtrlSvrSlot             ctrl_svr;
    tworldsvr::FakeItemStateRepository item_repo;
    item_repo.FailOn(300);

    tworldsvr::HandlerContext ctx{};
    ctx.io = &io; ctx.chars = &chars; ctx.guilds = &guilds;
    ctx.peers = &peers;
    ctx.ctrl_svr = &ctrl_svr;
    ctx.item_state_repo = &item_repo;
    ctx.nation = 0;

    tworldsvr::WorldServerConfig svr_cfg{};
    svr_cfg.port = 0; svr_cfg.max_connections = 4; svr_cfg.ctx = ctx;
    tworldsvr::WorldServer server(io, svr_cfg);
    const std::uint16_t port = server.Port();
    boost::asio::co_spawn(io, server.Run(), boost::asio::detached);
    std::thread io_thread([&io] { io.run(); });
    std::this_thread::sleep_for(20ms);

    boost::asio::io_context client_io;
    tcp::socket p1(client_io), p2(client_io), pc(client_io);
    const auto ep = tcp::endpoint(
        boost::asio::ip::make_address_v4("127.0.0.1"), port);
    p1.connect(ep); p2.connect(ep); pc.connect(ep);
    std::this_thread::sleep_for(20ms);

    // p1 + p2 register as map peers; pc stays an unregistered
    // ctrl-svr socket (no RW_RELAYSVR handshake).
    SendFramed(p1, ToUint16(MessageId::RW_RELAYSVR_REQ),
        RelaysvrBody(0x0042));
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK)); }
    SendFramed(p2, ToUint16(MessageId::RW_RELAYSVR_REQ),
        RelaysvrBody(0x0043));
    { auto [w, _] = ReadFramed(p2);
      EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK)); }
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::MW_RELAYCONNECT_REQ)); }

    // --- W6-36a: all-success batch, ctrl-svr not yet identified ----
    // The MW broadcast reaches both map peers; pc gets nothing (the
    // slot is empty — verified below by W6-36b's ACK being the FIRST
    // frame pc ever reads).
    SendFramed(pc, ToUint16(MessageId::CT_ITEMSTATE_REQ),
        StateBody(/*id=*/1, {{100, 1}, {200, 0}}));
    for (auto* s : {&p1, &p2})
    {
        auto [w, b] = ReadFramed(*s);
        EXPECT(w == ToUint16(MessageId::MW_ITEMSTATE_REQ));
        auto pkt = ParseState(b);
        EXPECT(pkt.id == 1);
        EXPECT(pkt.rows.size() == 2);
        EXPECT(pkt.rows[0].first == 100);
        EXPECT(pkt.rows[0].second == 1);
        EXPECT(pkt.rows[1].first == 200);
        EXPECT(pkt.rows[1].second == 0);
    }

    // --- W6-36b: ctrl-svr identifies; stop-at-first-failure --------
    SendFramed(pc, ToUint16(MessageId::CT_CTRLSVR_REQ), {});
    std::this_thread::sleep_for(20ms);
    EXPECT(ctrl_svr.Get() != nullptr);

    // 150 applies, 300 fails (seeded), 250 must never be attempted.
    SendFramed(pc, ToUint16(MessageId::CT_ITEMSTATE_REQ),
        StateBody(/*id=*/2, {{150, 3}, {300, 7}, {250, 1}}));
    for (auto* s : {&p1, &p2})
    {
        auto [w, b] = ReadFramed(*s);
        EXPECT(w == ToUint16(MessageId::MW_ITEMSTATE_REQ));
        auto pkt = ParseState(b);
        EXPECT(pkt.id == 2);
        EXPECT(pkt.rows.size() == 1);
        EXPECT(pkt.rows[0].first == 150);
        EXPECT(pkt.rows[0].second == 3);
    }
    {
        // First frame pc ever receives — proves W6-36a sent nothing
        // to the unidentified ctrl socket.
        auto [w, b] = ReadFramed(pc);
        EXPECT(w == ToUint16(MessageId::CT_ITEMSTATE_ACK));
        auto pkt = ParseState(b);
        EXPECT(pkt.id == 2);
        EXPECT(pkt.rows.size() == 1);
        EXPECT(pkt.rows[0].first == 150);
        EXPECT(pkt.rows[0].second == 3);
    }
    {
        // Attempt log: 100, 200 (36a), 150, 300 (36b) — never 250.
        const auto calls = item_repo.Calls();
        EXPECT(calls.size() == 4);
        if (calls.size() == 4)
        {
            EXPECT(calls[0].item_id == 100 && calls[0].init_state == 1);
            EXPECT(calls[1].item_id == 200 && calls[1].init_state == 0);
            EXPECT(calls[2].item_id == 150 && calls[2].init_state == 3);
            EXPECT(calls[3].item_id == 300 && calls[3].init_state == 7);
        }
    }

    // --- W6-36c: first row fails → empty-but-present broadcast -----
    // Legacy fans the ACK out unconditionally, so success_count=0
    // still produces both frames.
    SendFramed(pc, ToUint16(MessageId::CT_ITEMSTATE_REQ),
        StateBody(/*id=*/3, {{300, 5}}));
    for (auto* s : {&p1, &p2})
    {
        auto [w, b] = ReadFramed(*s);
        EXPECT(w == ToUint16(MessageId::MW_ITEMSTATE_REQ));
        auto pkt = ParseState(b);
        EXPECT(pkt.id == 3);
        EXPECT(pkt.rows.empty());
    }
    {
        auto [w, b] = ReadFramed(pc);
        EXPECT(w == ToUint16(MessageId::CT_ITEMSTATE_ACK));
        auto pkt = ParseState(b);
        EXPECT(pkt.id == 3);
        EXPECT(pkt.rows.empty());
    }

    // --- W6-36d: truncated row → silent drop ------------------------
    // count=2 but only one row's bytes present. Sentinel: the next
    // well-formed batch is the next frame on every socket.
    {
        namespace wire = tworldsvr::wire;
        std::vector<std::byte> b;
        wire::WritePOD<std::uint32_t>(b, 9);
        wire::WritePOD<std::uint16_t>(b, 2);
        wire::WritePOD<std::uint16_t>(b, 111);
        wire::WritePOD<std::uint8_t>(b, 1);
        SendFramed(pc, ToUint16(MessageId::CT_ITEMSTATE_REQ), b);
    }
    std::this_thread::sleep_for(30ms);
    SendFramed(pc, ToUint16(MessageId::CT_ITEMSTATE_REQ),
        StateBody(/*id=*/4, {{400, 9}}));
    for (auto* s : {&p1, &p2, &pc})
    {
        auto [w, b] = ReadFramed(*s);
        EXPECT(w == ToUint16(s == &pc ? MessageId::CT_ITEMSTATE_ACK
                                      : MessageId::MW_ITEMSTATE_REQ));
        auto pkt = ParseState(b);
        EXPECT(pkt.id == 4);   // sentinel — nothing leaked from id=9
        EXPECT(pkt.rows.size() == 1);
        EXPECT(pkt.rows[0].first == 400);
        EXPECT(pkt.rows[0].second == 9);
    }

    p1.close(); p2.close(); pc.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_itemstate_handlers\n");
    else
        std::printf("FAIL test_tworldsvr_asio_itemstate_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
