// Characterization test for CS_CHGCHANNEL_REQ.
//
// Source of truth: Server/TMapSvr/CSHandler.cpp:12145-?
//
// Wire shape (body): BYTE bChannel — target channel id
//
// Branches:
//   §1  :12150  !m_pMAP || !m_bMain → silent drop (ACTIVE)
//   §2  :12156  bChannel == m_bChannel → no-op (no ack, no broadcast)
//       → ACTIVE once §1 falls through (covered by post-CONNECT path).
//   §3  in-game cross-channel switch → MW round-trip + AOI cleanup
//       → PENDING — requires F2b World peer + F3 cell grid

#include "spec_fixture.h"

using namespace tmapsvr::spec;
using tnetlib::protocol::MessageId;
using tnetlib::protocol::ToUint16;

namespace {

void TestBeforeConnect()
{
    std::printf("[§1 CHGCHANNEL before CONNECT → silent drop  "
                "(CSHandler.cpp:12150)]\n");
    ServerFixture fx(/*accept_all=*/true);
    std::vector<std::byte> body;
    tmapsvr::wire::WritePOD<std::uint8_t>(body, 2);  // target channel
    const auto r = RunSinglePacket(fx.Port(),
        ToUint16(MessageId::CS_CHGCHANNEL_REQ), body);
    Check(r.wIds.empty(), "no ACK (gate trips)");
}

void TestCrossChannelSwitch_PENDING()
{
    Pending("cross-channel switch flow",
            "CSHandler.cpp:12158+ — requires F2b world peer + F3 cell grid");
}

} // namespace

int main()
{
    std::printf("=== tmapsvr_asio  OnCS_CHGCHANNEL_REQ characterization spec ===\n");
    std::printf("    Source of truth: Server/TMapSvr/CSHandler.cpp:12145+\n\n");
    try { TestBeforeConnect(); TestCrossChannelSwitch_PENDING(); }
    catch (const std::exception& ex) {
        std::printf("  FAIL     unexpected exception: %s\n", ex.what());
        ++g_failed;
    }
    return ResultExitCode();
}
