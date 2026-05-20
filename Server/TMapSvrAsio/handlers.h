#pragma once

// Wire dispatch for TMapSvrAsio.
//
// F1:  OnCS_CONNECT_REQ — handshake + IMapSessionValidator check.
// F2b: OnCS_CONREADY_REQ §2b — sends CS_CHARINFO_ACK when snapshot is
//      loaded from IPlayerService (standalone path, no TWorldSvr).
//      Remaining §2–§4 branches land with F3 (IMapState + AOI grid).
//
// Legacy parity references:
//   * Server/TMapSvr/CSHandler.cpp:249  — OnCS_CONNECT_REQ wire shape
//   * Server/TMapSvr/CSSender.cpp:78    — SendCS_CONNECT_ACK encoding
//   * Server/TMapSvr/CSHandler.cpp:402  — OnCS_CONREADY_REQ branches
//   * Server/TMapSvr/CSSender.cpp:121   — SendCS_CHARINFO_ACK wire shape
//   * Lib/Own/TProtocol/include/NetCode.h:319 — CN_* result enum

#include "asio_session.h"
#include "MessageId.h"
#include "legacy_port/types_layer3.h"
#include "services/session_validator.h"
#include "services/player_service.h"
#include "services/world_client.h"
#include "services/session_registry.h"
#include "map_state.h"
#include "monster_state.h"
#include "level_chart.h"
#include "player_hp_registry.h"

#include <boost/asio/awaitable.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace tmapsvr {

// Shared per-request context handed to every handler. Non-owning
// pointers — caller (MapServer) owns the lifetimes.
struct HandlerContext
{
    IMapSessionValidator*             validator         = nullptr;
    const std::vector<std::uint16_t>* accepted_versions = nullptr;

    // Cluster shutdown hook. Invoked by OnQuitServiceReq when the
    // peer signals SM_QUITSERVICE_REQ. Caller (typically main.cpp)
    // wires this to `io.stop()` so the wire-protocol shutdown
    // matches the SIGINT/SIGTERM path. Null = log + ignore.
    std::function<void()>             on_quit_request;

    // Live-session count source for CT_SERVICEMONITOR_ACK replies.
    // F2 collapses the legacy SESSION/PLAYER/ACTIVE_USER triple into
    // a single live count (matches the TLoginSvrAsio shape); F3
    // splits them once the player vs. session distinction lands.
    // Null → monitor replies with zeros.
    std::function<std::uint32_t()>    live_session_count;

    // F2b standalone: character data loader (no TWorldSvr).
    // If non-null AND world_client is null, OnConnectReq calls
    // LoadChar and stores snapshot immediately.
    IPlayerService*                   player_service    = nullptr;

    // F2b cluster: world-server client. If non-null, OnConnectReq
    // sends MW_ADDCHAR_ACK to TWorldSvr instead of loading char
    // locally. DM_LOADCHAR_REQ → DM_LOADCHAR_ACK round-trip handled
    // by AsioWorldClient's read loop (handlers_world.cpp).
    IWorldClient*                     world_client      = nullptr;

    // F2b Part 4: weak reference to the per-session MapSessionState.
    // Set by HandleConnection (shared_ptr owner) before entering the
    // read loop; passed to world_client->RegisterPendingSession so
    // MW_CONRESULT_REQ can route CS_CHARINFO_ACK to the right client.
    // Intentionally NOT shared — this is a per-request copy of ctx.
    std::weak_ptr<MapSessionState>    session_state_weak;

    // F3: AOI cell grid. If non-null, OnConReadyReq registers the
    // player in EnterMap and OnMoveReq calls OnMove for broadcast.
    // Null = no AOI (standalone testing without world state).
    IMapState*                        map_state         = nullptr;

    // F3: session lookup for AOI broadcasts. If non-null, OnMoveReq /
    // OnConReadyReq look up neighbour sessions to send CS_ENTER_ACK
    // and CS_MOVE_ACK. Null = log only (no actual sends).
    ISessionRegistry*                 session_registry  = nullptr;

    // F4: live monster registry.
    IMonsterRegistry*                 monster_registry  = nullptr;

    // F4: level chart for HP/exp/damage formulas.
    ILevelChart*                      level_chart       = nullptr;

    // F4: spawn manager — receives OnMonsterDied for respawn scheduling.
    class ISpawnManager*              spawn_manager     = nullptr;

