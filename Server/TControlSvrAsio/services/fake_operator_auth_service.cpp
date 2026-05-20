#include "fake_operator_auth_service.h"

namespace tcontrolsvr {

void FakeOperatorAuthService::AddOperator(std::string id,
                                          std::string password,
                                          std::uint8_t authority)
{
    m_users[std::move(id)] = Entry{std::move(password), authority};
}

OperatorAuthResult
FakeOperatorAuthService::Authenticate(const std::string& user_id,
                                      const std::string& password)
{
    OperatorAuthResult res{};
    auto it = m_users.find(user_id);
    if (it == m_users.end()) return res;
    if (it->second.password != password) return res;
    if (it->second.authority == 0) return res;
    res.ok        = true;
    res.authority = it->second.authority;
    return res;
}

} // namespace tcontrolsvr
