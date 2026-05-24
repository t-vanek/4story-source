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
    case MessageId::MW_ENTERSVR_ACK:
        co_await OnEnterSvrAck(std::move(peer), std::move(body), ctx);
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
    case MessageId::MW_GUILDDISORGANIZATION_ACK:
        co_await OnGuildDisorganizationAck(std::move(peer),
            std::move(body), ctx);
        co_return;
    case MessageId::MW_GUILDPOINTREWARD_ACK:
        co_await OnGuildPointRewardAck(std::move(peer), std::move(body),
            ctx);
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

    // ---- W3a-18: guild establishment ---------------------------
    case MessageId::MW_GUILDESTABLISH_ACK:
        co_await OnGuildEstablishAck(std::move(peer), std::move(body),
            ctx);
        co_return;

    // ---- W3a-20: vestigial DB-server ACK echoes ----------------
    case MessageId::DM_GUILDESTABLISH_ACK:
        co_await OnGuildEstablishAckEcho(std::move(peer), std::move(body),
            ctx);
        co_return;
    case MessageId::DM_GUILDDISORGANIZATION_ACK:
        co_await OnGuildDisorganizationAckEcho(std::move(peer),
            std::move(body), ctx);
        co_return;
    case MessageId::DM_GUILDEXTINCTION_ACK:
        co_await OnGuildExtinctionAckEcho(std::move(peer),
            std::move(body), ctx);
        co_return;

    // ---- W3a-21: PvP record audit log --------------------------
    case MessageId::DM_PVPRECORD_REQ:
        co_await OnPvPRecordReq(std::move(peer), std::move(body), ctx);
        co_return;

    // ---- W3a-22: full-row guild update fan-in ------------------
    case MessageId::DM_GUILDUPDATE_REQ:
        co_await OnGuildUpdateReq(std::move(peer), std::move(body),
            ctx);
        co_return;

    // ---- W3a-23: PvP record list reader ------------------------
    case MessageId::MW_GUILDPVPRECORD_ACK:
        co_await OnGuildPvPRecordAck(std::move(peer), std::move(body),
            ctx);
        co_return;

    // ---- W3a-24: per-period war-result fan-in ------------------
    case MessageId::MW_LOCALRECORD_ACK:
        co_await OnLocalRecordAck(std::move(peer), std::move(body),
            ctx);
        co_return;

    // ---- W3a-26/W3a-37: cabinet flow ---------------------------
    case MessageId::MW_GUILDCABINETLIST_ACK:
        co_await OnGuildCabinetListAck(std::move(peer), std::move(body),
            ctx);
        co_return;
    case MessageId::MW_GUILDCABINETPUTIN_ACK:
        co_await OnGuildCabinetPutinAck(std::move(peer), std::move(body),
            ctx);
        co_return;
    case MessageId::MW_GUILDCABINETTAKEOUT_ACK:
        co_await OnGuildCabinetTakeoutAck(std::move(peer),
            std::move(body), ctx);
        co_return;

    // ---- W3a-27: PvP point reward log reader -------------------
    case MessageId::MW_GUILDPOINTLOG_ACK:
        co_await OnGuildPointLogAck(std::move(peer), std::move(body),
            ctx);
        co_return;

    // ---- W3a-29: PvP-point gain/use fan-in ---------------------
    case MessageId::MW_GAINPVPPOINT_ACK:
        co_await OnGainPvPointAck(std::move(peer), std::move(body),
            ctx);
        co_return;

    // ---- W3a-31: tactics wanted board --------------------------
    case MessageId::MW_GUILDTACTICSWANTEDADD_ACK:
        co_await OnGuildTacticsWantedAddAck(std::move(peer),
            std::move(body), ctx);
        co_return;
    case MessageId::MW_GUILDTACTICSWANTEDDEL_ACK:
        co_await OnGuildTacticsWantedDelAck(std::move(peer),
            std::move(body), ctx);
        co_return;
    case MessageId::MW_GUILDTACTICSWANTEDLIST_ACK:
        co_await OnGuildTacticsWantedListAck(std::move(peer),
            std::move(body), ctx);
        co_return;

    // ---- W3a-32: tactics volunteer (applicant) flow ------------
    case MessageId::MW_GUILDTACTICSVOLUNTEERING_ACK:
        co_await OnGuildTacticsVolunteeringAck(std::move(peer),
            std::move(body), ctx);
        co_return;
    case MessageId::MW_GUILDTACTICSVOLUNTEERINGDEL_ACK:
        co_await OnGuildTacticsVolunteeringDelAck(std::move(peer),
            std::move(body), ctx);
        co_return;
    case MessageId::MW_GUILDTACTICSVOLUNTEERLIST_ACK:
        co_await OnGuildTacticsVolunteerListAck(std::move(peer),
            std::move(body), ctx);
        co_return;

    // ---- W3a-33: tactics reply (accept/reject hire) ------------
    case MessageId::MW_GUILDTACTICSREPLY_ACK:
        co_await OnGuildTacticsReplyAck(std::move(peer),
            std::move(body), ctx);
        co_return;

    // ---- W3a-34: tactics kickout + list ------------------------
    case MessageId::MW_GUILDTACTICSKICKOUT_ACK:
        co_await OnGuildTacticsKickoutAck(std::move(peer),
            std::move(body), ctx);
        co_return;
    case MessageId::MW_GUILDTACTICSLIST_ACK:
        co_await OnGuildTacticsListAck(std::move(peer),
            std::move(body), ctx);
        co_return;

    // ---- W3a-35: tactics invite + answer (chief-initiated) -----
    case MessageId::MW_GUILDTACTICSINVITE_ACK:
        co_await OnGuildTacticsInviteAck(std::move(peer),
            std::move(body), ctx);
        co_return;
    case MessageId::MW_GUILDTACTICSANSWER_ACK:
        co_await OnGuildTacticsAnswerAck(std::move(peer),
            std::move(body), ctx);
        co_return;

    // ---- W3b-1/W3b-2: party invite + formation (handlers_party) -
    case MessageId::MW_PARTYADD_ACK:
        co_await OnPartyAddAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_PARTYJOIN_ACK:
        co_await OnPartyJoinAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_PARTYDEL_ACK:
        co_await OnPartyDelAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_PARTYMANSTAT_ACK:
        co_await OnPartyManstatAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_CHGPARTYCHIEF_ACK:
        co_await OnChgPartyChiefAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_CHGPARTYTYPE_ACK:
        co_await OnChgPartyTypeAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_PARTYMEMBERRECALL_ACK:
        co_await OnPartyMemberRecallAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_PARTYMEMBERRECALLANS_ACK:
        co_await OnPartyMemberRecallAnsAck(std::move(peer), std::move(body),
            ctx);
        co_return;
    case MessageId::MW_PARTYORDERTAKEITEM_ACK:
        co_await OnPartyOrderTakeItemAck(std::move(peer), std::move(body),
            ctx);
        co_return;

    // ---- W3c-1/2: corps invite + formation (handlers_corps.cpp) -
    case MessageId::MW_CORPSASK_ACK:
        co_await OnCorpsAskAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_CORPSREPLY_ACK:
        co_await OnCorpsReplyAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_CORPSLEAVE_ACK:
        co_await OnCorpsLeaveAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_CHGCORPSCOMMANDER_ACK:
        co_await OnChgCorpsCommanderAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_PARTYMOVE_ACK:
        co_await OnPartyMoveAck(std::move(peer), std::move(body), ctx);
        co_return;

    // ---- W6-7: solo-instance party lifecycle (handlers_party.cpp)
    case MessageId::MW_ENTERSOLOMAP_ACK:
        co_await OnEnterSoloMapAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_LEAVESOLOMAP_ACK:
        co_await OnLeaveSoloMapAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_CORPSCMD_ACK:
        co_await OnCorpsCmdAck(std::move(peer), std::move(body), ctx);
        co_return;

    // ---- W3c-7: corps enemy-list family + HP (handlers_corps) ---
    case MessageId::MW_CORPSENEMYLIST_ACK:
        co_await OnCorpsEnemyListAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_MOVECORPSENEMY_ACK:
        co_await OnMoveCorpsEnemyAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_MOVECORPSUNIT_ACK:
        co_await OnMoveCorpsUnitAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_ADDCORPSENEMY_ACK:
        co_await OnAddCorpsEnemyAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_DELCORPSENEMY_ACK:
        co_await OnDelCorpsEnemyAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_CORPSHP_ACK:
        co_await OnCorpsHpAck(std::move(peer), std::move(body), ctx);
        co_return;

    // ---- W4-1: friend invite (handlers_friend.cpp) -------------
    case MessageId::MW_FRIENDASK_ACK:
        co_await OnFriendAskAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_FRIENDPROTECTEDASK_ACK:
        co_await OnFriendProtectedAskAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_PROTECTEDCHECK_ACK:
        co_await OnProtectedCheckAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_FRIENDREPLY_ACK:
        co_await OnFriendReplyAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_FRIENDERASE_ACK:
        co_await OnFriendEraseAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_FRIENDGROUPMAKE_ACK:
        co_await OnFriendGroupMakeAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_FRIENDGROUPDELETE_ACK:
        co_await OnFriendGroupDeleteAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_FRIENDGROUPCHANGE_ACK:
        co_await OnFriendGroupChangeAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_FRIENDGROUPNAME_ACK:
        co_await OnFriendGroupNameAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_FRIENDLIST_ACK:
        co_await OnFriendListAck(std::move(peer), std::move(body), ctx);
        co_return;

    // ---- W4-5: chat relay (handlers_chat.cpp) ------------------
    case MessageId::MW_CHAT_ACK:
        co_await OnChatAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_CHATBAN_ACK:
        co_await OnChatBanAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::CT_CHARMSG_ACK:
        co_await OnCharMsgAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::CT_USERPOSITION_ACK:
        co_await OnUserPositionAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::CT_USERMOVE_ACK:
        co_await OnUserMoveAck(std::move(peer), std::move(body), ctx);
        co_return;

    // ---- W6-13: connection-list reconcile (handlers_conn.cpp) --
    case MessageId::MW_CONLIST_ACK:
        co_await OnConListAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_MAPSVRLIST_ACK:
        co_await OnMapSvrListAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_CHECKMAIN_ACK:
        co_await OnCheckMainAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_RELEASEMAIN_ACK:
        co_await OnReleaseMainAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_BEGINTELEPORT_ACK:
        co_await OnBeginTeleportAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_CHECKCONNECT_ACK:
        co_await OnCheckConnectAck(std::move(peer), std::move(body), ctx);
        co_return;

    // ---- W6-20: connection-completion sub-flow (handlers_conn.cpp)
    case MessageId::MW_ROUTE_ACK:
        co_await OnRouteAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_ENTERCHAR_ACK:
        co_await OnEnterCharAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_CHARDATA_ACK:
        co_await OnCharDataAck(std::move(peer), std::move(body), ctx);
        co_return;

    // ---- W6-21: teleport confirm (handlers_conn.cpp) -----------
    case MessageId::MW_TELEPORT_ACK:
        co_await OnTeleportAck(std::move(peer), std::move(body), ctx);
        co_return;

    // ---- W6-24: Bow battleground (handlers_bow.cpp) ------------
    case MessageId::MW_ADDTOBOWQUEUE_REQ:
        co_await OnAddToBowQueueReq(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_CANCELBOWQUEUE_REQ:
        co_await OnCancelBowQueueReq(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_BOWPOINTSUPDATE_REQ:
        co_await OnBowPointsUpdateReq(std::move(peer), std::move(body), ctx);
        co_return;

    // ---- W6-25: Battle Royale (handlers_br.cpp) ----------------
    case MessageId::MW_ADDTOBRQUEUE_REQ:
        co_await OnAddToBrQueueReq(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_BRTEAMMATEADD_REQ:
        co_await OnBrTeamMateAddReq(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_BRTEAMMATEDEL_REQ:
        co_await OnBrTeamMateDelReq(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_BRTEAMMATEADDRESULT_ACK:
        co_await OnBrTeamMateAddResultAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_VOTEFORBRMAP_REQ:
        co_await OnVoteForBrMapReq(std::move(peer), std::move(body), ctx);
        co_return;

    // ---- W6-26: leave-battlefield (handlers_bow.cpp) -----------
    case MessageId::MW_LEAVEBATTLEFIELD_REQ:
        co_await OnLeaveBattlefieldReq(std::move(peer), std::move(body), ctx);
        co_return;

    // ---- W6-27: BattleMode status + CM teleport (handlers_bow.cpp) --
    case MessageId::MW_BATTLEMODESTATUS_REQ:
        co_await OnBattleModeStatusReq(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_CMTELEPORTBATTLEMODE_REQ:
        co_await OnCmTeleportBattleModeReq(std::move(peer), std::move(body), ctx);
        co_return;

    // ---- W6-28: arena join (handlers_party.cpp) -----------------
    case MessageId::MW_ARENAJOIN_ACK:
        co_await OnArenaJoinAck(std::move(peer), std::move(body), ctx);
        co_return;

    // ---- W6-29: RPS event (handlers_rps.cpp) -------------------
    case MessageId::MW_RPSGAME_ACK:
        co_await OnRpsGameAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::DM_RPSGAMERECORD_REQ:
        co_await OnRpsGameRecordReq(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::CT_RPSGAMEDATA_REQ:
        co_await OnRpsGameDataReq(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::CT_RPSGAMECHANGE_REQ:
        co_await OnRpsGameChangeReq(std::move(peer), std::move(body), ctx);
        co_return;

    // ---- W5-1: territory occupation (handlers_occupy.cpp) ------
    case MessageId::MW_CASTLEOCCUPY_ACK:
        co_await OnCastleOccupyAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_LOCALOCCUPY_ACK:
        co_await OnLocalOccupyAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_MISSIONOCCUPY_ACK:
        co_await OnMissionOccupyAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_CASTLEAPPLY_ACK:
        co_await OnCastleApplyAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::SM_BATTLESTATUS_REQ:
        co_await OnBattleStatusReq(std::move(peer), std::move(body), ctx);
        co_return;

    // ---- W6-1: timed-event broadcast (handlers_event.cpp) ------
    case MessageId::SM_CHANGEDAY_REQ:
        co_await OnChangeDayReq(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::SM_EVENTQUARTER_REQ:
        co_await OnEventQuarterReq(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::SM_EVENTQUARTERNOTIFY_REQ:
        co_await OnEventQuarterNotifyReq(std::move(peer), std::move(body), ctx);
        co_return;

    // ---- W6-2: combat / taming relays (handlers_combat.cpp) ----
    case MessageId::MW_MAGICMIRROR_ACK:
        co_await OnMagicMirrorAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_MONTEMPT_ACK:
        co_await OnMonTemptAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_MONTEMPTEVO_ACK:
        co_await OnMonTemptEvoAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_MONSTERDIE_ACK:
        co_await OnMonsterDieAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_TAKEMONMONEY_ACK:
        co_await OnTakeMonMoneyAck(std::move(peer), std::move(body), ctx);
        co_return;

    // ---- W6-10: item-result relays (handlers_item.cpp) ---------
    case MessageId::MW_ADDITEMRESULT_ACK:
        co_await OnAddItemResultAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_DEALITEMERROR_ACK:
        co_await OnDealItemErrorAck(std::move(peer), std::move(body), ctx);
        co_return;

    // ---- W6-3: global announcement broadcasts (handlers_rank.cpp)
    case MessageId::MW_FAMERANKUPDATE_ACK:
        co_await OnFameRankUpdateAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_HEROSELECT_ACK:
        co_await OnHeroSelectAck(std::move(peer), std::move(body), ctx);
        co_return;

    // ---- W6-4: recall-mon sync (handlers_recallmon.cpp) --------
    case MessageId::MW_CREATERECALLMON_ACK:
        co_await OnCreateRecallMonAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_RECALLMONDATA_ACK:
        co_await OnRecallMonDataAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_RECALLMONDEL_ACK:
        co_await OnRecallMonDelAck(std::move(peer), std::move(body), ctx);
        co_return;

    // ---- W6-5: companion-mon sync (handlers_recallmon.cpp) -----
    case MessageId::MW_CREATESPOLECNIKMON_ACK:
        co_await OnCreateSpolecnikMonAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_SPOLECNIKMONDEL_ACK:
        co_await OnSpolecnikMonDelAck(std::move(peer), std::move(body), ctx);
        co_return;

    // ---- W4-6: soulmate (handlers_soulmate.cpp) ----------------
    case MessageId::MW_SOULMATESEARCH_ACK:
        co_await OnSoulmateSearchAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_SOULMATEREG_ACK:
        co_await OnSoulmateRegAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_SOULMATEEND_ACK:
        co_await OnSoulmateEndAck(std::move(peer), std::move(body), ctx);
        co_return;

    // ---- W4-8: region update (handlers_char.cpp) ---------------
    case MessageId::MW_REGION_ACK:
        co_await OnRegionAck(std::move(peer), std::move(body), ctx);
        co_return;

    // ---- W4-9: level update (handlers_char.cpp) ----------------
    case MessageId::MW_LEVELUP_ACK:
        co_await OnLevelUpAck(std::move(peer), std::move(body), ctx);
        co_return;

    // ---- W4-10: inspect-player stat relay (handlers_char.cpp) --
    case MessageId::MW_CHARSTATINFO_ACK:
        co_await OnCharStatInfoAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_CHARSTATINFOANS_ACK:
        co_await OnCharStatInfoAnsAck(std::move(peer), std::move(body), ctx);
        co_return;

    // ---- W4-11: TMS conference channels (handlers_tms.cpp) -----
    case MessageId::MW_TMSSEND_ACK:
        co_await OnTmsSendAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_TMSINVITEASK_ACK:
        co_await OnTmsInviteAskAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_TMSINVITE_ACK:
        co_await OnTmsInviteAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_TMSOUT_ACK:
        co_await OnTmsOutAck(std::move(peer), std::move(body), ctx);
        co_return;

    // ---- W4-13: mail delivery relay (handlers_post.cpp) --------
    case MessageId::MW_POSTRECV_ACK:
        co_await OnPostRecvAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::DM_RESERVEDPOSTRECV_ACK:
        co_await OnReservedPostRecvAck(std::move(peer), std::move(body), ctx);
        co_return;

    // ---- W4-14: per-character visual state (handlers_char.cpp) -
    case MessageId::MW_PETRIDING_ACK:
        co_await OnPetRidingAck(std::move(peer), std::move(body), ctx);
        co_return;
    case MessageId::MW_HELMETHIDE_ACK:
        co_await OnHelmetHideAck(std::move(peer), std::move(body), ctx);
        co_return;

    // W4-15+ picks up login presence (connect fan-out) + the
    // friend/soulmate DB load. … see README §4.

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
