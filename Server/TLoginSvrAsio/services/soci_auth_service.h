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

namespace tloginsvr::db { class SessionPool; }

namespace tloginsvr::services {

class SociAuthService : public IAuthService
{
public:
    // `pool` is non-owning; lifetime must exceed this service.
    explicit SociAuthService(db::SessionPool& pool);

    AuthResult Authenticate(const AuthRequest& req) override;

private:
    db::SessionPool& m_pool;
};

} // namespace tloginsvr::services
