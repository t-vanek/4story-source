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

    // JP/TW trailing DWORD dwSiteCode. The shipped client with
    // `MODIFY_DIRECTLOGIN=TRUE` (TNetSender.cpp:46, set by
    // TNationOption::SetNation for TNATION_JAPAN + TNATION_TAIWAN)
    // appends 4 bytes after llChecksum. Legacy server's
    // CSHandler.cpp:173 only reads the low byte (`bChanneling`),
    // which silently truncates the wire DWORD; the upper 3 bytes
    // get parsed as part of the next packet's header but the recv
    // boundary makes that harmless. Modern impl reads the full
    // DWORD and keeps the low byte as the legacy-compatible
    // `bChanneling` projection for SP-call parity.
    std::uint32_t site_code = 0;
    bool          site_code_present = false;

    // Convenience accessor: low byte of site_code. Matches legacy
    // CSPLoginJP's IN-param semantics.
    std::uint8_t channeling() const
    {
        return static_cast<std::uint8_t>(site_code & 0xFF);
    }
};

// Status codes mirror the legacy LR_* values in NetCode.h. The
// caller (OnLoginReq) maps these straight into CS_LOGIN_ACK::bResult.
enum class AuthStatus : std::uint8_t
{
    Success          = 0,   // LR_SUCCESS
    VersionMismatch  = 4,   // LR_VERSION
    NoUser           = 1,   // LR_NOUSER
    WrongPassword    = 2,   // LR_INVALIDPASSWD
    Duplicate        = 3,   // LR_DUPLICATE — peer logs in while another session is live
    SecurityRequired = 5,   // LR_SECURITY  — new device, 2FA challenge required
    Banned           = 6,   // LR_BLOCK    — user-level ban
    IpBanned         = 7,   // LR_IPBLOCK  — IP-level block
    AgreementNeeded  = 9,   // LR_NEEDAGREEMENT — first-time terms-of-service
    InternalError    = 8,   // LR_INTERNAL — DB / system fault
    RateLimited      = 10,  // not in legacy; modern addition. Map back to LR_INTERNAL on the wire if compat matters.
};

struct AuthResult
{
    AuthStatus    status = AuthStatus::InternalError;
    std::int32_t  user_id = 0;        // populated on Success / Duplicate / AgreementNeeded
    std::uint32_t session_key = 0;    // populated on Success — 32-bit slice of dwKEY
    std::uint8_t  create_char_count = 0; // remaining char slots; populated on Success
    std::uint8_t  in_pc_bang = 0;     // 1 if peer IP matches a PCBang range
    std::uint32_t premium_id = 0;     // active premium tier

    // Last-played character ID — legacy TLogin SP returns this as the
    // 7th OUT param (CSPLogin::m_dwCharID, DBAccess.h:33). The shipped
    // client uses it in the lobby to highlight / preselect the slot the
    // user logged off from. Zero on first-time login or when the last
    // char was deleted. Populated from TUSERINFOTABLE.dwLastCharID on
    // the SOCI side; the in-memory backend leaves it at 0.
    std::uint32_t last_char_id = 0;

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

    // Per-user 2FA email + toggle. Empty optional → no email on file
    // (2FA challenge can't be sent, login proceeds without check).
    struct EmailRecord
    {
        std::string  email;
        bool         two_factor_enabled = false;
    };
    virtual std::optional<EmailRecord> LookupEmail(std::int32_t user_id) = 0;

    // Trusted-IP whitelist for 2FA. IsTrustedIp returns true if the
    // (uid, ip) pair has been confirmed before — login skips the
    // challenge. AddTrustedIp INSERTs after a successful
    // CS_SECURITYCONFIRM_ACK. Idempotent (PK conflict swallowed).
    virtual bool IsTrustedIp(std::int32_t user_id,
                             const std::string& client_ip) = 0;
    virtual void AddTrustedIp(std::int32_t user_id,
                              const std::string& client_ip) = 0;

    // Complete a 2FA-deferred login: insert TCURRENTUSER + TLOG rows
    // exactly like the success branch of Authenticate would have done.
    // Returns the dwKEY for the new session row, or 0 on failure. The
    // caller (CS_SECURITYCONFIRM_ACK handler) hands the result back to
    // the client in a deferred CS_LOGIN_ACK.
    virtual std::uint32_t CompleteSecurityLogin(std::int32_t user_id,
                                                const std::string& client_ip) = 0;

    // Lookup the user's last-played char ID. Mirrors the OUT param the
    // legacy TLogin SP returns as m_dwCharID; used by the deferred
    // CS_LOGIN_ACK path (CS_SECURITYCONFIRM_ACK handler) to populate
    // the same field. Returns 0 on unknown user / first-time login.
    virtual std::uint32_t LookupLastCharId(std::int32_t user_id) = 0;
};

} // namespace tloginsvr::services
