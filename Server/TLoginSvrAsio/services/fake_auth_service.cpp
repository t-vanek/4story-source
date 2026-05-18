#include "fake_auth_service.h"

#include <utility>

namespace tloginsvr::services {

void FakeAuthService::AddUser(std::string user_id, std::string password,
                              std::int32_t db_id, std::uint8_t create_char_count)
{
    std::lock_guard<std::mutex> lock(m_mtx);
    m_users[std::move(user_id)] = UserRecord{
        .password = std::move(password),
        .db_id = db_id,
        .create_char_count = create_char_count,
    };
}

void FakeAuthService::BanUser(std::int32_t db_id, std::string reason)
{
    std::lock_guard<std::mutex> lock(m_mtx);
    m_user_bans[db_id] = std::move(reason);
}

void FakeAuthService::BanIp(std::string ip)
{
    std::lock_guard<std::mutex> lock(m_mtx);
    m_ip_bans.insert(std::move(ip));
}

AuthResult FakeAuthService::Authenticate(const AuthRequest& req)
{
    std::lock_guard<std::mutex> lock(m_mtx);

    // Order matches legacy TLogin SP (Server/TLoginSvr DB plan):
    //   1. IP banlist  →  LR_IPBLOCK
    //   2. User exists →  LR_NOUSER
    //   3. Password    →  LR_INVALIDPASSWD
    //   4. User-level ban → LR_BLOCK
    //   5. Otherwise  → LR_SUCCESS

    if (m_ip_bans.contains(req.client_ip))
    {
        return AuthResult{ .status = AuthStatus::IpBanned };
    }

    const auto it = m_users.find(req.user_id);
    if (it == m_users.end())
    {
        return AuthResult{ .status = AuthStatus::NoUser };
    }

    const auto& rec = it->second;
    if (rec.password != req.password)
    {
        return AuthResult{ .status = AuthStatus::WrongPassword };
    }

    const auto ban_it = m_user_bans.find(rec.db_id);
    if (ban_it != m_user_bans.end())
    {
        return AuthResult{
            .status = AuthStatus::Banned,
            .user_id = rec.db_id,
            .ban_reason = ban_it->second,
        };
    }

    const std::uint32_t key = m_next_session_key++;
    return AuthResult{
        .status = AuthStatus::Success,
        .user_id = rec.db_id,
        .session_key = key,
        .create_char_count = rec.create_char_count,
        .in_pc_bang = 0,
        .premium_id = 0,
    };
}

void FakeAuthService::SetAgreement(std::int32_t user_id)
{
    if (user_id == 0) return;
    std::lock_guard<std::mutex> lock(m_mtx);
    m_agreed.insert(user_id);
}

bool FakeAuthService::HasAgreed(std::int32_t user_id) const
{
    std::lock_guard<std::mutex> lock(m_mtx);
    return m_agreed.contains(user_id);
}

bool FakeAuthService::VerifyPassword(std::int32_t user_id,
                                    const std::string& password)
{
    if (user_id == 0) return false;
    std::lock_guard<std::mutex> lock(m_mtx);
    for (const auto& [k, rec] : m_users)
    {
        if (rec.db_id == user_id) return rec.password == password;
    }
    return false;
}

namespace {
std::string Upper(std::string s)
{
    for (auto& c : s) if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
    return s;
}
} // namespace

bool FakeAuthService::VerifySecurityCode(std::int32_t user_id,
                                        const std::string& code)
{
    if (user_id == 0 || code.empty()) return false;
    std::lock_guard<std::mutex> lock(m_mtx);
    auto it = m_security_codes.find(user_id);
    if (it == m_security_codes.end() || it->second.empty()) return false;
    const bool ok = Upper(code) == Upper(it->second);
    if (ok) m_security_codes.erase(it);
    return ok;
}

std::string FakeAuthService::IssueSecurityCode(std::int32_t user_id)
{
    if (user_id == 0) return {};
    // Deterministic fake code per user — easier for tests to assert against
    // than a random string.
    std::string code = "TEST" + std::to_string(user_id);
    std::lock_guard<std::mutex> lock(m_mtx);
    m_security_codes[user_id] = code;
    return code;
}

std::optional<IAuthService::EmailRecord>
FakeAuthService::LookupEmail(std::int32_t user_id)
{
    if (user_id == 0) return std::nullopt;
    std::lock_guard<std::mutex> lock(m_mtx);
    if (auto it = m_emails.find(user_id); it != m_emails.end())
        return it->second;
    return std::nullopt;
}

bool FakeAuthService::IsTrustedIp(std::int32_t user_id, const std::string& client_ip)
{
    if (user_id == 0 || client_ip.empty()) return false;
    std::lock_guard<std::mutex> lock(m_mtx);
    return m_trusted_ips.contains(std::to_string(user_id) + "|" + client_ip);
}

void FakeAuthService::AddTrustedIp(std::int32_t user_id, const std::string& client_ip)
{
    if (user_id == 0 || client_ip.empty()) return;
    std::lock_guard<std::mutex> lock(m_mtx);
    m_trusted_ips.insert(std::to_string(user_id) + "|" + client_ip);
}

std::uint32_t FakeAuthService::CompleteSecurityLogin(std::int32_t user_id,
                                                    const std::string& /*client_ip*/)
{
    if (user_id == 0) return 0;
    std::lock_guard<std::mutex> lock(m_mtx);
    return m_next_session_key++;
}

void FakeAuthService::SetUserEmail(std::int32_t user_id, std::string email,
                                  bool two_factor_enabled)
{
    std::lock_guard<std::mutex> lock(m_mtx);
    m_emails[user_id] = EmailRecord{
        .email = std::move(email),
        .two_factor_enabled = two_factor_enabled,
    };
}

void FakeAuthService::SeedTrustedIp(std::int32_t user_id, std::string ip)
{
    std::lock_guard<std::mutex> lock(m_mtx);
    m_trusted_ips.insert(std::to_string(user_id) + "|" + ip);
}

AuthResult FakeAuthService::AuthenticateTest(const std::string& /*client_ip*/)
{
    // Test-pool login — pick the first seeded user. Tests can seed a
    // dedicated user via AddUser to make the pick deterministic.
    std::lock_guard<std::mutex> lock(m_mtx);
    if (m_users.empty())
    {
        return AuthResult{ .status = AuthStatus::InternalError };
    }
    const auto& rec = m_users.begin()->second;
    const std::uint32_t key = m_next_session_key++;
    return AuthResult{
        .status = AuthStatus::Success,
        .user_id = rec.db_id,
        .session_key = key,
        .create_char_count = rec.create_char_count,
    };
}

} // namespace tloginsvr::services
