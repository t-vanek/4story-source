// Characterization test for CS_VERIFYSESSION_ACK.
//
// Source of truth: Server/TMapSvr/CSHandler.cpp:19649-19660
//
// Wire shape (body):
//   DWORD dwCharID — client echoes back the char id from the server's
//                     verify probe; legacy ignores the value
//                     server-side, only logs "Verified".
//
// Branches:
//   §1  CSHandler.cpp:19657-19658  always
//       → log "Verified", set m_bCheckedSession = TRUE
//       → modern: ACTIVE (state.session_verified)

#include "spec_fixture.h"

using namespace tmapsvr::spec;
using tnetlib::protocol::MessageId;
using tnetlib::protocol::ToUint16;

namespace {

void TestVerifySessionFlipsFlag()
{
    std::printf("[§1 CS_VERIFYSESSION_ACK → flag flips, no ack  "
                "(CSHandler.cpp:19657-19658)]\n");
    ServerFixture fx(/*accept_all=*/true);

    std::vector<std::byte> body;
    tmapsvr::wire::WritePOD<std::uint32_t>(body, 2002u);  // dwCharID, ignored
    const auto r = RunSinglePacket(fx.Port(),
        ToUint16(MessageId::CS_VERIFYSESSION_ACK), body);

    Check(r.wIds.empty(), "no ACK back (legacy returns EC_NOERROR)");
    Check(!r.socket_closed_by_peer, "session stays open");
    // The flag flip is per-session state — not wire-observable until
    // a GM-protected handler gates on it. F2 captures it for parity.
}

} // namespace

int main()
{
    std::printf("=== tmapsvr_asio  OnCS_VERIFYSESSION_ACK characterization spec ===\n");
    std::printf("    Source of truth: Server/TMapSvr/CSHandler.cpp:19649-19660\n\n");
    try { TestVerifySessionFlipsFlag(); }
    catch (const std::exception& ex) {
        std::printf("  FAIL     unexpected exception: %s\n", ex.what());
        ++g_failed;
    }
    return ResultExitCode();
}
