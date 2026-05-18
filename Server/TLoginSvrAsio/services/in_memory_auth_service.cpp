#include "in_memory_auth_service.h"

#include <utility>

namespace tloginsvr::services {

void InMemoryAuthService::AddUser(std::string user_id, std::string password,
                                  std::int32_t db_id, std::uint8_t create_char_count)
{
    std::lock_guard<std::mutex> lock(m_mtx);
    m_users[std::move(user_id)] = UserRecord{
        .password = std::move(password),
        .db_id = db_id,
        .create_char_count = create_char_count,
    };
}

void InMemoryAuthService::BanUser(std::int32_t db_id, std::string reason)
{
    std::lock_guard<std::mutex> lock(m_mtx);
    m_user_bans[db_id] = std::move(reason);
}

void InMemoryAuthService::BanIp(std::string ip)
{
    std::lock_guard<std::mutex> lock(m_mtx);
    m_ip_bans.insert(std::move(ip));
}

AuthResult InMemoryAuthService::Authenticate(const AuthRequest& req)
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

void InMemoryAuthService::SetAgreement(std::int32_t user_id)
{
    if (user_id == 0) return;
    std::lock_guard<std::mutex> lock(m_mtx);
    m_agreed.insert(user_id);
}

bool InMemoryAuthService::HasAgreed(std::int32_t user_id) const
{
    std::lock_guard<std::mutex> lock(m_mtx);
    return m_agreed.contains(user_id);
}

} // namespace tloginsvr::services
