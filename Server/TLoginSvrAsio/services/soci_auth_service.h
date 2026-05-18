#pragma once

// SOCI-backed IAuthService. Talks to:
//   IPBLACKLIST_game   — IP banlist (CSPCheckIP equivalent)
//   TACCOUNT_PW        — credentials (CSPLogin's auth check)
//   TUSERPROTECTED     — user-level bans (CSPLogin's ban check)
//   TCURRENTUSER       — live-session marker (LR_DUPLICATE detection)
//   TLOG               — audit log insert on success
//
// Order matches legacy TLogin SP:
//   1. IP banned          → AuthStatus::IpBanned (LR_IPBLOCK)
//   2. User exists?       → AuthStatus::NoUser   (LR_NOUSER)
//   3. Password match?    → AuthStatus::WrongPassword (LR_INVALIDPASSWD)
//   4. User-level ban?    → AuthStatus::Banned (LR_BLOCK)
//   5. Duplicate session? → AuthStatus::Duplicate (LR_DUPLICATE)
//   6. else → success: insert TCURRENTUSER, insert TLOG, return key
//
// Password hashing: rows with $2a$ / $2b$ / $2y$ prefix are treated as
// BCrypt and verified via libsodium / crypt_blowfish; everything else
// is treated as legacy plaintext with transparent upgrade-on-success
// (rewrites the row with a fresh BCrypt hash). The upgrade path is
// Phase C — for now plaintext rows match directly.

#include "auth_service.h"

#include <memory>
#include <string>

namespace fourstory::db { class SessionPool; }

namespace tloginsvr::services {

class SociAuthService : public IAuthService
{
public:
    // `pool` is non-owning; lifetime must exceed this service.
    explicit SociAuthService(fourstory::db::SessionPool& pool);

    AuthResult Authenticate(const AuthRequest& req) override;

    // Mirrors legacy CSPAgreement SP — upserts TUSERINFOTABLE.bAgreement=1
    // for the user. Idempotent — re-calling for an already-agreed user
    // is a harmless no-op.
    void SetAgreement(std::int32_t user_id) override;

    // Mirrors legacy CSPCheckPasswd SP: SELECT szPasswd FROM TACCOUNT_PW
    // WHERE dwUserID = ?; compare via the same BCrypt/plaintext path as
    // Authenticate. Used by CS_DELCHAR_REQ to confirm intent before
    // destructive ops. Returns false on user_id=0, missing row, NULL
    // password, or mismatch.
    bool VerifyPassword(std::int32_t user_id,
                        const std::string& password) override;

    // Mirrors legacy CSPTestLogin SP: picks a random dwUserID from
    // TTESTLOGINUSER, looks up the matching row in TACCOUNT_PW, and
    // returns it as a Success result with a fresh session_key (no
    // password / agreement check). Performs the same TCURRENTUSER +
    // TLOG inserts as a normal login so disconnect cleanup works.
    AuthResult AuthenticateTest(const std::string& client_ip) override;
    bool VerifySecurityCode(std::int32_t user_id,
                            const std::string& code) override;
    std::string IssueSecurityCode(std::int32_t user_id) override;
    std::optional<EmailRecord> LookupEmail(std::int32_t user_id) override;
    bool IsTrustedIp(std::int32_t user_id,
                     const std::string& client_ip) override;
    void AddTrustedIp(std::int32_t user_id,
                      const std::string& client_ip) override;
    std::uint32_t CompleteSecurityLogin(std::int32_t user_id,
                                        const std::string& client_ip) override;

private:
    fourstory::db::SessionPool& m_pool;
};

} // namespace tloginsvr::services
