#pragma once

// Handler dispatch for TControlSvrAsio. F1 wires only the post-login
// flow:
//
//   CT_OPLOGIN_REQ → CT_OPLOGIN_ACK
//                    CT_GROUPLIST_ACK
//                    CT_MACHINELIST_ACK
//                    CT_SVRTYPELIST_ACK
//                    CT_SERVICEAUTOSTART_ACK
//   CT_STLOGIN_REQ → CT_STLOGIN_ACK
//   CT_SERVICEAUTOSTART_REQ → broadcast CT_SERVICEAUTOSTART_ACK
//
// Service / event / admin / patch handlers ship in F2..F5. Unknown
// packet ids are logged and silently dropped so the GUI client
// doesn't crash on packets we haven't implemented yet.

#include "../control_session.h"
#include "../operator_session.h"
#include "../peer_session.h"
#include "../services/admin_audit_logger.h"
#include "../services/alerter.h"
#include "../services/chat_ban_repository.h"
#include "../services/event_registry.h"
#include "../services/event_repository.h"
#include "../services/operator_auth_service.h"
#include "../services/operator_registry.h"
#include "../services/patch_metadata_service.h"
#include "../services/peer_registry.h"
#include "../services/service_controller.h"
#include "../services/service_inventory.h"
#include "../services/user_protected_service.h"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>

#include <cstdint>
#include <memory>
#include <string>

namespace tcontrolsvr {

// Shared per-request context handed to every handler. Owned by the
// ControlServer; non-owning pointers so the context is cheap to copy
// per-packet and so test fakes can substitute parts independently.
class PeerDialer;

struct HandlerContext
{
    IOperatorAuthService*    auth         = nullptr;
    IServiceInventory*       inventory    = nullptr;
    OperatorRegistry*        operators    = nullptr;
    PeerRegistry*            peers        = nullptr;
    IServiceController*      controller   = nullptr;
    PeerDialer*              dialer       = nullptr;
    IAdminAuditLogger*       audit        = nullptr;
    IUserProtectedService*   user_ban     = nullptr;
    ChatBanRepository*       chat_bans    = nullptr;
    EventRegistry*           events       = nullptr;
    IEventRepository*        event_repo   = nullptr;
    IPatchMetadataService*   patch_meta   = nullptr;
    IAlerter*                alerter      = nullptr;
    boost::asio::io_context* io           = nullptr;