    // F4 Part 3: server-side player vitals for monster damage.
    IPlayerHpRegistry*                player_hp         = nullptr;
};

// Per-session state. F1 carries the player id assigned on
// CS_CONNECT_REQ. F2b adds the character snapshot loaded by
// IPlayerService::LoadChar immediately after the token validates
// (standalone path) or after MW_ADDCHAR_ACK arrives (cluster path,
// F2b Part 3).
struct MapSessionState
{
    std::uint32_t user_id = 0;
    std::uint32_t char_id = 0;
    std::uint8_t  channel = 0;
    bool          connected = false;   // true after CS_CONNECT_ACK(SUCCESS)

    // Legacy CTPlayer::m_bCheckedSession. Flipped to true when the
    // client echoes back CS_VERIFYSESSION_ACK. Legacy uses this to
    // gate certain admin-protected actions (gm-only commands check
    // m_bCheckedSession; without it, the command is rejected with
    // an audit log). F2 just records the bit — gating lands later.
    bool          session_verified = false;

    // F2b: character data loaded from DB by IPlayerService::LoadChar.
    // Set in OnConnectReq (standalone) or OnMwAddCharAck (cluster,
    // F2b Part 3). Empty = player not yet in world.
    std::optional<legacy::CharSnapshot> snapshot;

    // F3: true after OnConReadyReq calls map_state->EnterMap.
    // Gates OnMoveReq and LeaveMap cleanup in HandleConnection.
    bool in_world = false;

    // F4 Part 4: set when player_hp->ApplyHpDelta reduces HP to 0.
    // Gates movement, attack, and skill use while dead. Cleared by
    // OnRevivalReq after revival is broadcast.
    bool is_dead = false;
};

// Top-level dispatch. Logs + drops unknown ids; future phases extend
// the switch in handlers.cpp. The signature mirrors TLoginSvr / TControl
// for consistency across the modernized servers.
boost::asio::awaitable<void> Dispatch(
    std::shared_ptr<tnetlib::AsioSession> sess,
    MapSessionState&                     state,
    const tnetlib::DecodedPacket&        packet,
    const HandlerContext&                ctx);

// Individual handlers — exposed so unit tests can drive each one
// without going through the wire dispatcher.

boost::asio::awaitable<void> OnConnectReq(
    std::shared_ptr<tnetlib::AsioSession> sess,
    MapSessionState&                     state,
    const tnetlib::DecodedPacket&        packet,
    const HandlerContext&                ctx);

// CS_CHARINFO_ACK wire serializer — declared public so handlers_map.cpp
// can call it from OnConReadyReq and test fixtures can verify the packet.
// Source: CSSender.cpp:121 — SendCS_CHARINFO_ACK
boost::asio::awaitable<void> SendCharInfoAck(
    std::shared_ptr<tnetlib::AsioSession> sess,
    const legacy::CharSnapshot&           snap);

// Client "I'm ready to receive world data" signal.
// F2b §2b standalone: sends CS_CHARINFO_ACK when snapshot loaded.
// F3 §2 full: registers in map_state, broadcasts CS_ENTER_ACK to
//   AOI neighbours, then sends CS_CHARINFO_ACK.
// Source: CSHandler.cpp:402-415.
boost::asio::awaitable<void> OnConReadyReq(
    std::shared_ptr<tnetlib::AsioSession> sess,
    MapSessionState&                     state,
    const tnetlib::DecodedPacket&        packet,
    const HandlerContext&                ctx);

// F3: movement update. Reads CS_MOVE_REQ body, calls map_state->OnMove,
// broadcasts CS_MOVE_ACK / CS_ENTER_ACK / CS_LEAVE_ACK via session_registry.
// Source: CSHandler.cpp:439-485.
boost::asio::awaitable<void> OnMoveReq(
    std::shared_ptr<tnetlib::AsioSession> sess,
    MapSessionState&                     state,
    const tnetlib::DecodedPacket&        packet,
    const HandlerContext&                ctx);

// F4 Part 4: client requests revival after death.
// Parses pos + revival type, restores HP, broadcasts CS_REVIVAL_ACK.
// Source: CSHandler.cpp:1067.
boost::asio::awaitable<void> OnRevivalReq(
    std::shared_ptr<tnetlib::AsioSession> sess,
    MapSessionState&                     state,
    const tnetlib::DecodedPacket&        packet,
    const HandlerContext&                ctx);

