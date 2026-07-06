#pragma once

// Full EVENTINFO wire codec (legacy tagEVENTINFO::WrapPacketOut,
// TWorldType.h:780). The W6-31 EventRegistry keeps the body opaque
// for the store/replay path; the W6-47 LOTTERY / GIFTTIME reward
// runs need the structured fields, so this reader parses the tail
// that follows the (dwIndex, bID) prefix the dispatcher already
// consumed.
//
// Wire order after (index u32, id u8): state u8, group u8,
// svr_type u8, svr_id u8, start i64, end i64, value u16, map u16,
// start_alarm u32, end_alarm u32, start_msg, end_msg, title,
// part_time u8, lot_msg, cash{u16 n ×(u16 id, u8 sale)},
// mon{u8 start, u8 end, u16 n × u16 spawn}, regen{u16 n ×(u16 mon,
// u32 delay, u16 map, f32 x, f32 y, f32 z)}, lottery{u16 n ×
// (u16 item, u8 num, u16 winner)}. The b*Alarm BYTE members are
// not serialized (legacy quirk).

#include "../wire_codec.h"

#include <cstdint>
#include <string>
#include <vector>

namespace tworldsvr {

struct EventLotteryRow
{
    std::uint16_t item_id = 0;
    std::uint8_t  num     = 0;
    std::uint16_t winner  = 0;
};

struct EventInfoTail
{
    std::uint8_t  state = 0;
    std::uint8_t  group_id = 0;
    std::uint8_t  svr_type = 0;
    std::uint8_t  svr_id = 0;
    std::int64_t  start_date = 0;
    std::int64_t  end_date = 0;
    std::uint16_t value = 0;
    std::uint16_t map_id = 0;
    std::uint32_t start_alarm = 0;
    std::uint32_t end_alarm = 0;
    std::string   start_msg;
    std::string   end_msg;
    std::string   title;
    std::uint8_t  part_time = 0;
    std::string   lot_msg;
    std::vector<EventLotteryRow> lottery;
};

inline bool ReadEventInfoTail(wire::Reader& r, EventInfoTail& out)
{
    if (!r.Read(out.state) || !r.Read(out.group_id) ||
        !r.Read(out.svr_type) || !r.Read(out.svr_id) ||
        !r.Read(out.start_date) || !r.Read(out.end_date) ||
        !r.Read(out.value) || !r.Read(out.map_id) ||
        !r.Read(out.start_alarm) || !r.Read(out.end_alarm) ||
        !r.ReadString(out.start_msg) || !r.ReadString(out.end_msg) ||
        !r.ReadString(out.title) || !r.Read(out.part_time) ||
        !r.ReadString(out.lot_msg))
        return false;

    std::uint16_t n = 0;
    if (!r.Read(n)) return false;              // cash-sale rows
    for (std::uint16_t i = 0; i < n; ++i)
    {
        std::uint16_t id = 0; std::uint8_t sale = 0;
        if (!r.Read(id) || !r.Read(sale)) return false;
    }

    std::uint8_t mon_start = 0, mon_end = 0;
    if (!r.Read(mon_start) || !r.Read(mon_end) || !r.Read(n))
        return false;                          // mon-event spawns
    for (std::uint16_t i = 0; i < n; ++i)
    {
        std::uint16_t spawn = 0;
        if (!r.Read(spawn)) return false;
    }

    if (!r.Read(n)) return false;              // mon-regen rows
    for (std::uint16_t i = 0; i < n; ++i)
    {
        std::uint16_t mon = 0, map = 0;
        std::uint32_t delay = 0;
        float x = 0, y = 0, z = 0;
        if (!r.Read(mon) || !r.Read(delay) || !r.Read(map) ||
            !r.Read(x) || !r.Read(y) || !r.Read(z))
            return false;
    }

    if (!r.Read(n)) return false;              // lottery rows
    for (std::uint16_t i = 0; i < n; ++i)
    {
        EventLotteryRow row{};
        if (!r.Read(row.item_id) || !r.Read(row.num) ||
            !r.Read(row.winner))
            return false;
        out.lottery.push_back(row);
    }
    return true;
}

} // namespace tworldsvr
