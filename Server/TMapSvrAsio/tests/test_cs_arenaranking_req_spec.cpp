// Characterization test for CS_ARENARANKING_REQ.
//
// Source of truth: Server/TMapSvr/CSHandler.cpp:21106-21120
//
// Wire shape (body):
//   BYTE Arena    — arena id the client wants ranking for
//
// Branches:
//
//   §1  CSHandler.cpp:21109  !m_pMAP || !m_bMain
//       → return EC_NOERROR (silent drop, no ack, session alive)
//       → modern: ACTIVE — same observable as "before CONNECT" since
//         we don't have map state yet
//
//   §2  CSHandler.cpp:21114-21117  in-game
//       → read BYTE Arena, send CS_ARENARANKING_ACK (legacy sender
//         currently emits an empty body — feature appears half-
//         implemented in the shipped source)
//       → modern: PENDING — requires F3 IMapState (gate on m_bMain)

#include "spec_fixture.h"

using namespace tmapsvr::spec;
using tnetlib::protocol::MessageId;
using tnetlib::protocol::ToUint16;

namespace {

void TestArenaBeforeConnect()
{
    std::printf("[§1 CS_ARENARANKING_REQ before CONNECT → silent drop  "
                "(CSHandler.cpp:21109)]\n");
    ServerFixture fx(/*accept_all=*/true);

    std::vector<std::byte> body;
    tmapsvr::wire::WritePOD<std::uint8_t>(body, 0);  // Arena = 0
    const auto r = RunSinglePacket(fx.Port(),
        ToUint16(MessageId::CS_ARENARANKING_REQ), body);

    Check(r.wIds.empty(),
        "no ACK (m_pMAP gate trips → EC_NOERROR with no sender call)");
    Check(!r.socket_closed_by_peer,
        "session stays open");
}

void TestArenaInGame_PENDING()
{
    Pending("in-game arena ranking → CS_ARENARANKING_ACK",
            "CSHandler.cpp:21114-21117 — requires F3 IMapState");
}

} // namespace

int main()
{
    std::printf("=== tmapsvr_asio  OnCS_ARENARANKING_REQ characterization spec ===\n");
    std::printf("    Source of truth: Server/TMapSvr/CSHandler.cpp:21106-21120\n\n");
    try
    {
        TestArenaBeforeConnect();
        TestArenaInGame_PENDING();
    }
    catch (const std::exception& ex) {
        std::printf("  FAIL     unexpected exception: %s\n", ex.what());
        ++g_failed;
    }
    return ResultExitCode();
}
