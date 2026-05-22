#include "handlers.h"

#include "MessageId.h"

#include <spdlog/spdlog.h>

namespace tworldsvr::handlers {

boost::asio::awaitable<void>
Dispatch(std::shared_ptr<PeerSession>  peer,
         std::uint16_t                 wId,
         std::vector<std::byte>        body,
         const HandlerContext&         ctx)
{
    using tnetlib::protocol::MessageId;
    using tnetlib::protocol::ToMessageId;
    using tnetlib::protocol::NameOf;

    const auto id = ToMessageId(wId);
    switch (id)
    {
    // ---- W3a-2/W3a-3: RW handshake + relay (handlers_relay.cpp) ---
    case MessageId::RW_RELAYSVR_REQ:
        co_await OnRelaysvrReq(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::RW_ENTERCHAR_REQ:
        co_await OnEnterCharReq(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::RW_RELAYCONNECT_REQ:
        co_await OnRelayConnectReq(std::move(peer), std::move(body), ctx);
        co_return;

    // ---- W2: char lifecycle (handlers_char.cpp) -------------------
    case MessageId::MW_ADDCHAR_ACK:
        co_await OnAddCharAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_CLOSECHAR_ACK:
        co_await OnCloseCharAck(std::move(peer), std::move(body), ctx);
        co_return;

    // ---- W3a-3: char base update (handlers_char_base.cpp) ---------
    case MessageId::MW_CHANGECHARBASE_ACK:
        co_await OnChangeCharBaseAck(std::move(peer), std::move(body), ctx);
        co_return;

    // ---- W3a-1: guild load (handlers_guild.cpp) -------------------
    case MessageId::DM_GUILDLOAD_ACK:
        co_await OnGuildLoadAck(std::move(peer), std::move(body), ctx);
        co_return;

    // ---- W3a-4 / W3a-4b / W3a-4c: guild mutating handlers ---------
    case MessageId::MW_GUILDLEAVE_ACK:
        co_await OnGuildLeaveAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::DM_GUILDDISORGANIZATION_REQ:
        co_await OnGuildDisorganizationReq(std::move(peer), std::move(body),
            ctx);
        co_return;
    case MessageId::MW_GUILDDUTY_ACK:
        co_await OnGuildDutyAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_GUILDFAME_ACK:
        co_await OnGuildFameAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_GUILDKICKOUT_ACK:
        co_await OnGuildKickoutAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_GUILDCONTRIBUTION_ACK:
        co_await OnGuildContributionAck(std::move(peer), std::move(body),
            ctx);
        co_return;
    case MessageId::DM_GUILDMEMBERADD_REQ:
        co_await OnGuildMemberAddReq(std::move(peer), std::move(body), ctx);
        co_return;

    // ---- W3a-5: peerage + cabinet ----------------------------------
    case MessageId::MW_GUILDPEER_ACK:
        co_await OnGuildPeerAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::DM_GUILDCABINETMAX_REQ:
        co_await OnGuildCabinetMaxReq(std::move(peer), std::move(body), ctx);
        co_return;

    // W3a-6+ picks up Invite answer / Articles / Tactics /
    // Volunteers / PvP record / Point reward handlers.
    // … see README §4.

    default:
        break;
    }

    const auto name = NameOf(id);
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (name.empty())
    {
        spdlog::warn("world_dispatch[{}]: unknown wID=0x{:04X} body={} bytes "
                     "— dropped", ip, wId, body.size());
    }
    else
    {
        spdlog::info("world_dispatch[{}]: wID=0x{:04X} ({}) body={} bytes "
                     "— no handler yet", ip, wId, name, body.size());
    }
    co_return;
}

} // namespace tworldsvr::handlers
