// Bow / BR run-time mode handlers — CS_REGISTERBOW_REQ,
// CS_CANCELBOWQUEUE_REQ, CS_CASHBOWRESPAWN_REQ.
//
// Each gates on ctx.mode (set from cfg.mode at boot — pve / bow / br)
// and logs the intent. The MW_ADDTOBOWQUEUE_REQ relay, medal
// deduction, and Revival call land with the bow/br consolidation
// pass.
//
// Legacy parity: CSHandler.cpp:19679 (RegisterBow), :19690
// (CancelBowQueue), :19707 (CashBowRespawn). Bow/BR constants come
// from BRSettings.h / TBowSettings.h modernized in c18ea89.

#include "common.h"
#include "config.h"  // Mode + ModeName
#include "handlers.h"

#include <spdlog/spdlog.h>

#include <cstddef>
#include <vector>

namespace tmapsvr {

boost::asio::awaitable<void>
OnRegisterBowReq(std::shared_ptr<tnetlib::AsioSession> sess,
                 std::vector<std::byte>                body,
                 const HandlerContext&                 ctx)
{
    // CS_REGISTERBOW_REQ body is empty in legacy CSHandler.cpp:19679
    // — request to join the Bow queue. Routes to MW_ADDTOBOWQUEUE_REQ
    // on the world peer. Gated on cfg.mode == Bow (or Br, since BR
    // mode reuses some Bow infrastructure).
    (void)body;
    if (ctx.mode != Mode::Bow && ctx.mode != Mode::Br)
    {
        spdlog::debug("CS_REGISTERBOW_REQ: server mode={} (not bow/br) — "
                      "dropping",
            ModeName(ctx.mode));
        co_return;
    }
    spdlog::info("CS_REGISTERBOW_REQ char={} mode={} — F16 stub "
                 "(MW_ADDTOBOWQUEUE_REQ relay lands with bow/br "
                 "consolidation)",
        handlers_detail::SenderCharId(sess, ctx), ModeName(ctx.mode));
    co_return;
}

boost::asio::awaitable<void>
OnCancelBowQueueReq(std::shared_ptr<tnetlib::AsioSession> sess,
                    std::vector<std::byte>                body,
                    const HandlerContext&                 ctx)
{
    // CS_CANCELBOWQUEUE_REQ body is empty — request to leave the
    // queue. Mirror gate of OnRegisterBowReq.
    (void)body;
    if (ctx.mode != Mode::Bow && ctx.mode != Mode::Br)
    {
        spdlog::debug("CS_CANCELBOWQUEUE_REQ: mode={} — dropping",
            ModeName(ctx.mode));
        co_return;
    }
    spdlog::info("CS_CANCELBOWQUEUE_REQ char={} mode={} — F16 stub",
        handlers_detail::SenderCharId(sess, ctx), ModeName(ctx.mode));
    co_return;
}

boost::asio::awaitable<void>
OnCashBowRespawnReq(std::shared_ptr<tnetlib::AsioSession> sess,
                    std::vector<std::byte>                body,
                    const HandlerContext&                 ctx)
{
    // CS_CASHBOWRESPAWN_REQ body is empty (legacy CSHandler.cpp:19707).
    // Pays Bow medals from the player's purse to revive after death
    // — uses ::tmapsvr::bow::RespawnMedalBase /
    // RespawnMedalAdder constants from BRSettings/TBowSettings.
    // F16 only gates and logs; the medal deduction + Revival call +
    // CS_UPDATEMEDALS_REQ broadcast lands with the consolidation
    // pass.
    (void)body;
    if (ctx.mode != Mode::Bow)
    {
        spdlog::debug("CS_CASHBOWRESPAWN_REQ: mode={} (not bow) — dropping",
            ModeName(ctx.mode));
        co_return;
    }
    spdlog::info("CS_CASHBOWRESPAWN_REQ char={} mode={} — F16 stub "
                 "(medal deduction + Revival lands with bow "
                 "consolidation)",
        handlers_detail::SenderCharId(sess, ctx), ModeName(ctx.mode));
    co_return;
}

} // namespace tmapsvr
