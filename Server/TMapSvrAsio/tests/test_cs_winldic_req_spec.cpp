// Characterization test for CS_WINLDIC_REQ.
//
// Source of truth: Server/TMapSvr/CSHandler.cpp:9-25
//
// The legacy handler body is entirely commented out (the original
// `UsePvPoint` + `CloseSession` logic was a PvP-tied disconnect after
// a normal-kill point deduction; the shipped source has it disabled).
// Only the `return EC_NOERROR` stub remains, meaning the wire id is
// reserved but no observable effect.
//
// Branches:
//
//   §1  CSHandler.cpp:24  always → EC_NOERROR (no ack, session alive)
//       → modern: ACTIVE

#include "spec_fixture.h"

using namespace tmapsvr::spec;
using tnetlib::protocol::MessageId;
using tnetlib::protocol::ToUint16;

namespace {

void TestSilentNoOp()
{
    std::printf("[§1 CS_WINLDIC_REQ → silent no-op  (CSHandler.cpp:9-25)]\n");
    ServerFixture fx(/*accept_all=*/true);
    const auto r = RunSinglePacket(fx.Port(),
        ToUint16(MessageId::CS_WINLDIC_REQ), {});
    Check(r.wIds.empty(),  "no ACK sent");
    Check(!r.socket_closed_by_peer,
        "session stays open (legacy returns EC_NOERROR)");
}

} // namespace

int main()
{
    std::printf("=== tmapsvr_asio  OnCS_WINLDIC_REQ characterization spec ===\n");
    std::printf("    Source of truth: Server/TMapSvr/CSHandler.cpp:9-25\n\n");
    try { TestSilentNoOp(); }
    catch (const std::exception& ex) {
        std::printf("  FAIL     unexpected exception: %s\n", ex.what());
        ++g_failed;
    }
    return ResultExitCode();
}
