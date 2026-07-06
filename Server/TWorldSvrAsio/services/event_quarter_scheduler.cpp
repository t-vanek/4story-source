#include "event_quarter_scheduler.h"

#include <cstdio>
#include <ctime>

namespace tworldsvr {

std::string BuildNetAnnounce(const std::string& body)
{
    char size[10]{};
    std::snprintf(size, sizeof(size), "%04X%04X", 0,
        static_cast<unsigned>(body.size()));
    return std::string(size) + body;
}

std::int64_t NextEventQuarterTime(std::uint8_t day, std::uint8_t hour,
                                  std::uint8_t minute,
                                  std::int64_t now_unix)
{
    if (day == 0 || day > 7 || hour > 24 || minute > 60)
        return -1;

    std::time_t now_t = static_cast<std::time_t>(now_unix);
    std::tm     lt{};
#ifdef _WIN32
    localtime_s(&lt, &now_t);
#else
    localtime_r(&now_t, &lt);
#endif
    // CTime::GetDayOfWeek: 1 = Sunday .. 7 = Saturday.
    const std::uint8_t cur_week =
        static_cast<std::uint8_t>(lt.tm_wday + 1);

    std::tm ev = lt;
    ev.tm_hour = hour;
    ev.tm_min  = minute;
    ev.tm_sec  = 0;
    ev.tm_isdst = -1;

    std::uint8_t target = day;
    if (cur_week > target)
        target = static_cast<std::uint8_t>(target + 7);
    const int day_delta = target - cur_week;

    std::time_t ev_t = std::mktime(&ev);
    if (ev_t == -1)
        return -1;
    ev_t += static_cast<std::time_t>(day_delta) * 24 * 60 * 60;
    if (ev_t < now_t)
        ev_t += 7 * 24 * 60 * 60;
    return static_cast<std::int64_t>(ev_t);
}

void EventQuarterScheduler::Index(const EventQuarterEntry& e)
{
    m_by_id.insert_or_assign(e.id, e);
    m_by_time.emplace(e.next_time, e.id);
}

void EventQuarterScheduler::LoadFrom(
    const std::vector<EventQuarterEntry>& rows, std::int64_t now_unix)
{
    std::lock_guard g(m_mtx);
    m_by_id.clear();
    m_by_time.clear();
    for (const auto& r : rows)
    {
        EventQuarterEntry e = r;
        e.announce  = e.announce.empty()
                          ? std::string{}
                          : BuildNetAnnounce(e.announce);
        e.notice    = false;
        e.next_time = NextEventQuarterTime(e.day, e.hour, e.minute,
                                           now_unix);
        Index(e);
    }
}

EventQuarterActions EventQuarterScheduler::Tick(std::int64_t now)
{
    std::lock_guard g(m_mtx);
    EventQuarterActions out{};

    auto it_time = m_by_time.begin();
    if (it_time == m_by_time.end())
        return out;
    auto it = m_by_id.find(it_time->second);
    if (it == m_by_id.end())
    {
        m_by_time.erase(it_time);       // orphaned index row
        return out;
    }
    EventQuarterEntry& e = it->second;
    if (e.next_time < 0)
        return out;                     // invalid slot never fires

    // Notice window: event − 5 min; presents-less entries announce
    // at the event time itself (legacy NoticeTime = EventTime).
    const std::int64_t notice_time =
        e.present.empty() ? e.next_time : e.next_time - 5 * 60;

    if (!e.notice && now >= notice_time && !e.announce.empty())
    {
        out.announce = e.announce;
        e.notice = true;
    }

    if (now >= e.next_time)
    {
        if (!e.present.empty())
        {
            EventQuarterActions::Present p{};
            p.day = e.day; p.hour = e.hour; p.minute = e.minute;
            p.present = e.present;
            out.present = std::move(p);
        }
        e.notice = false;
        m_by_time.erase(it_time);
        e.next_time = NextEventQuarterTime(e.day, e.hour, e.minute,
                                           now);
        m_by_time.emplace(e.next_time, e.id);
    }
    return out;
}

void EventQuarterScheduler::ApplyDel(std::uint16_t id)
{
    std::lock_guard g(m_mtx);
    auto it = m_by_id.find(id);
    if (it == m_by_id.end())
        return;
    for (auto t = m_by_time.begin(); t != m_by_time.end(); ++t)
        if (t->second == id)
        {
            m_by_time.erase(t);
            break;
        }
    m_by_id.erase(it);
}

void EventQuarterScheduler::ApplyAdd(const EventQuarterEntry& row,
                                     std::int64_t now_unix)
{
    std::lock_guard g(m_mtx);
    if (m_by_id.count(row.id))
        return;                          // legacy map::insert no-op
    EventQuarterEntry e = row;
    e.announce  = e.announce.empty() ? std::string{}
                                     : BuildNetAnnounce(e.announce);
    e.notice    = false;
    e.next_time = NextEventQuarterTime(e.day, e.hour, e.minute,
                                       now_unix);
    Index(e);
}

void EventQuarterScheduler::ApplyUpdate(const EventQuarterEntry& row,
                                        std::int64_t now_unix)
{
    std::lock_guard g(m_mtx);
    auto it = m_by_id.find(row.id);
    if (it == m_by_id.end())
        return;                          // legacy updates known ids only
    for (auto t = m_by_time.begin(); t != m_by_time.end(); ++t)
        if (t->second == row.id)
        {
            m_by_time.erase(t);
            break;
        }
    EventQuarterEntry e = row;
    e.announce  = e.announce.empty() ? std::string{}
                                     : BuildNetAnnounce(e.announce);
    e.notice    = false;
    e.next_time = NextEventQuarterTime(e.day, e.hour, e.minute,
                                       now_unix);
    it->second = e;
    m_by_time.emplace(e.next_time, e.id);
}

std::size_t EventQuarterScheduler::Size() const
{
    std::lock_guard g(m_mtx);
    return m_by_id.size();
}

std::optional<EventQuarterEntry>
EventQuarterScheduler::Get(std::uint16_t id) const
{
    std::lock_guard g(m_mtx);
    auto it = m_by_id.find(id);
    if (it == m_by_id.end())
        return std::nullopt;
    return it->second;
}

} // namespace tworldsvr
