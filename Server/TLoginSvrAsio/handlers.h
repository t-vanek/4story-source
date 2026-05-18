#pragma once

#include <boost/asio/awaitable.hpp>

#include <functional>

#include "asio_session.h"
#include "fourstory/audit/audit_logger.h"
#include "services/auth_service.h"
#include "services/char_service.h"
#include "services/connection_registry.h"
#include "services/event_registry.h"
#include "services/map_server_locator.h"
#include "fourstory/ops/rate_limiter.h"
#include "fourstory/smtp/smtp_client.h"

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
    services::IConnectionRegistry* connection_registry = nullptr,
    fourstory::audit::IAuditLogger* audit_logger = nullptr,
    fourstory::ops::LoginRateLimiter* rate_limiter = nullptr,
    std::span<const std::uint16_t> accepted_versions = {},
    fourstory::smtp::ISmtpClient* smtp_client = nullptr);

// CS_GROUPLIST_REQ → CS_GROUPLIST_ACK. Phase-3 stub: returns an empty
// world-group list (BYTE bCount = 0 + BYTE bCheckFilePoint = 0).
// Real implementation queries TGROUP table for active worlds.
//
// Wire layout from CSHandler.cpp::OnCS_GROUPLIST_REQ:
//   request body: (none — empty in current legacy build; the commented
//                  INT64 dlCheckFile check is dead code there)
//   ack body:  BYTE bCount, DWORD dwCheckPoint=0,
//              per group: STRING szNAME, BYTE bGroupID, BYTE bType,
//                         BYTE bStatus, BYTE bFlags
// With map_server_locator+connection_registry: real list from
// IMapServerLocator::ListGroups. Null locator → empty stub.
boost::asio::awaitable<void> OnGroupListReq(
    std::shared_ptr<tnetlib::AsioSession> session,
    std::span<const std::byte> body,
    services::IMapServerLocator* map_server_locator = nullptr,
    services::IConnectionRegistry* connection_registry = nullptr);

// CS_CHANNELLIST_REQ(BYTE bGroupID) → CS_CHANNELLIST_ACK.
//   ack body:  BYTE bCount, DWORD dwCheckPoint=0,
//              per channel: STRING szNAME, BYTE bChannel, BYTE bStatus
boost::asio::awaitable<void> OnChannelListReq(
    tnetlib::AsioSession& session,
    std::span<const std::byte> body,
    services::IMapServerLocator* map_server_locator = nullptr);

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
    services::IConnectionRegistry* connection_registry = nullptr,
    fourstory::audit::IAuditLogger* audit_logger = nullptr);

// CS_DELCHAR_REQ → CS_DELCHAR_ACK.
// Parses (bGroupID, strPasswd, dwCharID). If `auth_service` is non-null
// the password is verified via IAuthService::VerifyPassword (legacy
// CSPCheckPasswd parity) before calling ICharService::Delete. A
// failed verify replies DR_INVALIDPASSWD (=2). Null auth_service
// accepts whatever the client sends — used by the in-memory test path.
boost::asio::awaitable<void> OnDelCharReq(
    std::shared_ptr<tnetlib::AsioSession> session,
    std::span<const std::byte> body,
    services::ICharService* char_service = nullptr,
    services::IConnectionRegistry* connection_registry = nullptr,
    services::IAuthService* auth_service = nullptr,
    fourstory::audit::IAuditLogger* audit_logger = nullptr);

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
    services::IConnectionRegistry* connection_registry = nullptr,
    fourstory::audit::IAuditLogger* audit_logger = nullptr);

// CS_AGREEMENT_REQ(WORD wVersion) — no ack. Persists TACCOUNT_PW.bCheck=1
// via IAuthService::SetAgreement so the user doesn't see the EULA again.
// Without services wired the call is a no-op log.
boost::asio::awaitable<void> OnAgreementReq(
    std::shared_ptr<tnetlib::AsioSession> session,
    std::span<const std::byte> body,
    services::IAuthService* auth_service = nullptr,
    services::IConnectionRegistry* connection_registry = nullptr);

// CS_HOTSEND_REQ — legacy client's exec-file integrity heartbeat
// (TNetHandler.cpp:337 + :721 fire one per GROUPLIST/CHANNELLIST ack).
// Silently accepted and dropped here: validation belongs to the
// anti-cheat tooling that's intentionally out of scope for this
// rewrite.
boost::asio::awaitable<void> OnHotsendReq(
    std::shared_ptr<tnetlib::AsioSession> session,
    std::span<const std::byte> body);

// CS_VETERAN_REQ → CS_VETERAN_ACK(BYTE bOption=3, BYTE level1,
// BYTE level2, BYTE level3). Reads three level rows from
// ICharService::GetVeteranLevels (cached at construction from
// TVETERANCHART). Null service → no-bonus stub.
boost::asio::awaitable<void> OnVeteranReq(
    tnetlib::AsioSession& session,
    std::span<const std::byte> body,
    services::ICharService* char_service = nullptr);

