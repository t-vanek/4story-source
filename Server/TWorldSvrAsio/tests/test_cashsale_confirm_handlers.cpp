// W6-37 wire tests: cash-sale confirmation barrier + persistence.
//
// MW_CASHITEMSALE_ACK(dw_index, value, ret):
//   - flips the sender's per-peer CashSaleConfirmed flag
//   - all registered peers confirmed → persist every campaign item
//     via ICashSaleRepository (first failure aborts — no broadcast)
//   - success → erase the campaign when value==0, broadcast
//     MW_CASHSHOPSTOP_REQ(0, 0) to every map, echo
//     CT_CASHITEMSALE_ACK(dw_index, value) to the ctrl-svr on the
//     value==0 path only
//   - unknown dw_index after all-confirm → silent skip
//
// The ctrl peer identifies via CT_CTRLSVR_REQ but never does the
// RW_RELAYSVR handshake, so it also proves the barrier + broadcasts
// only involve registered map peers.

#include "../handlers/handlers.h"
#include "../services/cash_item_sale_registry.h"
#include "../services/char_registry.h"
#include "../services/ctrl_svr_slot.h"
#include "../services/fake_cash_sale_repository.h"
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

// CT_CASHITEMSALE_REQ body builder.
std::vector<std::byte>
SaleBody(std::uint32_t dw_index, std::uint16_t value,
         const std::vector<std::pair<std::uint16_t, std::uint8_t>>& items)
{
    namespace wire = tworldsvr::wire;
    std::vector<std::byte> b;
    wire::WritePOD<std::uint32_t>(b, dw_index);
    wire::WritePOD<std::uint16_t>(b, value);
    wire::WritePOD<std::uint16_t>(b,
        static_cast<std::uint16_t>(items.size()));
    for (auto& it : items)
    {
        wire::WritePOD<std::uint16_t>(b, it.first);
        wire::WritePOD<std::uint8_t>(b, it.second);
    }
    return b;
}

// MW_CASHITEMSALE_ACK body builder (map → world confirm).
std::vector<std::byte>
AckBody(std::uint32_t dw_index, std::uint16_t value, std::uint8_t ret)
{
    namespace wire = tworldsvr::wire;
    std::vector<std::byte> b;
    wire::WritePOD<std::uint32_t>(b, dw_index);
    wire::WritePOD<std::uint16_t>(b, value);
    wire::WritePOD<std::uint8_t>(b, ret);
    return b;
}

