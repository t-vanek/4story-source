#include "services/fake_item_state_repository.h"

namespace tworldsvr {

void FakeItemStateRepository::FailOn(std::uint16_t item_id)
{
    std::lock_guard g(m_mtx);
    m_fail_ids.insert(item_id);
}

bool FakeItemStateRepository::ChangeState(std::uint16_t item_id,
                                          std::uint8_t  init_state)
{
    std::lock_guard g(m_mtx);
    m_calls.push_back({item_id, init_state});
    return m_fail_ids.count(item_id) == 0;
}

std::vector<ItemStateChange> FakeItemStateRepository::Calls() const
{
    std::lock_guard g(m_mtx);
    return m_calls;
}

} // namespace tworldsvr
