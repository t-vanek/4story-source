#include "cash_item_sale_registry.h"

#include <mutex>

namespace tworldsvr {

void CashItemSaleRegistry::Set(TCashItemSaleEvent event)
{
    const std::uint32_t key = event.dw_index;
    std::unique_lock lk(m_lock);
    m_events.insert_or_assign(key, std::move(event));
}

bool CashItemSaleRegistry::Deactivate(std::uint32_t dw_index,
                                      TCashItemSaleEvent& out)
{
    std::unique_lock lk(m_lock);
    auto it = m_events.find(dw_index);
    if (it == m_events.end())
        return false;
    it->second.value = 0;
    for (auto& item : it->second.items)
        item.sale_value = 0;
    out = it->second;
    return true;
}

std::vector<TCashItemSaleEvent> CashItemSaleRegistry::Snapshot() const
{
    std::shared_lock lk(m_lock);
    std::vector<TCashItemSaleEvent> out;
    out.reserve(m_events.size());
    for (const auto& [_, ev] : m_events)
        out.push_back(ev);
    return out;
}

bool CashItemSaleRegistry::Erase(std::uint32_t dw_index)
{
    std::unique_lock lk(m_lock);
    return m_events.erase(dw_index) != 0;
}

std::size_t CashItemSaleRegistry::Size() const
{
    std::shared_lock lk(m_lock);
    return m_events.size();
}

} // namespace tworldsvr