// CS_TERMINATE_REQ(DWORD dwKey) — no ack. Clean logout request from
// client. Legacy magic key 0x2AF3A9D1 (validated on the wire). Stub
// closes the session.
boost::asio::awaitable<void> OnTerminateReq(
    tnetlib::AsioSession& session,
    std::span<const std::byte> body);

// CS_SECURITYCONFIRM_ACK(STRING strCode) → CS_SECURITYRESULT_ACK.
// Validates a user-entered 2FA code via IAuthService::VerifySecurityCode
// (TSECURECODE row lookup). Replies CODE_CORRECT (0) on match, CODE_INCORRECT
// (1) otherwise. Empty / unauthenticated requests are rejected too.
boost::asio::awaitable<void> OnSecurityConfirmAck(
    std::shared_ptr<tnetlib::AsioSession> session,
    std::span<const std::byte> body,
    services::IAuthService* auth_service = nullptr,
    services::IConnectionRegistry* connection_registry = nullptr,
    fourstory::audit::IAuditLogger* audit_logger = nullptr);

// CT_SERVICEMONITOR_ACK(DWORD dwTick) → CT_SERVICEMONITOR_REQ.
// Control-server polling shim — replies with session counts so the GM
// dashboard can render load. Phase-B stub returns zeros because the
// real connection-registry size only matters once TControlSvr is on
// the network. Wire: in (DWORD dwTick); out (DWORD dwTick, DWORD
// dwSESSION, DWORD dwTUSER, DWORD dwTACTIVEUSER).
boost::asio::awaitable<void> OnControlServiceMonitor(
    tnetlib::AsioSession& session,
    std::span<const std::byte> body,
    services::IConnectionRegistry* connection_registry = nullptr);

// CT_SERVICEDATACLEAR_ACK — rebuild m_mapACTIVEUSER from m_mapTUSER on
// the legacy server. The modernized server's IConnectionRegistry is
// always in sync so this is a no-op log. No reply.
boost::asio::awaitable<void> OnControlServiceDataClear(
    tnetlib::AsioSession& session,
    std::span<const std::byte> body);

// CT_CTRLSVR_REQ — keep-alive heartbeat from the control server. Legacy
// just `return EC_NOERROR;` (TLoginSvr/CSHandler.cpp:82). No reply.
boost::asio::awaitable<void> OnControlCtrlSvr(
    tnetlib::AsioSession& session,
    std::span<const std::byte> body);

// CT_EVENTUPDATE_REQ — GM event registry update (event start/end).
// Wire: BYTE bEventID, WORD wValue, EVENTINFO struct. Persists into
// IEventRegistry on wValue != 0, removes on wValue == 0 (matches legacy
// m_mapEVENT semantics in CSHandler.cpp:87-120). No reply.
boost::asio::awaitable<void> OnControlEventUpdate(
    tnetlib::AsioSession& session,
    std::span<const std::byte> body,
    services::IEventRegistry* event_registry = nullptr);

// CT_EVENTMSG_REQ — broadcast text. Legacy reads (bEventID, bMsgType,
// strMsg) and returns EC_NOERROR without action. Same here. No reply.
boost::asio::awaitable<void> OnControlEventMsg(
    tnetlib::AsioSession& session,
    std::span<const std::byte> body);

// CS_TESTLOGIN_REQ — Svr Tester stress login. Picks a random user
// from TTESTLOGINUSER, replies CS_LOGIN_ACK as if the user had just
// logged in. Disabled by default; opt-in via the server config flag.
boost::asio::awaitable<void> OnTestLoginReq(
    std::shared_ptr<tnetlib::AsioSession> session,
    std::span<const std::byte> body,
    services::IAuthService* auth_service = nullptr,
    services::IConnectionRegistry* connection_registry = nullptr,
    fourstory::audit::IAuditLogger* audit_logger = nullptr);

// CS_TESTVERSION_REQ — Svr Tester version probe. Returns the current
// protocol version so a test harness can detect a drifted client.
boost::asio::awaitable<void> OnTestVersionReq(
    tnetlib::AsioSession& session,
    std::span<const std::byte> body);

// SM_QUITSERVICE_REQ — graceful-shutdown signal from the legacy
// service manager peer (SS-typed connection). Stops the io_context
// from the inside; the SIGINT/SIGTERM path is still the primary
// trigger for local restarts. On_stop is invoked synchronously
// before this returns.
boost::asio::awaitable<void> OnQuitServiceReq(
    tnetlib::AsioSession& session,
    std::span<const std::byte> body,
    std::function<void()> on_stop);

} // namespace tloginsvr::handlers
