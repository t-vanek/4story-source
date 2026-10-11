#pragma once

// EventRegistry — in-memory store of "active" timed events. Mirrors
// legacy `m_mapEVENT` (TWorldSvr SSHandler.cpp:294) keyed by the
// 32-bit event index. Holds the EVENTINFO body as an opaque tail so
// world doesn't need to round-trip through the full WrapPacketOut /
// WrapPacketIn parse — only the few fields a routing handler peeks
// at (dwIndex, bID) are surfaced explicitly. Re-broadcast on a new
// map peer connect (legacy SSHandler.cpp:664) is the natural reader;
// that wiring is in OnRelaysvrReq. Full EVENTINFO is validated before storage.

#include <cstdint>
#include <shared_mutex>
#include <map>
#include <memory>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <vector>

namespace tworldsvr {

struct TEventInfo
{
    // Outer-header values from CT_EVENTUPDATE_REQ. Legacy uses these
    // to gate the wValue==0 erase semantics and to re-emit the same
    // pair on every MW_EVENTUPDATE_REQ to peers.
    std::uint8_t  event_id = 0;   // outer BYTE bEventID
    std::uint16_t value    = 0;   // outer WORD wValue (drives "active" bit)

    // Inner EVENTINFO fields needed by routing logic. dwIndex is the
    // map key; b_id picks the EVENT_LOTTERY / EVENT_GIFTTIME short-
    // circuits in OnCtEventUpdateReq.
    std::uint32_t dw_index = 0;
    std::uint8_t  b_id     = 0;

    // Full EVENTINFO body as it arrived on the wire (WrapPacketOut
    // input). Stored opaquely so re-emit just appends these bytes
    // after `event_id, value` — same shape SendMW_EVENTUPDATE_REQ
    // produces on the legacy side via pEvent->WrapPacketIn.
    std::vector<std::byte> body;
};

class EventRegistry
{
public:
    using DeliveryPermit=boost::asio::experimental::channel<void(boost::system::error_code,int)>;
    class DeliveryLease {
    public:
        explicit DeliveryLease(std::shared_ptr<DeliveryPermit> permit):m_permit(std::move(permit)){}
        DeliveryLease(DeliveryLease&&)=default;
        DeliveryLease(const DeliveryLease&)=delete;
        DeliveryLease& operator=(const DeliveryLease&)=delete;
        ~DeliveryLease(){if(m_permit)m_permit->try_send(boost::system::error_code{},0);}
    private:
        std::shared_ptr<DeliveryPermit> m_permit;
    };
    // Reactor-scoped gate spans registry mutation and complete peer fan-out.
    // Registration holds it before exposing the new peer through its replay.
    // This prevents a suspended old replay overwriting a newer live update.
    boost::asio::awaitable<DeliveryLease> AcquireDelivery();
    EventRegistry() = default;
    EventRegistry(const EventRegistry&) = delete;
    EventRegistry& operator=(const EventRegistry&) = delete;

    // Insert or replace the entry under `info.dw_index`. Legacy
    // parity: caller drives the wValue==0 erase first (SSHandler.cpp:
    // 295-298 — `find + erase, then insert if(wValue)`); this
    // method is the *insert*, callers also have Erase().
    void Set(TEventInfo info);

    // Drop the entry under `dw_index`. Idempotent (no-op on miss).
    bool Erase(std::uint32_t dw_index);

    // Snapshot in unsigned DWORD index order, matching source MAPEVENTINFO.
    // Duplicate event types deliberately replay in this order (last wins).
    std::vector<TEventInfo> Snapshot() const;

    std::size_t Size() const;

private:
    std::shared_ptr<DeliveryPermit> m_delivery;
    mutable std::shared_mutex                       m_lock;
    std::map<std::uint32_t, TEventInfo>   m_events;
};

} // namespace tworldsvr
