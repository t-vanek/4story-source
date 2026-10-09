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
//     (ODBC for MSSQL, native SOCI/libpq for PostgreSQL).
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

    // Internal connection-bound grant; never decoded from client-controlled fields.
    std::string security_retry_token;

    // Convenience accessor: low byte of site_code. Matches legacy
    // CSPLoginJP's IN-param semantics.
    std::uint8_t channeling() const
    {
        return static_cast<std::uint8_t>(site_code & 0xFF);
    }
};

// Values follow NetCode.h except internal categories Banned/RateLimited.
// ToLoginWireResult maps those categories to the original result codes.
enum class AuthStatus : std::uint8_t
{
    Success          = 0,   // LR_SUCCESS
    VersionMismatch  = 4,   // LR_VERSION
    NoUser           = 1,   // LR_NOUSER
    WrongPassword    = 2,   // LR_INVALIDPASSWD
    Duplicate        = 3,   // LR_DUPLICATE — peer logs in while another session is live
    SecurityRequired = 10,   // LR_SECURITY  — new device, 2FA challenge required
    Banned           = 254, // Internal category; backed-up TLogin maps account bans to LR_IPBLOCK (7).
    IpRestricted     = 6,   // LR_BLOCK — TCheckIP restriction
    IpBanned         = 7,   // LR_IPBLOCK  — IP-level block
    AgreementNeeded  = 8,   // LR_NEEDAGREEMENT — first-time terms-of-service
    InternalError    = 5,   // LR_INTERNAL — DB / system fault
    RateLimited      = 255,  // not in legacy; modern addition. Map back to LR_INTERNAL on the wire if compat matters.
};

constexpr std::uint8_t ToLoginWireResult(AuthStatus status)
{
    if (status == AuthStatus::Banned) return 7;
    if (status == AuthStatus::RateLimited) return 5;
    return static_cast<std::uint8_t>(status);
}

struct SecurityChallenge
{
    std::string token; // private connection handle, never sent on the game protocol
    std::string code;  // transient mail payload; never logged or persisted in clear
    std::string email;
};

enum class SecurityCodeResult { Incorrect, Correct, Unavailable };

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
    std::optional<SecurityChallenge> security_challenge;
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
    // dwUserID = ?; verify via the same BCrypt wire-credential policy as
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

    // Validate only the current connection's challenge, with expiry and attempt
    // bounds. Correct authorizes one subsequent LOGIN; it creates no session.
    virtual SecurityCodeResult VerifySecurityCode(const std::string& token,
                                                 const std::string& client_ip,
                                                 const std::string& code) = 0;
    virtual void CancelSecurityChallenge(const std::string& token) = 0;
};

} // namespace tloginsvr::services
