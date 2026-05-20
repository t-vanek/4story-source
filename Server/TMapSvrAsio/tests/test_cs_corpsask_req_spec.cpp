// Characterization test for CS_CORPSASK_REQ.
//
// Source of truth: Server/TMapSvr/CSHandler.cpp:8092-8103
//
// Wire shape (body): CString strName  (int32-length + CP1252 bytes)
//
// Branches:
//   §1  :8094  !m_pMAP || !m_bMain → silent drop (ACTIVE)
//   §2  :8101  in-game → MW_CORPSASK_ACK(dwID, dwKEY, strName) to World
//       → PENDING — F2b world peer

#include "spec_fixture.h"

using namespace tmapsvr::spec;
using tnetlib::protocol::MessageId;
using tnetlib::protocol::ToUint16;

namespace {

void TestBeforeConnect()
{
    std::printf("[§1 CORPSASK before CONNECT → silent drop  "
                "(CSHandler.cpp:8094)]\n");
    ServerFixture fx(/*accept_all=*/true);
    std::vector<std::byte> body;
    tmapsvr::wire::WriteString(body, "TargetPlayer");
    const auto r = RunSinglePacket(fx.Port(),
        ToUint16(MessageId::CS_CORPSASK_REQ), body);
    Check(r.wIds.empty(), "no ACK (gate trips)");
}

void TestForwardToWorld_PENDING()
{
    Pending("MW_CORPSASK_ACK forward with target name",
            "CSHandler.cpp:8101 — requires F2b world peer");
}

} // namespace

int main()
{
    std::printf("=== tmapsvr_asio  OnCS_CORPSASK_REQ characterization spec ===\n");
    std::printf("    Source of truth: Server/TMapSvr/CSHandler.cpp:8092-8103\n\n");
    try { TestBeforeConnect(); TestForwardToWorld_PENDING(); }
    catch (const std::exception& ex) {
        std::printf("  FAIL     unexpected exception: %s\n", ex.what());
        ++g_failed;
    }
    return ResultExitCode();
}
