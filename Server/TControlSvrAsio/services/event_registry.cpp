#include "event_registry.h"

#include <ctime>
#include <utility>
#include <limits>
#include <stdexcept>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>

namespace tcontrolsvr {

namespace {

// Time-of-day in minutes from a unix epoch (legacy uses
// CTime::GetHour()*60 + GetMinute()). Used only when both events
// are "daily" (part_time == 0).
int MinutesOfDay(std::int64_t unix_seconds)
{
    std::time_t t = static_cast<std::time_t>(unix_seconds);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    return tm.tm_hour * 60 + tm.tm_min;
}

bool DateRangeOverlap(std::int64_t a_start, std::int64_t a_end,
                      std::int64_t b_start, std::int64_t b_end)
{
    return (a_start <= b_end) && (b_start <= a_end);
}

} // namespace

boost::asio::awaitable<EventRegistry::MutationLease> EventRegistry::AcquireMutation()
{
    const auto executor=co_await boost::asio::this_coro::executor;
    if(!m_mutation_signal)m_mutation_signal=std::make_shared<MutationSignal>(executor,1);
    // A buffered signal can predate this attempt. Re-check the state after
    // every wake so neither a late signal nor a competing waiter owns it twice.
    while(!TryBeginMutation())co_await m_mutation_signal->async_receive(boost::asio::use_awaitable);
    co_return MutationLease(*this);
}

void EventRegistry::EndMutation()
{
    m_mutating=false;
    if(m_mutation_signal)m_mutation_signal->try_send(boost::system::error_code{},0);
}

void EventRegistry::LoadFrom(std::vector<EventInfo> events)
{
    m_events.clear();
    m_next_index = 0;
    for (auto& ev : events)
    {
        if (ev.index > m_next_index) m_next_index = ev.index;
        m_events.emplace(ev.index, std::move(ev));
    }
}

EventInfo* EventRegistry::Find(std::uint32_t index)
{
    auto it = m_events.find(index);
    return (it == m_events.end()) ? nullptr : &it->second;
}

const EventInfo* EventRegistry::Find(std::uint32_t index) const
{
    auto it = m_events.find(index);
    return (it == m_events.end()) ? nullptr : &it->second;
}

void EventRegistry::Upsert(EventInfo ev)
{
    if (ev.index > m_next_index) m_next_index = ev.index;
    m_events[ev.index] = std::move(ev);
}

bool EventRegistry::Erase(std::uint32_t index)
{
    return m_events.erase(index) > 0;
}

std::uint32_t EventRegistry::NextIndex()
{
    if(m_next_index==std::numeric_limits<std::uint32_t>::max())
        throw std::runtime_error("Control event index exhausted");
    return ++m_next_index;
}

std::vector<EventInfo> EventRegistry::Snapshot() const
{
    std::vector<EventInfo> out;
    out.reserve(m_events.size());
    for (const auto& [k, v] : m_events) out.push_back(v);
    return out;
}

bool EventRegistry::OverlapsExisting(const EventInfo& candidate,
                                     std::uint32_t update_index) const
{
    for (const auto& [idx, ev] : m_events)
    {
        if (update_index != 0 && idx == update_index) continue;
        if (ev.kind != candidate.kind) continue;
        if (!DateRangeOverlap(candidate.start_unix, candidate.end_unix,
                              ev.start_unix, ev.end_unix))
            continue;
        // Date range collides. Time-of-day check applies only when
        // both events are daily (legacy behavior: term-vs-anything
        // collision is unconditional).
        if (ev.part_time != 0 || candidate.part_time != 0)
            return true;
        const int a_start = MinutesOfDay(candidate.start_unix);
        const int a_end   = MinutesOfDay(candidate.end_unix);
        const int b_start = MinutesOfDay(ev.start_unix);
        const int b_end   = MinutesOfDay(ev.end_unix);
        if ((b_start <= a_start && b_end >= a_start) ||
            (b_start <= a_end   && b_end >= a_end) ||
            (a_start <= b_start && a_end >= b_start) ||
            (a_start <= b_end   && a_end >= b_end))
            return true;
    }
    return false;
}

} // namespace tcontrolsvr
