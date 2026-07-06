#pragma once

// FakeWarOpsRepository — in-memory IWarOpsRepository for tests.

#include "services/war_ops_repository.h"

#include <mutex>
#include <tuple>

namespace tworldsvr {

class FakeWarOpsRepository : public IWarOpsRepository
{
public:
    void SeedActiveChar(ActiveCharRow row);

    std::vector<ActiveCharRow>
        LoadActiveChars(std::int64_t prune_before_unix) override;
    bool SaveCastleApplicant(std::uint16_t castle, std::uint32_t char_id,
                             std::uint8_t camp) override;

    std::vector<std::int64_t> LoadCalls() const;
    std::vector<std::tuple<std::uint16_t, std::uint32_t, std::uint8_t>>
        SaveCalls() const;

private:
    mutable std::mutex          m_mtx;
    std::vector<ActiveCharRow>  m_rows;
    std::vector<std::int64_t>   m_load_calls;
    std::vector<std::tuple<std::uint16_t, std::uint32_t, std::uint8_t>>
        m_save_calls;
};

} // namespace tworldsvr
