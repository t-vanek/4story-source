#pragma once

// Outbound packet builders — the W3a-2 batch covers the senders
// that the W3a-1 and W3a-2 handlers need to reply with. Modelled
// on Server/TControlSvrAsio/senders.h: stateless free functions
// that take a PeerSession and the wire fields, build the body
// through wire::WritePOD / WriteString, and co_await
// PeerSession::Wire()->SendPacket(wID, body).
//
// Each phase appends here:
//   W3a-2 — SendRwRelaysvrAck, SendMwGuildEstablishReq.
//   W3a-3 — guild info / member-list / disorg / leave / kickout /
//           duty / fame / contribution / point-reward / pvp /
//           cabinet / article senders (~25 total).
//   W3b   — party + corps senders.
//   W4    — friend / chat / soulmate senders.
//   …
//
// The legacy TWorldSvr/SSSender.cpp is 4046 LOC across 196 sender
// functions; the family-file split lives under senders/senders_*.cpp.

#include "../peer_session.h"

#include <boost/asio/awaitable.hpp>

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace tworldsvr::senders {

// --- RW family ------------------------------------------------------

// RW_RELAYSVR_ACK — reply to RW_RELAYSVR_REQ. Tells the registering
// peer the cluster's nation flag, the current operator list, and
// the per-server motd table. The latter two are read-only snapshots
// the peer caches for the lifetime of its connection.
//
// Wire layout (RWSender.cpp:1):
//   BYTE bNation
//   WORD operator_count
//   DWORD operators[operator_count]
//   WORD msg_count
//   { STRING key, STRING value } × msg_count
boost::asio::awaitable<void> SendRwRelaysvrAck(
    std::shared_ptr<PeerSession>      peer,
    std::uint8_t                       nation,
    const std::vector<std::uint32_t>&  operators,
    const std::map<std::string, std::string>& svr_msgs);

// --- MW family (guild) ----------------------------------------------

// MW_GUILDESTABLISH_REQ — sent back to the originating map server
// after world ingests a DM_GUILDLOAD_ACK (or after a real guild-
// create flow lands in W3a-3). The map server forwards the result
// down to the client.
//
// Wire layout (SSSender.cpp:843):
//   DWORD dwCharID
//   DWORD dwKey
//   BYTE  bRet              -- GUILD_SUCCESS / failure codes
//   DWORD dwGuildID
//   STRING strName
//   BYTE  bEstablish        -- 1 = freshly created, 0 = re-confirm
boost::asio::awaitable<void> SendMwGuildEstablishReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 result,
    std::uint32_t                guild_id,
    const std::string&           name,
    std::uint8_t                 establish);

// Result codes for SendMwGuildEstablishReq.bRet and the rest of
// the MW_GUILD* family live in services/guild_constants.h —
// include that instead. The pre-W3a-4b versions of this header
// carried ad-hoc constants (kGuildSuccess, kGuildLeaveSelf) that
// silently mismatched the legacy enum values; the shared header
// is the single source of truth now.

// --- RW family — W3a-3 batch ---------------------------------------

// RW_ENTERCHAR_ACK — reply to RW_ENTERCHAR_REQ. The relay server
// asked "is this char online and where?"; we answer with the
// char's current cluster-wide state. For W3a-3 most cluster
// state (guild, party, corps, tactics) is still zero-default;
// W3a-4 will populate those once the matching registries land.
//
// Wire layout (RWSender.cpp::SendRW_ENTERCHAR_ACK):
//   DWORD dwCharID
//   STRING strName
//   BYTE   bResult           -- TRUE if char found + key matched
//   BYTE   bCountry
//   BYTE   bAidCountry
//   DWORD  dwGuildID
//   DWORD  dwGuildChief
//   BYTE   bDuty
//   WORD   wPartyID
//   DWORD  dwPartyChiefID
//   WORD   wCorpsID
//   DWORD  dwGeneralID
//   DWORD  dwTacticsID
//   DWORD  dwTacticsChief
//   WORD   wMapID
//   WORD   wUnitID           -- MAKEWORD(BYTE(posX/UNIT), BYTE(posZ/UNIT))
boost::asio::awaitable<void> SendRwEntercharAck(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    const std::string&           name,
    std::uint8_t                 result,
    std::uint8_t                 country,
    std::uint8_t                 aid_country,
    std::uint32_t                guild_id,
    std::uint32_t                guild_chief,
    std::uint8_t                 duty,
    std::uint16_t                party_id,
    std::uint32_t                party_chief_id,
    std::uint16_t                corps_id,
    std::uint32_t                general_id,
    std::uint32_t                tactics_id,
    std::uint32_t                tactics_chief,
    std::uint16_t                map_id,
    std::uint16_t                unit_id);

