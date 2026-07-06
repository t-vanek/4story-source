#include "services/fake_lucky_event_repository.h"

namespace tworldsvr {

void FakeLuckyEventRepository::SeedRow(LuckyEvent e)
{
    std::lock_guard g(m_mtx);
    m_rows.push_back(std::move(e));
}

void FakeLuckyEventRepository::SetNextAddId(std::uint16_t id)
{
    std::lock_guard g(m_mtx);
    m_next_id = id;
}

std::string FakeLuckyEventRepository::NameOf(std::uint16_t id)
{
    return id ? "item" + std::to_string(id) : "";
}

std::vector<LuckyEvent> FakeLuckyEventRepository::List(std::uint8_t day)
{
    std::lock_guard g(m_mtx);
    std::vector<LuckyEvent> out;
    for (const auto& r : m_rows)
        if (r.day == day)
        {
            LuckyEvent e = r;
            for (int i = 0; i < 5; ++i)
                e.item_name[i] = NameOf(e.item_id[i]);
            out.push_back(std::move(e));
        }
    return out;
}

std::optional<LuckyEventUpdateResult>
FakeLuckyEventRepository::Update(std::uint8_t type,
                                 const LuckyEvent& event)
{
    std::lock_guard g(m_mtx);
    m_update_calls.emplace_back(type, event);
    LuckyEventUpdateResult res{};
    res.ret = 0;                        // success
    res.event = event;
    if (type == kEkAdd)
        res.event.id = m_next_id++;
    for (int i = 0; i < 5; ++i)
        res.event.item_name[i] = NameOf(res.event.item_id[i]);
    return res;
}

std::vector<std::pair<std::uint8_t, LuckyEvent>>
FakeLuckyEventRepository::UpdateCalls() const
{
    std::lock_guard g(m_mtx);
    return m_update_calls;
}

std::vector<LuckyEvent> FakeLuckyEventRepository::ListAll()
{
    std::lock_guard g(m_mtx);
    return m_rows;
}

} // namespace tworldsvr
