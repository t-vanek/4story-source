#pragma once

// FakeLuckyEventRepository — in-memory ILuckyEventRepository.

#include "services/lucky_event_repository.h"

#include <mutex>

namespace tworldsvr {

class FakeLuckyEventRepository : public ILuckyEventRepository
{
public:
    void SeedRow(LuckyEvent e);
    void SetNextAddId(std::uint16_t id);
    // Item-name resolution stub: name = "item<ID>" for non-zero ids.
    static std::string NameOf(std::uint16_t id);

    std::vector<LuckyEvent> List(std::uint8_t day) override;
    std::optional<LuckyEventUpdateResult>
        Update(std::uint8_t type, const LuckyEvent& event) override;

    std::vector<std::pair<std::uint8_t, LuckyEvent>> UpdateCalls() const;

private:
    mutable std::mutex      m_mtx;
    std::vector<LuckyEvent> m_rows;
    std::uint16_t           m_next_id = 500;
    std::vector<std::pair<std::uint8_t, LuckyEvent>> m_update_calls;
};

} // namespace tworldsvr
