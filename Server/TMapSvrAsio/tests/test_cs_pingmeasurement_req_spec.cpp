// Characterization test for CS_PINGMEASUREMENT_REQ.
//
// Source of truth: Server/TMapSvr/CSHandler.cpp:19662-19676
//
// Wire shape (body):
//   DWORD dwTick — client's monotonic tick at send time
// Reply:
//   CS_PINGMEASUREMENT_ACK { DWORD dwTick }  ← same value echoed back
//
// Branches:
//   §1  CSHandler.cpp:19665  !pPlayer->m_pMAP
//       → return EC_NOERROR (silent drop, no ack)
//       → modern: ACTIVE — same observable as "before CONNECT"
//
//   §2  CSHandler.cpp:19668-19673  in-game
//       → echo CS_PINGMEASUREMENT_ACK
//       → modern: PENDING — requires F3 IMapState (gate on real
//         m_pMAP/m_bMain rather than the F2 connected proxy).
//
// The F2 implementation honors §1 via state.connected; the in-game
// echo flips on once state.connected is set. So §2 is actually
// covered for the post-handshake case. We test both paths.

#include "spec_fixture.h"

using namespace tmapsvr::spec;
using tnetlib::protocol::MessageId;
using tnetlib::protocol::ToUint16;

namespace {

// §1: client sends PING before CS_CONNECT_REQ ever ran → drop.
void TestPingBeforeConnect()
{
    std::printf("[§1 PING before CONNECT → silent drop  "
                "(CSHandler.cpp:19665)]\n");
    ServerFixture fx(/*accept_all=*/true);

    std::vector<std::byte> body;
    tmapsvr::wire::WritePOD<std::uint32_t>(body, 0x12345678u);
    const auto r = RunSinglePacket(fx.Port(),
        ToUint16(MessageId::CS_PINGMEASUREMENT_REQ), body);

    Check(r.wIds.empty(), "no ACK (m_pMAP gate trips)");
}

// §2: post-CONNECT ping → ACK with same tick.
void TestPingPostConnectEchoesTick()
{
    std::printf("[§2 PING post-CONNECT → ACK echoes tick  "
                "(CSHandler.cpp:19668-19673)]\n");
    ServerFixture fx(/*accept_all=*/true);

    // Combined builder: send CONNECT first, then PING.
    const auto r = RunClient(fx.Port(),
        [](std::shared_ptr<tnetlib::AsioSession> sess)
            -> boost::asio::awaitable<void>
        {
            // CS_CONNECT_REQ body
            std::vector<std::byte> conn;
            tmapsvr::wire::WritePOD<std::uint16_t>(conn, 0x2918);  // version
            tmapsvr::wire::WritePOD<std::uint8_t> (conn, 0);       // channel
            tmapsvr::wire::WritePOD<std::uint32_t>(conn, 1001u);   // uid
            tmapsvr::wire::WritePOD<std::uint32_t>(conn, 2002u);   // cid
            tmapsvr::wire::WritePOD<std::uint32_t>(conn, 0xdeadbeef);
            tmapsvr::wire::WritePOD<std::uint32_t>(conn, 0u);      // ip
            tmapsvr::wire::WritePOD<std::uint16_t>(conn, 0u);      // port
            tmapsvr::wire::WritePOD<std::int64_t> (conn, 0);       // checksum
            co_await sess->SendPacket(
                ToUint16(MessageId::CS_CONNECT_REQ),
                std::span<const std::byte>(conn.data(), conn.size()));

            std::vector<std::byte> ping;
            tmapsvr::wire::WritePOD<std::uint32_t>(ping, 0xCAFEBABEu);
            co_await sess->SendPacket(
                ToUint16(MessageId::CS_PINGMEASUREMENT_REQ),
                std::span<const std::byte>(ping.data(), ping.size()));
        });

    Check(r.wIds.size() == 2,
        "got CS_CONNECT_ACK + CS_PINGMEASUREMENT_ACK back");
    if (r.wIds.size() == 2)
    {
        Check(r.wIds[0] == ToUint16(MessageId::CS_CONNECT_ACK),
            "first reply is CS_CONNECT_ACK");
        Check(r.wIds[1] == ToUint16(MessageId::CS_PINGMEASUREMENT_ACK),
            "second reply is CS_PINGMEASUREMENT_ACK");
        if (r.bodies[1].size() == 4)
        {
            std::uint32_t echoed = 0;
            std::memcpy(&echoed, r.bodies[1].data(), 4);
            Check(echoed == 0xCAFEBABEu, "echoed tick matches request");
        }
    }
}

} // namespace

int main()
{
    std::printf("=== tmapsvr_asio  OnCS_PINGMEASUREMENT_REQ characterization spec ===\n");
    std::printf("    Source of truth: Server/TMapSvr/CSHandler.cpp:19662-19676\n\n");
    try
    {
        TestPingBeforeConnect();
        TestPingPostConnectEchoesTick();
    }
    catch (const std::exception& ex) {
        std::printf("  FAIL     unexpected exception: %s\n", ex.what());
        ++g_failed;
    }
    return ResultExitCode();
}
