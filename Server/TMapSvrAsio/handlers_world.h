#pragma once

// World-server → Map-server handler declarations.
//
// These handlers run on the AsioWorldClient read loop (the outbound
// connection from MapSvr to TWorldSvr), NOT on the client listener.
// They use the same IPlayerService + IWorldClient services wired into
// MapServer at startup.
//
// F2b Part 3 scope:
//   * OnDmLoadCharReq — WorldSvr asks MapSvr to load a character.
//     Calls IPlayerService::LoadChar, sends DM_LOADCHAR_ACK.
//   * SendMwAddCharAck — wire helper wrapping IWorldClient::SendMwAddCharAck.
//
// Legacy parity references:
//   * Server/TMapSvr/SSHandler.cpp:3311 — OnDM_LOADCHAR_REQ handler
//   * Server/TMapSvr/SSSender.cpp:237   — SendMW_ADDCHAR_ACK wire shape
//   * Server/TMapSvr/SSHandler.cpp:4424 — OnDM_LOADCHAR_ACK parse (World side)

#include "asio_session.h"
#include "services/player_service.h"
#include "services/world_client.h"

#include <boost/asio/awaitable.hpp>
#include <memory>

namespace tmapsvr {

// Context for world-side handlers (different from HandlerContext
// which is client-side). Non-owning pointers, caller owns lifetimes.
struct WorldHandlerContext
{
    IPlayerService* player_service = nullptr;
    IWorldClient*   world_client   = nullptr;
};

// Handle DM_LOADCHAR_REQ arriving from WorldSvr on the world connection.
//
// Branches (SSHandler.cpp:3311-3445):
//   §1  char_id + user_id + dw_key: look up IPlayerService
//   §2  LoadChar returns nullopt → send DM_LOADCHAR_ACK(CN_NOCHAR)
//   §3  LoadChar returns snapshot → send DM_LOADCHAR_ACK(CN_SUCCESS, data)
//
// Source: SSHandler.cpp:OnDM_LOADCHAR_REQ
boost::asio::awaitable<void> OnDmLoadCharReq(
    std::shared_ptr<tnetlib::AsioSession> world_sess,
    const tnetlib::DecodedPacket&         packet,
    const WorldHandlerContext&            ctx);

// Handle MW_CONRESULT_REQ — WorldSvr acknowledges the player's world
// registration after the DM_LOADCHAR round-trip. Delivers the pre-loaded
// snapshot to any pending client session (race path).
//
// Wire: DWORD dwCharID, DWORD dwKEY, BYTE bResult, BYTE bCount,
//       [BYTE bServerID × bCount]
// Source: SSHandler.cpp:1332 — OnMW_CONRESULT_REQ
boost::asio::awaitable<void> OnMwConResultReq(
    std::shared_ptr<tnetlib::AsioSession> world_sess,
    const tnetlib::DecodedPacket&         packet,
    const WorldHandlerContext&            ctx);

// Dispatch for world-server messages. Routes DM_* and MW_* inbound
// packets from the world connection; unknown ids are logged at debug.
boost::asio::awaitable<void> DispatchWorld(
    std::shared_ptr<tnetlib::AsioSession> world_sess,
    const tnetlib::DecodedPacket&         packet,
    const WorldHandlerContext&            ctx);

} // namespace tmapsvr
