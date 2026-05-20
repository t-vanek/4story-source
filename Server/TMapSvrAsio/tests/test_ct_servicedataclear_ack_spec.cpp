// Characterization test for CT_SERVICEDATACLEAR_ACK.
//
// Source of truth: Server/TMapSvr/CSHandler.cpp:214-228
//
// Wire shape: empty body. CT_SERVICEDATACLEAR_ACK is TControlSvr's
// "rebuild the active-user index from the live player map" command —
// legacy clears `m_mapACTIVEUSER` and re-derives it from each
// `m_mapPLAYER` entry's `m_dwUserID`.
//
// Branches:
//
//   §1  CSHandler.cpp:216-225  always
//       → clear m_mapACTIVEUSER, repopulate from m_mapPLAYER
//       → modern: PENDING — `m_mapACTIVEUSER` and `m_mapPLAYER` are
//         F3 infrastructure. Until F3 lands the modern session
//         registry tracks one count (live sessions); there's no
//         derived index that could drift.
//
// Wire-observable behavior: no ACK back, session stays alive. Modern
// preserves the same shape — handler logs the call (so operators can
// see the rebuild request) and returns. F3 ports the actual
// rebuild semantics.

#include "spec_fixture.h"

using namespace tmapsvr::spec;
using tnetlib::protocol::MessageId;
using tnetlib::protocol::ToUint16;

namespace {

void TestRebuildRequestNoAck()
{
    std::printf("[§1 CT_SERVICEDATACLEAR_ACK → log + no ack  "
                "(CSHandler.cpp:214-228)]\n");
    ServerFixture fx(/*accept_all=*/true);
    const auto r = RunSinglePacket(fx.Port(),
        ToUint16(MessageId::CT_SERVICEDATACLEAR_ACK), {});
    Check(r.wIds.empty(),
        "no ACK back to TControlSvr (legacy returns EC_NOERROR)");
    Check(!r.socket_closed_by_peer,
        "control-peer session stays open after rebuild request");
}

void TestActiveUserRebuild_PENDING()
{
    Pending("clear + rebuild m_mapACTIVEUSER from m_mapPLAYER",
            "CSHandler.cpp:216-225 — requires F3 map-wide player registry");
}

} // namespace

int main()
{
    std::printf("=== tmapsvr_asio  OnCT_SERVICEDATACLEAR_ACK characterization spec ===\n");
    std::printf("    Source of truth: Server/TMapSvr/CSHandler.cpp:214-228\n\n");
    try
    {
        TestRebuildRequestNoAck();
        TestActiveUserRebuild_PENDING();
    }
    catch (const std::exception& ex) {
        std::printf("  FAIL     unexpected exception: %s\n", ex.what());
        ++g_failed;
    }
    return ResultExitCode();
}
