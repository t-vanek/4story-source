#pragma once

// PeerDialer — outbound connect helper for the cluster orchestration
// flow. Legacy CTServer::Connect blocks on a synchronous WSAConnect,
// sets up IOCP, then calls SendCT_CTRLSVR_REQ. The modern equivalent
// is a single coroutine: resolve, connect, hand back a fresh
// ControlSession wrapped in a PeerSession, and spawn the read loop.
//
// Retry budget is intentionally small (1 attempt with a short
// timeout). Legacy gives up immediately on first failure and waits
// for the next CT_NEWCONNECT_REQ from the operator; we keep that
// contract so the GUI's "reconnect" button still has work to do.

#include "control_session.h"
#include "peer_session.h"
#include "services/peer_registry.h"
#include "services/service_inventory.h"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>

#include <chrono>
#include <memory>
#include <string>

namespace tcontrolsvr {

struct PeerDialResult
{
    std::shared_ptr<PeerSession> session;     // nullptr on failure
    std::string                  failure_reason;
};

class PeerDialer
{
public:
    PeerDialer(boost::asio::io_context& io,
               PeerRegistry& registry,
               const IServiceInventory& inventory,
               std::chrono::milliseconds connect_timeout
                   = std::chrono::seconds(5))
        : m_io(io)
        , m_registry(registry)
        , m_inventory(inventory)
        , m_timeout(connect_timeout)
    {}

    // Dial the service's machine first private address + service
    // port. Returns a PeerSession on success (already registered
    // with PeerRegistry). On failure returns a result with an empty
    // session and a human-readable reason in `failure_reason`.
    boost::asio::awaitable<PeerDialResult> Dial(const ServiceInstance& svc);

private:
    boost::asio::io_context&    m_io;
    PeerRegistry&               m_registry;
    const IServiceInventory&    m_inventory;
    std::chrono::milliseconds   m_timeout;
};

} // namespace tcontrolsvr
