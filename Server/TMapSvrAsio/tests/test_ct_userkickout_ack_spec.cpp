// Characterization test for CT_USERKICKOUT_ACK.
//
// Source of truth: Server/TMapSvr/CSHandler.cpp:92-110
//
// Wire shape (body):
//   length-prefixed string strUser (CP1252)
//
// Branches:
//
//   §1  CSHandler.cpp:94-103  always
//       → read strUser, upper-case it, log "TCONTROL - FUNCTION DEBUG"
//       → modern: ACTIVE (read + log)
//
//   §2  CSHandler.cpp:105-107  strUser ∈ m_mapPLAYERNAME
//       → CloseSession(target_player) — the GM's kick lands
//       → modern: PENDING — requires F3 by-name player registry
//
// Wire-observable from a single CT_USERKICKOUT_ACK with no players
// in-world: log emitted, no ACK back, session stays alive. That's
// the F2 baseline.

#include "spec_fixture.h"

using namespace tmapsvr::spec;
using tnetlib::protocol::MessageId;
using tnetlib::protocol::ToUint16;

namespace {

std::vector<std::byte> LengthPrefixedString(const std::string& s)
{
    std::vector<std::byte> body;
    tmapsvr::wire::WriteString(body, s);
    return body;
}

void TestKickoutReadNoAck()
{
    std::printf("[§1 CT_USERKICKOUT_ACK → read + log, no ack  "
                "(CSHandler.cpp:94-103)]\n");
    ServerFixture fx(/*accept_all=*/true);

    const auto body = LengthPrefixedString("CheaterAlice");
    const auto r = RunSinglePacket(fx.Port(),
        ToUint16(MessageId::CT_USERKICKOUT_ACK), body);

    Check(r.wIds.empty(),
        "no ACK back to control peer (legacy returns EC_NOERROR)");
    Check(!r.socket_closed_by_peer,
        "control-peer session stays open");
}

void TestKickoutTargetFound_PENDING()
{
    Pending("named-target lookup + CloseSession",
            "CSHandler.cpp:105-107 — requires F3 by-name player index");
}

} // namespace

int main()
{
    std::printf("=== tmapsvr_asio  OnCT_USERKICKOUT_ACK characterization spec ===\n");
    std::printf("    Source of truth: Server/TMapSvr/CSHandler.cpp:92-110\n\n");
    try
    {
        TestKickoutReadNoAck();
        TestKickoutTargetFound_PENDING();
    }
    catch (const std::exception& ex) {
        std::printf("  FAIL     unexpected exception: %s\n", ex.what());
        ++g_failed;
    }
    return ResultExitCode();
}
