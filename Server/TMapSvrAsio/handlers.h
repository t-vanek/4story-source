#pragma once

// Per-packet handler entry points. The MapServer's per-connection
// coroutine builds a HandlerContext, calls Dispatch for every
// DecodedPacket it gets out of the AsioSession's wire loop, and
// dispatches on the message id to the OnXxx function below.
//
// Handlers are co_await-able so they can SendPacket back through the
// session without blocking the reactor. Each handler is responsible
// for its own response (CS_xxx_ACK) and for logging unexpected
// inputs; the dispatcher itself only logs unhandled message ids.

#include "asio_session.h"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/thread_pool.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace tmapsvr {

class IMapSessionValidator;
class IWorldClient;
class ISessionRegistry;
class IChannelPresence;
class IPlayerService;
class IInventoryService;
class INpcService;
class ISkillService;
class IQuestService;
class IMonsterChart;
class ISpawnChart;
class IMonsterRegistry;
class ICompanionService;
class ICharStateStore;
class IServerRouteResolver;
class ILogPeer;
class IRateLimiter;
enum class Mode : std::uint8_t;

namespace audit { class IAuditLog; }
namespace ops   { class IMetrics;  }

// Per-session context handed to every handler. Pointers are
// non-owning; main() keeps the lifetimes.
struct HandlerContext
{
    IMapSessionValidator*  validator         = nullptr;
    IWorldClient*          world_client      = nullptr;
    ISessionRegistry*      session_reg       = nullptr;
    IChannelPresence*      presence          = nullptr;
    IPlayerService*        player_service    = nullptr;
    IInventoryService*     inventory_service = nullptr;
    INpcService*           npc_service       = nullptr;
    ISkillService*         skill_service     = nullptr;
    IQuestService*         quest_service     = nullptr;
    IMonsterChart*         monster_chart     = nullptr;
    ISpawnChart*           spawn_chart       = nullptr;
    IMonsterRegistry*      monster_registry  = nullptr;
    ICompanionService*     companion_service = nullptr;
    ICharStateStore*       char_state        = nullptr;   // live char snapshots
    IServerRouteResolver*  route_resolver    = nullptr;   // server_id → ip/port (MW_ROUTELIST)
    std::atomic<std::uint32_t>* monster_seq   = nullptr;   // runtime monster instance-id allocator (respawn)
    ILogPeer*              log_peer          = nullptr;   // T3 UDP transport
    audit::IAuditLog*      audit             = nullptr;   // T4 structured audit
    ops::IMetrics*         metrics           = nullptr;   // T4 counters/latency
    IRateLimiter*          rate_limiter      = nullptr;   // T5 per-session gate
    boost::asio::thread_pool* db_pool        = nullptr;   // worker pool for SOCI offload
    Mode                   mode              = Mode{0};   // PvE default
    std::uint8_t           expected_group    = 0;         // [server] / world group id
};

// Top-level dispatcher. Looks up the wId in a switch, calls the
// matching OnXxx coroutine. Unknown ids log + drop.
//
// The caller is responsible for copying the body out of the
// AsioSession's internal recv buffer before spawning Dispatch — the
// DecodedPacket span the wire loop hands its callback is only valid
// during the synchronous callback frame, not across co_awaits.
boost::asio::awaitable<void> Dispatch(
    std::shared_ptr<tnetlib::AsioSession> sess,
    std::uint16_t                         wId,
    std::vector<std::byte>                body,
    const HandlerContext&                 ctx);

// Per-handler entry points. One per message id. All take a span over
// the decoded body — they own the copy if they need to outlive the
// dispatch call.
boost::asio::awaitable<void> OnConnectReq(
    std::shared_ptr<tnetlib::AsioSession> sess,
    std::vector<std::byte>                body,
    const HandlerContext&                 ctx);

boost::asio::awaitable<void> OnConReadyReq(
    std::shared_ptr<tnetlib::AsioSession> sess,
    std::vector<std::byte>                body,
    const HandlerContext&                 ctx);

boost::asio::awaitable<void> OnMoveReq(
    std::shared_ptr<tnetlib::AsioSession> sess,
    std::vector<std::byte>                body,
    const HandlerContext&                 ctx);

boost::asio::awaitable<void> OnNpcTalkReq(
    std::shared_ptr<tnetlib::AsioSession> sess,
    std::vector<std::byte>                body,
    const HandlerContext&                 ctx);

boost::asio::awaitable<void> OnSkillUseReq(
    std::shared_ptr<tnetlib::AsioSession> sess,
    std::vector<std::byte>                body,
    const HandlerContext&                 ctx);

