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

// --- W3a-23 — PvP record list ---------------------------------

// One per-member row carrying both the rolling weekrecord and a
// "last record" placeholder slot. The legacy sender at
// SSSender.cpp:3184 emits the last vRecord entry when its
// dwDate >= dwRecentRecordDate, otherwise zeros — we always
// emit zeros for the last slot until the per-day vRecord history
// ports (deferred to a later W3a-* batch).
struct GuildPvPRecordRow
{
    std::uint32_t char_id    = 0;
    std::uint16_t kill_count = 0;
    std::uint16_t die_count  = 0;
    // Wire emits the first 6 buckets (PVPE_KILL_H .. PVPE_WIN-1
    // — see TWorldSvr/SSSender.cpp:3204). Storage carries all 8
    // for parity with the W3a-21 audit-log; this struct uses 6
    // to match the wire exactly.
    std::array<std::uint32_t, 6> points{};
};

// --- W3a-31 — tactics wanted board --------------------------------

// MW_GUILDTACTICSWANTEDADD_REQ / DEL_REQ — 3-byte result replies
// (char_id, key, result). Mirror SSSender's tactics-wanted ack
// senders.
boost::asio::awaitable<void> SendMwGuildTacticsWantedAddReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 result);

boost::asio::awaitable<void> SendMwGuildTacticsWantedDelReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 result);

// MW_GUILDTACTICSWANTEDLIST_REQ — variable-length list filtered
// by country. Richer per-row payload than the guild wanted board
// (adds id + reward fields). The `already_applied` byte marks
// whether the requester applied to this posting — always 0 until
// the tactics-volunteer subsystem ports.
//
// Wire layout (SSSender.cpp:1406):
//   DWORD  dwCharID
//   DWORD  dwKey
//   DWORD  entry_count
//   × entry_count:
//     DWORD  dwID
//     DWORD  dwGuildID
//     STRING strName
//     STRING strTitle
//     STRING strText
//     BYTE   bDay
//     BYTE   bMinLevel
//     BYTE   bMaxLevel
//     DWORD  dwPoint
//     DWORD  dwGold
//     DWORD  dwSilver
//     DWORD  dwCooper
//     INT64  end_time_unix
//     BYTE   already_applied
struct GuildTacticsWantedRow
{
    std::uint32_t id              = 0;
    std::uint32_t guild_id        = 0;
    std::string   name;
    std::string   title;
    std::string   text;
    std::uint8_t  day             = 0;
    std::uint8_t  min_level       = 0;
    std::uint8_t  max_level       = 0;
    std::uint32_t point           = 0;
    std::uint32_t gold            = 0;
    std::uint32_t silver          = 0;
    std::uint32_t cooper          = 0;
    std::int64_t  end_time_unix   = 0;
    std::uint8_t  already_applied = 0;
};

boost::asio::awaitable<void> SendMwGuildTacticsWantedListReq(
    std::shared_ptr<PeerSession>                 peer,
    std::uint32_t                                char_id,
    std::uint32_t                                key,
    const std::vector<GuildTacticsWantedRow>&    entries);

// --- W3a-32 — tactics volunteer (applicant) flow ------------------

// MW_GUILDTACTICSVOLUNTEERING_REQ / VOLUNTEERINGDEL_REQ — 3-byte
// result replies for the apply / cancel branches.
boost::asio::awaitable<void> SendMwGuildTacticsVolunteeringReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 result);

boost::asio::awaitable<void> SendMwGuildTacticsVolunteeringDelReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 result);

// MW_GUILDTACTICSVOLUNTEERLIST_REQ — chief's applicant list.
//
// Wire layout (SSSender.cpp:1483):
//   DWORD  dwCharID
//   DWORD  dwKey
//   DWORD  applicant_count
//   × applicant_count:
//     DWORD  dwCharID
//     STRING strName
//     BYTE   bLevel
//     BYTE   bClass
//     DWORD  dwRegion
//     BYTE   bDay
//     DWORD  dwPoint
//     DWORD  dwGold
//     DWORD  dwSilver
//     DWORD  dwCooper
struct GuildTacticsVolunteerRow
{
    std::uint32_t char_id = 0;
    std::string   name;
    std::uint8_t  level   = 0;
    std::uint8_t  klass   = 0;
    std::uint32_t region  = 0;
    std::uint8_t  day     = 0;
    std::uint32_t point   = 0;
    std::uint32_t gold    = 0;
    std::uint32_t silver  = 0;
    std::uint32_t cooper  = 0;
};

