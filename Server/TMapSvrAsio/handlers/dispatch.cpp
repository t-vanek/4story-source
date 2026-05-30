// Central dispatch — switch over MessageId to the matching OnXxx
// coroutine. Handler implementations live in sibling files:
//
//   session.cpp   CONNECT_REQ, CONREADY_REQ
//   movement.cpp  MOVE_REQ
//   npc.cpp       NPCTALK_REQ
//   skill.cpp     SKILLUSE_REQ
//   quest.cpp     QUESTEXEC_REQ, QUESTDROP_REQ
//   social.cpp    CHAT_REQ, PARTY{ADD,JOIN,DEL}_REQ
//   bow.cpp       REGISTERBOW_REQ, CANCELBOWQUEUE_REQ, CASHBOWRESPAWN_REQ
//   control.cpp   CT_{ANNOUNCEMENT,USERKICKOUT,SERVICEMONITOR,
//                     SERVICEDATACLEAR}_ACK, CT_CTRLSVR_REQ
//
// T4: every dispatch call records a counter + latency in
// ctx.metrics (when configured) and emits a HandlerInvokeEvent
// to ctx.audit (when configured). Exceptions are caught here so a
// throwing handler still produces an audit / metrics record for
// the failed call.

#include "handlers.h"

#include "audit/audit_log.h"
#include "audit/event.h"
#include "ops/metrics.h"
#include "services/rate_limiter.h"

#include "MessageId.h"

#include <spdlog/spdlog.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <utility>
#include <vector>

namespace tmapsvr {

namespace {

boost::asio::awaitable<void>
DispatchInner(std::shared_ptr<tnetlib::AsioSession> sess,
              std::uint16_t                         wId,
              std::vector<std::byte>                body,
              const HandlerContext&                 ctx)
{
    using tnetlib::protocol::MessageId;
    using tnetlib::protocol::ToMessageId;

    const auto id = ToMessageId(wId);
    switch (id)
    {
    case MessageId::CS_CONNECT_REQ:
        co_await OnConnectReq(sess, std::move(body), ctx); break;
    case MessageId::CS_CONREADY_REQ:
        co_await OnConReadyReq(sess, std::move(body), ctx); break;
    case MessageId::CS_MOVE_REQ:
        co_await OnMoveReq(sess, std::move(body), ctx); break;
    case MessageId::CS_NPCTALK_REQ:
        co_await OnNpcTalkReq(sess, std::move(body), ctx); break;
    case MessageId::CS_SKILLUSE_REQ:
        co_await OnSkillUseReq(sess, std::move(body), ctx); break;
    case MessageId::CS_ACTION_REQ:
        co_await OnActionReq(sess, std::move(body), ctx); break;
    case MessageId::CS_QUESTEXEC_REQ:
        co_await OnQuestExecReq(sess, std::move(body), ctx); break;
    case MessageId::CS_QUESTDROP_REQ:
        co_await OnQuestDropReq(sess, std::move(body), ctx); break;
    case MessageId::CS_CHAT_REQ:
        co_await OnChatReq(sess, std::move(body), ctx); break;
    case MessageId::CS_PARTYADD_REQ:
        co_await OnPartyAddReq(sess, std::move(body), ctx); break;
    case MessageId::CS_PARTYJOIN_REQ:
        co_await OnPartyJoinReq(sess, std::move(body), ctx); break;
    case MessageId::CS_PARTYDEL_REQ:
        co_await OnPartyDelReq(sess, std::move(body), ctx); break;
    case MessageId::CS_REGISTERBOW_REQ:
        co_await OnRegisterBowReq(sess, std::move(body), ctx); break;
    case MessageId::CS_CANCELBOWQUEUE_REQ:
        co_await OnCancelBowQueueReq(sess, std::move(body), ctx); break;
    case MessageId::CS_CASHBOWRESPAWN_REQ:
        co_await OnCashBowRespawnReq(sess, std::move(body), ctx); break;
    case MessageId::CT_ANNOUNCEMENT_ACK:
        co_await OnCtAnnouncementAck(sess, std::move(body), ctx); break;
    case MessageId::CT_USERKICKOUT_ACK:
        co_await OnCtUserKickoutAck(sess, std::move(body), ctx); break;
    case MessageId::CT_SERVICEMONITOR_ACK:
        co_await OnCtServiceMonitorAck(sess, std::move(body), ctx); break;
    case MessageId::CT_SERVICEDATACLEAR_ACK:
        co_await OnCtServiceDataClearAck(sess, std::move(body), ctx); break;
    case MessageId::CT_CTRLSVR_REQ:
        co_await OnCtCtrlSvrReq(sess, std::move(body), ctx); break;

    default:
        spdlog::debug("map_server: unhandled wId=0x{:04X} body={} bytes",
            wId, body.size());
        break;
    }
}

} // namespace

boost::asio::awaitable<void>
Dispatch(std::shared_ptr<tnetlib::AsioSession> sess,
         std::uint16_t                         wId,
         std::vector<std::byte>                body,
         const HandlerContext&                 ctx)
{
    const auto body_size = static_cast<std::uint32_t>(body.size());

    // T5 rate-limit gate. Key by raw session pointer so pre-auth +
    // post-auth share one bucket per connection. Limiter returns
    // true immediately when burst/refill are zero (default config)
    // so the gate has zero cost when disabled.
    if (ctx.rate_limiter)
    {
        const auto key = reinterpret_cast<std::uint64_t>(sess.get());
        if (!ctx.rate_limiter->TryAcquire(key))
        {
            if (ctx.metrics) ctx.metrics->HandlerErrors(wId).Add();
            if (ctx.audit)
            {
                audit::HandlerInvokeEvent ev{};
                ev.hdr.corr   = ctx.audit->NextCorrelation();
                ev.wId        = wId;
                ev.body_size  = body_size;
                ev.latency_us = 0;
                ev.ok         = 0;   // rate-limited = failed dispatch
                ctx.audit->Emit(ev);
            }
            spdlog::warn("dispatch: wId=0x{:04X} rate-limited (peer={})",
                wId, sess->RemoteIPv4());
            co_return;
        }
    }

    if (ctx.metrics) ctx.metrics->HandlerCalls(wId).Add();

    const auto t0 = std::chrono::steady_clock::now();

    bool ok = true;
    std::exception_ptr saved_ex;
    try
    {
        co_await DispatchInner(sess, wId, std::move(body), ctx);
    }
    catch (...)
    {
        ok = false;
        saved_ex = std::current_exception();
        if (ctx.metrics) ctx.metrics->HandlerErrors(wId).Add();
    }

    const auto elapsed = std::chrono::steady_clock::now() - t0;
    const auto us = static_cast<std::uint32_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count());

    if (ctx.metrics) ctx.metrics->HandlerLatency(wId).Record(us);

    if (ctx.audit)
    {
        audit::HandlerInvokeEvent ev{};
        ev.hdr.corr   = ctx.audit->NextCorrelation();
        ev.wId        = wId;
        ev.body_size  = body_size;
        ev.latency_us = us;
        ev.ok         = ok ? 1 : 0;
        ctx.audit->Emit(ev);
    }

    // Re-throw — the per-connection coroutine in map_server.cpp has
    // a top-level handler that logs and absorbs. Re-throwing here
    // keeps that error path intact.
    if (saved_ex) std::rethrow_exception(saved_ex);
}

} // namespace tmapsvr
