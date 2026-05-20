#pragma once

// Event-domain types — POD structs that survive across the SOCI
// repository, the in-memory scheduler, and the wire codec.
// EventInfo mirrors the legacy EVENTINFO struct in
// Server/TControlSvr/TControlType.h (~30 fields including nested
// cash-item / mon-event / mon-regen / lottery vectors).
//
// The bID values match the legacy EVENT_* enum — these are the
// in-game event categories the GM editor produces:
//
//   1 = CashSale     — temporary cash-shop discount
//   2 = Lottery      — broadcast item raffle; auto-deletes on emit
//   3 = GiftTime     — broadcast giveaway; auto-deletes on emit
//   4 = MonSpawn     — spawn a one-off monster wave
//   5 = MonRegen     — re-tune monster respawn rate
//   6 = ExpRate      — server-wide XP multiplier window
//   7 = DropRate     — drop-rate multiplier window
//
// Other values exist in the legacy DB but only the five referenced
// in the CheckEvent scheduler (CashSale / Lottery / GiftTime / value-
// based) feed special-case branches.

#include <cstdint>
#include <string>
#include <vector>

namespace tcontrolsvr {

namespace event_kind {
constexpr std::uint8_t kCashSale  = 1;
constexpr std::uint8_t kLottery   = 2;
constexpr std::uint8_t kGiftTime  = 3;
constexpr std::uint8_t kMonSpawn  = 4;
constexpr std::uint8_t kMonRegen  = 5;
constexpr std::uint8_t kExpRate   = 6;
constexpr std::uint8_t kDropRate  = 7;
} // namespace event_kind

namespace event_op {
constexpr std::uint8_t kDel    = 0;   // EK_DEL
constexpr std::uint8_t kAdd    = 1;   // EK_ADD
constexpr std::uint8_t kUpdate = 2;   // EK_UPDATE
} // namespace event_op

namespace event_result {
constexpr std::uint8_t kSuccess        = 0;   // ER_SUCCESS
constexpr std::uint8_t kFail           = 1;   // ER_FAIL
constexpr std::uint8_t kNotFound       = 2;   // ER_NOTFOUNDEVENT
constexpr std::uint8_t kAlreadyRunning = 3;   // ER_RUN
constexpr std::uint8_t kInvalidTime    = 4;   // ER_INVALIDTIME
constexpr std::uint8_t kMaxCount       = 5;   // ER_MAXCOUNT
} // namespace event_result

// Cash-shop sale entry — used by CT_CASHITEMSALE_REQ + the
// EventInfo cash list.
struct CashItemSale
{
    std::uint16_t item_id = 0;
    std::uint8_t  sale_value = 0;   // 1..100 (% discount)
};

// One row of TCASHSHOPITEMCHART. Read by CT_CASHITEMLIST_REQ so
// the operator can pick items for new CashSale events.
struct CashItem
{
    std::uint16_t  id = 0;
    std::string    name;
};

// MonEvent — monster wave configuration (event kind = MonSpawn).
struct MonEventCfg
{
    std::uint8_t              start_action = 0;
    std::uint8_t              end_action   = 0;
    std::vector<std::uint16_t> spawn_ids;
};

// MonRegen — per-monster respawn entry (event kind = MonRegen).
struct MonRegen
{
    std::uint16_t mon_id  = 0;
    std::uint32_t delay   = 0;
    std::uint16_t map_id  = 0;
    float         pos_x   = 0;
    float         pos_y   = 0;
    float         pos_z   = 0;
};

// Lottery — item payout row (event kind = Lottery).
struct Lottery
{
    std::uint16_t  item_id = 0;
    std::uint8_t   count   = 0;
    std::uint16_t  winner  = 0;
};

// EventInfo — the legacy EVENTINFO struct minus the
// CPacket-serialization methods. Lifetime: loaded once at startup
// from TEVENTCHART (via IEventRepository), mutated in place by the
// scheduler state machine, persisted back via TEventUpdate SP.
struct EventInfo
{
    std::uint32_t   index           = 0;     // m_dwIndex (PK)
    std::uint8_t    kind            = 0;     // m_bID — event_kind::*
    std::uint8_t    state           = 0;     // m_bState — 0 idle, 1 running
    std::uint8_t    group_id        = 0;     // m_bGroupID — 0 = ALL
    std::uint8_t    server_type     = 0;     // m_bSvrType
    std::uint8_t    server_id       = 0;     // m_bSvrID — 0 = ALL
    std::int64_t    start_unix      = 0;     // m_dStartDate
    std::int64_t    end_unix        = 0;     // m_dEndDate
    std::uint16_t   value           = 0;     // m_wValue
    std::uint16_t   map_id          = 0xFF;  // m_wMapID — 0xFF = ALL
    std::uint32_t   start_alarm     = 0;     // m_dwStartAlarm
    std::uint32_t   end_alarm       = 0;     // m_dwEndAlarm
    std::uint8_t    start_alarm_fired = 0;   // m_bStartAlarm
    std::uint8_t    end_alarm_fired   = 0;   // m_bEndAlarm
    std::string     start_msg;               // m_strStartMsg
    std::string     end_msg;                 // m_strEndMsg
    std::string     title;                   // m_strTitle
    std::uint8_t    part_time       = 0;     // m_bPartTime — 0 daily, 1 term
    std::string     lottery_msg;             // m_strLotMsg
    std::vector<CashItemSale>  cash_items;
    MonEventCfg                mon_event;
    std::vector<MonRegen>      mon_regens;
    std::vector<Lottery>       lotteries;
};

} // namespace tcontrolsvr