// F4 Part 3: client reports an incoming attack result.
// Server validates, broadcasts CS_DEFEND_ACK + CS_HPMP_ACK.
// Source: CSHandler.cpp:1485.
boost::asio::awaitable<void> OnDefendReq(
    std::shared_ptr<tnetlib::AsioSession> sess,
    MapSessionState&                     state,
    const tnetlib::DecodedPacket&        packet,
    const HandlerContext&                ctx);

// F4: basic action (attack, use item on target, etc.).
// CS_ACTION_ACK + CS_HPMP_ACK broadcast; damage calc stub.
// Source: CSHandler.cpp:1248.
boost::asio::awaitable<void> OnActionReq(
    std::shared_ptr<tnetlib::AsioSession> sess,
    MapSessionState&                     state,
    const tnetlib::DecodedPacket&        packet,
    const HandlerContext&                ctx);

// F4: skill cast. F4 Part 1 parses and logs only; full skill execution
// (CS_SKILLUSE_ACK + CS_DEFEND_REQ) is F4 Part 2.
// Source: CSHandler.cpp:2459.
boost::asio::awaitable<void> OnSkillUseReq(
    std::shared_ptr<tnetlib::AsioSession> sess,
    MapSessionState&                     state,
    const tnetlib::DecodedPacket&        packet,
    const HandlerContext&                ctx);

// Legacy backdoor-detection handler — logs the peer IP and returns
// without an ACK. Session stays alive. Source: CSHandler.cpp:14876.
boost::asio::awaitable<void> OnTerminateReq(
    std::shared_ptr<tnetlib::AsioSession> sess,
    MapSessionState&                     state,
    const tnetlib::DecodedPacket&        packet,
    const HandlerContext&                ctx);

// Legacy CSHandler.cpp:9-25 — disabled PvP-tied disconnect, body
// commented out in the shipped source. Modern preserves the wire id
// as a silent no-op.
boost::asio::awaitable<void> OnWinLdicReq(
    std::shared_ptr<tnetlib::AsioSession> sess,
    MapSessionState&                     state,
    const tnetlib::DecodedPacket&        packet,
    const HandlerContext&                ctx);

// Cluster shutdown signal — same wire path as TLoginSvr's
// SM_QUITSERVICE_REQ. Invokes ctx.on_quit_request if set.
// Source: SSHandler.cpp:9-23.
boost::asio::awaitable<void> OnQuitServiceReq(
    std::shared_ptr<tnetlib::AsioSession> sess,
    MapSessionState&                     state,
    const tnetlib::DecodedPacket&        packet,
    const HandlerContext&                ctx);

// TControlSvr 1-Hz heartbeat. Reads dwTick from the body, replies
// with CT_SERVICEMONITOR_REQ carrying { tick, session_count,
// player_count, active_user_count }. Source: CSHandler.cpp:27-68.
boost::asio::awaitable<void> OnServiceMonitorAck(
    std::shared_ptr<tnetlib::AsioSession> sess,
    MapSessionState&                     state,
    const tnetlib::DecodedPacket&        packet,
    const HandlerContext&                ctx);

// TControlSvr → Map post-dial handshake. Pure no-op. CSHandler.cpp:230.
boost::asio::awaitable<void> OnCtrlSvrReq(
    std::shared_ptr<tnetlib::AsioSession> sess,
    MapSessionState&                     state,
    const tnetlib::DecodedPacket&        packet,
    const HandlerContext&                ctx);

// Rebuild request from TControlSvr — F2 stub (logs + no-op, no ACK).
// CSHandler.cpp:214.
boost::asio::awaitable<void> OnServiceDataClearAck(
    std::shared_ptr<tnetlib::AsioSession> sess,
    MapSessionState&                     state,
    const tnetlib::DecodedPacket&        packet,
    const HandlerContext&                ctx);

// GM-issued global announcement — F2 stub reads + logs. Broadcast to
// in-game players is PENDING (F3). CSHandler.cpp:72.
boost::asio::awaitable<void> OnAnnouncementAck(
    std::shared_ptr<tnetlib::AsioSession> sess,
    MapSessionState&                     state,
    const tnetlib::DecodedPacket&        packet,
    const HandlerContext&                ctx);

