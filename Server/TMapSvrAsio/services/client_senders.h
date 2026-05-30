#pragma once

// client_senders — body encoders for the CS_ packets the map sends down
// to the *client* as a result of inbound World→Map cluster traffic
// (MW_ADDCONNECT / MW_CONRESULT / MW_CLOSECHAR / MW_ROUTELIST). Kept as a
// declared-here / defined-in-.cpp unit (like world_senders) so the
// encoders compile against TMap's wire_codec.h once and stay unit-
// testable without a live client socket — AsioSession::SendPacket isn't
// virtual and needs a real connection, so the byte layout is what the
// tests pin down (the handler glue that finds the session and sends is
// thin).
//
// Byte layouts mirror the legacy CTPlayer::SendCS_* / inline CS_ACK
// builders in Server/TMapSvrAsio/legacy_src/CSSender.cpp + SSHandler.cpp.

#include <cstddef>
#include <cstdint>
#include <vector>

namespace tmapsvr {

// One entry of the cross-server connect/route list: where a peer map
// server lives. Shared by CS_ADDCONNECT_ACK (MW_ADDCONNECT_REQ relay)
// and, later, CS_ROUTE_ACK.
struct ConnectRoute
{
    std::uint32_t ip_addr   = 0;
    std::uint16_t port      = 0;
    std::uint8_t  server_id = 0;
};

// CS_ADDCONNECT_ACK body — BYTE count + count × (DWORD ip, WORD port,
// BYTE server_id). Mirrors the inline builder in legacy
// SSHandler.cpp:6797 (OnMW_ADDCONNECT_REQ → pPlayer->Say). The map
// relays the peer-server list TWorld handed it down to the client so the
// client can open its cross-server connections.
std::vector<std::byte> EncodeAddConnectAck(
    const std::vector<ConnectRoute>& routes);

} // namespace tmapsvr
