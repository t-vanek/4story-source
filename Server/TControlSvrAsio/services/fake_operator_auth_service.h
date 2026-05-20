#pragma once

// In-memory IOperatorAuthService for tests and bring-up. Operators
// are registered with `AddOperator(id, password, authority)`. The
// fake matches the production SP's contract: authority 0 means
// "rejected", any other value is the resolved MANAGER_CLASS.

#include "operator_auth_service.h"

#include <string>
#include <unordered_map>

namespace tcontrolsvr {

class FakeOperatorAuthService final : public IOperatorAuthService
{
public:
    void AddOperator(std::string id,
                     std::string password,
                     std::uint8_t authority);

    OperatorAuthResult Authenticate(const std::string& user_id,
                                    const std::string& password) override;

private:
    struct Entry
    {
        std::string  password;
        std::uint8_t authority = 0;
    };
    std::unordered_map<std::string, Entry> m_users;
};

} // namespace tcontrolsvr
