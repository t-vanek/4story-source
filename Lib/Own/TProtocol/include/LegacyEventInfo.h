#pragma once

// EVENTINFO::WrapPacketOut/In (TWorldType.h), shared by the modern World
// and Map. All fields are read individually through their existing wire
// readers. Strings remain opaque source-encoded bytes, not UTF-8 conversions.
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace fourstory::protocol {
inline constexpr std::uint8_t kEventCount=16;
inline constexpr std::size_t kEventValueOffset=4+5+8+8; // inside EVENTINFO, before WORD value
struct EventCashSale { std::uint16_t item{}; std::uint8_t sale{}; };
struct EventRegen {
    std::uint16_t monster{};
    std::uint32_t delay{};
    std::uint16_t map{};
    float x{},y{},z{};
};
struct EventLotteryRow {
    std::uint16_t item_id{};
    std::uint8_t num{};
    std::uint16_t winner{};
};
struct EventInfoTail {
    std::uint8_t state{},group_id{},svr_type{},svr_id{};
    std::int64_t start_date{},end_date{};
    std::uint16_t value{},map_id{};
    std::uint32_t start_alarm{},end_alarm{};
    std::string start_msg,end_msg,title;
    std::uint8_t part_time{};
    std::string lot_msg;
    std::vector<EventCashSale> cash;
    std::uint8_t monster_start{},monster_end{};
    std::vector<std::uint16_t> spawns;
    std::vector<EventRegen> regen;
    std::vector<EventLotteryRow> lottery;
};

// The caller has read DWORD index and BYTE inner event ID. The original
// bStartAlarm/bEndAlarm members are deliberately absent from this wire format.
template<class Reader>
bool ReadEventInfoTail(Reader& r,EventInfoTail& out) {
    EventInfoTail value;
    if(!r.Read(value.state)||!r.Read(value.group_id)||!r.Read(value.svr_type)||!r.Read(value.svr_id)||
       !r.Read(value.start_date)||!r.Read(value.end_date)||!r.Read(value.value)||!r.Read(value.map_id)||
       !r.Read(value.start_alarm)||!r.Read(value.end_alarm)||!r.ReadString(value.start_msg)||
       !r.ReadString(value.end_msg)||!r.ReadString(value.title)||!r.Read(value.part_time)||!r.ReadString(value.lot_msg))return false;
    std::uint16_t count=0;
    if(!r.Read(count))return false;
    for(unsigned i=0;i<count;++i) {
        EventCashSale row;
        if(!r.Read(row.item)||!r.Read(row.sale))return false;
        value.cash.push_back(row);
    }
    if(!r.Read(value.monster_start)||!r.Read(value.monster_end)||!r.Read(count))return false;
    for(unsigned i=0;i<count;++i) {
        std::uint16_t spawn=0;if(!r.Read(spawn))return false;value.spawns.push_back(spawn);
    }
    if(!r.Read(count))return false;
    for(unsigned i=0;i<count;++i) {
        EventRegen row;
        if(!r.Read(row.monster)||!r.Read(row.delay)||!r.Read(row.map)||!r.Read(row.x)||!r.Read(row.y)||!r.Read(row.z))return false;
        value.regen.push_back(row);
    }
    if(!r.Read(count))return false;
    for(unsigned i=0;i<count;++i) {
        EventLotteryRow row;
        if(!r.Read(row.item_id)||!r.Read(row.num)||!r.Read(row.winner))return false;
        value.lottery.push_back(row);
    }
    out=std::move(value);return true;
}
}
