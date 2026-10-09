#pragma once

// IConnectionRegistry — tracks authenticated AsioSessions by their
// authenticated user_id, so the login flow can enforce the legacy
// duplicate-kick policy:
//
//   1. User A is logged in on session X.
//   2. User A logs in again on a fresh session Y.
//   3. Registry: Register(A, Y) returns X (previous holder).
//   4. A defensive local replacement closes X; native DB duplicate handling
//      sends LR_DUPLICATE to Y and closes both authenticated connections.
//
// Duplicate replies close both authenticated Login connections. Pending security
// challenges remain connection-specific and do not displace another connection.
//
// Implementation pointer: registry holds weak_ptr to each AsioSession
// (sessions are owned by their per-connection HandleConnection
// coroutine via shared_ptr). On Unregister it removes by raw pointer
// since the shared_ptr may have already expired by then.

#include "asio_session.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace tloginsvr::services {

// Per-connection pending or authenticated state. Pending entries have no
// session key or account permissions. Close-time cleanup revokes their challenge;
// authenticated entries instead drive key-specific SessionTerminator cleanup.
struct ConnectionEntry
{
    std::int32_t  user_id = 0;
    std::uint32_t session_key = 0;
    // Set by OnStartReq's SR_SUCCESS path. Flips the close-time
    // termination reason from Disconnect to MapHandoff so the real
    // SOCI impl can preserve TCURRENTUSER for the Map server's key
    // validation.
    bool          handoff_to_map = false;
    // Per-session agreement gate. Mirrors legacy CTUser::m_bAgreement
    // (TLoginSvr/TUser.h:28). Set to true when LR_SUCCESS comes back
    // from IAuthService::Authenticate (agreement already on file) or
    // when CS_AGREEMENT_REQ is acknowledged. CharList / Create /
    // Delete / Start / Veteran all gate on this — legacy
    // CSHandler.cpp:600 returns EC_SESSION_INVALIDCHAR if not set.
    bool          agreed = false;
    // Selected world group. Stamped on the first CS_CHARLIST_REQ (the
    // group_id byte the client sends). DELCHAR refuses if the request
    // arrives with a mismatching bGroupID — same defensive check as
    // legacy CSHandler.cpp:1223.
    std::uint8_t  group_id = 0;
    // Random 64-bit nonce stamped at LOGIN. Echoed in CS_LOGIN_ACK so
    // the client can derive a per-session HMAC seed for the legacy
    // exec-check feature. Modernized server doesn't use it but
    // populates it to match the wire layout.
    std::int64_t  check_key = 0;

    // A challenge is private to this connection. Confirmation leaves the session
    // pending until a client-driven LOGIN retry passes full authentication.
    bool awaiting_security = false;
    std::string pending_client_ip;
    std::string security_token;
    bool security_verified = false;
    std::chrono::steady_clock::time_point security_deadline{};

    // Character ID the client launched into (from CS_START_REQ
    // SR_SUCCESS). Stamped by OnStartReq right before MarkHandoff;
    // the close-time SessionTerminator forwards it so:
    //   * TLOG.dwCharID gets the actual char_id of the session
    //     (legacy CSPLogout's m_dwCharID arg)
    //   * TUSERINFOTABLE.dwLastCharID gets updated so the next
    //     login's CS_LOGIN_ACK.dwCharID highlights the right slot
    // Zero when the session never reached CS_START_REQ (lobby-only
    // disconnect) — terminator skips both updates in that case.
    std::int32_t  last_char_id = 0;
};

class IConnectionRegistry
{
public:
    virtual ~IConnectionRegistry() = default;

    // Register an authenticated session under `entry.user_id`. If
    // another session was previously registered under the same
    // user_id, returns a shared_ptr to it (caller should close it
    // to enforce duplicate-kick). Returns nullptr if no previous
    // holder.
    virtual std::shared_ptr<tnetlib::AsioSession>
    Register(ConnectionEntry entry,
             std::shared_ptr<tnetlib::AsioSession> session) = 0;

    // Lookup the entry for a session. Returns nullopt if not
    // registered (unauthenticated session).
    virtual std::optional<ConnectionEntry>
    Lookup(const std::shared_ptr<tnetlib::AsioSession>& session) const = 0;

    // Flip handoff_to_map for the registered session.
    virtual void MarkHandoff(
        const std::shared_ptr<tnetlib::AsioSession>& session) = 0;

    // Combined flip: MarkHandoff + stamp last_char_id. Used by
    // OnStartReq's SR_SUCCESS branch — the session leaves login
    // for the map server holding this char, so the close-time
    // terminator needs to know which char it was for TLOG /
    // TUSERINFOTABLE bookkeeping.
    virtual void MarkHandoffWithChar(
        const std::shared_ptr<tnetlib::AsioSession>& session,
        std::int32_t char_id) = 0;

    // Flip the per-session agreement gate. Called from OnAgreementReq
    // after IAuthService::SetAgreement returns, and from OnLoginReq
    // when LR_SUCCESS arrives (agreement already on file). No-op if
    // the session isn't registered.
    virtual void MarkAgreed(
        const std::shared_ptr<tnetlib::AsioSession>& session) = 0;

    // Stamp the per-session selected world group. First CHARLIST_REQ
    // sets this; downstream handlers (DELCHAR, START) read it back
    // for cross-check. No-op if the session isn't registered.
    virtual void SetGroupId(
        const std::shared_ptr<tnetlib::AsioSession>& session,
        std::uint8_t group_id) = 0;

    // Permit only the next LOGIN request on this still-pending connection.
    virtual void MarkSecurityVerified(
        const std::shared_ptr<tnetlib::AsioSession>& session) = 0;

    // Remove a session. No-op if not registered. Always safe to call
    // from connection-close paths.
    virtual void Unregister(
        const std::shared_ptr<tnetlib::AsioSession>& session) = 0;

    // Currently-registered session count.
    virtual std::size_t Count() const = 0;

    // Snapshot of all live entries. Used by the shutdown path to drive
    // a bulk SessionTerminator::Terminate sweep (legacy
    // CTLoginSvrModule::UpdateData walks m_mapTSESSION and calls
    // CSPLogout for every session with m_bLogout=TRUE). The pair holds
    // the entry data + a strong shared_ptr to the session so callers
    // can close it after the terminator runs without racing the
    // session's natural close path.
    struct LiveEntry
    {
        ConnectionEntry                          entry;
        std::shared_ptr<tnetlib::AsioSession>    session;
    };
    virtual std::vector<LiveEntry> Snapshot() const = 0;
};

} // namespace tloginsvr::services