    // Mirror of legacy CTControlSvrModule::m_bAutoStart — whether
    // the cluster scheduler auto-restarts a crashed daemon. Mutated
    // by CT_SERVICEAUTOSTART_REQ broadcast.
    std::uint8_t*          auto_start = nullptr;
};

namespace handlers {

// Top-level dispatch. Returns when the body is unsupported in F1 (no
// reply), or after the corresponding ack chain has been emitted.
boost::asio::awaitable<void> Dispatch(
    std::shared_ptr<OperatorSession> op,
    std::uint16_t wId,
    std::vector<std::byte> body,
    const HandlerContext& ctx);

// Individual handlers — exposed so unit tests can drive each one
// without going through the wire dispatcher.

boost::asio::awaitable<void> OnOpLoginReq(
    std::shared_ptr<OperatorSession> op,
    std::vector<std::byte> body,
    const HandlerContext& ctx);

boost::asio::awaitable<void> OnStLoginReq(
    std::shared_ptr<OperatorSession> op,
    std::vector<std::byte> body,
    const HandlerContext& ctx);

boost::asio::awaitable<void> OnServiceAutoStartReq(
    std::shared_ptr<OperatorSession> op,
    std::vector<std::byte> body,
    const HandlerContext& ctx);

// --- F2: service lifecycle + monitoring -----------------------------

boost::asio::awaitable<void> OnServiceStatReq(
    std::shared_ptr<OperatorSession> op,
    std::vector<std::byte> body,
    const HandlerContext& ctx);

boost::asio::awaitable<void> OnServiceControlReq(
    std::shared_ptr<OperatorSession> op,
    std::vector<std::byte> body,
    const HandlerContext& ctx);

boost::asio::awaitable<void> OnNewConnectReq(
    std::shared_ptr<OperatorSession> op,
    std::vector<std::byte> body,
    const HandlerContext& ctx);

boost::asio::awaitable<void> OnReconnectReq(
    std::shared_ptr<OperatorSession> op,
    std::vector<std::byte> body,
    const HandlerContext& ctx);

// Inbound from peer servers (Login / Map / World / Patch / Log):
//   CT_SERVICEMONITOR_REQ = {
//     DWORD dwTick, DWORD dwSession, DWORD dwUser, DWORD dwActiveUser
//   }
boost::asio::awaitable<void> OnServiceMonitorReq(
    std::shared_ptr<PeerSession> peer,
    std::vector<std::byte> body,
    const HandlerContext& ctx);

// Drive the read loop for an outbound peer connection. Dispatches
// CT_* packets the peer pushes to us (SERVICEMONITOR_REQ in F2; the
// admin-ack forwarders ITEMFIND_ACK / ITEMSTATE_ACK / CHATBAN_ACK /
// CASTLEINFO_ACK / etc. arrive here in F3).
boost::asio::awaitable<void> RunPeerLoop(
    std::shared_ptr<PeerSession> peer,
    HandlerContext ctx);

// --- F3: admin operations (operator → control → peer broadcast) -----

boost::asio::awaitable<void> OnAnnouncementReq(
    std::shared_ptr<OperatorSession> op,
    std::vector<std::byte> body,
    const HandlerContext& ctx);

boost::asio::awaitable<void> OnUserKickoutReq(
    std::shared_ptr<OperatorSession> op,
    std::vector<std::byte> body,
    const HandlerContext& ctx);

boost::asio::awaitable<void> OnUserMoveReq(
    std::shared_ptr<OperatorSession> op,
    std::vector<std::byte> body,
    const HandlerContext& ctx);

boost::asio::awaitable<void> OnUserPositionReq(
    std::shared_ptr<OperatorSession> op,
    std::vector<std::byte> body,
    const HandlerContext& ctx);

boost::asio::awaitable<void> OnUserProtectedReq(
    std::shared_ptr<OperatorSession> op,
    std::vector<std::byte> body,
    const HandlerContext& ctx);

boost::asio::awaitable<void> OnCharMsgReq(
    std::shared_ptr<OperatorSession> op,
    std::vector<std::byte> body,
    const HandlerContext& ctx);

boost::asio::awaitable<void> OnChatBanReq(
    std::shared_ptr<OperatorSession> op,
    std::vector<std::byte> body,
    const HandlerContext& ctx);

// Inbound from World/Relay peer: CT_CHATBAN_ACK = { BYTE bRet,
// DWORD ban_seq, DWORD manager_id }. Aggregates per-seq; when all
// expected acks land, the operator gets one CT_CHATBAN_ACK.
boost::asio::awaitable<void> OnPeerChatBanAck(
    std::shared_ptr<PeerSession> peer,
    std::vector<std::byte> body,
    const HandlerContext& ctx);

boost::asio::awaitable<void> OnChatBanListReq(
    std::shared_ptr<OperatorSession> op,
    std::vector<std::byte> body,
    const HandlerContext& ctx);

boost::asio::awaitable<void> OnChatBanListDelReq(
    std::shared_ptr<OperatorSession> op,
    std::vector<std::byte> body,
    const HandlerContext& ctx);

boost::asio::awaitable<void> OnMonSpawnFindReq(
    std::shared_ptr<OperatorSession> op,
    std::vector<std::byte> body,
    const HandlerContext& ctx);

// --- F4: event manager ------------------------------------------------

boost::asio::awaitable<void> OnEventListReq(
    std::shared_ptr<OperatorSession> op,
    std::vector<std::byte> body,
    const HandlerContext& ctx);

boost::asio::awaitable<void> OnEventChangeReq(
    std::shared_ptr<OperatorSession> op,
    std::vector<std::byte> body,
    const HandlerContext& ctx);

boost::asio::awaitable<void> OnEventDelReq(
    std::shared_ptr<OperatorSession> op,
    std::vector<std::byte> body,
    const HandlerContext& ctx);

boost::asio::awaitable<void> OnEventUpdateReq(
    std::shared_ptr<OperatorSession> op,
    std::vector<std::byte> body,
    const HandlerContext& ctx);

boost::asio::awaitable<void> OnEventMsgReq(
    std::shared_ptr<OperatorSession> op,
    std::vector<std::byte> body,
    const HandlerContext& ctx);

boost::asio::awaitable<void> OnCashItemSaleReq(
    std::shared_ptr<OperatorSession> op,
    std::vector<std::byte> body,
    const HandlerContext& ctx);

boost::asio::awaitable<void> OnCashShopStopReq(
    std::shared_ptr<OperatorSession> op,
    std::vector<std::byte> body,
    const HandlerContext& ctx);

boost::asio::awaitable<void> OnCashItemListReq(
    std::shared_ptr<OperatorSession> op,
    std::vector<std::byte> body,
    const HandlerContext& ctx);

// Generic forward — used by EVENTQUARTER*, TOURNAMENTEVENT,
// HELPMESSAGE, RPSGAME*, CMGIFT* handlers that just shuttle the
// packet to a peer World/Map of a given group + type.
boost::asio::awaitable<void> ForwardRawToType(
    std::shared_ptr<OperatorSession> op,
    std::uint16_t wId,
    std::vector<std::byte> body,
    std::uint8_t target_type,
    bool single_target,
    const HandlerContext& ctx);

// --- F5: patch metadata + castle ----------------------------------

boost::asio::awaitable<void> OnUpdatePatchReq(
    std::shared_ptr<OperatorSession> op,
    std::vector<std::byte> body,
    const HandlerContext& ctx);

boost::asio::awaitable<void> OnPreVersionTableReq(
    std::shared_ptr<OperatorSession> op,
    std::vector<std::byte> body,
    const HandlerContext& ctx);

boost::asio::awaitable<void> OnPreVersionUpdateReq(
    std::shared_ptr<OperatorSession> op,
    std::vector<std::byte> body,
    const HandlerContext& ctx);

boost::asio::awaitable<void> OnCastleInfoReq(
    std::shared_ptr<OperatorSession> op,
    std::vector<std::byte> body,
    const HandlerContext& ctx);

boost::asio::awaitable<void> OnCastleGuildChgReq(
    std::shared_ptr<OperatorSession> op,
    std::vector<std::byte> body,
    const HandlerContext& ctx);

boost::asio::awaitable<void> OnCastleEnableReq(
    std::shared_ptr<OperatorSession> op,
    std::vector<std::byte> body,
    const HandlerContext& ctx);

// Castle / item / monspawn ACKs forwarded from a peer back to the
// originating operator. Each body starts with DWORD manager_id —
// the rest of the body is repacked verbatim onto the operator
// session.
boost::asio::awaitable<void> OnPeerCastleInfoAck(
    std::shared_ptr<PeerSession> peer,
    std::vector<std::byte> body,
    const HandlerContext& ctx);
boost::asio::awaitable<void> OnPeerCastleGuildChgAck(
    std::shared_ptr<PeerSession> peer,
    std::vector<std::byte> body,
    const HandlerContext& ctx);

} // namespace handlers
} // namespace tcontrolsvr
