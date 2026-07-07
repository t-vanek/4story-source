#pragma once

// FakeBowRepository — in-memory IBowRepository with seeded reads and
// write-side call traces.

#include "services/bow_repository.h"

#include <mutex>
#include <utility>

namespace tworldsvr {

class FakeBowRepository : public IBowRepository
{
public:
    void SeedSettings(const BowSettings& s);
    void SeedStartTime(std::uint32_t clt);

    std::optional<BowSettings> LoadSettings() override;
    std::vector<std::uint32_t> LoadStartTimes() override;
    void AddPlayer(std::uint32_t char_id,
                   std::uint32_t user_id) override;
    void ClearPlayers() override;
    void DeleteSinglePlayer(std::uint32_t user_id) override;

    std::vector<std::pair<std::uint32_t, std::uint32_t>>
        AddCalls() const;
    int ClearCalls() const;
    std::vector<std::uint32_t> DeleteCalls() const;

private:
    mutable std::mutex m_mtx;
    std::optional<BowSettings> m_settings;
    std::vector<std::uint32_t> m_times;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> m_add_calls;
    int m_clear_calls = 0;
    std::vector<std::uint32_t> m_delete_calls;
};

} // namespace tworldsvr