boost::asio::awaitable<void> SendMwGuildTacticsVolunteerListReq(
    std::shared_ptr<PeerSession>                    peer,
    std::uint32_t                                   char_id,
    std::uint32_t                                   key,
    const std::vector<GuildTacticsVolunteerRow>&    applicants);

// MW_GUILDTACTICSREPLY_REQ — tactics-hire result. Sent to both
// the new tactics member's map peer and the chief's on a
// successful accept (each carries the same payload). On failure
// only the chief's peer gets it with the failure result byte.
//
// Wire layout (SSSender.cpp:1512):
//   DWORD  char_id
//   DWORD  key
//   BYTE   result
//   DWORD  member_id
//   DWORD  guild_id
//   STRING guild_name
//   STRING member_name
//   DWORD  gold
//   DWORD  silver
//   DWORD  cooper
boost::asio::awaitable<void> SendMwGuildTacticsReplyReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 result,
    std::uint32_t                member_id,
    std::uint32_t                guild_id,
    const std::string&           guild_name,
    const std::string&           member_name,
    std::uint32_t                gold,
    std::uint32_t                silver,
    std::uint32_t                cooper);

// MW_GUILDTACTICSINVITE_REQ — forwarded to a target's map peer
// when a chief invites them to become a tactics member. The
// target's client pops a "join as tactics member?" dialog.
//
// Wire layout (SSSender.cpp:1556):
//   DWORD char_id, key, STRING guild_name, inviter_name,
//   BYTE day, DWORD point, gold, silver, cooper
boost::asio::awaitable<void> SendMwGuildTacticsInviteReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    const std::string&           guild_name,
    const std::string&           inviter_name,
    std::uint8_t                 day,
    std::uint32_t                point,
    std::uint32_t                gold,
    std::uint32_t                silver,
    std::uint32_t                cooper);

// MW_GUILDTACTICSANSWER_REQ — invite outcome, sent to both the
// target's peer and the chief's peer.
//
// Wire layout (SSSender.cpp:1581):
//   DWORD char_id, key, BYTE result, DWORD guild_id,
//   STRING guild_name, DWORD member_id, STRING member_name,
//   DWORD gold, silver, cooper
boost::asio::awaitable<void> SendMwGuildTacticsAnswerReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 result,
    std::uint32_t                guild_id,
    const std::string&           guild_name,
    std::uint32_t                member_id,
    const std::string&           member_name,
    std::uint32_t                gold,
    std::uint32_t                silver,
    std::uint32_t                cooper);

// MW_GUILDTACTICSKICKOUT_REQ — result of a chief kicking a
// tactics member (or a member self-leaving).
//
// Wire layout (SSSender.cpp:1539):
//   DWORD char_id, key, BYTE result, DWORD target, BYTE kick
boost::asio::awaitable<void> SendMwGuildTacticsKickoutReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 result,
    std::uint32_t                target,
    std::uint8_t                 kick);

// MW_GUILDTACTICSLIST_REQ — the guild's hired tactics members.
//
// Wire layout (SSSender.cpp:1608):
//   DWORD char_id, key, DWORD member_count
//   × member_count:
//     DWORD  id
//     STRING name
//     BYTE   level
//     BYTE   klass
//     BYTE   day
//     DWORD  reward_point
//     INT64  reward_money
//     INT64  end_time
//     DWORD  gain_point
//     DWORD  region   (0 — TChar region not modelled)
//     WORD   castle   (0 — W5 castle war)
//     BYTE   camp     (0 — W5 castle war)
struct GuildTacticsMemberRow
{
    std::uint32_t id           = 0;
    std::string   name;
    std::uint8_t  level        = 0;
    std::uint8_t  klass        = 0;
    std::uint8_t  day          = 0;
    std::uint32_t reward_point = 0;
    std::int64_t  reward_money = 0;
    std::int64_t  end_time     = 0;
    std::uint32_t gain_point   = 0;
};

