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
#include <vector>

namespace tloginsvr::services {

struct EventEntry
{
    std::uint8_t  event_id;   // bEventID from the wire
    std::uint16_t value;      // wValue from the wire (0 = "removed")
    std::vector<std::byte> opaque_payload; // raw EVENTINFO bytes after (id, value)
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
