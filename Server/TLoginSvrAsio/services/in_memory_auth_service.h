#pragma once

// In-memory authentication backend. No DB, no persistence. Used by:
//   * the test suite (deterministic seeded users)
//   * dev mode when no DB connection string is configured (the server
//     binary boots with a one-user fake account so smoke-testing
//     wire flow doesn't require Postgres on the dev box)
//
// Implementation is intentionally trivial — no rate limiting, no
// session tracking, no concurrency hardening beyond a single mutex.
// Production use should compose with ConnectionRegistry +
// rate-limiter outside the service.

#include "auth_service.h"

#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace tloginsvr::services {

class InMemoryAuthService : public IAuthService
{
public:
    InMemoryAuthService() = default;

    // Seed methods — call before serving traffic. Thread-safe but
    // intended for setup, not hot path.
    void AddUser(std::string user_id, std::string password, std::int32_t db_id,
                 std::uint8_t create_char_count = 6);
    void BanUser(std::int32_t db_id, std::string reason);
    void BanIp(std::string ip);

    // IAuthService
    AuthResult Authenticate(const AuthRequest& req) override;
    void SetAgreement(std::int32_t user_id) override;

    // Test introspection — true after SetAgreement() was called for user.
    bool HasAgreed(std::int32_t user_id) const;

private:
    struct UserRecord
    {
        std::string   password;  // plaintext for fake — production uses BCrypt
        std::int32_t  db_id;
        std::uint8_t  create_char_count;
    };

    mutable std::mutex                                 m_mtx;
    std::unordered_map<std::string, UserRecord>        m_users;     // user_id → record
    std::unordered_map<std::int32_t, std::string>      m_user_bans; // db_id → reason
    std::unordered_set<std::string>                    m_ip_bans;
    std::unordered_set<std::int32_t>                   m_agreed;
    std::uint32_t                                      m_next_session_key = 1;
};

} // namespace tloginsvr::services
