#pragma once

// EventRegistry — in-memory store of active events (legacy
// m_mapEVENT). Loaded from IEventRepository at startup, mutated by
// the CRUD handlers (CT_EVENTCHANGE / DEL) and by the 1Hz scheduler
// state machine.
//
// Validation: detects overlapping time windows for events of the
// same kind (rejected at the wire boundary so the GM editor can
// show an error). Tracks m_dwEventIndex equivalent — the next
// synthetic index handed out for EK_ADD when the client doesn't
// supply one.

#include "event_types.h"
#include <boost/asio/awaitable.hpp>
#include <boost/asio/experimental/channel.hpp>

#include <cstdint>
#include <optional>
#include <map>
#include <memory>
#include <utility>
#include <vector>

namespace tcontrolsvr {

class EventRegistry
{
public:
    // Seed the registry from the persistent store. Clears any
    // existing state first.
    void LoadFrom(std::vector<EventInfo> events);

    EventInfo*       Find(std::uint32_t index);
    const EventInfo* Find(std::uint32_t index) const;

    // Insert or replace. Used by both EK_ADD (after assigning a
    // fresh index) and EK_UPDATE.
    void Upsert(EventInfo ev);

    // Drop an event by index. Returns true if anything was removed.
    bool Erase(std::uint32_t index);

    // Allocate the next synthetic index for an EK_ADD where the
    // client passed index=0. Mirrors legacy `++m_dwEventIndex`.
    std::uint32_t NextIndex();

    // Snapshot iteration for CT_EVENTLIST_REQ + scheduler tick.
    std::vector<EventInfo> Snapshot() const;
    std::size_t Size() const { return m_events.size(); }
    bool Mutating() const { return m_mutating; }
    bool TryBeginMutation() { if(m_mutating)return false;m_mutating=true;return true; }
    void EndMutation();
    class MutationLease {
    public:
        explicit MutationLease(EventRegistry& owner):m_owner(&owner){}
        MutationLease(MutationLease&& other) noexcept:m_owner(std::exchange(other.m_owner,nullptr)){}
        MutationLease(const MutationLease&)=delete;
        MutationLease& operator=(const MutationLease&)=delete;
        ~MutationLease(){if(m_owner)m_owner->EndMutation();}
    private:
        EventRegistry* m_owner;
    };
    // One reactor-owned gate for CRUD, scheduler delivery, manual pushes and
    // peer replay. Never keep a pointer into the map across an unguarded await.
    boost::asio::awaitable<MutationLease> AcquireMutation();

    // Overlap check used by CT_EVENTCHANGE_REQ. Compares against
    // every existing event of the same kind. `update_index` is the
    // event being edited (skipped from the comparison); pass 0 for
    // EK_ADD.
    bool OverlapsExisting(const EventInfo& candidate,
                          std::uint32_t update_index) const;

    // Direct map access for the scheduler — it needs to mutate
    // state / alarm flags in place.
    std::map<std::uint32_t, EventInfo>&       Map()       { return m_events; }
    const std::map<std::uint32_t, EventInfo>& Map() const { return m_events; }

private:
    std::map<std::uint32_t, EventInfo>  m_events;
    std::uint32_t                                 m_next_index = 0;
    bool m_mutating=false; // io_context-owned guard across offloaded persistence.
    using MutationSignal=boost::asio::experimental::channel<void(boost::system::error_code,int)>;
    std::shared_ptr<MutationSignal> m_mutation_signal;
};

} // namespace tcontrolsvr