// GM-issued kick by name — F2 stub reads + logs. Target lookup is
// PENDING (F3). CSHandler.cpp:92.
boost::asio::awaitable<void> OnUserKickoutAck(
    std::shared_ptr<tnetlib::AsioSession> sess,
    MapSessionState&                     state,
    const tnetlib::DecodedPacket&        packet,
    const HandlerContext&                ctx);

// Arena-ranking request — F2 silent drop (the m_pMAP gate trips in
// legacy too before F3 player state lands). CSHandler.cpp:21106.
boost::asio::awaitable<void> OnArenaRankingReq(
    std::shared_ptr<tnetlib::AsioSession> sess,
    MapSessionState&                     state,
    const tnetlib::DecodedPacket&        packet,
    const HandlerContext&                ctx);

// Client-initiated graceful close. Legacy just calls CloseSession.
// CSHandler.cpp:12138.
boost::asio::awaitable<void> OnDisconnectReq(
    std::shared_ptr<tnetlib::AsioSession> sess,
    MapSessionState&                     state,
    const tnetlib::DecodedPacket&        packet,
    const HandlerContext&                ctx);

// Client echoes back the server's verify probe — flips
// state.session_verified. CSHandler.cpp:19649.
boost::asio::awaitable<void> OnVerifySessionAck(
    std::shared_ptr<tnetlib::AsioSession> sess,
    MapSessionState&                     state,
    const tnetlib::DecodedPacket&        packet,
    const HandlerContext&                ctx);

// Round-trip latency probe — echo DWORD dwTick back as
// CS_PINGMEASUREMENT_ACK. CSHandler.cpp:19662.
boost::asio::awaitable<void> OnPingMeasurementReq(
    std::shared_ptr<tnetlib::AsioSession> sess,
    MapSessionState&                     state,
    const tnetlib::DecodedPacket&        packet,
    const HandlerContext&                ctx);

// BoW queue register — gated, forwards MW_ADDTOBOWQUEUE_REQ to World.
// CSHandler.cpp:19678.
boost::asio::awaitable<void> OnRegisterBowReq(
    std::shared_ptr<tnetlib::AsioSession> sess,
    MapSessionState&                     state,
    const tnetlib::DecodedPacket&        packet,
    const HandlerContext&                ctx);

// BoW queue cancel — gated, forwards MW_CANCELBOWQUEUE_REQ to World.
// CSHandler.cpp:19691.
boost::asio::awaitable<void> OnCancelBowQueueReq(
    std::shared_ptr<tnetlib::AsioSession> sess,
    MapSessionState&                     state,
    const tnetlib::DecodedPacket&        packet,
    const HandlerContext&                ctx);

// Combat/peace mode toggle (BYTE bMode). CSHandler.cpp:1220.
boost::asio::awaitable<void> OnChgModeReq(
    std::shared_ptr<tnetlib::AsioSession> sess,
    MapSessionState&                     state,
    const tnetlib::DecodedPacket&        packet,
    const HandlerContext&                ctx);

// Channel switch request. CSHandler.cpp:12145.
boost::asio::awaitable<void> OnChgChannelReq(
    std::shared_ptr<tnetlib::AsioSession> sess,
    MapSessionState&                     state,
    const tnetlib::DecodedPacket&        packet,
    const HandlerContext&                ctx);

// Guild-corps invite. CSHandler.cpp:8092.
boost::asio::awaitable<void> OnCorpsAskReq(
    std::shared_ptr<tnetlib::AsioSession> sess,
    MapSessionState&                     state,
    const tnetlib::DecodedPacket&        packet,
    const HandlerContext&                ctx);

// Hero list query. CSHandler.cpp:14862.
boost::asio::awaitable<void> OnHeroListReq(
    std::shared_ptr<tnetlib::AsioSession> sess,
    MapSessionState&                     state,
    const tnetlib::DecodedPacket&        packet,
    const HandlerContext&                ctx);

// Pet recall cancel. CSHandler.cpp:15443.
boost::asio::awaitable<void> OnPetCancelReq(
    std::shared_ptr<tnetlib::AsioSession> sess,
    MapSessionState&                     state,
    const tnetlib::DecodedPacket&        packet,
    const HandlerContext&                ctx);

} // namespace tmapsvr
