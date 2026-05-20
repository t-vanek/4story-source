#pragma once

// EventScheduler — 1Hz tick that drives the daily / term event
// state machine (legacy `CTControlSvrModule::CheckEvent`).
//
// Logic split into two layers:
//
//   - StepOnce(now_unix, registry): pure state-transition function.
//     Returns the list of `SchedulerAction` records the dispatcher
//     should fan out (event-msg / event-update / cash-sale /
//     cash-shop-stop / event-del). Easy to drive from unit tests
//     by feeding synthetic timestamps.
//
//   - RunLoop(io, registry, peers, audit, repository): asio
//     `steady_timer` that wakes once per second, calls StepOnce
//     with the wall clock, and executes the resulting actions.
//
// The state machine transitions an event from idle → start-alarm
// → running → end-alarm → idle as time crosses the configured
// boundaries. Lottery / GiftTime events fire once and auto-delete
// (legacy behavior).

#include "services/event_registry.h"
#include "services/event_repository.h"
#include "services/peer_registry.h"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>

#include <chrono>
#include <cstdint>
#include <memory>
#include <vector>

namespace tcontrolsvr {

// One actionable outcome of a scheduler tick.
struct SchedulerAction
{
    enum class Kind
    {
        EventMsgStart,    // bTmp=1 — start alarm fired
        EventMsgEnd,      // bTmp=2 — end alarm fired
        EventUpdateStart, // bTmp=3 — state transition to running
        EventUpdateEnd,   // bTmp=4 — state transition to idle
        CashSaleStart,    // CashSale: bTmp=3 specialization
        CashSaleEnd,      // CashSale: bTmp=4 specialization
        CashShopStop,     // CashSale: bTmp=1/2 announcement
        EventDelete,      // Lottery / GiftTime auto-delete after fire
    };
    Kind          kind;
    std::uint32_t event_index = 0;
    std::uint16_t value       = 0;   // wValue carried into the wire packet
    std::uint8_t  msg_type    = 0;   // 0 = start, 1 = end (for EventMsg/CashShopStop)
};

// Pure state machine — no I/O. Walks every event in the registry,
// updates alarm/state flags as the clock crosses boundaries, and
// returns the wire actions the caller should emit. Designed for
// deterministic unit testing — pass a synthetic `now_unix`.
std::vector<SchedulerAction>
StepScheduler(std::int64_t now_unix, EventRegistry& registry);

// Driver: spawns a 1Hz steady_timer loop. Each tick calls
// StepScheduler and fans the actions out to the appropriate peer
// services via the senders.
class EventSchedulerLoop
{
public:
    EventSchedulerLoop(boost::asio::io_context& io,
                       EventRegistry&    registry,
                       PeerRegistry&     peers,
                       IEventRepository* repo,
                       std::chrono::milliseconds tick
                           = std::chrono::seconds(1))
        : m_io(io)
        , m_registry(registry)
        , m_peers(peers)
        , m_repo(repo)
        , m_tick(tick)
    {}

    boost::asio::awaitable<void> Run();

private:
    boost::asio::awaitable<void> Apply(const SchedulerAction& action);

    boost::asio::io_context&    m_io;
    EventRegistry&              m_registry;
    PeerRegistry&               m_peers;
    IEventRepository*           m_repo = nullptr;
    std::chrono::milliseconds   m_tick;
};

} // namespace tcontrolsvr