// --- MW family — W3a-3 batch ---------------------------------------

// MW_RELAYCONNECT_REQ — sent to a map server to tell it to relay
// the named char's data through the relay server. The bRelayOn
// byte is 0 in the broadcast path (OnRelaysvrReq's fan-out) and
// 1 in the per-char path (OnRW_RELAYCONNECT_REQ → main map).
//
// Wire layout (SSSender.cpp:3062):
//   DWORD dwCharID
//   BYTE  bRelayOn
boost::asio::awaitable<void> SendMwRelayconnectReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint8_t                 relay_on);

// MW_GUILDLEAVE_REQ — sent back to the originating map server
// after world removes a member from a guild. The map server
// forwards the confirmation down to the client + broadcasts the
// member-offline event to other guild members.
//
// Wire layout (SSHandler.cpp:3600 SendMW_GUILDLEAVE_REQ):
//   DWORD  dwCharID
//   DWORD  dwKey
//   STRING strName     -- the leaving char's name
//   BYTE   bLeave      -- reason (GUILD_LEAVE_SELF / KICKOUT / DISORG)
//   DWORD  dwTime      -- m_timeCurrent (Unix sec) when the leave landed
boost::asio::awaitable<void> SendMwGuildLeaveReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    const std::string&           name,
    std::uint8_t                 leave_reason,
    std::uint32_t                time_unix);

// MW_GUILDDISORGANIZATION_REQ — sent back to the requesting map
// after the in-memory disorg flag has been flipped + persisted.
//
// Wire layout (SSSender.cpp:863):
//   DWORD dwCharID
//   DWORD dwKey
//   BYTE  bDisorg     -- 1 = disbanding, 0 = cancelled
boost::asio::awaitable<void> SendMwGuildDisorganizationReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 disorg);

// MW_GUILDDUTY_REQ — sent back after world records a member's
// duty change. Broadcast to the requesting peer AND to the
// target member's own map peer if they're online elsewhere.
//
// Wire layout (SSSender.cpp:938):
//   DWORD  dwCharID
//   DWORD  dwKey
//   STRING strTarget  -- the renamed member's name (legacy ships
//                        the target string even though dwCharID
//                        carries the id — clients rely on it)
//   BYTE   bDuty      -- new duty (GUILD_DUTY_*)
boost::asio::awaitable<void> SendMwGuildDutyReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    const std::string&           target_name,
    std::uint8_t                 duty);

// MW_GUILDFAME_REQ — fame change reply. Broadcast to every
// online guild member's main map peer. The bResult byte tells
// the client whether the change took (GUILD_SUCCESS) or the
// guild ran out of PvP points (GUILD_NOPOINT).
//
// Wire layout (SSSender.cpp:1223):
//   DWORD dwCharID    -- the recipient (varies per broadcast)
//   DWORD dwKey
//   BYTE  bResult     -- GUILD_SUCCESS / GUILD_NOPOINT
//   DWORD dwID        -- the requester's char_id (constant across
//                        the broadcast — lets clients identify
//                        who triggered the change)
//   DWORD dwFame
//   DWORD dwFameColor
boost::asio::awaitable<void> SendMwGuildFameReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 result,
    std::uint32_t                originator_char_id,
    std::uint32_t                fame,
    std::uint32_t                fame_color);

// --- W3a-11 guild wanted board ------------------------------------

// MW_GUILDWANTEDADD_REQ — 3-byte result reply after a chief
// posts a wanted entry. Mirrors SSSender.cpp:1242.
boost::asio::awaitable<void> SendMwGuildWantedAddReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 result);

// MW_GUILDWANTEDDEL_REQ — 3-byte result reply for the delete
// branch. Mirrors SSSender.cpp:1256.
boost::asio::awaitable<void> SendMwGuildWantedDelReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 result);

