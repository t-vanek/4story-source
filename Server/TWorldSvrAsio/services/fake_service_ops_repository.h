#pragma once

// FakeServiceOpsRepository — in-memory IServiceOpsRepository for
// tests. Records every call in order; never fails.

#include "services/service_ops_repository.h"

#include <mutex>
#include <vector>

namespace tworldsvr {

class FakeServiceOpsRepository : public IServiceOpsRepository
{
public:
    struct HelpMessage
    {
        std::uint8_t id = 0;
        std::int64_t start_unix = 0;
        std::int64_t end_unix = 0;
        std::string  message;
    };
    struct ClearCall
    {
        std::uint8_t group_id = 0;
        std::uint8_t server_id = 0;
        std::uint8_t svr_type = 0;
    };

    bool SaveHelpMessage(std::uint8_t id, std::int64_t start_unix,
                         std::int64_t end_unix,
                         const std::string& message) override;
    bool ClearMapCurrentUser(std::uint8_t group_id,
                             std::uint8_t server_id,
                             std::uint8_t svr_type) override;

    std::vector<HelpMessage> HelpMessages() const;
    std::vector<ClearCall>   ClearCalls() const;

private:
    mutable std::mutex        m_mtx;
    std::vector<HelpMessage>  m_help;
    std::vector<ClearCall>    m_clear;
};

} // namespace tworldsvr
