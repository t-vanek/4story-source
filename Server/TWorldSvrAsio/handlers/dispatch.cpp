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
    case MessageId::MW_GUILDINVITE_ACK:
        co_await OnGuildInviteAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_GUILDINVITEANSWER_ACK:
        co_await OnGuildInviteAnswerAck(std::move(peer), std::move(body),
            ctx);
        co_return;
    case MessageId::MW_GUILDMEMBERLIST_ACK:
        co_await OnGuildMemberListAck(std::move(peer), std::move(body),
            ctx);
        co_return;
    case MessageId::MW_GUILDARTICLELIST_ACK:
        co_await OnGuildArticleListAck(std::move(peer), std::move(body),
            ctx);
        co_return;
    case MessageId::MW_GUILDARTICLEADD_ACK:
        co_await OnGuildArticleAddAck(std::move(peer), std::move(body),
            ctx);
        co_return;
    case MessageId::MW_GUILDARTICLEDEL_ACK:
        co_await OnGuildArticleDelAck(std::move(peer), std::move(body),
            ctx);
        co_return;
    case MessageId::MW_GUILDARTICLEUPDATE_ACK:
        co_await OnGuildArticleUpdateAck(std::move(peer), std::move(body),
            ctx);
        co_return;
    case MessageId::MW_GUILDINFO_ACK:
        co_await OnGuildInfoAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_GUILDMONEYRECOVER_ACK:
        co_await OnGuildMoneyRecoverAck(std::move(peer), std::move(body),
            ctx);
        co_return;
    case MessageId::DM_GUILDEXTINCTION_REQ:
        co_await OnGuildExtinctionReq(std::move(peer), std::move(body),
            ctx);
        co_return;
    case MessageId::MW_GUILDWANTEDADD_ACK:
        co_await OnGuildWantedAddAck(std::move(peer), std::move(body),
            ctx);
        co_return;
    case MessageId::MW_GUILDWANTEDDEL_ACK:
        co_await OnGuildWantedDelAck(std::move(peer), std::move(body),
            ctx);
        co_return;
    case MessageId::MW_GUILDWANTEDLIST_ACK:
        co_await OnGuildWantedListAck(std::move(peer), std::move(body),
            ctx);
        co_return;
    case MessageId::MW_GUILDVOLUNTEERING_ACK:
        co_await OnGuildVolunteeringAck(std::move(peer), std::move(body),
            ctx);
        co_return;
    case MessageId::MW_GUILDVOLUNTEERINGDEL_ACK:
        co_await OnGuildVolunteeringDelAck(std::move(peer),
            std::move(body), ctx);
        co_return;
    case MessageId::MW_GUILDVOLUNTEERLIST_ACK:
        co_await OnGuildVolunteerListAck(std::move(peer), std::move(body),
            ctx);
        co_return;
    case MessageId::MW_GUILDVOLUNTEERREPLY_ACK:
        co_await OnGuildVolunteerReplyAck(std::move(peer), std::move(body),
            ctx);
        co_return;
    case MessageId::DM_GUILDPVPOINT_REQ:
        co_await OnGuildPvPointReq(std::move(peer), std::move(body), ctx);
        co_return;

    // ---- W3a-14: DB-side fan-in --------------------------------
    case MessageId::DM_GUILDDUTY_REQ:
        co_await OnGuildDutyReq(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::DM_GUILDPEER_REQ:
        co_await OnGuildPeerReq(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::DM_GUILDCONTRIBUTION_REQ:
        co_await OnGuildContributionReq(std::move(peer), std::move(body),
            ctx);
        co_return;
    case MessageId::DM_GUILDLEVEL_REQ:
        co_await OnGuildLevelReq(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::DM_GUILDPOINTREWARD_REQ:
        co_await OnGuildPointRewardReq(std::move(peer), std::move(body),
            ctx);
        co_return;

    // ---- W3a-15: fame + article DB fan-in ----------------------
    case MessageId::DM_GUILDFAME_REQ:
        co_await OnGuildFameReq(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::DM_GUILDARTICLEADD_REQ:
        co_await OnGuildArticleAddReq(std::move(peer), std::move(body),
            ctx);
        co_return;
    case MessageId::DM_GUILDARTICLEDEL_REQ:
        co_await OnGuildArticleDelReq(std::move(peer), std::move(body),
            ctx);
        co_return;
    case MessageId::DM_GUILDARTICLEUPDATE_REQ:
        co_await OnGuildArticleUpdateReq(std::move(peer), std::move(body),
            ctx);
        co_return;

    // ---- W3a-16: wanted/volunteering DB fan-in -----------------
    case MessageId::DM_GUILDWANTEDADD_REQ:
        co_await OnGuildWantedAddReq(std::move(peer), std::move(body),
            ctx);
        co_return;
    case MessageId::DM_GUILDWANTEDDEL_REQ:
        co_await OnGuildWantedDelReq(std::move(peer), std::move(body),
            ctx);
        co_return;
    case MessageId::DM_GUILDVOLUNTEERING_REQ:
        co_await OnGuildVolunteeringReq(std::move(peer), std::move(body),
            ctx);
        co_return;
    case MessageId::DM_GUILDVOLUNTEERINGDEL_REQ:
        co_await OnGuildVolunteeringDelReq(std::move(peer), std::move(body),
            ctx);
        co_return;

    // ---- W3a-17: leave/kickout DB fan-in -----------------------
    case MessageId::DM_GUILDLEAVE_REQ:
        co_await OnGuildLeaveReq(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::DM_GUILDKICKOUT_REQ:
        co_await OnGuildKickoutReq(std::move(peer), std::move(body), ctx);
        co_return;

    // W3a-18+ picks up tactics subsystem, cabinet item codec,
    // PvP record listing. … see README §4.

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