// MW_GUILDWANTEDLIST_REQ — variable-length list of wanted
// entries filtered by country. The `applied` byte per row marks
// whether the requesting char has an applicant record for that
// guild — always false in W3a-11 (volunteer/applicant subsystem
// arrives in W3a-12+).
//
// Wire layout (SSSender.cpp:1269 — DWORD count + 8 fields per row):
//   DWORD  dwCharID
//   DWORD  dwKey
//   DWORD  entry_count
//   × entry_count:
//     DWORD  dwGuildID
//     STRING strName
//     STRING strTitle
//     STRING strText
//     BYTE   bMinLevel
//     BYTE   bMaxLevel
//     INT64  end_time_unix
//     BYTE   already_applied
struct GuildWantedRow
{
    std::uint32_t guild_id        = 0;
    std::string   name;
    std::string   title;
    std::string   text;
    std::uint8_t  min_level       = 0;
    std::uint8_t  max_level       = 0;
    std::int64_t  end_time_unix   = 0;
    std::uint8_t  already_applied = 0;
};

boost::asio::awaitable<void> SendMwGuildWantedListReq(
    std::shared_ptr<PeerSession>         peer,
    std::uint32_t                        char_id,
    std::uint32_t                        key,
    const std::vector<GuildWantedRow>&   entries);

// --- W3a-12 volunteer / applicant flow ----------------------------

// MW_GUILDVOLUNTEERING_REQ — player applied to a wanted entry.
// 3-byte result reply.
boost::asio::awaitable<void> SendMwGuildVolunteeringReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 result);

// MW_GUILDVOLUNTEERINGDEL_REQ — player canceled their app.
// 3-byte result reply.
boost::asio::awaitable<void> SendMwGuildVolunteeringDelReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 result);

// MW_GUILDVOLUNTEERLIST_REQ — chief browses applicants for their
// guild's wanted entry. Variable-length tail (DWORD count + 5
// fields per applicant).
struct GuildVolunteerRow
{
    std::uint32_t char_id = 0;
    std::string   name;
    std::uint8_t  level   = 0;
    std::uint8_t  klass   = 0;
    std::uint32_t region  = 0;
};

boost::asio::awaitable<void> SendMwGuildVolunteerListReq(
    std::shared_ptr<PeerSession>             peer,
    std::uint32_t                            char_id,
    std::uint32_t                            key,
    const std::vector<GuildVolunteerRow>&    applicants);

// MW_GUILDVOLUNTEERREPLY_REQ — sent to the chief on failed
// accept (e.g. applicant joined another guild meanwhile). Legacy
// only fires this on errors; the success path uses the standard
// MW_GUILDJOIN_REQ broadcast from the invite flow.
boost::asio::awaitable<void> SendMwGuildVolunteerReplyReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 result);

// --- W3a-9 single guild info refresh ------------------------------

// MW_GUILDINFO_REQ — large composite reply with the requester's
// guild summary. Sent when a client opens the guild info pane.
// Fields are mostly TGuild meta + the 2 vice-chief names
// (NAME_NULL-padded to always emit 2 strings) + the requester's
// own duty/peer.
//
// Wire layout (SSSender.cpp:1015 — densest sender in the family).
struct GuildInfoPayload
{
    std::uint32_t guild_id            = 0;
    std::string   name;
    std::int64_t  establish_time      = 0;
    std::uint16_t member_count        = 0;
    std::uint16_t max_member          = 0;
    std::string   chief_name;
    std::uint8_t  chief_peer          = 0;
    std::array<std::string, 2> vice_chief_names;   // empty → NAME_NULL
    std::uint8_t  level               = 0;
    std::uint32_t fame                = 0;
    std::uint32_t fame_color          = 0;
    std::uint32_t gi                  = 0;
    std::uint32_t exp                 = 0;
    std::uint32_t level_exp           = 0;
    std::uint8_t  guild_points        = 0;
    std::uint8_t  status              = 0;
    std::uint32_t gold                = 0;
    std::uint32_t silver              = 0;
    std::uint32_t cooper              = 0;
    std::uint8_t  requester_duty      = 0;
    std::uint8_t  requester_peer      = 0;
    std::string   article_title;
    std::uint32_t pvp_total_point     = 0;
    std::uint32_t pvp_useable_point   = 0;
    std::uint32_t pvp_month_point     = 0;
    std::uint32_t rank_total          = 0;
    std::uint32_t rank_month          = 0;
    std::uint8_t  stat_level          = 0;
    std::uint8_t  stat_point          = 0;
    std::uint32_t stat_exp            = 0;
};

