// Characterization test for CT_SERVICEMONITOR_ACK.
//
// Source of truth: Server/TMapSvr/CSHandler.cpp:27-68
//                  Server/TMapSvr/CSSender.cpp (SendCT_SERVICEMONITOR_REQ)
//
// CT_SERVICEMONITOR_ACK is the per-tick heartbeat from TControlSvr.
// Wire shape:
//   inbound :  DWORD dwTick
//   outbound:  CT_SERVICEMONITOR_REQ {
//     DWORD dwTick,
//     DWORD dwSESSION,     ← m_mapSESSION.size()
//     DWORD dwTUSER,       ← m_mapPLAYER.size()
//     DWORD dwTACTIVEUSER  ← m_mapACTIVEUSER.size()
//   }
//
// Branches in legacy OnCT_SERVICEMONITOR_ACK:
//
//   §1  CSHandler.cpp:31-35  always
//       → reply with CT_SERVICEMONITOR_REQ carrying the four counts
//       → modern: ACTIVE — the three counts collapse to a single
//         live-session number for F2 (matches TLoginSvrAsio shape);
//         F3 splits them out once the player vs. session distinction
//         lands.
//
//   §2  CSHandler.cpp:37-65  every-5s GC sweep
//       → walks m_mapSESSION; closes sessions whose accept-tick or
//         close-tick is >10s stale
//       → modern: PENDING — the F1 pre-auth watchdog + per-session
//         coroutine teardown already covers the leaked-half-open
//         case differently; full ported parity is F3 work.

#include "spec_fixture.h"

using namespace tmapsvr::spec;
using tnetlib::protocol::MessageId;
using tnetlib::protocol::ToUint16;

namespace {

// =====================================================================
// §1  CT_SERVICEMONITOR_ACK → CT_SERVICEMONITOR_REQ with tick + counts
// =====================================================================
void TestMonitorAckEchoesCounts()
{
    std::printf("[§1 monitor ack → REQ {tick, counts...}  "
                "(CSHandler.cpp:31-35)]\n");
    ServerFixture fx(/*accept_all=*/true);

    std::vector<std::byte> body;
    tmapsvr::wire::WritePOD<std::uint32_t>(body, 0xCAFEBABEu);
    const auto r = RunSinglePacket(fx.Port(),
        ToUint16(MessageId::CT_SERVICEMONITOR_ACK), body);

    Check(r.wIds.size() == 1, "exactly one outbound packet");
    if (r.wIds.size() == 1)
    {
        Check(r.wIds[0] == ToUint16(MessageId::CT_SERVICEMONITOR_REQ),
            "reply id == CT_SERVICEMONITOR_REQ");
        // Wire shape: 4 × DWORD = 16 bytes.
        Check(r.bodies[0].size() == 16,
            "reply body is 16 bytes (4 × DWORD)");
        if (r.bodies[0].size() == 16)
        {
            std::uint32_t echoed_tick = 0;
            std::memcpy(&echoed_tick, r.bodies[0].data(), 4);
            Check(echoed_tick == 0xCAFEBABEu,
                "tick echoed back unchanged (sentinel value)");
        }
    }
}

void TestPeriodicGcSweep_PENDING()
{
    Pending("every-5s session GC sweep",
            "CSHandler.cpp:37-65 — pre-auth watchdog covers the F1 case");
}

} // namespace

int main()
{
    std::printf("=== tmapsvr_asio  OnCT_SERVICEMONITOR_ACK characterization spec ===\n");
    std::printf("    Source of truth: Server/TMapSvr/CSHandler.cpp:27-68\n\n");
    try
    {
        TestMonitorAckEchoesCounts();
        TestPeriodicGcSweep_PENDING();
    }
    catch (const std::exception& ex)
    {
        std::printf("  FAIL     unexpected exception: %s\n", ex.what());
        ++g_failed;
    }
    return ResultExitCode();
}
