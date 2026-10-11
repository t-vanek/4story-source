#include "event_registry.h"

#include <mutex>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>

namespace tworldsvr {

boost::asio::awaitable<EventRegistry::DeliveryLease> EventRegistry::AcquireDelivery() {
    const auto executor=co_await boost::asio::this_coro::executor;
    // World handlers run on one reactor. Lazy creation also keeps the plain
    // registry usable by offline callers that never perform network delivery.
    if(!m_delivery) {
        m_delivery=std::make_shared<DeliveryPermit>(executor,1);
        m_delivery->try_send(boost::system::error_code{},0);
    }
    co_await m_delivery->async_receive(boost::asio::use_awaitable);
    co_return DeliveryLease(m_delivery);
}

void EventRegistry::Set(TEventInfo info)
{
    const std::uint32_t key = info.dw_index;
    std::unique_lock lk(m_lock);
    m_events.insert_or_assign(key, std::move(info));
}

bool EventRegistry::Erase(std::uint32_t dw_index)
{
    std::unique_lock lk(m_lock);
    return m_events.erase(dw_index) != 0;
}

std::vector<TEventInfo> EventRegistry::Snapshot() const
{
    std::shared_lock lk(m_lock);
    std::vector<TEventInfo> out;
    out.reserve(m_events.size());
    for (const auto& [_, info] : m_events)
        out.push_back(info);
    return out;
}

std::size_t EventRegistry::Size() const
{
    std::shared_lock lk(m_lock);
    return m_events.size();
}

} // namespace tworldsvr