boost::asio::awaitable<void> SendMwGuildInfoReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 result,
    const GuildInfoPayload*      payload);  // nullptr on error branch

// --- W3a-8 article board ------------------------------------------

// MW_GUILDARTICLELIST_REQ — variable-length reply with the guild
// articles roster. Sent on explicit OnGuildArticleListAck and on
// every successful add / del / update (legacy ADD/DEL/UPDATE
// handlers all chase the ACK with a LIST refresh so the chief's
// UI re-renders).
//
// Wire layout (SSSender.cpp:1134):
//   DWORD dwCharID
//   DWORD dwKEY
//   BYTE  article_count          -- 0..MAX_GUILD_ARTICLE_COUNT
//   × article_count:
//     DWORD  dwID
//     BYTE   bDuty                -- writer's duty at post time
//     STRING strWriter
//     STRING strTitle
//     STRING strArticle
//     STRING strDate              -- "YYYY-MM-DD" formatted
struct GuildArticleRow
{
    std::uint32_t id    = 0;
    std::uint8_t  duty  = 0;
    std::string   writer;
    std::string   title;
    std::string   body;
    std::string   date;        // pre-formatted yyyy-mm-dd
};

boost::asio::awaitable<void> SendMwGuildArticleListReq(
    std::shared_ptr<PeerSession>           peer,
    std::uint32_t                          char_id,
    std::uint32_t                          key,
    const std::vector<GuildArticleRow>&    articles);

// MW_GUILDARTICLEADD_REQ — 3-byte ack with kSuccess / kFail.
boost::asio::awaitable<void> SendMwGuildArticleAddReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 result);

// MW_GUILDARTICLEDEL_REQ — same shape.
boost::asio::awaitable<void> SendMwGuildArticleDelReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 result);

// MW_GUILDARTICLEUPDATE_REQ — same shape.
boost::asio::awaitable<void> SendMwGuildArticleUpdateReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 result);

// MW_GUILDMEMBERLIST_REQ — variable-length reply with the full
// guild member roster. Used by MEMBERLIST refresh (client opens
// the guild window) and by future broadcast paths that need to
// re-sync member state cluster-wide.
//
// Wire layout (SSSender.cpp:972) — the only sender with a
// variable-length tail:
//
//   DWORD  dwCharID
//   DWORD  dwKEY
//   BYTE   bRet         -- kSuccess / kNotFound
//   [if bRet == kSuccess:]
//     DWORD dwGuildID
//     STRING strName    -- guild name
//     WORD  member_count
//     × member_count:
//       DWORD dwID
//       STRING strName  -- member name
//       BYTE  bLevel
//       BYTE  bClass
//       BYTE  bDuty
//       BYTE  bPeer
//       BYTE  online_bool
//       DWORD dwRegion  -- 0 when offline
//       WORD  wCastle
//       BYTE  bCamp
//       DWORD dwTactics
//       BYTE  bWarCountry
//       INT64 dlConnectedDate
//
// The `online_bool` byte is computed by the world side based on
// CharRegistry::Find(member.char_id) → non-null. dwRegion is
// pulled from the TChar (zero when the member is offline).
struct GuildMemberListRow
{
    std::uint32_t char_id      = 0;
    std::string   name;
    std::uint8_t  level        = 0;
    std::uint8_t  klass        = 0;
    std::uint8_t  duty         = 0;
    std::uint8_t  peer         = 0;
    std::uint8_t  online       = 0;
    std::uint32_t region       = 0;
    std::uint16_t castle       = 0;
    std::uint8_t  camp         = 0;
    std::uint32_t tactics      = 0;
    std::uint8_t  war_country  = 0;
    std::int64_t  connected_date_unix = 0;
};

boost::asio::awaitable<void> SendMwGuildMemberListReq(
    std::shared_ptr<PeerSession>           peer,
    std::uint32_t                          char_id,
    std::uint32_t                          key,
    std::uint8_t                           result,
    std::uint32_t                          guild_id,
    const std::string&                     guild_name,
    const std::vector<GuildMemberListRow>& members);

