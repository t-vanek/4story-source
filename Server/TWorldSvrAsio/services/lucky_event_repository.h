#pragma once

// ILuckyEventRepository — DB surface for the W6-46 EVENTQUARTER
// ("lucky event") operator tools: the TEVENTQUARTERCHART day listing
// (legacy CTBLEventQuarterList + the per-row TGetItemName name
// resolution) and the TEventQuarterUpdate ADD/UPDATE/DEL editor
// (SSHandler.cpp:12873 / 12949). The in-memory quarter-event
// SCHEDULER (m_mapEVQT reschedule + timer fire + announce/handout)
// stays with the lucky-event runtime slice.

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace tworldsvr {

// EVENT_KIND (TWorldType.h:242).
inline constexpr std::uint8_t kEkDel    = 0;
inline constexpr std::uint8_t kEkAdd    = 1;
inline constexpr std::uint8_t kEkUpdate = 2;

// One TEVENTQUARTERCHART row (legacy LUCKYEVENT, TWorldType.h:1073).
// Field order == WrapPacketIn/Out wire order.
struct LuckyEvent
{
    std::uint16_t id     = 0;
    std::uint8_t  day    = 0;
    std::uint8_t  hour   = 0;
    std::uint8_t  minute = 0;
    std::uint16_t item_id[5] = {0, 0, 0, 0, 0};
    std::uint8_t  count  = 0;
    std::string   present;
    std::string   announce;
    std::string   title;
    std::string   message;
    std::string   item_name[5];
};

struct LuckyEventUpdateResult
{
    std::uint8_t ret = 1;      // legacy bRet init TRUE (=failure code
                               // path when the SP never ran)
    LuckyEvent   event;        // names filled; ADD adopts wOutID
};

class ILuckyEventRepository
{
public:
    virtual ~ILuckyEventRepository() = default;

    // TEVENTQUARTERCHART WHERE bDay = :day + TGetItemName per row.
    virtual std::vector<LuckyEvent> List(std::uint8_t day) = 0;

    // Whole-chart scan for the W6-48 scheduler boot load (legacy
    // CTBLEventQuarterChart, TWorldSvr.cpp:885). Only id / day /
    // hour / minute / present / announce are consumed.
    virtual std::vector<LuckyEvent> ListAll() = 0;

    // TEventQuarterUpdate(type, event) — nullopt on a DB error
    // (legacy keeps bRet=TRUE and echoes the unresolved event).
    virtual std::optional<LuckyEventUpdateResult>
        Update(std::uint8_t type, const LuckyEvent& event) = 0;
};

} // namespace tworldsvr
