// Characterization test for CT_CTRLSVR_REQ.
//
// Source of truth: Server/TMapSvr/CSHandler.cpp:230-233
//
// Wire shape: empty body. CT_CTRLSVR_REQ is the post-dial handshake
// TControlSvr sends to claim "I'm the control peer" — the legacy
// implementation is a pure no-op (return EC_NOERROR) because the
// authentication-by-IP gating happens earlier in Accept().
//
// Branches:
//
//   §1  CSHandler.cpp:232  always → EC_NOERROR (no ack, no state)
//       → modern: ACTIVE

#include "spec_fixture.h"

using namespace tmapsvr::spec;
using tnetlib::protocol::MessageId;
using tnetlib::protocol::ToUint16;

namespace {

void TestPureHeartbeat()
{
    std::printf("[§1 CT_CTRLSVR_REQ → no-op heartbeat  "
                "(CSHandler.cpp:230-233)]\n");
    ServerFixture fx(/*accept_all=*/true);
    const auto r = RunSinglePacket(fx.Port(),
        ToUint16(MessageId::CT_CTRLSVR_REQ), {});
    Check(r.wIds.empty(), "no ACK sent");
    Check(!r.socket_closed_by_peer, "session stays open");
}

} // namespace

int main()
{
    std::printf("=== tmapsvr_asio  OnCT_CTRLSVR_REQ characterization spec ===\n");
    std::printf("    Source of truth: Server/TMapSvr/CSHandler.cpp:230-233\n\n");
    try { TestPureHeartbeat(); }
    catch (const std::exception& ex) {
        std::printf("  FAIL     unexpected exception: %s\n", ex.what());
        ++g_failed;
    }
    return ResultExitCode();
}
