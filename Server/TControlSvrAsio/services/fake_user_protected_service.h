#pragma once

#include "user_protected_service.h"

#include <vector>

namespace tcontrolsvr {

class FakeUserProtectedService final : public IUserProtectedService
{
public:
    struct Ban
    {
        std::string  user_id;
        std::uint32_t duration_days = 0;
        std::string  reason;
        std::uint8_t permanent = 0;
        std::string  operator_id;
    };

    std::uint8_t AddBan(const std::string& user_id,
                        std::uint32_t duration_days,
                        const std::string& reason,
                        std::uint8_t permanent,
                        const std::string& operator_id) override
    {
        m_bans.push_back({user_id, duration_days, reason, permanent,
                          operator_id});
        return m_ret;
    }

    // Test hook — pin the SP return code for one assertion.
    void SetReturn(std::uint8_t ret) { m_ret = ret; }

    const std::vector<Ban>& Bans() const { return m_bans; }

private:
    std::uint8_t      m_ret = 1;
    std::vector<Ban>  m_bans;
};

} // namespace tcontrolsvr
