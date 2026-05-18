#pragma once

// IAuthService — DB-agnostic authentication abstraction.
//
// Used by handlers::OnLoginReq to validate a CS_LOGIN_REQ. Two
// implementations live alongside this interface:
//   * FakeAuthService — single-user fake for tests + dev mode
//     where no DB is configured. No persistence.
//   * SociAuthService (Phase B) — real DB-backed, BCrypt password
//     verification, IP-banlist, user-protected, duplicate-session
//     detection. Talks to TACCOUNT_PW / TUSERPROTECTED /
//     IPBLACKLIST_games / TCURRENTUSER via SOCI's pluggable backend
//     (ODBC for MSSQL, native libpqxx for PostgreSQL).
//
// The interface is synchronous on purpose. DB-backed impls should
// dispatch their IO onto a worker thread (asio::post(thread_pool, …))
// or rely on SOCI's own connection-pool serialization. Wrapping
// synchronous results as awaitable at the handler level is cheap
// and keeps the interface usable from non-Asio test contexts.

#include <cstdint>
#include <optional>
#include <string>

namespace tloginsvr::services {

struct AuthRequest
{
    std::string   user_id;          // CS_LOGIN_REQ strUserID
    std::string   password;         // CS_LOGIN_REQ strPasswd
    std::string   client_ip;        // peer ip — IPv4 dotted notation
    std::uint16_t client_version;   // CS_LOGIN_REQ wVersion (TVERSION)
};

// Status codes mirror the legacy LR_* values in NetCode.h. The
// caller (OnLoginReq) maps these straight into CS_LOGIN_ACK::bResult.
enum class AuthStatus : std::uint8_t
{
    Success         = 0,   // LR_SUCCESS
    VersionMismatch = 4,   // LR_VERSION
    NoUser          = 1,   // LR_NOUSER
    WrongPassword   = 2,   // LR_INVALIDPASSWD
    Duplicate       = 3,   // LR_DUPLICATE — peer logs in while another session is live
    Banned          = 6,   // LR_BLOCK    — user-level ban
    IpBanned        = 7,   // LR_IPBLOCK  — IP-level block
    AgreementNeeded = 9,   // LR_NEEDAGREEMENT — first-time terms-of-service
    InternalError   = 5,   // LR_INTERNAL — DB / system fault
    RateLimited     = 10,  // not in legacy; modern addition. Map back to LR_INTERNAL on the wire if compat matters.
};

struct AuthResult
{
    AuthStatus    status = AuthStatus::InternalError;
    std::int32_t  user_id = 0;        // populated on Success / Duplicate / AgreementNeeded
    std::uint32_t session_key = 0;    // populated on Success — 32-bit slice of dwKEY
    std::uint8_t  create_char_count = 0; // remaining char slots; populated on Success
    std::uint8_t  in_pc_bang = 0;     // 1 if peer IP matches a PCBang range
    std::uint32_t premium_id = 0;     // active premium tier
    // populated on Banned: human-readable reason for the ack tail (legacy code adds it on wire)
    std::optional<std::string> ban_reason;
};

class IAuthService
{
public:
    virtual ~IAuthService() = default;
    virtual AuthResult Authenticate(const AuthRequest& req) = 0;

    // CS_AGREEMENT_REQ: user accepted the terms-of-service / first-login
    // EULA. Persisted into TUSERINFOTABLE.bAgreement (matches legacy
    // CSPAgreement SP). No reply on the wire — the legacy server just
    // acks the handler and continues. Idempotent.
    virtual void SetAgreement(std::int32_t user_id) = 0;

    // CS_DELCHAR_REQ: confirm the user's password before destructive
    // ops. Matches legacy CSPCheckPasswd — SELECT TACCOUNT_PW WHERE
    // dwUserID = ?; verify via the same BCrypt/legacy-plaintext path as
    // Authenticate. Returns true on match. Empty / null stored
    // password rows are treated as a miss.
    virtual bool VerifyPassword(std::int32_t user_id,
                                const std::string& password) = 0;

    // CS_TESTLOGIN_REQ — debug stress-test login. Picks a random row
    // from TTESTLOGINUSER (or the equivalent test-pool), resolves to
    // a real user in TACCOUNT_PW, and returns a Success AuthResult
    // bypassing password / agreement checks. Same TCURRENTUSER +
    // TLOG side effects as a normal Authenticate. Returns
    // InternalError if the test-user pool is empty or DB unreachable.
    //
    // **Production builds should disable this handler** — config gate
    // in LoginServerConfig::test_handlers_enabled.
    virtual AuthResult AuthenticateTest(const std::string& client_ip) = 0;

    // CS_SECURITYCONFIRM_ACK — validate a user-entered 2FA code
    // against the per-user TSECURECODE row. Comparison is
    // case-insensitive (legacy `strCode.MakeUpper()`). Decrements
    // bTries on mismatch so a brute-force attempt eventually locks
    // the account. Returns true on match.
    virtual bool VerifySecurityCode(std::int32_t user_id,
                                    const std::string& code) = 0;

    // Generate a fresh 2FA code for a user, store it in TSECURECODE,
    // and return it so the caller can mail it. Used when a new MAC
    // address is detected during login. Returns empty string on DB
    // error or user_id=0.
    virtual std::string IssueSecurityCode(std::int32_t user_id) = 0;
};

} // namespace tloginsvr::services
