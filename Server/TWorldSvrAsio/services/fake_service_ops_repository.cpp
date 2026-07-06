#include "services/fake_service_ops_repository.h"

namespace tworldsvr {

bool FakeServiceOpsRepository::SaveHelpMessage(std::uint8_t id,
                                               std::int64_t start_unix,
                                               std::int64_t end_unix,
                                               const std::string& message)
{
    std::lock_guard g(m_mtx);
    m_help.push_back({id, start_unix, end_unix, message});
    return true;
}

bool FakeServiceOpsRepository::ClearMapCurrentUser(std::uint8_t group_id,
                                                   std::uint8_t server_id,
                                                   std::uint8_t svr_type)
{
    std::lock_guard g(m_mtx);
    m_clear.push_back({group_id, server_id, svr_type});
    return true;
}

std::vector<FakeServiceOpsRepository::HelpMessage>
FakeServiceOpsRepository::HelpMessages() const
{
    std::lock_guard g(m_mtx);
    return m_help;
}

std::vector<FakeServiceOpsRepository::ClearCall>
FakeServiceOpsRepository::ClearCalls() const
{
    std::lock_guard g(m_mtx);
    return m_clear;
}

} // namespace tworldsvr