// MW_GUILDINVITE_REQ — forwarded to the target's main map peer
// when a chief invites them by name. The target's client pops a
// "join guild?" dialog with the inviter's name.
//
// Wire layout (SSSender.cpp:876):
//   DWORD  target_char_id
//   DWORD  target_key
//   STRING guild_name
//   DWORD  inviter_char_id
//   STRING inviter_name
boost::asio::awaitable<void> SendMwGuildInviteReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                target_char_id,
    std::uint32_t                target_key,
    const std::string&           guild_name,
    std::uint32_t                inviter_char_id,
    const std::string&           inviter_name);

// MW_GUILDJOIN_REQ — answer to the invite flow. Sent to both
// inviter (chief) and invited (target) so each side's client
// gets the result (success / NOTFOUND / MEMBER_FULL / HAVEGUILD
// / FAIL). Result-only branches pass zeros for the guild meta.
//
// Wire layout (SSSender.cpp:894 — 10 fields, the longest sender
// in the guild family):
//   DWORD  dwCharID
//   DWORD  dwKey
//   BYTE   bRet            -- guild::kSuccess / kMemberFull /
//                             kHaveGuild / kNotFound / kFail
//   DWORD  dwGuildID
//   DWORD  dwFame
//   DWORD  dwFameColor
//   STRING strGuildName
//   DWORD  dwMemberID      -- the new member's char_id (success)
//                             or 0 (error)
//   STRING strMemberName   -- the new member's name or empty
//   BYTE   bMaxGuildMember -- legacy default 0; populated on
//                             MEMBER_FULL so the client knows
//                             the cap
boost::asio::awaitable<void> SendMwGuildJoinReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 result,
    std::uint32_t                guild_id,
    std::uint32_t                fame,
    std::uint32_t                fame_color,
    const std::string&           guild_name,
    std::uint32_t                member_id,
    const std::string&           member_name,
    std::uint8_t                 max_member);

// MW_GUILDPEER_REQ — reply after a peerage change (or rejection
// via CheckPeerage). Broadcast-shaped: both the requesting chief
// and the target's main map peer receive it.
//
// Wire layout (SSSender.cpp:953):
//   DWORD  dwCharID    -- recipient
//   DWORD  dwKEY
//   BYTE   bResult     -- GUILD_SUCCESS / GUILD_FAIL
//   STRING strTarget   -- the renamed member's name
//   BYTE   bPeer       -- new rank
//   BYTE   bOldPeer    -- rank before the change (legacy clients
//                         use this for the chat-log line)
boost::asio::awaitable<void> SendMwGuildPeerReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 result,
    const std::string&           target_name,
    std::uint8_t                 new_peer,
    std::uint8_t                 old_peer);

// MW_GUILDCABINETMAX_REQ — reply after the chief expands the
// guild's cabinet slot count via the cash-shop flow on the map
// side. Pure ACK with the new size.
//
// Wire layout: legacy uses an in-line broadcast (no dedicated
// SendMW_GUILDCABINETMAX_REQ — the map side reads bMaxCabinet
// off the guild's next refresh). We expose the symmetric ACK
// here so the matching handler can fire it; layout matches the
// W3a-5 client-facing protocol:
//   DWORD dwCharID    -- the requester
//   DWORD dwKey
//   BYTE  bMaxCabinet -- new cap
boost::asio::awaitable<void> SendMwGuildCabinetMaxReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 max_cabinet);

// MW_GUILDCONTRIBUTION_REQ — reply after a member contributes
// gold/silver/cooper/exp/pvp to the guild. The bResult byte
// surfaces failure modes (guild full level + 0 exp, member not
// found, etc.).
//
// Wire layout (SSSender.cpp:1111):
//   DWORD dwCharID
//   DWORD dwKey
//   BYTE  bResult
//   DWORD dwExp
//   DWORD dwGold
//   DWORD dwSilver
//   DWORD dwCooper
//   DWORD dwPvPoint
boost::asio::awaitable<void> SendMwGuildContributionReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 result,
    std::uint32_t                exp,
    std::uint32_t                gold,
    std::uint32_t                silver,
    std::uint32_t                cooper,
    std::uint32_t                pvp_point);

// Reason codes for SendMwGuildLeaveReq.bLeave (kLeaveSelf /
// kLeaveKick / kLeaveDisorganization) live in
// services/guild_constants.h alongside the rest of GUILD_*.

} // namespace tworldsvr::senders
