#include "services/fake_bow_repository.h"

namespace tworldsvr {

void FakeBowRepository::SeedSettings(const BowSettings& s)
{
    std::lock_guard g(m_mtx);
    m_settings = s;
}

void FakeBowRepository::SeedStartTime(std::uint32_t clt)
{
    std::lock_guard g(m_mtx);
    m_times.push_back(clt);
}

std::optional<BowSettings> FakeBowRepository::LoadSettings()
{
    std::lock_guard g(m_mtx);
    return m_settings;
}

std::vector<std::uint32_t> FakeBowRepository::LoadStartTimes()
{
    std::lock_guard g(m_mtx);
    return m_times;
}

void FakeBowRepository::AddPlayer(std::uint32_t char_id,
                                  std::uint32_t user_id)
{
    std::lock_guard g(m_mtx);
    m_add_calls.emplace_back(char_id, user_id);
}

void FakeBowRepository::ClearPlayers()
{
    std::lock_guard g(m_mtx);
    ++m_clear_calls;
}

void FakeBowRepository::DeleteSinglePlayer(std::uint32_t user_id)
{
    std::lock_guard g(m_mtx);
    m_delete_calls.push_back(user_id);
}

std::vector<std::pair<std::uint32_t, std::uint32_t>>
FakeBowRepository::AddCalls() const
{
    std::lock_guard g(m_mtx);
    return m_add_calls;
}

int FakeBowRepository::ClearCalls() const
{
    std::lock_guard g(m_mtx);
    return m_clear_calls;
}

std::vector<std::uint32_t> FakeBowRepository::DeleteCalls() const
{
    std::lock_guard g(m_mtx);
    return m_delete_calls;
}

} // namespace tworldsvr
