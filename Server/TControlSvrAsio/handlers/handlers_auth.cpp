#include "handlers.h"

#include "../senders.h"
#include "../wire_codec.h"
#include "../services/svr_type.h"
#include "MessageId.h"

#include <spdlog/spdlog.h>

namespace tcontrolsvr::handlers {

namespace {
using tnetlib::protocol::MessageId;
using tnetlib::protocol::ToMessageId;

// Legacy guard: authority 1 (MANAGER_ALL) is reserved for the
// console operator and only accepted from 127.0.0.1. Any other IP
// asking for authority 1 gets demoted to a flat-out rejection, same
// as a wrong password. See Server/TControlSvr/Handler.cpp:42-50.
bool AuthorityOneFromLoopback(const OperatorAuthResult& res,
                              const std::string& remote_ipv4)
{
    if (res.authority != 1) return true;
    return remote_ipv4 == "127.0.0.1";
}
} // namespace

boost::asio::awaitable<void>
OnOpLoginReq(std::shared_ptr<OperatorSession> op,
             std::vector<std::byte> body,
             const HandlerContext& ctx)
{
    wire::Reader r(body);
    std::string user_id, password;
    if (!r.ReadString(user_id) || !r.ReadString(password))
    {
        spdlog::warn("CT_OPLOGIN_REQ malformed body — closing");
        op->Wire()->Close();
        co_return;
    }

    const auto& remote_ip = op->Wire()->RemoteIPv4();
    spdlog::info("CT_OPLOGIN_REQ id='{}' ip={}", user_id, remote_ip);

    if (!ctx.auth)
    {
        // No auth service wired — reject loudly.
        spdlog::error("CT_OPLOGIN_REQ but no IOperatorAuthService configured");
        co_await senders::SendOpLoginAck(op->Wire(), 1, 0, 0);
        co_return;
    }

    auto result = ctx.auth->Authenticate(user_id, password);
    if (!result.ok || !AuthorityOneFromLoopback(result, remote_ip))
    {
        spdlog::info("CT_OPLOGIN_REQ id='{}' rejected", user_id);
        co_await senders::SendOpLoginAck(op->Wire(), 1, 0, 0);
        co_return;
    }

    // Duplicate-kick — legacy closes the previous session and aborts
    // the current handler with EC_SESSION_INVALIDCHAR. We do the
    // same: close the pre-existing socket, drop the new login (it
    // will retry after the kick is observed by the GUI). Without
    // this gate two GMs sharing credentials could both ack OK and
    // race their session counters.
    if (ctx.operators)
    {
        auto existing = ctx.operators->FindByUserId(user_id);
        if (existing && existing.get() != op.get())
        {
            spdlog::info("CT_OPLOGIN_REQ id='{}' duplicate — kicking previous "
                         "session (seq={})", user_id, existing->ManagerSeq());
            if (auto w = existing->Wire())
                w->Close();
            op->Wire()->Close();  // matches legacy EC_SESSION_INVALIDCHAR path
            co_return;
        }
    }

    std::uint32_t seq = 0;
    if (ctx.operators)
    {
        std::shared_ptr<OperatorSession> dup;  // already handled above
        seq = ctx.operators->Register(op, user_id, dup);
    }
    op->MarkLoggedIn(user_id, result.authority, seq);

    co_await senders::SendOpLoginAck(op->Wire(), 0, result.authority, seq);

    if (ctx.inventory)
    {
        co_await senders::SendGroupListAck(op->Wire(),     ctx.inventory->Groups());
        co_await senders::SendMachineListAck(op->Wire(),   ctx.inventory->Machines());
        co_await senders::SendSvrTypeListAck(op->Wire(),   ctx.inventory->Types());
    }
    const std::uint8_t auto_start = ctx.auto_start ? *ctx.auto_start : 0;
    co_await senders::SendServiceAutoStartAck(op->Wire(), auto_start);
}

boost::asio::awaitable<void>
OnStLoginReq(std::shared_ptr<OperatorSession> op,
             std::vector<std::byte> body,
             const HandlerContext& ctx)
{
    wire::Reader r(body);
    std::string user_id, password;
    if (!r.ReadString(user_id) || !r.ReadString(password))
    {
        spdlog::warn("CT_STLOGIN_REQ malformed body — closing");
        op->Wire()->Close();
        co_return;
    }
    spdlog::info("CT_STLOGIN_REQ id='{}' ip={}", user_id, op->Wire()->RemoteIPv4());
    if (!ctx.auth)
    {
        co_await senders::SendStLoginAck(op->Wire(), 1, 0);
        co_return;
    }
    auto result = ctx.auth->Authenticate(user_id, password);
    if (!result.ok)
    {
        co_await senders::SendStLoginAck(op->Wire(), 1, 0);
        co_return;
    }
    // STLogin is the read-only stat-tool flow — no duplicate kick,
    // no manager_seq, no follow-up list acks (legacy only emits
    // CT_STLOGIN_ACK and that's it).
    op->MarkLoggedIn(user_id, result.authority, 0);
    co_await senders::SendStLoginAck(op->Wire(), 0, result.authority);
}

boost::asio::awaitable<void>
OnServiceAutoStartReq(std::shared_ptr<OperatorSession> op,
                      std::vector<std::byte> body,
                      const HandlerContext& ctx)
{
    wire::Reader r(body);
    std::uint8_t new_value = 0;
    if (!r.Read(new_value))
    {
        spdlog::warn("CT_SERVICEAUTOSTART_REQ malformed body");
        co_return;
    }
    if (ctx.auto_start) *ctx.auto_start = new_value;
    spdlog::info("CT_SERVICEAUTOSTART set to {} by op='{}' (seq={})",
        new_value, op->UserId(), op->ManagerSeq());

    // Broadcast new state to every logged-in operator (legacy walks
    // m_mapMANAGER). Iteration is over a snapshot so a session
    // closing under us doesn't invalidate the loop.
    if (ctx.operators)
    {
        for (const auto& peer : ctx.operators->SnapshotLoggedIn())
        {
            if (!peer || !peer->Wire() || !peer->Wire()->IsOpen()) continue;
            co_await senders::SendServiceAutoStartAck(peer->Wire(), new_value);
        }
    }
    else
    {
        co_await senders::SendServiceAutoStartAck(op->Wire(), new_value);
    }
}

boost::asio::awaitable<void>
Dispatch(std::shared_ptr<OperatorSession> op,
         std::uint16_t wId,
         std::vector<std::byte> body,
         const HandlerContext& ctx)
{
    const auto id = ToMessageId(wId);
    switch (id)
    {
    case MessageId::CT_OPLOGIN_REQ:
        co_await OnOpLoginReq(std::move(op), std::move(body), ctx);
        break;
    case MessageId::CT_STLOGIN_REQ:
        co_await OnStLoginReq(std::move(op), std::move(body), ctx);
        break;
    case MessageId::CT_SERVICEAUTOSTART_REQ:
        co_await OnServiceAutoStartReq(std::move(op), std::move(body), ctx);
        break;
    case MessageId::CT_SERVICESTAT_REQ:
        co_await OnServiceStatReq(std::move(op), std::move(body), ctx);
        break;
    case MessageId::CT_SERVICECONTROL_REQ:
        co_await OnServiceControlReq(std::move(op), std::move(body), ctx);
        break;
    case MessageId::CT_NEWCONNECT_REQ:
        co_await OnNewConnectReq(std::move(op), std::move(body), ctx);
        break;
    case MessageId::CT_RECONNECT_REQ:
        co_await OnReconnectReq(std::move(op), std::move(body), ctx);
        break;
    case MessageId::CT_ANNOUNCEMENT_REQ:
        co_await OnAnnouncementReq(std::move(op), std::move(body), ctx);
        break;
    case MessageId::CT_USERKICKOUT_REQ:
        co_await OnUserKickoutReq(std::move(op), std::move(body), ctx);
        break;
    case MessageId::CT_USERMOVE_REQ:
        co_await OnUserMoveReq(std::move(op), std::move(body), ctx);
        break;
    case MessageId::CT_USERPOSITION_REQ:
        co_await OnUserPositionReq(std::move(op), std::move(body), ctx);
        break;
    case MessageId::CT_USERPROTECTED_REQ:
        co_await OnUserProtectedReq(std::move(op), std::move(body), ctx);
        break;
    case MessageId::CT_CHARMSG_REQ:
        co_await OnCharMsgReq(std::move(op), std::move(body), ctx);
        break;
    case MessageId::CT_CHATBAN_REQ:
        co_await OnChatBanReq(std::move(op), std::move(body), ctx);
        break;
    case MessageId::CT_CHATBANLIST_REQ:
        co_await OnChatBanListReq(std::move(op), std::move(body), ctx);
        break;
    case MessageId::CT_CHATBANLISTDEL_REQ:
        co_await OnChatBanListDelReq(std::move(op), std::move(body), ctx);
        break;
    case MessageId::CT_MONSPAWNFIND_REQ:
        co_await OnMonSpawnFindReq(std::move(op), std::move(body), ctx);
        break;

    // --- F4: event manager ---------------------------------------
    case MessageId::CT_EVENTLIST_REQ:
        co_await OnEventListReq(std::move(op), std::move(body), ctx);
        break;
    case MessageId::CT_EVENTCHANGE_REQ:
        co_await OnEventChangeReq(std::move(op), std::move(body), ctx);
        break;
    case MessageId::CT_EVENTDEL_REQ:
        co_await OnEventDelReq(std::move(op), std::move(body), ctx);
        break;
    case MessageId::CT_EVENTUPDATE_REQ:
        co_await OnEventUpdateReq(std::move(op), std::move(body), ctx);
        break;
    case MessageId::CT_EVENTMSG_REQ:
        co_await OnEventMsgReq(std::move(op), std::move(body), ctx);
        break;
    case MessageId::CT_CASHITEMSALE_REQ:
        co_await OnCashItemSaleReq(std::move(op), std::move(body), ctx);
        break;
    case MessageId::CT_CASHSHOPSTOP_REQ:
        co_await OnCashShopStopReq(std::move(op), std::move(body), ctx);
        break;
    case MessageId::CT_CASHITEMLIST_REQ:
        co_await OnCashItemListReq(std::move(op), std::move(body), ctx);
        break;

    // F4 raw passthrough forwarders — quartal events, tournament,
    // help message, RPS game, CM gift. Each routes to a single
    // World server in the targeted group (legacy parity).
    case MessageId::CT_EVENTQUARTERLIST_REQ:
        co_await ForwardRawToType(std::move(op), wId, std::move(body),
            svr_type::kWorldSvr, /*single_target=*/true, ctx);
        break;
    case MessageId::CT_EVENTQUARTERUPDATE_REQ:
        co_await ForwardRawToType(std::move(op), wId, std::move(body),
            svr_type::kWorldSvr, /*single_target=*/false, ctx);
        break;
    case MessageId::CT_TOURNAMENTEVENT_REQ:
        co_await ForwardRawToType(std::move(op), wId, std::move(body),
            svr_type::kWorldSvr, /*single_target=*/false, ctx);
        break;
    case MessageId::CT_HELPMESSAGE_REQ:
        co_await ForwardRawToType(std::move(op), wId, std::move(body),
            svr_type::kWorldSvr, /*single_target=*/false, ctx);
        break;
    case MessageId::CT_RPSGAMEDATA_REQ:
    case MessageId::CT_RPSGAMECHANGE_REQ:
        co_await ForwardRawToType(std::move(op), wId, std::move(body),
            svr_type::kWorldSvr, /*single_target=*/false, ctx);
        break;
    case MessageId::CT_CMGIFT_REQ:
    case MessageId::CT_CMGIFTLIST_REQ:
    case MessageId::CT_CMGIFTCHARTUPDATE_REQ:
        co_await ForwardRawToType(std::move(op), wId, std::move(body),
            svr_type::kWorldSvr, /*single_target=*/false, ctx);
        break;

    // --- F5: patch metadata + castle ----------------------------
    case MessageId::CT_UPDATEPATCH_REQ:
        co_await OnUpdatePatchReq(std::move(op), std::move(body), ctx);
        break;
    case MessageId::CT_PREVERSIONTABLE_REQ:
        co_await OnPreVersionTableReq(std::move(op), std::move(body), ctx);
        break;
    case MessageId::CT_PREVERSIONUPDATE_REQ:
        co_await OnPreVersionUpdateReq(std::move(op), std::move(body), ctx);
        break;
    case MessageId::CT_CASTLEINFO_REQ:
        co_await OnCastleInfoReq(std::move(op), std::move(body), ctx);
        break;
    case MessageId::CT_CASTLEGUILDCHG_REQ:
        co_await OnCastleGuildChgReq(std::move(op), std::move(body), ctx);
        break;
    case MessageId::CT_CASTLEENABLE_REQ:
        co_await OnCastleEnableReq(std::move(op), std::move(body), ctx);
        break;

    // --- Round-2 audit: missing operator-side handlers -----------
    case MessageId::CT_ITEMFIND_REQ:
        co_await OnItemFindReq(std::move(op), std::move(body), ctx);
        break;
    case MessageId::CT_ITEMSTATE_REQ:
        co_await OnItemStateReq(std::move(op), std::move(body), ctx);
        break;
    case MessageId::CT_MONACTION_REQ:
        co_await OnMonActionReq(std::move(op), std::move(body), ctx);
        break;
    case MessageId::CT_SERVICEDATACLEAR_REQ:
        co_await OnServiceDataClearReq(std::move(op), std::move(body), ctx);
        break;
    case MessageId::CT_PLATFORM_REQ:
        co_await OnPlatformReq(std::move(op), std::move(body), ctx);
        break;
    default:
        // F3+ wires the rest of the 65 handlers. For now log the gap
        // so the bring-up notes can surface what TController.exe
        // actually sends in the wild.
        spdlog::warn("control_svr: unhandled CT_* id=0x{:04X} body={} bytes",
            wId, body.size());
        break;
    }
}

} // namespace tcontrolsvr::handlers
