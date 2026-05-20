// Characterization test for CS_REGISTERBOW_REQ.
//
// Source of truth: Server/TMapSvr/CSHandler.cpp:19678-19689
//
// Wire shape: empty body. BoW (Battle of Worlds) queue register.
//
// Branches:
//   §1  CSHandler.cpp:19681  !m_pMAP || !m_bMain || IsTournamentPlayer
//       → silent drop (no ack)
//       → modern: ACTIVE
//
//   §2  CSHandler.cpp:19684-19686  in-game
//       → SendMW_ADDTOBOWQUEUE_REQ(dwID, dwKEY) to World
//       → modern: PENDING — requires F2b outbound World peer

#include "spec_fixture.h"

using namespace tmapsvr::spec;
using tnetlib::protocol::MessageId;
using tnetlib::protocol::ToUint16;

namespace {

void TestRegisterBowBeforeConnect()
{
    std::printf("[§1 REGISTERBOW before CONNECT → silent drop  "
                "(CSHandler.cpp:19681)]\n");
    ServerFixture fx(/*accept_all=*/true);
    const auto r = RunSinglePacket(fx.Port(),
        ToUint16(MessageId::CS_REGISTERBOW_REQ), {});
    Check(r.wIds.empty(), "no ACK (gate trips)");
    Check(!r.socket_closed_by_peer, "session stays open");
}

void TestRegisterBowForwardsToWorld_PENDING()
{
    Pending("MW_ADDTOBOWQUEUE_REQ forward",
            "CSHandler.cpp:19684-19686 — requires F2b world peer");
}

} // namespace

int main()
{
    std::printf("=== tmapsvr_asio  OnCS_REGISTERBOW_REQ characterization spec ===\n");
    std::printf("    Source of truth: Server/TMapSvr/CSHandler.cpp:19678-19689\n\n");
    try
    {
        TestRegisterBowBeforeConnect();
        TestRegisterBowForwardsToWorld_PENDING();
    }
    catch (const std::exception& ex) {
        std::printf("  FAIL     unexpected exception: %s\n", ex.what());
        ++g_failed;
    }
    return ResultExitCode();
}
