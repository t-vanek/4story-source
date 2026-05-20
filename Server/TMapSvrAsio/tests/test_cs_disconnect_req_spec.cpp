// Characterization test for CS_DISCONNECT_REQ.
//
// Source of truth: Server/TMapSvr/CSHandler.cpp:12138-12143
//
// Wire shape: empty body. Client-initiated graceful close.
//
// Branches:
//   §1  CSHandler.cpp:12141  always → CloseSession(player) + EC_NOERROR
//       → modern: ACTIVE (sess->Close())

#include "spec_fixture.h"

using namespace tmapsvr::spec;
using tnetlib::protocol::MessageId;
using tnetlib::protocol::ToUint16;

namespace {

void TestDisconnectClosesSession()
{
    std::printf("[§1 CS_DISCONNECT_REQ → server closes session  "
                "(CSHandler.cpp:12138-12143)]\n");
    ServerFixture fx(/*accept_all=*/true);
    const auto r = RunSinglePacket(fx.Port(),
        ToUint16(MessageId::CS_DISCONNECT_REQ), {});
    Check(r.wIds.empty(), "no ACK sent");
    Check(r.socket_closed_by_peer,
        "server-side close fires (legacy CloseSession)");
}

} // namespace

int main()
{
    std::printf("=== tmapsvr_asio  OnCS_DISCONNECT_REQ characterization spec ===\n");
    std::printf("    Source of truth: Server/TMapSvr/CSHandler.cpp:12138-12143\n\n");
    try { TestDisconnectClosesSession(); }
    catch (const std::exception& ex) {
        std::printf("  FAIL     unexpected exception: %s\n", ex.what());
        ++g_failed;
    }
    return ResultExitCode();
}
