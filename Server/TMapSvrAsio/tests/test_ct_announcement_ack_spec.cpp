// Characterization test for CT_ANNOUNCEMENT_ACK.
//
// Source of truth: Server/TMapSvr/CSHandler.cpp:72-89
//
// Wire shape (body):
//   length-prefixed string strAnnounce (CP1252, int32 length)
//
// Branches:
//
//   §1  CSHandler.cpp:77-80  always
//       → read `strAnnounce`, truncate to 1 KB
//       → modern: ACTIVE (the read + log; truncation is defensive)
//
//   §2  CSHandler.cpp:82-86  always
//       → for every player with m_bMain: send CS_CHAT_ACK { CHAT_WORLD,
//         0, "[Operator]", strAnnounce } as a global chat message.
//       → modern: PENDING — requires F3 map-wide player registry +
//         IPlayerService::Broadcast.
//
// Wire-observable from a single CT_ANNOUNCEMENT_ACK with no players
// in-world: log emitted, no ACK back. That's the F2 baseline.

#include "spec_fixture.h"

using namespace tmapsvr::spec;
using tnetlib::protocol::MessageId;
using tnetlib::protocol::ToUint16;

namespace {

// Build an int32-length-prefixed CP1252 string body — same shape the
// legacy CPacket::operator<<(CString) emits.
std::vector<std::byte> LengthPrefixedString(const std::string& s)
{
    std::vector<std::byte> body;
    tmapsvr::wire::WriteString(body, s);
    return body;
}

void TestAnnouncementReadNoAck()
{
    std::printf("[§1 CT_ANNOUNCEMENT_ACK → read + log, no ack  "
                "(CSHandler.cpp:77-80)]\n");
    ServerFixture fx(/*accept_all=*/true);

    const auto body = LengthPrefixedString("Server restart in 5 minutes");
    const auto r = RunSinglePacket(fx.Port(),
        ToUint16(MessageId::CT_ANNOUNCEMENT_ACK), body);

    Check(r.wIds.empty(),
        "no ACK back to control peer (legacy returns EC_NOERROR)");
    Check(!r.socket_closed_by_peer,
        "control-peer session stays open after announcement");
}

void TestBroadcastToInGamePlayers_PENDING()
{
    Pending("fan-out CS_CHAT_ACK{CHAT_WORLD} to every in-game player",
            "CSHandler.cpp:82-86 — requires F3 map-wide player registry");
}

} // namespace

int main()
{
    std::printf("=== tmapsvr_asio  OnCT_ANNOUNCEMENT_ACK characterization spec ===\n");
    std::printf("    Source of truth: Server/TMapSvr/CSHandler.cpp:72-89\n\n");
    try
    {
        TestAnnouncementReadNoAck();
        TestBroadcastToInGamePlayers_PENDING();
    }
    catch (const std::exception& ex) {
        std::printf("  FAIL     unexpected exception: %s\n", ex.what());
        ++g_failed;
    }
    return ResultExitCode();
}
