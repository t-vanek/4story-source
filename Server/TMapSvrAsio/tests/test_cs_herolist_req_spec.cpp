// Characterization test for CS_HEROLIST_REQ.
//
// Source of truth: Server/TMapSvr/CSHandler.cpp:14862-14874
//
// Wire shape: empty body.
//
// Branches:
//   §1  :14864-14866 (BOW_COMPILE_MODE) → return EC_NOERROR (no ack)
//       In a BoW-compiled shard the entire hero system is disabled.
//       Modern doesn't carry the BoW vs default compile distinction
//       at runtime — F1 acts as the default (non-BoW) build.
//   §2  :14868  !m_pMAP || !m_bMain → silent drop (ACTIVE)
//   §3  :14871  in-game → SendCS_HEROLIST_ACK
//       → PENDING — F3 player + IHeroService

#include "spec_fixture.h"

using namespace tmapsvr::spec;
using tnetlib::protocol::MessageId;
using tnetlib::protocol::ToUint16;

namespace {

void TestBeforeConnect()
{
    std::printf("[§2 HEROLIST before CONNECT → silent drop  "
                "(CSHandler.cpp:14868)]\n");
    ServerFixture fx(/*accept_all=*/true);
    const auto r = RunSinglePacket(fx.Port(),
        ToUint16(MessageId::CS_HEROLIST_REQ), {});
    Check(r.wIds.empty(), "no ACK (gate trips)");
}

void TestHeroListAck_PENDING()
{
    Pending("in-game hero-list response",
            "CSHandler.cpp:14871 — requires F3 player state + IHeroService");
}

} // namespace

int main()
{
    std::printf("=== tmapsvr_asio  OnCS_HEROLIST_REQ characterization spec ===\n");
    std::printf("    Source of truth: Server/TMapSvr/CSHandler.cpp:14862-14874\n\n");
    try { TestBeforeConnect(); TestHeroListAck_PENDING(); }
    catch (const std::exception& ex) {
        std::printf("  FAIL     unexpected exception: %s\n", ex.what());
        ++g_failed;
    }
    return ResultExitCode();
}