// Expect the next frame on `s` to be MW_CASHSHOPSTOP_REQ(type, sp).
void ExpectStop(boost::asio::ip::tcp::socket& s,
                std::uint8_t type, std::uint8_t send_player)
{
    using tnetlib::protocol::MessageId;
    using tnetlib::protocol::ToUint16;
    auto [w, b] = ReadFramed(s);
    EXPECT(w == ToUint16(MessageId::MW_CASHSHOPSTOP_REQ));
    tworldsvr::wire::Reader r(b);
    std::uint8_t t = 0xFF, sp = 0xFF;
    r.Read(t); r.Read(sp);
    EXPECT(t == type);
    EXPECT(sp == send_player);
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
    tworldsvr::CharRegistry            chars;
    tworldsvr::GuildRegistry           guilds;
    tworldsvr::PeerRegistry            peers;
    tworldsvr::CashItemSaleRegistry    cash_sales;
    tworldsvr::CtrlSvrSlot             ctrl_svr;
    tworldsvr::FakeCashSaleRepository  sale_repo;
    sale_repo.FailOn(666);

    tworldsvr::HandlerContext ctx{};
    ctx.io = &io; ctx.chars = &chars; ctx.guilds = &guilds;
    ctx.peers = &peers;
    ctx.cash_sales = &cash_sales;
    ctx.ctrl_svr = &ctrl_svr;
    ctx.cash_sale_repo = &sale_repo;
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

    // p1 + p2 register as map peers; pc identifies as ctrl-svr only.
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
    SendFramed(pc, ToUint16(MessageId::CT_CTRLSVR_REQ), {});
    std::this_thread::sleep_for(20ms);
    EXPECT(ctrl_svr.Get() != nullptr);

    // --- W6-37a: activate, barrier completes → persist + stop ------
    SendFramed(pc, ToUint16(MessageId::CT_CASHITEMSALE_REQ),
        SaleBody(/*dw_index=*/100, /*value=*/50, {{1001, 25}, {1002, 50}}));
    for (auto* s : {&p1, &p2})
    {   // drain the campaign broadcast
        auto [w, _] = ReadFramed(*s);
        EXPECT(w == ToUint16(MessageId::MW_CASHITEMSALE_REQ));
    }

    // First confirm only — barrier incomplete, no persist yet.
    SendFramed(p1, ToUint16(MessageId::MW_CASHITEMSALE_ACK),
        AckBody(100, 50, 1));
    std::this_thread::sleep_for(30ms);
    EXPECT(sale_repo.Calls().empty());

    // Second confirm — barrier complete: persist + stop broadcast.
    SendFramed(p2, ToUint16(MessageId::MW_CASHITEMSALE_ACK),
        AckBody(100, 50, 1));
    for (auto* s : {&p1, &p2})
        ExpectStop(*s, /*type=*/0, /*send_player=*/0);
    {
        const auto calls = sale_repo.Calls();
        EXPECT(calls.size() == 2);
        if (calls.size() == 2)
        {
            EXPECT(calls[0].first == 1001 && calls[0].second == 25);
            EXPECT(calls[1].first == 1002 && calls[1].second == 50);
        }
    }
    EXPECT(cash_sales.Size() == 1);   // value!=0 → campaign stays

    // --- W6-37b: deactivate → erase + stop + ctrl echo -------------
    SendFramed(pc, ToUint16(MessageId::CT_CASHITEMSALE_REQ),
        SaleBody(/*dw_index=*/100, /*value=*/0, {}));
    for (auto* s : {&p1, &p2})
    {   // drain the zeroed re-broadcast
        auto [w, _] = ReadFramed(*s);
        EXPECT(w == ToUint16(MessageId::MW_CASHITEMSALE_REQ));
    }
    SendFramed(p1, ToUint16(MessageId::MW_CASHITEMSALE_ACK),
        AckBody(100, 0, 1));
    SendFramed(p2, ToUint16(MessageId::MW_CASHITEMSALE_ACK),
        AckBody(100, 0, 1));
    for (auto* s : {&p1, &p2})
        ExpectStop(*s, 0, 0);
    {   // ctrl-svr gets the deactivate echo — its FIRST frame ever
        // (proves W6-37a's value!=0 path skipped the ctrl echo).
        auto [w, b] = ReadFramed(pc);
        EXPECT(w == ToUint16(MessageId::CT_CASHITEMSALE_ACK));
        wire::Reader r(b);
        std::uint32_t idx = 0; std::uint16_t val = 0xFFFF;
        r.Read(idx); r.Read(val);
        EXPECT(idx == 100);
        EXPECT(val == 0);
    }
    EXPECT(cash_sales.Size() == 0);   // deactivated row erased
    {   // zeroed values persisted
        const auto calls = sale_repo.Calls();
        EXPECT(calls.size() == 4);
        if (calls.size() == 4)
        {
            EXPECT(calls[2].first == 1001 && calls[2].second == 0);
            EXPECT(calls[3].first == 1002 && calls[3].second == 0);
        }
    }

    // --- W6-37c: persist failure → no stop, no erase, no echo ------
    SendFramed(pc, ToUint16(MessageId::CT_CASHITEMSALE_REQ),
        SaleBody(/*dw_index=*/200, /*value=*/10,
            {{2001, 10}, {666, 20}, {2002, 30}}));
    for (auto* s : {&p1, &p2})
    {
        auto [w, _] = ReadFramed(*s);
        EXPECT(w == ToUint16(MessageId::MW_CASHITEMSALE_REQ));
    }
    SendFramed(p1, ToUint16(MessageId::MW_CASHITEMSALE_ACK),
        AckBody(200, 10, 1));
    SendFramed(p2, ToUint16(MessageId::MW_CASHITEMSALE_ACK),
        AckBody(200, 10, 1));
    std::this_thread::sleep_for(30ms);
    {   // 2001 then 666 attempted; 2002 never (first-failure break)
        const auto calls = sale_repo.Calls();
        EXPECT(calls.size() == 6);
        if (calls.size() == 6)
        {
            EXPECT(calls[4].first == 2001 && calls[4].second == 10);
            EXPECT(calls[5].first == 666  && calls[5].second == 20);
        }
    }
    EXPECT(cash_sales.Size() == 1);   // failed persist → row stays

    // --- W6-37d: unknown campaign after all-confirm → silent -------
    SendFramed(p1, ToUint16(MessageId::MW_CASHITEMSALE_ACK),
        AckBody(999, 0, 1));
    std::this_thread::sleep_for(30ms);
    EXPECT(sale_repo.Calls().size() == 6);   // nothing persisted

    // Sentinel for c+d: an operator CASHSHOPSTOP must be the next
    // frame both maps see (no stray stop/echo leaked from c or d).
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint8_t>(b, 77);
        SendFramed(pc, ToUint16(MessageId::CT_CASHSHOPSTOP_REQ), b);
    }
    for (auto* s : {&p1, &p2})
        ExpectStop(*s, 77, 1);   // operator path always send_player=1

    p1.close(); p2.close(); pc.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_cashsale_confirm_handlers\n");
    else
        std::printf("FAIL test_tworldsvr_asio_cashsale_confirm_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
