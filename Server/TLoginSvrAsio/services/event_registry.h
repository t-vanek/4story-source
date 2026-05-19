#pragma once

// IEventRegistry — GM-broadcast event store, populated via the
// CT_EVENTUPDATE_REQ control-protocol message. Legacy server kept the
// same data in `CTLoginSvrModule::m_mapEVENT` (keyed by EVENTINFO's
// `dwIndex`); the new server keeps it behind an interface so a
// Redis-backed registry could be swapped in for sharded deploys.
//
// The event payload is treated as opaque bytes — we don't introspect
// the EVENTINFO struct, just store + replay the wire body so any
// downstream consumer (eventual GroupList ack handler that reports
// active events) gets the legacy-faithful encoding back.

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace tloginsvr::services {

// Cash-shop discount entry (legacy TCASHITEMSALE in TLoginType.h:127).
struct CashItemSale
{
    std::uint16_t item_id    = 0;
    std::uint8_t  sale_value = 0;
};

// Monster respawn entry (legacy MONREGEN in TLoginType.h:139).
struct MonsterRegen
{
    std::uint16_t mon_id  = 0;
    std::uint32_t delay   = 0;
    std::uint16_t map_id  = 0;
    float         pos_x   = 0.0f;
    float         pos_y   = 0.0f;
    float         pos_z   = 0.0f;
};

// Lottery prize entry (legacy TLOTTERY in TLoginType.h:150).
struct LotteryItem
{
    std::uint16_t item_id = 0;
    std::uint8_t  count   = 0;
    std::uint16_t winner  = 0;
};

// Decoded GM event entry — full parity with legacy EVENTINFO::WrapPacketOut
// (TLoginType.h:212). Empty `parsed` flag means the wire body didn't
// match the expected layout and the raw bytes are kept in
// opaque_payload for later inspection. Consumers that don't care about
// the decoded fields (e.g. group-list ack rendering, dashboards) can
// ignore `parsed` and just look at event_id + value.
struct EventEntry
{
    std::uint8_t  event_id   = 0;  // bEventID from the wire (outer scalar)
    std::uint16_t value      = 0;  // wValue (0 = "remove signal")

    // The legacy EVENTINFO struct, decoded. Valid only when parsed == true.
    bool          parsed     = false;

    std::uint32_t index            = 0;
    std::uint8_t  id_inner         = 0;  // m_bID inside the payload (vs outer bEventID)
    std::uint8_t  state            = 0;
    std::uint8_t  group_id         = 0;
    std::uint8_t  svr_type         = 0;
    std::uint8_t  svr_id           = 0;
    std::int64_t  start_date       = 0;  // __time64_t (seconds since 1970)
    std::int64_t  end_date         = 0;
    std::uint16_t map_id           = 0;
    std::uint32_t start_alarm      = 0;
    std::uint32_t end_alarm        = 0;
    std::string   start_msg;
    std::string   end_msg;
    std::string   title;
    std::uint8_t  part_time        = 0;
    std::string   lot_msg;

    std::vector<CashItemSale>  cash_items;
    std::uint8_t               mon_start_action = 0;
    std::uint8_t               mon_end_action   = 0;
    std::vector<std::uint16_t> spawn_ids;
    std::vector<MonsterRegen>  mon_regens;
    std::vector<LotteryItem>   lottery;

    // Raw payload (the bytes after the outer event_id + value). Populated
    // unconditionally so Snapshot consumers can re-emit the legacy wire
    // form to peers that expect EVENTINFO::WrapPacketIn semantics.
    std::vector<std::byte>     opaque_payload;
};

class IEventRegistry
{
public:
    virtual ~IEventRegistry() = default;

    // Upsert (replace existing entry with the same event_id).
    virtual void Upsert(EventEntry entry) = 0;

    // Remove entry for event_id. No-op if not present.
    virtual void Remove(std::uint8_t event_id) = 0;

    // Snapshot of all active entries. Order is unspecified; callers
    // that need a stable order sort by event_id.
    virtual std::vector<EventEntry> Snapshot() const = 0;
};

} // namespace tloginsvr::services