// CS_ACTION_REQ — the *animation* half of an attack/skill. The legacy
// OnCS_ACTION_REQ (CSHandler.cpp:1233) only broadcasts the action to
// everyone in view (CS_ACTION_ACK); it computes no damage. Lives in
// handlers/combat.cpp.
boost::asio::awaitable<void> OnActionReq(
    std::shared_ptr<tnetlib::AsioSession> sess,
    std::vector<std::byte>                body,
    const HandlerContext&                 ctx);

// CS_DEFEND_REQ — the *damage* half of an attack. The client sends its
// attack powers (phys/magic min/max, attack level, crit flag, skill);
// the server rolls the final damage against the target's defense, applies
// it, and broadcasts the result (HP / death + EXP). Legacy
// OnCS_DEFEND_REQ (CSHandler.cpp:1438 → CTObjBase::Defend). Lives in
// handlers/combat.cpp.
boost::asio::awaitable<void> OnDefendReq(
    std::shared_ptr<tnetlib::AsioSession> sess,
    std::vector<std::byte>                body,
    const HandlerContext&                 ctx);

// CS_REVIVAL_REQ — a dead player revives at a chosen position. Clears the
// death state, restores HP, repositions, and broadcasts CS_REVIVAL_ACK.
// Legacy OnCS_REVIVAL_REQ (CSHandler.cpp:1067). Lives in handlers/combat.cpp.
boost::asio::awaitable<void> OnRevivalReq(
    std::shared_ptr<tnetlib::AsioSession> sess,
    std::vector<std::byte>                body,
    const HandlerContext&                 ctx);

boost::asio::awaitable<void> OnQuestExecReq(
    std::shared_ptr<tnetlib::AsioSession> sess,
    std::vector<std::byte>                body,
    const HandlerContext&                 ctx);

boost::asio::awaitable<void> OnQuestDropReq(
    std::shared_ptr<tnetlib::AsioSession> sess,
    std::vector<std::byte>                body,
    const HandlerContext&                 ctx);

boost::asio::awaitable<void> OnChatReq(
    std::shared_ptr<tnetlib::AsioSession> sess,
    std::vector<std::byte>                body,
    const HandlerContext&                 ctx);

boost::asio::awaitable<void> OnPartyAddReq(
    std::shared_ptr<tnetlib::AsioSession> sess,
    std::vector<std::byte>                body,
    const HandlerContext&                 ctx);

boost::asio::awaitable<void> OnPartyJoinReq(
    std::shared_ptr<tnetlib::AsioSession> sess,
    std::vector<std::byte>                body,
    const HandlerContext&                 ctx);

boost::asio::awaitable<void> OnPartyDelReq(
    std::shared_ptr<tnetlib::AsioSession> sess,
    std::vector<std::byte>                body,
    const HandlerContext&                 ctx);

boost::asio::awaitable<void> OnRegisterBowReq(
    std::shared_ptr<tnetlib::AsioSession> sess,
    std::vector<std::byte>                body,
    const HandlerContext&                 ctx);

boost::asio::awaitable<void> OnCancelBowQueueReq(
    std::shared_ptr<tnetlib::AsioSession> sess,
    std::vector<std::byte>                body,
    const HandlerContext&                 ctx);

boost::asio::awaitable<void> OnCashBowRespawnReq(
    std::shared_ptr<tnetlib::AsioSession> sess,
    std::vector<std::byte>                body,
    const HandlerContext&                 ctx);

// CT_* — operator control protocol from TControlSvrAsio. Routed
// through the same dispatch switch as CS_*; auth gating (peer IP
// against the configured control server) lands with the
// consolidation pass.
boost::asio::awaitable<void> OnCtAnnouncementAck(
    std::shared_ptr<tnetlib::AsioSession> sess,
    std::vector<std::byte>                body,
    const HandlerContext&                 ctx);

boost::asio::awaitable<void> OnCtUserKickoutAck(
    std::shared_ptr<tnetlib::AsioSession> sess,
    std::vector<std::byte>                body,
    const HandlerContext&                 ctx);

boost::asio::awaitable<void> OnCtServiceMonitorAck(
    std::shared_ptr<tnetlib::AsioSession> sess,
    std::vector<std::byte>                body,
    const HandlerContext&                 ctx);

boost::asio::awaitable<void> OnCtServiceDataClearAck(
    std::shared_ptr<tnetlib::AsioSession> sess,
    std::vector<std::byte>                body,
    const HandlerContext&                 ctx);

boost::asio::awaitable<void> OnCtCtrlSvrReq(
    std::shared_ptr<tnetlib::AsioSession> sess,
    std::vector<std::byte>                body,
    const HandlerContext&                 ctx);

} // namespace tmapsvr
