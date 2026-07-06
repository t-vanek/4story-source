#pragma once

// EventQuarterScheduler — the lucky-event ("quarter event") runtime
// (legacy m_mapEVQT + m_mapTimeEVQT + CheckEventQuarter,
// TWorldSvr.cpp:882/7776/7800). Entries load from TEVENTQUARTERCHART
// at boot; each carries a weekly (day, hour, minute) slot. The tick
// looks at the EARLIEST entry only: five minutes before the slot it
// fires the announce line once (skipped straight to the event when
// the present is empty), at the slot it fires the present handout
// and reschedules the entry one week ahead.
//
// The announce string is stored pre-wrapped in the legacy NetString
// framing ("%04X%04X" + header + body with an empty header —
// TWorldSvr.cpp:5490 BuildNetString), matching what the legacy
// loader put on the wire.

#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace tworldsvr {

struct EventQuarterEntry
{
    std::uint16_t id     = 0;
    std::uint8_t  day    = 0;   // 1=Sunday .. 7=Saturday (CTime)
    std::uint8_t  hour   = 0;
    std::uint8_t  minute = 0;
    bool          notice = false;
    std::int64_t  next_time = -1;
    std::string   present;      // empty → announce-only entry
    std::string   announce;     // pre-wrapped NetString; empty → none
};

// What one tick decided to fire.
struct EventQuarterActions
{
    std::optional<std::string> announce;
    struct Present
    {
        std::uint8_t day = 0, hour = 0, minute = 0;
        std::string  present;
    };
    std::optional<Present> present;
};

// Legacy BuildNetString(NAME_NULL, body) (TWorldSvr.cpp:5490).
std::string BuildNetAnnounce(const std::string& body);

// Legacy GetNextEventTime (TWorldSvr.cpp:7776): the next local-time
// occurrence of (weekday, hour, minute); -1 on invalid input.
std::int64_t NextEventQuarterTime(std::uint8_t day, std::uint8_t hour,
                                  std::uint8_t minute,
                                  std::int64_t now_unix);

class EventQuarterScheduler
{
public:
    EventQuarterScheduler() = default;
    EventQuarterScheduler(const EventQuarterScheduler&) = delete;
    EventQuarterScheduler& operator=(const EventQuarterScheduler&) = delete;

    // Boot hydrate: wraps announce via BuildNetAnnounce and computes
    // each entry's first fire time (rows with next_time == -1 are
    // kept but never fire — legacy inserted them at key -1 too).
    void LoadFrom(const std::vector<EventQuarterEntry>& rows,
                  std::int64_t now_unix);

    // The CheckEventQuarter tick. Head entry only.
    EventQuarterActions Tick(std::int64_t now_unix);

    // W6-46 chart-editor mirror (legacy OnDM_EVENTQUARTERUPDATE_ACK):
    // DEL erases; ADD inserts; UPDATE re-slots an existing entry
    // (announce re-wrapped, notice cleared, time recomputed).
    void ApplyDel(std::uint16_t id);
    void ApplyAdd(const EventQuarterEntry& row, std::int64_t now_unix);
    void ApplyUpdate(const EventQuarterEntry& row, std::int64_t now_unix);

    std::size_t Size() const;
    std::optional<EventQuarterEntry> Get(std::uint16_t id) const;

private:
    void Index(const EventQuarterEntry& e);

    mutable std::mutex m_mtx;
    std::map<std::uint16_t, EventQuarterEntry>       m_by_id;
    std::multimap<std::int64_t, std::uint16_t>       m_by_time;
};

} // namespace tworldsvr
