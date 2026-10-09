#pragma once

// RegistryEventBus — in-process pub/sub for peer registry transitions.
// Lets the admin shell (and future log forwarders / metrics exporters /
// operator GUI websocket bridges) subscribe to a live stream of
// "peer registered / heartbeat / lease expired" events without
// polling PeerRegistry on a timer.
//
// Lifecycle: every PeerRegistry mutation (Register, Heartbeat,
// Deregister, ExpireStale) emits one event. Subscribers run
// synchronously on the publisher thread — typically the io_context
// reactor — so they MUST NOT block. The streaming admin-shell session
// (admin_shell.cpp::RunRegistryStream) handles that by pushing each
// event onto a per-session queue and letting a writer coroutine
// drain to the socket asynchronously.

#include "service_controller.h"

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>

namespace tcontrolsvr {

enum class RegistryEventKind : std::uint8_t
{
    Registered,        // CT_PEER_REGISTER_REQ accepted
    Heartbeat,         // CT_PEER_HEARTBEAT_REQ accepted
    Deregistered,      // CT_PEER_DEREGISTER_REQ accepted
    Expired,           // lease-expiry sweep dropped the entry
    ScmStatusChanged,  // periodic reconciliation noticed status flip
};

struct RegistryEvent
{
    RegistryEventKind kind            = RegistryEventKind::Registered;
    std::uint32_t     service_id      = 0;
    std::string       reported_name;
    std::string       reported_addr;
    std::uint16_t     reported_port   = 0;
    std::string       version;
    std::uint64_t     lease_epoch     = 0;
    std::uint32_t     cur_users       = 0;
    std::uint32_t     max_users       = 0;

    // Populated only for ScmStatusChanged events. Other kinds leave
    // both at Unknown — streaming consumers should branch on `kind`
    // before reading these.
    ServiceStatus     service_status_prev = ServiceStatus::Unknown;
    ServiceStatus     service_status      = ServiceStatus::Unknown;
};

const char* RegistryEventKindName(RegistryEventKind k);

class RegistryEventBus
{
public:
    using Subscriber = std::function<void(const RegistryEvent&)>;

    // Add a subscriber; returns an opaque token used to Unsubscribe.
    // Tokens are monotonically allocated and never reused so an
    // unsubscribe-after-resubscribe sequence can't accidentally drop
    // someone else's subscription.
    std::uint64_t Subscribe(Subscriber fn);
    void          Unsubscribe(std::uint64_t token);

    // Synchronously dispatches to every live subscriber. Holds the
    // mutex for the duration so subscribers run serially — keeps the
    // contract simple (no event reordering) at the cost of forcing
    // subscribers to be cheap.
    void Publish(const RegistryEvent& ev);

    std::size_t SubscriberCount() const;

private:
    mutable std::mutex                                m_mtx;
    std::uint64_t                                     m_next_token = 1;
    std::unordered_map<std::uint64_t, Subscriber>     m_subs;
};

} // namespace tcontrolsvr
