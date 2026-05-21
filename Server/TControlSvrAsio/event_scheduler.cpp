#include "event_scheduler.h"

#include "peer_session.h"
#include "senders.h"

#include <boost/asio/redirect_error.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <spdlog/spdlog.h>

#include <ctime>

namespace tcontrolsvr {

namespace {

// Minutes-since-midnight + 1440 offset so the legacy daily-event
// arithmetic stays positive on the cross-midnight comparison.
std::uint32_t DayMinutes(std::int64_t unix_seconds)
{
    std::time_t t = static_cast<std::time_t>(unix_seconds);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    return static_cast<std::uint32_t>(tm.tm_hour * 60 + tm.tm_min + 1440);
}

bool IsCashSale(const EventInfo& e)  { return e.kind == event_kind::kCashSale; }
bool IsLottery (const EventInfo& e)  { return e.kind == event_kind::kLottery;  }
bool IsGiftTime(const EventInfo& e)  { return e.kind == event_kind::kGiftTime; }

} // namespace

std::vector<SchedulerAction>
StepScheduler(std::int64_t now_unix, EventRegistry& registry)
{
    std::vector<SchedulerAction> out;
    std::vector<std::uint32_t>   to_delete;

    for (auto& [idx, ev] : registry.Map())
    {
        // Past-due idle events: schedule a delete.
        if (ev.end_unix < now_unix && ev.state == 0)
        {
            out.push_back({SchedulerAction::Kind::EventDelete, ev.index, 0, 0});
            to_delete.push_back(ev.index);
            continue;
        }

        const std::int64_t start_alarm_unix = ev.start_unix
            - static_cast<std::int64_t>(ev.start_alarm) * 60;
        if (now_unix < start_alarm_unix) continue;

        std::uint8_t  fire   = 0;
        std::uint16_t value  = 0;

        if (ev.part_time == 0)
        {
            // Daily event — wraps around clock minutes.
            const std::uint32_t cur     = DayMinutes(now_unix);
            const std::uint32_t s       = DayMinutes(ev.start_unix);
            const std::uint32_t e       = DayMinutes(ev.end_unix);
            const std::uint32_t s_alarm = s - ev.start_alarm;
            const std::uint32_t e_alarm = e - ev.end_alarm;

            if (cur < s_alarm) continue;

            if (ev.state == 0)
            {
                if (!ev.start_alarm_fired && s_alarm <= cur && cur < e)
                {
                    fire = 1;
                    ev.start_alarm_fired = 1;
                }
                else if (s <= cur && cur < e)
                {
                    fire = 3;
                    value = ev.value;
                    ev.state = 1;
                }
            }
            else if (ev.state == 1)
            {
                if (!ev.end_alarm_fired && e_alarm <= cur && cur <= e)
                {
                    fire = 2;
                    ev.end_alarm_fired = 1;
                }
                else if (e <= cur)
                {
                    fire = 4;
                    ev.state = 0;
                    ev.start_alarm_fired = 0;
                    ev.end_alarm_fired   = 0;
                }
            }
        }
        else
        {
            // Term event — absolute wall-clock comparison.
            const std::int64_t e_alarm_unix = ev.end_unix
                - static_cast<std::int64_t>(ev.end_alarm) * 60;
            if (ev.state == 0)
            {
                if (!ev.start_alarm_fired &&
                    start_alarm_unix <= now_unix && now_unix < ev.end_unix)
                {
                    fire = 1;
                    ev.start_alarm_fired = 1;
                }
                else if (ev.start_unix <= now_unix && now_unix < ev.end_unix)
                {
                    fire = 3;
                    value = ev.value;
                    ev.state = 1;
                }
            }
            else if (ev.state == 1)
            {
                if (!ev.end_alarm_fired &&
                    e_alarm_unix <= now_unix && now_unix <= ev.end_unix)
                {
                    fire = 2;
                    ev.end_alarm_fired = 1;
                }
                else if (ev.end_unix <= now_unix)
                {
                    fire = 4;
                    ev.state = 0;
                    ev.start_alarm_fired = 0;
                    ev.end_alarm_fired   = 0;
                }
            }
        }

        switch (fire)
        {
        case 1:
            out.push_back({SchedulerAction::Kind::EventMsgStart,
                           ev.index, 0, 0});
            if (IsCashSale(ev))
                out.push_back({SchedulerAction::Kind::CashShopStop,
                               ev.index, 0, 0});
            break;
        case 2:
            out.push_back({SchedulerAction::Kind::EventMsgEnd,
                           ev.index, 0, 1});
            if (IsCashSale(ev))
                out.push_back({SchedulerAction::Kind::CashShopStop,
                               ev.index, 0, 1});
            break;
        case 3:
            out.push_back({IsCashSale(ev)
                               ? SchedulerAction::Kind::CashSaleStart
                               : SchedulerAction::Kind::EventUpdateStart,
                           ev.index, value, 0});
            if (IsLottery(ev) || IsGiftTime(ev))
            {
                // One-shot — legacy auto-deletes on the start firing.
                out.push_back({SchedulerAction::Kind::EventDelete,
                               ev.index, 0, 0});
                to_delete.push_back(ev.index);
            }
            break;
        case 4:
            out.push_back({IsCashSale(ev)
                               ? SchedulerAction::Kind::CashSaleEnd
                               : SchedulerAction::Kind::EventUpdateEnd,
                           ev.index, 0, 0});
            break;
        default: break;
        }
    }

    for (auto idx : to_delete)
        registry.Erase(idx);

    return out;
}

boost::asio::awaitable<void>
EventSchedulerLoop::Run()
{
    boost::asio::steady_timer timer(m_io);
    while (true)
    {
        timer.expires_after(m_tick);
        boost::system::error_code ec;
        co_await timer.async_wait(
            boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        if (ec) break;

        const std::int64_t now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        for (const auto& act : StepScheduler(now, m_registry))
            co_await Apply(act);
    }
}

namespace {

// Find all peer connections matching an event's (group, server_type,
// server_id) targeting rules — group=0 means "all groups",
// server_id=0 means "all servers of that type".
std::vector<std::shared_ptr<PeerSession>>
PeersForEvent(const EventInfo& ev, PeerRegistry& peers)
{
    std::vector<std::shared_ptr<PeerSession>> out;
    for (const auto& svc : peers.Services())
    {
        if (svc.type_id != ev.server_type) continue;
        if (ev.group_id  != 0 && svc.group_id  != ev.group_id)  continue;
        if (ev.server_id != 0 && svc.server_id != ev.server_id) continue;
        auto conn = peers.Connection(svc.service_id);
        if (conn && conn->Wire() && conn->Wire()->IsOpen())
            out.push_back(std::move(conn));
    }
    return out;
}

} // namespace

boost::asio::awaitable<void>
EventSchedulerLoop::Apply(const SchedulerAction& action)
{
    if (action.kind == SchedulerAction::Kind::EventDelete)
    {
        if (m_repo)
        {
            // The registry was already updated by StepScheduler; the
            // DB call here is fire-and-forget — we don't await an
            // operator response. Errors are logged in the repo impl.
            EventInfo dummy{};
            dummy.index = action.event_index;
            m_repo->Persist(dummy, event_op::kDel, "");
        }
        spdlog::info("event_scheduler: deleted event idx={}",
            action.event_index);
        co_return;
    }

    const auto* ev = m_registry.Find(action.event_index);
    if (!ev) co_return;
    const auto recipients = PeersForEvent(*ev, m_peers);

    for (const auto& peer : recipients)
    {
        switch (action.kind)
        {
        case SchedulerAction::Kind::EventMsgStart:
        case SchedulerAction::Kind::EventMsgEnd:
            co_await senders::SendEventMsgReq(peer->Wire(),
                ev->kind, action.msg_type,
                action.msg_type == 0 ? ev->start_msg : ev->end_msg);
            break;
        case SchedulerAction::Kind::EventUpdateStart:
        case SchedulerAction::Kind::EventUpdateEnd:
            co_await senders::SendEventUpdateReq(peer->Wire(),
                ev->kind, action.value, *ev);
            break;
        case SchedulerAction::Kind::CashSaleStart:
        case SchedulerAction::Kind::CashSaleEnd:
            co_await senders::SendCashItemSaleReq(peer->Wire(),
                ev->index, action.value, ev->cash_items);
            break;
        case SchedulerAction::Kind::CashShopStop:
            co_await senders::SendCashShopStopReq(peer->Wire(), 1);
            break;
        case SchedulerAction::Kind::EventDelete: break;
        }
    }
}

} // namespace tcontrolsvr
