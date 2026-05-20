// Characterization test for CS_CANCELBOWQUEUE_REQ.
//
// Source of truth: Server/TMapSvr/CSHandler.cpp:19691-19702
//
// Wire shape: empty body. Twin of CS_REGISTERBOW_REQ — pulls the
// player back out of the BoW match queue.
//
// Branches:
//   §1  CSHandler.cpp:19694  !m_pMAP || !m_bMain → silent drop
//       → modern: ACTIVE
//
//   §2  CSHandler.cpp:19697-19699  in-game
//       → SendMW_CANCELBOWQUEUE_REQ(dwID, dwKEY) to World
//       → modern: PENDING — requires F2b world peer

#include "spec_fixture.h"

using namespace tmapsvr::spec;
using tnetlib::protocol::MessageId;
using tnetlib::protocol::ToUint16;

namespace {

void TestCancelBowBeforeConnect()
{
    std::printf("[§1 CANCELBOWQUEUE before CONNECT → silent drop  "
                "(CSHandler.cpp:19694)]\n");
    ServerFixture fx(/*accept_all=*/true);
    const auto r = RunSinglePacket(fx.Port(),
        ToUint16(MessageId::CS_CANCELBOWQUEUE_REQ), {});
    Check(r.wIds.empty(), "no ACK (gate trips)");
    Check(!r.socket_closed_by_peer, "session stays open");
}

void TestCancelBowForwardsToWorld_PENDING()
{
    Pending("MW_CANCELBOWQUEUE_REQ forward",
            "CSHandler.cpp:19697-19699 — requires F2b world peer");
}

} // namespace

int main()
{
    std::printf("=== tmapsvr_asio  OnCS_CANCELBOWQUEUE_REQ characterization spec ===\n");
    std::printf("    Source of truth: Server/TMapSvr/CSHandler.cpp:19691-19702\n\n");
    try
    {
        TestCancelBowBeforeConnect();
        TestCancelBowForwardsToWorld_PENDING();
    }
    catch (const std::exception& ex) {
        std::printf("  FAIL     unexpected exception: %s\n", ex.what());
        ++g_failed;
    }
    return ResultExitCode();
}
