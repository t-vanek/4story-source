#pragma once

// FakeAuthService — TEST-ONLY in-memory IAuthService implementation.
//
// Used by:
//   * the ctest suite (deterministic seeded users)
//   * dev mode when no DB connection string is configured (the server
//     binary boots with a one-user fake account so smoke-testing
//     wire flow doesn't require a live DB on the dev box)
//
// **Not for production.** Production deploys MUST configure
// [database] in tloginsvr.toml so SociAuthService takes over —
// FakeAuthService stores passwords in plaintext memory and has no
// persistence, rate-limit composition, or audit interaction.

#include "auth_service.h"

#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace tloginsvr::services {

class FakeAuthService : public IAuthService
{
public:
    FakeAuthService() = default;

    // Seed methods — call before serving traffic. Thread-safe but
    // intended for setup, not hot path.
    void AddUser(std::string user_id, std::string password, std::int32_t db_id,
                 std::uint8_t create_char_count = 6);
    void BanUser(std::int32_t db_id, std::string reason);
    void BanIp(std::string ip);

    // IAuthService
    AuthResult Authenticate(const AuthRequest& req) override;
    void SetAgreement(std::int32_t user_id) override;
    bool VerifyPassword(std::int32_t user_id,
                        const std::string& password) override;
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

    // Test seeds for 2FA state.
    void SetUserEmail(std::int32_t user_id, std::string email,
                      bool two_factor_enabled);
    void SeedTrustedIp(std::int32_t user_id, std::string ip);

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
    std::unordered_map<std::int32_t, std::string>      m_security_codes; // uid → code
    std::unordered_map<std::int32_t, EmailRecord>      m_emails;
    std::unordered_set<std::string>                    m_trusted_ips; // "uid|ip"
    std::uint32_t                                      m_next_session_key = 1;
};

} // namespace tloginsvr::services
