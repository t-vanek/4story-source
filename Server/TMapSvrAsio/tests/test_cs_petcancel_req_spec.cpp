// Characterization test for CS_PETCANCEL_REQ.
//
// Source of truth: Server/TMapSvr/CSHandler.cpp:15443-15454
//
// Wire shape: empty body. Cancels an outstanding pet recall.
//
// Branches:
//   §1  :15446  FindRecallPet() → non-null
//       → MW_RECALLMONDEL_ACK(dwID, dwKEY, recall->dwID) to World
//       → PENDING — F2b world peer + F3 pet state
//   §2  :15448  FindRecallPet() → null
//       → CS_PETRECALL_ACK(PET_FAIL) to client
//       → PENDING — same dependencies as §1
//
// F2 trip-wire: legacy doesn't gate the function on m_pMAP, but
// FindRecallPet() returns null on un-initialized sessions — so the
// observable for pre-CONNECT is "CS_PETRECALL_ACK(PET_FAIL) comes
// back". Modern F2 doesn't have FindRecallPet() yet; we silent-drop
// to match the safer no-ack shape until F3 ships pet state.

#include "spec_fixture.h"

using namespace tmapsvr::spec;
using tnetlib::protocol::MessageId;
using tnetlib::protocol::ToUint16;

namespace {

void TestBeforeConnect()
{
    std::printf("[F2 baseline PETCANCEL before CONNECT → silent drop  "
                "(diverges from legacy PET_FAIL ack — F3 will close gap)]\n");
    ServerFixture fx(/*accept_all=*/true);
    const auto r = RunSinglePacket(fx.Port(),
        ToUint16(MessageId::CS_PETCANCEL_REQ), {});
    Check(r.wIds.empty(),
        "F2 sends no ACK (legacy would send PET_FAIL — recorded as "
        "MODERN-MISMATCH for F3)");
}

void TestRecallPetFound_PENDING()
{
    Pending("active recall → MW_RECALLMONDEL_ACK to World",
            "CSHandler.cpp:15446 — F2b world peer + F3 pet state");
}

void TestNoRecallPet_PENDING()
{
    Pending("no active recall → CS_PETRECALL_ACK(PET_FAIL)",
            "CSHandler.cpp:15448 — needs F3 pet state to enumerate recall list");
}

} // namespace

int main()
{
    std::printf("=== tmapsvr_asio  OnCS_PETCANCEL_REQ characterization spec ===\n");
    std::printf("    Source of truth: Server/TMapSvr/CSHandler.cpp:15443-15454\n\n");
    try
    {
        TestBeforeConnect();
        TestRecallPetFound_PENDING();
        TestNoRecallPet_PENDING();
    }
    catch (const std::exception& ex) {
        std::printf("  FAIL     unexpected exception: %s\n", ex.what());
        ++g_failed;
    }
    return ResultExitCode();
}
