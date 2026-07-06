#include "services/fake_war_ops_repository.h"

namespace tworldsvr {

void FakeWarOpsRepository::SeedActiveChar(ActiveCharRow row)
{
    std::lock_guard g(m_mtx);
    m_rows.push_back(row);
}

std::vector<ActiveCharRow>
FakeWarOpsRepository::LoadActiveChars(std::int64_t prune_before_unix)
{
    std::lock_guard g(m_mtx);
    m_load_calls.push_back(prune_before_unix);
    return m_rows;
}

bool FakeWarOpsRepository::SaveCastleApplicant(std::uint16_t castle,
                                               std::uint32_t char_id,
                                               std::uint8_t  camp)
{
    std::lock_guard g(m_mtx);
    m_save_calls.emplace_back(castle, char_id, camp);
    return true;
}

std::vector<std::int64_t> FakeWarOpsRepository::LoadCalls() const
{
    std::lock_guard g(m_mtx);
    return m_load_calls;
}

std::vector<std::tuple<std::uint16_t, std::uint32_t, std::uint8_t>>
FakeWarOpsRepository::SaveCalls() const
{
    std::lock_guard g(m_mtx);
    return m_save_calls;
}

} // namespace tworldsvr
