#pragma once

#include "event_repository.h"

#include <unordered_map>
#include <utility>

namespace tcontrolsvr {

class FakeEventRepository final : public IEventRepository
{
public:
    void AddEvent(EventInfo ev)
    {
        m_events[ev.index] = std::move(ev);
    }
    void AddCashItem(CashItem ci)
    {
        m_cash_items.push_back(std::move(ci));
    }
    void SetPersistReturn(std::uint8_t rc) { m_persist_rc = rc; }

    std::vector<EventInfo> LoadAll() override
    {
        std::vector<EventInfo> out;
        out.reserve(m_events.size());
        for (const auto& [k, v] : m_events) out.push_back(v);
        return out;
    }

    std::vector<CashItem> ListCashItems() override
    {
        return m_cash_items;
    }

    std::uint8_t Persist(const EventInfo& ev,
                         std::uint8_t op,
                         const std::string& /*value_blob*/) override
    {
        if (m_persist_rc != 0) return m_persist_rc;
        if (op == event_op::kDel)
            m_events.erase(ev.index);
        else
            m_events[ev.index] = ev;
        return 0;
    }

    std::size_t Size() const { return m_events.size(); }

private:
    std::unordered_map<std::uint32_t, EventInfo>  m_events;
    std::vector<CashItem>                         m_cash_items;
    std::uint8_t                                  m_persist_rc = 0;
};

} // namespace tcontrolsvr
