#pragma once

#include <boost/asio/awaitable.hpp>

#include "asio_session.h"
#include "services/auth_service.h"
#include "services/char_service.h"
#include "services/connection_registry.h"
#include "services/map_server_locator.h"

#include <cstddef>
#include <memory>
#include <span>
#include <vector>

namespace tloginsvr::handlers {

// CS_LOGIN_REQ → CS_LOGIN_ACK. Phase-3 stub: validates wire format,
// returns a hardcoded success response with placeholder fields. Real
// authentication, DB lookup, session registry, and duplicate-kick
// logic are deferred to follow-up commits.
//
// Wire layout from CSHandler.cpp::OnCS_LOGIN_REQ:
//   WORD   wVersion
//   STRING Zombie3
//   STRING strPasswd
//   STRING Zombie1
//   STRING Zombie2
//   STRING strUserID
//   INT64  dlCheck
//   INT64  llChecksum_recv
//   [BYTE  bChanneling  — JP nation only]
//
// This stub only reads wVersion and replies. Everything else is
// ignored. That's enough to demonstrate the codec + dispatch
// pipeline end-to-end without needing the auth DB plumbed in.
// Optional services:
//   * auth_service — when non-null, the handler reads the full legacy
//     wire format (userId, password) and consults the service. When
//     null, falls back to the Phase-3 stub behavior (version check
//     only, replies LR_SUCCESS to everyone).
//   * connection_registry — when non-null, on auth success the
//     handler registers the session and kicks any previous session
//     for the same user_id (duplicate-kick).
boost::asio::awaitable<void> OnLoginReq(
    std::shared_ptr<tnetlib::AsioSession> session,
    std::span<const std::byte> body,
    services::IAuthService* auth_service = nullptr,
    services::IConnectionRegistry* connection_registry = nullptr);

// CS_GROUPLIST_REQ → CS_GROUPLIST_ACK. Phase-3 stub: returns an empty
// world-group list (BYTE bCount = 0 + BYTE bCheckFilePoint = 0).
// Real implementation queries TGROUP table for active worlds.
//
// Wire layout from CSHandler.cpp::OnCS_GROUPLIST_REQ:
//   request body: (none — empty)
//   ack body:  BYTE bCount, BYTE bCheckFilePoint, [per group: STRING szNAME,
//              BYTE bGroupID, BYTE bType, BYTE bStatus, BYTE bCharCount]
boost::asio::awaitable<void> OnGroupListReq(
    tnetlib::AsioSession& session,
    std::span<const std::byte> body);

// CS_CHANNELLIST_REQ(BYTE bGroupID) → CS_CHANNELLIST_ACK. Phase-3 stub:
// returns empty channel list. Real impl queries TCHANNEL.
boost::asio::awaitable<void> OnChannelListReq(
    tnetlib::AsioSession& session,
    std::span<const std::byte> body);

// CS_CHARLIST_REQ(BYTE bGroupID) → CS_CHARLIST_ACK.
// When char_service is non-null AND the session is registered (i.e.
// authenticated), the list comes from ICharService::List for the
// session's user_id. Otherwise returns the empty-list stub.
boost::asio::awaitable<void> OnCharListReq(
    std::shared_ptr<tnetlib::AsioSession> session,
    std::span<const std::byte> body,
    services::ICharService* char_service = nullptr,
    services::IConnectionRegistry* connection_registry = nullptr);

// CS_CREATECHAR_REQ → CS_CREATECHAR_ACK.
// With both services wired: parses the 14-byte char-template body,
// looks up the session's user_id, calls ICharService::Create.
// Without services: stub returns CR_INTERNAL.
boost::asio::awaitable<void> OnCreateCharReq(
    std::shared_ptr<tnetlib::AsioSession> session,
    std::span<const std::byte> body,
    services::ICharService* char_service = nullptr,
    services::IConnectionRegistry* connection_registry = nullptr);

// CS_DELCHAR_REQ → CS_DELCHAR_ACK.
// Parses (bGroupID, strPasswd, dwCharID). With services wired, calls
// ICharService::Delete. The password is passed through; the in-memory
// impl ignores it, real Phase-B impl will route via IAuthService.
boost::asio::awaitable<void> OnDelCharReq(
    std::shared_ptr<tnetlib::AsioSession> session,
    std::span<const std::byte> body,
    services::ICharService* char_service = nullptr,
    services::IConnectionRegistry* connection_registry = nullptr);

// CS_START_REQ(bGroupID, bChannel, dwCharID) → CS_START_ACK.
// When map_server_locator is non-null, real lookup → SR_SUCCESS (0)
// with the resolved IPv4 + port + server_id. Null or no-hit lookup
// falls back to SR_NOSERVER (1).
// On SR_SUCCESS with a connection_registry, the session is marked
// for Map handoff so session-terminator cleanup preserves the
// TCURRENTUSER row for the Map server's dwKEY validation.
boost::asio::awaitable<void> OnStartReq(
    std::shared_ptr<tnetlib::AsioSession> session,
    std::span<const std::byte> body,
    services::IMapServerLocator* map_server_locator = nullptr,
    services::IConnectionRegistry* connection_registry = nullptr);

// CS_AGREEMENT_REQ(WORD wVersion) — no ack. Legacy upserts TUSERINFOTABLE
// row + flips per-session m_bAgreement; we just log for now.
boost::asio::awaitable<void> OnAgreementReq(
    tnetlib::AsioSession& session,
    std::span<const std::byte> body);

// CS_HOTSEND_REQ(INT64 dlValue, BYTE bAll) — no ack. Exec-file
// integrity heartbeat. Stub: log and ignore.
boost::asio::awaitable<void> OnHotsendReq(
    tnetlib::AsioSession& session,
    std::span<const std::byte> body);

// CS_VETERAN_REQ → CS_VETERAN_ACK. Stub: returns bOption=0 (no
// returning-player bonus offered). Real impl reads TVETERANCHART.
boost::asio::awaitable<void> OnVeteranReq(
    tnetlib::AsioSession& session,
    std::span<const std::byte> body);

// CS_TERMINATE_REQ(DWORD dwKey) — no ack. Clean logout request from
// client. Legacy magic key 0x2AF3A9D1 (validated on the wire). Stub
// closes the session.
boost::asio::awaitable<void> OnTerminateReq(
    tnetlib::AsioSession& session,
    std::span<const std::byte> body);

// CS_SECURITYCONFIRM_ACK(STRING strCode) → CS_SECURITYRESULT_ACK.
// Dead-code path on the legacy server (commented out in CSHandler.cpp);
// kept here so the dispatcher doesn't log it as unhandled. Always
// replies CODE_CORRECT (0).
boost::asio::awaitable<void> OnSecurityConfirmAck(
    tnetlib::AsioSession& session,
    std::span<const std::byte> body);

} // namespace tloginsvr::handlers
