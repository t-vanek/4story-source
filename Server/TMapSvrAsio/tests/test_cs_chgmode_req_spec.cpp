// Characterization test for CS_CHGMODE_REQ.
//
// Source of truth: Server/TMapSvr/CSHandler.cpp:1220-1231
//
// Wire shape (body): BYTE bMode
//
// Branches:
//   §1  :1224  !m_pMAP || !m_bMain → silent drop (ACTIVE)
//   §2  :1227-1228  in-game → m_bMode = bMode; ChgMode() broadcasts to AOI
//       → PENDING — requires F3 cell-based AOI broadcast

#include "spec_fixture.h"

using namespace tmapsvr::spec;
using tnetlib::protocol::MessageId;
using tnetlib::protocol::ToUint16;

namespace {

void TestBeforeConnect()
{
    std::printf("[§1 CHGMODE before CONNECT → silent drop  "
                "(CSHandler.cpp:1224)]\n");
    ServerFixture fx(/*accept_all=*/true);
    std::vector<std::byte> body;
    tmapsvr::wire::WritePOD<std::uint8_t>(body, 1);  // arbitrary mode byte
    const auto r = RunSinglePacket(fx.Port(),
        ToUint16(MessageId::CS_CHGMODE_REQ), body);
    Check(r.wIds.empty(), "no ACK (gate trips)");
    Check(!r.socket_closed_by_peer, "session stays open");
}

void TestModeBroadcast_PENDING()
{
    Pending("in-game mode change → AOI broadcast",
            "CSHandler.cpp:1227-1228 — requires F3 cell-based AOI");
}

} // namespace

int main()
{
    std::printf("=== tmapsvr_asio  OnCS_CHGMODE_REQ characterization spec ===\n");
    std::printf("    Source of truth: Server/TMapSvr/CSHandler.cpp:1220-1231\n\n");
    try { TestBeforeConnect(); TestModeBroadcast_PENDING(); }
    catch (const std::exception& ex) {
        std::printf("  FAIL     unexpected exception: %s\n", ex.what());
        ++g_failed;
    }
    return ResultExitCode();
}
