#pragma once

// FakeItemStateRepository — in-memory IItemStateRepository for tests.
// Every ChangeState attempt is recorded in `calls` (successful or
// not); ids seeded via FailOn() return false, which lets tests drive
// the handler's stop-at-first-failure loop.

#include "services/item_state_repository.h"

#include <mutex>
#include <unordered_set>
#include <vector>

namespace tworldsvr {

class FakeItemStateRepository : public IItemStateRepository
{
public:
    // Make ChangeState(item_id, …) fail from now on.
    void FailOn(std::uint16_t item_id);

    bool ChangeState(std::uint16_t item_id,
                     std::uint8_t  init_state) override;

    // Snapshot of every attempted change, in call order.
    std::vector<ItemStateChange> Calls() const;

private:
    mutable std::mutex                  m_mtx;
    std::unordered_set<std::uint16_t>   m_fail_ids;
    std::vector<ItemStateChange>        m_calls;
};

} // namespace tworldsvr
