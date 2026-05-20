#include "event_registry.h"

#include <ctime>
#include <utility>

namespace tcontrolsvr {

namespace {

// Time-of-day in minutes from a unix epoch (legacy uses
// CTime::GetHour()*60 + GetMinute()). Used only when both events
// are "daily" (part_time == 0).
int MinutesOfDay(std::int64_t unix_seconds)
{
    std::time_t t = static_cast<std::time_t>(unix_seconds);
    std::tm tm{};
    localtime_r(&t, &tm);
    return tm.tm_hour * 60 + tm.tm_min;
}

bool DateRangeOverlap(std::int64_t a_start, std::int64_t a_end,
                      std::int64_t b_start, std::int64_t b_end)
{
    return (a_start <= b_end) && (b_start <= a_end);
}

} // namespace

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