boost::asio::awaitable<void> SendMwGuildTacticsListReq(
    std::shared_ptr<PeerSession>                 peer,
    std::uint32_t                                char_id,
    std::uint32_t                                key,
    const std::vector<GuildTacticsMemberRow>&    members);

// MW_GAINPVPPOINT_REQ — relay forwarded to a char's main map
// peer when the owner of a PvP-point delta is a character
// (TOWNER_CHAR). The map server applies the per-char delta +
// shows the gain/loss toast. Guild-owned deltas (TOWNER_GUILD)
// don't relay — the world applies them to the guild bank
// directly (see OnGainPvPointAck).
//
// Wire layout (SSSender.cpp:3117):
//   DWORD owner_id
//   DWORD point
//   BYTE  event
//   BYTE  type
//   BYTE  gain
//   STRING name
//   BYTE  klass
//   BYTE  level
boost::asio::awaitable<void> SendMwGainPvPointReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                owner_id,
    std::uint32_t                point,
    std::uint8_t                 event,
    std::uint8_t                 type,
    std::uint8_t                 gain,
    const std::string&           name,
    std::uint8_t                 klass,
    std::uint8_t                 level);

// MW_GUILDPOINTLOG_REQ — reply to OnGuildPointLogAck. Returns
// the guild's rolling PvP point-reward audit log (TGuild.point_log
// vector populated by the W3a-14 OnGuildPointRewardReq fan-in
// + the W3a-27 in-memory mirror).
//
// Wire layout (SSSender.cpp:3140):
//   DWORD dwCharID
//   DWORD dwKey
//   WORD  entry_count
//     [entry_count times]
//       INT64  date_unix
//       STRING recipient_name
//       DWORD  point
struct GuildPointLogEntry
{
    std::int64_t  date_unix = 0;
    std::string   recipient_name;
    std::uint32_t point     = 0;
};
boost::asio::awaitable<void> SendMwGuildPointLogReq(
    std::shared_ptr<PeerSession>                  peer,
    std::uint32_t                                 char_id,
    std::uint32_t                                 key,
    const std::vector<GuildPointLogEntry>&        entries);

// MW_GUILDCABINETLIST_REQ — reply to OnGuildCabinetListAck. The
// legacy reply emits the guild's max_cabinet cap + every item
// in `m_mapTCabinet`. Our W3a-26 stub always emits count=0
// because the full item codec (TItem WrapItem — 18 scalar
// fields + variable-length magic array — see
// TWorldSvr/TWorldSvr.cpp:5498) hasn't ported yet. Clients
// receive a wire-compat "cabinet is empty" reply, which is
// truthful for our port (nothing else populates the cabinet).
//
// Wire layout (SSSender.cpp:1087):
//   DWORD dwCharID
//   DWORD dwKey
//   BYTE  max_cabinet
//   BYTE  item_count   (always 0 in the W3a-26 stub)
//   [item_count times: DWORD itemID + WrapItem]
boost::asio::awaitable<void> SendMwGuildCabinetListReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 max_cabinet);

// MW_GUILDPVPRECORD_REQ — reply to OnGuildPvPRecordAck. Returns
// every member's rolling weekly PvP outcome aggregate plus a
// per-member "last record" slot (currently always zeros — see
// note above).
//
// Wire layout (SSSender.cpp:3184):
//   DWORD dwCharID
//   DWORD dwKey
//   WORD  member_count
//     [member_count times]
//       DWORD member.dwID
//       WORD  weekrecord.wKillCount
//       WORD  weekrecord.wDieCount
//       DWORD weekrecord.points[6]   (PVPE_KILL_H..PVPE_WIN-1)
//       WORD  last.wKillCount   (or 0)
//       WORD  last.wDieCount    (or 0)
//       DWORD last.points[6]    (or 0)
boost::asio::awaitable<void> SendMwGuildPvPRecordReq(
    std::shared_ptr<PeerSession>               peer,
    std::uint32_t                              char_id,
    std::uint32_t                              key,
    const std::vector<GuildPvPRecordRow>&      members);

} // namespace tworldsvr::senders
