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
#include "../services/guild_registry.h"

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

// MW_LEVELUP_REQ — propagate a char's new level to another map
// server it is visible on (the char's non-main connections).
//
// Wire layout (SSSender.cpp:795):
//   DWORD char_id, DWORD key, BYTE level
boost::asio::awaitable<void> SendMwLevelUpReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 level);

// --- W4-22 fresh-login ENTERSVR completion (senders_relay.cpp) ----

// MW_CHARINFO_REQ — the fat composite the main map ships to the
// client right after a fresh login completes (legacy
// SendMW_CHARINFO_REQ, SSSender.cpp:523). Carries the char's
// guild / tactics / party identity + duty + peer + castle / camp
// + title / rank-point + a BR/Bow "battleground" flag.
//
// Wire layout: char_id, key, guild_id, guild_country, guild_name,
//   fame, fame_color, tactics_id, tactics_name, duty, peer,
//   castle, camp, party_id, party_obtain_type, party_chief_id,
//   title_id, rank_point, bow_release (BYTE).
// Empty guild / tactics emit (0, TCONTRY_N, "") / (0, "") — the
// legacy NAME_NULL sentinel is just an empty string.
struct CharInfoPayload
{
    std::uint32_t guild_id          = 0;
    std::uint8_t  guild_country     = 2;   // TCONTRY_N
    std::string   guild_name;              // "" when guildless
    std::uint32_t fame              = 0;
    std::uint32_t fame_color        = 0;
    std::uint32_t tactics_id        = 0;
    std::string   tactics_name;            // "" when not in tactics
    std::uint8_t  duty              = 0;
    std::uint8_t  peer              = 0;
    std::uint16_t castle            = 0;
    std::uint8_t  camp              = 0;
    std::uint16_t party_id          = 0;
    std::uint8_t  party_obtain_type = 0;
    std::uint32_t party_chief_id    = 0;
    std::uint16_t title_id          = 0;
    std::uint32_t rank_point        = 0;
    std::uint8_t  bow_release       = 0;   // 1 when arriving back from BR/Bow
};

boost::asio::awaitable<void> SendMwCharInfoReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    const CharInfoPayload&       payload);

// MW_ROUTE_REQ — kick off the route-resolution leg of the legacy
// connection handshake (legacy SendMW_ROUTE_REQ, SSSender.cpp:638).
// Sent on a fresh login so the main can resolve the additional map
// servers the char needs (it answers MW_ROUTE_ACK, which W6-20
// processes).
//   Wire: char_id, key, channel, map_id, pos_x, pos_y, pos_z
boost::asio::awaitable<void> SendMwRouteReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 channel,
    std::uint16_t                map_id,
    float                        pos_x,
    float                        pos_y,
    float                        pos_z);

// MW_PETRIDING_REQ — propagate a char's active mount to another map
// session it is visible on (the char's non-originating connections).
//   Wire (SSSender.cpp): DWORD char_id, key, DWORD riding
boost::asio::awaitable<void> SendMwPetRidingReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint32_t                riding);

// MW_HELMETHIDE_REQ — confirm a char's helmet-visibility toggle back
// to the originating map (which then renders + broadcasts locally).
//   Wire (SSSender.cpp): DWORD char_id, key, BYTE hide
boost::asio::awaitable<void> SendMwHelmetHideReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 hide);

// MW_CHARSTATINFOANS_REQ — ask a target's map to gather the
// inspected char's stat block (step 1 of the inspect-player relay).
//
// Wire layout (SSSender.cpp:1917):
//   DWORD req_char_id, DWORD char_id
boost::asio::awaitable<void> SendMwCharStatInfoAnsReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                req_char_id,
    std::uint32_t                char_id);

// MW_CHARSTATINFO_REQ — relay the gathered stat block back to the
// requester's map (step 2). The body is forwarded verbatim (the
// inbound CHARSTATINFOANS_ACK payload, leading with req_char_id) —
// world doesn't interpret the stats.
boost::asio::awaitable<void> SendMwCharStatInfoReq(
    std::shared_ptr<PeerSession>   peer,
    const std::vector<std::byte>&  body);

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

// MW_GUILDPOINTREWARD_REQ — reply to a chief's point-reward
// grant (OnGuildPointRewardAck). The bResult byte is a GPR_*
// code (services/guild_constants.h).
//
// Wire layout (SSSender.cpp:3161):
//   BYTE result, DWORD char_id, key, remain_point, point,
//   target_id, STRING target_name, message
boost::asio::awaitable<void> SendMwGuildPointRewardReq(
    std::shared_ptr<PeerSession> peer,
    std::uint8_t                 result,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint32_t                remain_point,
    std::uint32_t                point,
    std::uint32_t                target_id,
    const std::string&           target_name,
    const std::string&           message);

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

// MW_GUILDCABINETLIST_REQ — reply to OnGuildCabinetListAck.
// Emits the guild's max_cabinet cap + every cabinet item
// (W3a-37: real items via the WrapItem codec; W3a-26 shipped a
// count=0 stub).
//
// Wire layout (SSSender.cpp:1087):
//   DWORD dwCharID
//   DWORD dwKey
//   BYTE  max_cabinet
//   BYTE  item_count
//   × item_count: DWORD slot_id + WrapItem(item)
boost::asio::awaitable<void> SendMwGuildCabinetListReq(
    std::shared_ptr<PeerSession>               peer,
    std::uint32_t                              char_id,
    std::uint32_t                              key,
    std::uint8_t                               max_cabinet,
    const std::vector<TGuildCabinetItem>&      items);

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

// --- W3b-1 party invite relay -------------------------------------

// MW_ENTERSOLOMAP_REQ — mirror a char's solo-instance party state to
// each of its map connections.
//   Wire: DWORD char_id, key, WORD party_id, BYTE party_type,
//     DWORD chief_id
boost::asio::awaitable<void> SendMwEnterSoloMapReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint16_t                party_id,
    std::uint8_t                 party_type,
    std::uint32_t                chief_id);

// MW_PARTYADD_REQ — the party-invite result/dialog packet. On a
// failure result it lands on the requester's map (their client
// shows the toast); on PARTY_AGREE it lands on the target's map
// (their client pops the "join party?" dialog, keyed by
// request_char_id). The dwRequest field is the inviter's char_id —
// 0 on the failure branches, the inviter on AGREE.
//
// Wire layout (SSSender.cpp:694):
//   DWORD  char_id          -- recipient
//   DWORD  key              -- recipient's session key
//   STRING request_name     -- inviter's name
//   STRING target_name      -- invitee's name
//   BYTE   obtain_type      -- proposed loot mode (PT_*)
//   BYTE   result           -- PARTY_* (party::k*)
//   DWORD  request_char_id   -- inviter id (AGREE) / 0 (failure)
boost::asio::awaitable<void> SendMwPartyAddReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    const std::string&           request_name,
    const std::string&           target_name,
    std::uint8_t                 obtain_type,
    std::uint8_t                 result,
    std::uint32_t                request_char_id);

// --- W3b-2 party formation ----------------------------------------

// Describe-fields for the member being announced in a
// MW_PARTYJOIN_REQ. The legacy sender pulls them straight off the
// TChar (pNew->m_*); we snapshot them into this POD so the sender
// stays a stateless field-writer.
struct PartyMemberInfo
{
    std::uint32_t char_id = 0;
    std::string   name;
    std::string   guild_name;        // NAME_NULL ("") when guildless
    std::uint8_t  level   = 0;
    std::uint32_t max_hp  = 0;
    std::uint32_t hp      = 0;
    std::uint32_t max_mp  = 0;
    std::uint32_t mp      = 0;
    std::uint8_t  race    = 0;
    std::uint8_t  sex     = 0;
    std::uint8_t  face    = 0;
    std::uint8_t  hair    = 0;
    std::uint8_t  klass   = 0;
};

// MW_PARTYJOIN_REQ — sent to every party member (and the joiner)
// when a char joins, announcing one member to one recipient. The
// JoinParty fan-out fires it pairwise: each existing member learns
// about the joiner and the joiner learns about each member.
//
// Wire layout (SSSender.cpp:716):
//   DWORD  recipient_char_id
//   DWORD  recipient_key
//   WORD   party_id
//   STRING member.name
//   DWORD  member.char_id
//   DWORD  chief_id
//   WORD   commander_id          -- corps commander (0, no corps)
//   STRING member.guild_name
//   BYTE   member.level
//   DWORD  member.max_hp
//   DWORD  member.hp
//   DWORD  member.max_mp
//   DWORD  member.mp
//   BYTE   member.race
//   BYTE   member.sex
//   BYTE   member.face
//   BYTE   member.hair
//   BYTE   obtain_type
//   BYTE   member.klass
boost::asio::awaitable<void> SendMwPartyJoinReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                recipient_char_id,
    std::uint32_t                recipient_key,
    std::uint16_t                party_id,
    std::uint32_t                chief_id,
    std::uint16_t                commander_id,
    std::uint8_t                 obtain_type,
    const PartyMemberInfo&       member);

// MW_PARTYATTR_REQ — pushed to a char after their party membership
// changes so their client re-renders the party HUD (id / loot mode
// / chief / corps commander). Sent with party_id=0 (and zeroed
// meta) when the char leaves a party (W3b-3).
//
// Wire layout (SSSender.cpp:1856):
//   DWORD char_id, DWORD key, WORD party_id, BYTE party_type,
//   DWORD chief_id, WORD commander_id
boost::asio::awaitable<void> SendMwPartyAttrReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint16_t                party_id,
    std::uint8_t                 party_type,
    std::uint32_t                chief_id,
    std::uint16_t                commander_id);

// --- W3b-3 party leave / kick -------------------------------------

// MW_PARTYDEL_REQ — sent to every party member when one leaves (or
// is kicked). The leaver receives it with chief_id = 0 and
// party_id = 0 (telling their client they're now partyless); the
// remaining members receive the surviving chief + party id so
// their roster re-renders. On a disband (party drops below two)
// every member sees chief_id/party_id = 0.
//
// Wire layout (SSSender.cpp:749):
//   DWORD recipient_char_id
//   DWORD recipient_key
//   DWORD leaver_char_id      -- who left / was kicked
//   DWORD chief_id            -- surviving chief (0 for the leaver
//                                / on disband)
//   WORD  commander_id        -- corps commander (0, no corps)
//   WORD  party_id            -- 0 when chief_id is 0
//   BYTE  kick                -- 1 = kicked, 0 = voluntary leave
boost::asio::awaitable<void> SendMwPartyDelReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                recipient_char_id,
    std::uint32_t                recipient_key,
    std::uint32_t                leaver_char_id,
    std::uint32_t                chief_id,
    std::uint16_t                commander_id,
    std::uint16_t                party_id,
    std::uint8_t                 kick);

// --- W3b-4 party attribute changes --------------------------------

// MW_PARTYMANSTAT_REQ — one party member's combat stats / level
// changed; broadcast to every member so their roster HUD updates.
// `member_char_id` is the subject; the recipient is each member in
// turn.
//
// Wire layout (SSSender.cpp:770):
//   DWORD recipient_char_id, DWORD recipient_key,
//   DWORD member_char_id, BYTE type, BYTE level,
//   DWORD max_hp, DWORD hp, DWORD max_mp, DWORD mp
boost::asio::awaitable<void> SendMwPartyManstatReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                recipient_char_id,
    std::uint32_t                recipient_key,
    std::uint32_t                member_char_id,
    std::uint8_t                 type,
    std::uint8_t                 level,
    std::uint32_t                max_hp,
    std::uint32_t                hp,
    std::uint32_t                max_mp,
    std::uint32_t                mp);

// MW_CHGPARTYCHIEF_REQ — result of a chief handing leadership to
// another member. On success (PARTY_CHGCHIEF) only the requesting
// chief gets this reply; the roster-wide refresh rides on the
// MW_PARTYATTR_REQ broadcast. Failure branches (NOUSER / NOPARTY /
// NOTCHIEF / ALREADY) also reply to the requester.
//
// Wire layout (SSSender.cpp:1844):
//   DWORD char_id, DWORD key, BYTE result
boost::asio::awaitable<void> SendMwChgPartyChiefReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 result);

// MW_CHGPARTYTYPE_REQ — result of a chief changing the loot mode.
// On success broadcast to every member with result=0; the
// not-chief failure replies only to the requester.
//
// Wire layout (SSSender.cpp:678):
//   DWORD char_id, DWORD key, BYTE result, BYTE party_type
boost::asio::awaitable<void> SendMwChgPartyTypeReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 result,
    std::uint8_t                 party_type);

// --- W3b-5 party member recall ------------------------------------

// MW_PARTYMEMBERRECALLANS_REQ — forwarded to the other party (the
// recall target on a summon, or the destination member on a
// move-to) so their client pops the recall confirmation dialog.
//
// Wire layout (SSSender.cpp:2831):
//   DWORD char_id, DWORD key, STRING other_name, BYTE type,
//   BYTE inven_id, BYTE item_id
boost::asio::awaitable<void> SendMwPartyMemberRecallAnsReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    const std::string&           other_name,
    std::uint8_t                 type,
    std::uint8_t                 inven_id,
    std::uint8_t                 item_id);

// MW_PARTYMEMBERRECALL_REQ — the recall outcome relayed back to the
// char being teleported: a failure result (IU_TARGETBUSY) with the
// trailing destination fields zeroed, or the granted destination
// (channel + map + position) from the answer.
//
// Wire layout (SSSender.cpp:2799):
//   DWORD char_id, DWORD key, BYTE result, STRING target_name,
//   BYTE type, BYTE inven_id, BYTE item_id, BYTE channel,
//   WORD map_id, FLOAT pos_x, FLOAT pos_y, FLOAT pos_z
boost::asio::awaitable<void> SendMwPartyMemberRecallReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 result,
    const std::string&           target_name,
    std::uint8_t                 type,
    std::uint8_t                 inven_id,
    std::uint8_t                 item_id,
    std::uint8_t                 channel,
    std::uint16_t                map_id,
    float                        pos_x,
    float                        pos_y,
    float                        pos_z);

// --- W3b-6 party round-robin loot (PT_ORDER) ----------------------

// MW_PARTYORDERTAKEITEM_REQ — hands the next looter (chosen by the
// party's turn cursor) the dropped item. The trailing item is
// written with the W3a-37 cabinet codec (legacy WrapItem).
//
// Wire layout (SSSender.cpp:1698):
//   DWORD char_id, DWORD key, BYTE server_id, BYTE channel,
//   WORD map_id, DWORD mon_id, WORD temp_mon_id, <WrapItem>
boost::asio::awaitable<void> SendMwPartyOrderTakeItemReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 server_id,
    std::uint8_t                 channel,
    std::uint16_t                map_id,
    std::uint32_t                mon_id,
    std::uint16_t                temp_mon_id,
    const TGuildCabinetItem&     item);

// MW_ADDITEMRESULT_REQ — generic item-pickup result; the PT_ORDER
// path uses it to report MIT_NOTFOUND when the party id is stale.
//
// Wire layout (SSSender.cpp:1889):
//   DWORD char_id, DWORD key, BYTE channel, WORD map_id,
//   DWORD mon_id, BYTE item_id, BYTE result
boost::asio::awaitable<void> SendMwAddItemResultReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 channel,
    std::uint16_t                map_id,
    std::uint32_t                mon_id,
    std::uint8_t                 item_id,
    std::uint8_t                 result);

// MW_DEALITEMERROR_REQ — a trade/deal error relayed to the affected
// char's map.
//   Wire (SSSender.cpp): STRING target, STRING error_char, BYTE error
boost::asio::awaitable<void> SendMwDealItemErrorReq(
    std::shared_ptr<PeerSession> peer,
    const std::string&           target,
    const std::string&           error_char,
    std::uint8_t                 error);

// MW_PARTYMOVE_REQ — result of a corps general reshuffling a member
// between squads (move or swap). CORPS_* result code.
//
// Wire layout (SSSender.cpp:2160):
//   DWORD char_id, DWORD key, BYTE result
boost::asio::awaitable<void> SendMwPartyMoveReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 result);

// --- W3c-1 corps invite relay (senders_corps.cpp) -----------------

// MW_CORPSASK_REQ — forwarded to the target chief's map when a
// party chief invites them to ally into a corps. Their client pops
// the confirm dialog keyed by the inviter.
//
// Wire layout (SSSender.cpp:2021):
//   DWORD char_id, DWORD key, DWORD inviter_char_id,
//   STRING inviter_name
boost::asio::awaitable<void> SendMwCorpsAskReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint32_t                inviter_char_id,
    const std::string&           inviter_name);

// MW_CORPSREPLY_REQ — the corps-invite result relayed back to a
// chief (CORPS_* code + the other chief's name).
//
// Wire layout (SSSender.cpp:1954):
//   DWORD char_id, DWORD key, BYTE result, STRING name
boost::asio::awaitable<void> SendMwCorpsReplyReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 result,
    const std::string&           name);

// --- W3c-2 corps formation ----------------------------------------

// One squad member as serialised in MW_ADDSQUAD_REQ. The legacy
// packet also carries per-member real-time command/target state
// (m_command.*) which the world side doesn't model — those fields
// are emitted as 0 (the corps-command handler will own them later).
struct SquadMemberInfo
{
    std::uint32_t char_id = 0;
    std::string   name;
    std::uint32_t max_hp  = 0;
    std::uint32_t hp      = 0;
    std::uint16_t map_id  = 0;
    std::uint16_t pos_x   = 0;   // WORD-truncated world X
    std::uint16_t pos_z   = 0;
    std::uint8_t  level   = 0;
    std::uint8_t  klass   = 0;
    std::uint8_t  race    = 0;
    std::uint8_t  sex     = 0;
    std::uint8_t  face    = 0;
    std::uint8_t  hair    = 0;
};

// MW_ADDSQUAD_REQ — announces one squad (party) + its members to a
// recipient. The corps-join fan-out fires it pairwise so every
// existing-squad member learns the joining squad and vice versa.
//
// Wire layout (SSSender.cpp:1968):
//   DWORD recipient_char_id, DWORD recipient_key,
//   DWORD chief_id, WORD party_id, BYTE member_count,
//   × member_count:
//     DWORD char_id, STRING name, FLOAT 1.0, DWORD tg_obj(0),
//     DWORD max_hp, DWORD hp, WORD tg_pos_x(0), WORD tg_pos_z(0),
//     WORD map_id, WORD pos_x, WORD pos_z, WORD MOVE_NONE(1800),
//     BYTE tg_type(0), BYTE level, BYTE class, BYTE race, BYTE sex,
//     BYTE face, BYTE hair, BYTE command(0)
boost::asio::awaitable<void> SendMwAddSquadReq(
    std::shared_ptr<PeerSession>          peer,
    std::uint32_t                         recipient_char_id,
    std::uint32_t                         recipient_key,
    std::uint32_t                         chief_id,
    std::uint16_t                         party_id,
    const std::vector<SquadMemberInfo>&   members);

// MW_CORPSJOIN_REQ — tells a (just-joined) squad member their corps
// id + the commander party id, so their client shows corps state.
//
// Wire layout (SSSender.cpp:2036):
//   DWORD char_id, DWORD key, WORD corps_id, WORD commander_party_id
boost::asio::awaitable<void> SendMwCorpsJoinReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint16_t                corps_id,
    std::uint16_t                commander_party_id);

// MW_DELSQUAD_REQ — tells a member that a squad (party) has left
// their corps so its client drops it from the corps roster.
//
// Wire layout (SSSender.cpp:2008):
//   DWORD char_id, DWORD key, WORD squad_party_id
boost::asio::awaitable<void> SendMwDelSquadReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint16_t                squad_party_id);

// MW_CORPSCMD_REQ — the general's command (move / attack a target
// or position) relayed to every corps member so their client
// mirrors the order. `member_char_id` is the recipient;
// `commander_char_id` is the squad chief whose command this is.
//
// Wire layout (SSSender.cpp:2118):
//   DWORD member_char_id, DWORD key, WORD squad_id,
//   DWORD commander_char_id, WORD map_id, BYTE cmd,
//   DWORD target_id, BYTE target_type, WORD pos_x, WORD pos_z
boost::asio::awaitable<void> SendMwCorpsCmdReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                member_char_id,
    std::uint32_t                key,
    std::uint16_t                squad_id,
    std::uint32_t                commander_char_id,
    std::uint16_t                map_id,
    std::uint8_t                 cmd,
    std::uint32_t                target_id,
    std::uint8_t                 target_type,
    std::uint16_t                pos_x,
    std::uint16_t                pos_z);

// Generic corps-chief relay (legacy RelayCorpsMsg) — re-frames an
// inbound corps-chief packet to another squad's chief: the leading
// char_id + key are replaced with the recipient's, the rest of the
// body (`tail`) is forwarded verbatim under `msg_id`. Backs the
// CORPSENEMYLIST / ADD / DEL / MOVE-ENEMY / MOVE-UNIT / CORPSHP
// broadcasts, which are all opaque chief-to-chief passthroughs.
boost::asio::awaitable<void> SendMwCorpsChiefRelay(
    std::shared_ptr<PeerSession>   peer,
    std::uint16_t                  msg_id,
    std::uint32_t                  recipient_char_id,
    std::uint32_t                  recipient_key,
    const std::vector<std::byte>&  tail);

// MW_CHGCORPSCOMMANDER_REQ — result of the general handing the
// commander role to another squad (CORPS_* code).
//
// Wire layout (SSSender.cpp:1942):
//   DWORD char_id, DWORD key, BYTE result
boost::asio::awaitable<void> SendMwChgCorpsCommanderReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 result);

// --- W4-1 friend invite (senders_friend.cpp) ----------------------

// MW_FRIENDADD_REQ — the friend-request outcome relayed to the
// requester. On failure the trailing fields are zero + `name` is
// the attempted target name; on success they carry the new
// friend's row.
//
// Wire layout (SSSender.cpp:1792):
//   DWORD char_id, DWORD key, BYTE result, DWORD friend_id,
//   STRING name, BYTE level, BYTE group, BYTE class, DWORD region
boost::asio::awaitable<void> SendMwFriendAddReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 result,
    std::uint32_t                friend_id,
    const std::string&           name,
    std::uint8_t                 level,
    std::uint8_t                 group,
    std::uint8_t                 klass,
    std::uint32_t                region);

// MW_FRIENDASK_REQ — forwarded to a target's map when someone
// requests to befriend them; their client pops the confirm dialog.
//
// Wire layout (SSSender.cpp:1816):
//   DWORD char_id, DWORD key, STRING inviter_name, DWORD inviter_id
boost::asio::awaitable<void> SendMwFriendAskReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    const std::string&           inviter_name,
    std::uint32_t                inviter_id);

// MW_FRIENDCONNECTION_REQ — a presence (online/offline) update
// pushed to a friend when the other side connects or disconnects.
//
// Wire layout (SSSender.cpp:2499):
//   DWORD char_id, DWORD key, BYTE result (FRIEND_CONNECTION /
//   FRIEND_DISCONNECTION), STRING name, DWORD region
boost::asio::awaitable<void> SendMwFriendConnectionReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 result,
    const std::string&           name,
    std::uint32_t                region);

// MW_FRIENDERASE_REQ — result of removing a friend.
//
// Wire layout (SSSender.cpp:1830):
//   DWORD char_id, DWORD key, BYTE result, DWORD target_id
boost::asio::awaitable<void> SendMwFriendEraseReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 result,
    std::uint32_t                target_id);

// --- W4-4 friend list (senders_friend.cpp) ------------------------

// One non-pending friend row in MW_FRIENDLIST_REQ. level/class/
// connected/region are resolved live from the CharRegistry by the
// handler (online → real values; offline → 0); name + group come
// from the stored friend entry.
struct FriendListRow
{
    std::uint32_t id        = 0;
    std::string   name;
    std::uint8_t  level     = 0;
    std::uint8_t  group     = 0;
    std::uint8_t  klass     = 0;
    std::uint8_t  connected = 0;
    std::uint32_t region    = 0;
};

// MW_FRIENDLIST_REQ — the char's full friend list (sent when the
// client opens the friend window). The soulmate slot is emitted as
// the "no soulmate" sentinel (DWORD 0) until soulmate ports.
//
// Wire layout (SSSender.cpp:1723):
//   DWORD char_id, DWORD key,
//   DWORD soulmate_target (0 = none),
//   BYTE group_count, × { BYTE id, STRING name },
//   BYTE friend_count,
//   × { DWORD id, STRING name, BYTE level, BYTE group, BYTE class,
//       BYTE connected, DWORD region }
boost::asio::awaitable<void> SendMwFriendListReq(
    std::shared_ptr<PeerSession>                              peer,
    std::uint32_t                                             char_id,
    std::uint32_t                                             key,
    const std::vector<std::pair<std::uint8_t, std::string>>&  groups,
    const std::vector<FriendListRow>&                         friends);

// --- W4-3 friend groups (senders_friend.cpp) ----------------------

// MW_FRIENDGROUPMAKE_REQ / MW_FRIENDGROUPNAME_REQ — result of
// creating / renaming a named friend group.
//   Wire (SSSender.cpp:2437/2483):
//     DWORD char_id, key, BYTE result, BYTE group, STRING name
boost::asio::awaitable<void> SendMwFriendGroupMakeReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 result,
    std::uint8_t                 group,
    const std::string&           name);
boost::asio::awaitable<void> SendMwFriendGroupNameReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 result,
    std::uint8_t                 group,
    const std::string&           name);

// MW_FRIENDGROUPDELETE_REQ — result of deleting a group.
//   Wire (SSSender.cpp:2453): DWORD char_id, key, BYTE result, group
boost::asio::awaitable<void> SendMwFriendGroupDeleteReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 result,
    std::uint8_t                 group);

// MW_FRIENDGROUPCHANGE_REQ — result of moving a friend to a group.
//   Wire (SSSender.cpp:2467):
//     DWORD char_id, key, BYTE result, group, DWORD friend_id
boost::asio::awaitable<void> SendMwFriendGroupChangeReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 result,
    std::uint8_t                 group,
    std::uint32_t                friend_id);

// --- W4-6 soulmate (senders_soulmate.cpp) -------------------------

// MW_SOULMATESEARCH_REQ — matchmaking result.
//   Wire (SSSender.cpp): DWORD char_id, key, BYTE result,
//     DWORD soul_id, STRING soul_name, DWORD region,
//     BYTE npc_inven, BYTE npc_item
boost::asio::awaitable<void> SendMwSoulmateSearchReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 result,
    std::uint32_t                soul_id,
    const std::string&           soul_name,
    std::uint32_t                region,
    std::uint8_t                 npc_inven,
    std::uint8_t                 npc_item);

// MW_SOULMATEREG_REQ — register/preview result.
//   Wire: DWORD char_id, key, BYTE result, reg, npc_inven,
//     npc_item, DWORD soulmate, STRING soul_name, DWORD region
boost::asio::awaitable<void> SendMwSoulmateRegReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 result,
    std::uint8_t                 reg,
    std::uint8_t                 npc_inven,
    std::uint8_t                 npc_item,
    std::uint32_t                soulmate,
    const std::string&           soul_name,
    std::uint32_t                region);

// MW_SOULMATEEND_REQ — divorce result.
//   Wire: DWORD char_id, key, BYTE result, DWORD time
boost::asio::awaitable<void> SendMwSoulmateEndReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 result,
    std::uint32_t                time_unix);

// --- W4-5 chat relay (senders_chat.cpp) ---------------------------

// MW_CHAT_REQ — a chat message delivered to one recipient (or, for
// the global channels, broadcast per map peer with char_id/key=0).
// country/war_country carry the sender's m_bCountry / m_bAidCountry.
//
// Wire layout (SSSender.cpp:1641):
//   DWORD char_id, DWORD key, BYTE channel, DWORD sender_id,
//   STRING sender_name, BYTE country, BYTE war_country, BYTE type,
//   BYTE group, DWORD target_id, STRING talk
boost::asio::awaitable<void> SendMwChatReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 channel,
    std::uint32_t                sender_id,
    const std::string&           sender_name,
    std::uint8_t                 country,
    std::uint8_t                 war_country,
    std::uint8_t                 type,
    std::uint8_t                 group,
    std::uint32_t                target_id,
    const std::string&           talk);

// MW_CHATBAN_REQ — result of a GM chat-ban action, sent to the
// banned char's map (to enforce) and echoed to the issuing GM's map.
//   Wire (SSSender.cpp): STRING name, INT64 ban_time, BYTE result,
//     DWORD char_id, DWORD key
boost::asio::awaitable<void> SendMwChatBanReq(
    std::shared_ptr<PeerSession> peer,
    const std::string&           name,
    std::int64_t                 ban_time,
    std::uint8_t                 result,
    std::uint32_t                char_id,
    std::uint32_t                key);

// MW_CHARMSG_REQ — a system / GM message delivered to a named char's
// map (the control server's CT_CHARMSG relay).
//   Wire (SSSender.cpp): STRING name, STRING message
boost::asio::awaitable<void> SendMwCharMsgReq(
    std::shared_ptr<PeerSession> peer,
    const std::string&           name,
    const std::string&           message);

// --- W5-1 territory occupation broadcasts (senders_occupy.cpp) ----
//
// Each is broadcast to every map peer so the new owner/flag shows
// cluster-wide. Mirrors SSSender.cpp.

// MW_CASTLEOCCUPY_REQ — a castle changed hands.
//   Wire: BYTE type, WORD castle, DWORD guild_id, BYTE country,
//     STRING guild_name
boost::asio::awaitable<void> SendMwCastleOccupyReq(
    std::shared_ptr<PeerSession> peer,
    std::uint8_t                 type,
    std::uint16_t                castle_id,
    std::uint32_t                guild_id,
    std::uint8_t                 country,
    const std::string&           guild_name);

// MW_LOCALOCCUPY_REQ — a local (territory) changed hands.
//   Wire: BYTE type, WORD local, BYTE country, DWORD guild_id,
//     STRING guild_name
boost::asio::awaitable<void> SendMwLocalOccupyReq(
    std::shared_ptr<PeerSession> peer,
    std::uint8_t                 type,
    std::uint16_t                local_id,
    std::uint8_t                 country,
    std::uint32_t                guild_id,
    const std::string&           guild_name);

// MW_MISSIONOCCUPY_REQ — a mission objective changed hands.
//   Wire: BYTE type, WORD local, BYTE country
boost::asio::awaitable<void> SendMwMissionOccupyReq(
    std::shared_ptr<PeerSession> peer,
    std::uint8_t                 type,
    std::uint16_t                local_id,
    std::uint8_t                 country);

// --- W5-2 castle-war apply (senders_occupy.cpp) -------------------

// MW_CASTLEAPPLY_REQ — result of a chief assigning a member to a
// castle siege, sent to the chief's map and (if different) the
// assigned member's map.
//   Wire: DWORD char_id, key, BYTE result, WORD castle, DWORD target,
//     BYTE camp
boost::asio::awaitable<void> SendMwCastleApplyReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 result,
    std::uint16_t                castle_id,
    std::uint32_t                target,
    std::uint8_t                 camp);

// MW_CASTLEAPPLICANTCOUNT_REQ — broadcast the new applicant count for
// one (guild, castle) so every map's siege UI updates. The legacy
// wire splits the WORD count into two bytes (hi, lo).
//   Wire: WORD castle, DWORD guild_id, BYTE count_hi, BYTE count_lo
boost::asio::awaitable<void> SendMwCastleApplicantCountReq(
    std::shared_ptr<PeerSession> peer,
    std::uint16_t                castle_id,
    std::uint32_t                guild_id,
    std::uint16_t                count);

// --- W5-4 war-window enable broadcasts (senders_occupy.cpp) -------
//
// Sent to every map peer when the scheduler opens/closes a war
// window (SM_BATTLESTATUS_REQ fan-out).

// MW_LOCALENABLE_REQ — local/territory war window.
//   Wire: BYTE status, DWORD second, DWORD local_start,
//     BYTE castle_day, DWORD castle_start
boost::asio::awaitable<void> SendMwLocalEnableReq(
    std::shared_ptr<PeerSession> peer,
    std::uint8_t                 status,
    std::uint32_t                second,
    std::uint32_t                local_start,
    std::uint8_t                 castle_day,
    std::uint32_t                castle_start);

// MW_CASTLEENABLE_REQ — castle war window.
//   Wire: BYTE status, DWORD second
boost::asio::awaitable<void> SendMwCastleEnableReq(
    std::shared_ptr<PeerSession> peer,
    std::uint8_t                 status,
    std::uint32_t                second);

// MW_MISSIONENABLE_REQ — mission war window.
//   Wire: BYTE status, DWORD start, DWORD second
boost::asio::awaitable<void> SendMwMissionEnableReq(
    std::shared_ptr<PeerSession> peer,
    std::uint8_t                 status,
    std::uint32_t                start,
    std::uint32_t                second);

// --- W6-1 timed-event broadcast (senders_event.cpp) ---------------

// MW_EVENTQUARTER_REQ — announce a timed "present quarter" event to
// every map peer; `select` is the (server-chosen) present bucket.
//   Wire: BYTE day, hour, minute, select, STRING present
boost::asio::awaitable<void> SendMwEventQuarterReq(
    std::shared_ptr<PeerSession> peer,
    std::uint8_t                 day,
    std::uint8_t                 hour,
    std::uint8_t                 minute,
    std::uint8_t                 select,
    const std::string&           present);

// --- W6-2 combat / taming cross-server relays (senders_combat.cpp)-

// MW_MAGICMIRROR_REQ — spell-reflection, routed to the attacker's
// map. Body forwarded verbatim (world doesn't interpret it).
boost::asio::awaitable<void> SendMwMagicMirrorReq(
    std::shared_ptr<PeerSession>   peer,
    const std::vector<std::byte>&  body);

// MW_MONTEMPT_REQ — monster-taming attempt result, to the attacker's
// map.
//   Wire: DWORD char_id, key, WORD mon_id
boost::asio::awaitable<void> SendMwMonTemptReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint16_t                mon_id);

// MW_MONTEMPTEVO_REQ — taming-evolution result, to the attacker's map.
//   Wire: DWORD char_id, key, host_id, BYTE host_type
boost::asio::awaitable<void> SendMwMonTemptEvoReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint32_t                host_id,
    std::uint8_t                 host_type);

// MW_MONSTERDIE_REQ / MW_TAKEMONMONEY_REQ — a monster-result the map
// asked world about, routed verbatim back to the char's main map.
boost::asio::awaitable<void> SendMwMonsterDieReq(
    std::shared_ptr<PeerSession>   peer,
    const std::vector<std::byte>&  body);
boost::asio::awaitable<void> SendMwTakeMonMoneyReq(
    std::shared_ptr<PeerSession>   peer,
    const std::vector<std::byte>&  body);

// --- W6-3 global announcement broadcasts (senders_rank.cpp) -------

// MW_FAMERANKUPDATE_REQ — fame-ranking refresh, forwarded verbatim
// to every map peer (world doesn't interpret the table).
boost::asio::awaitable<void> SendMwFameRankUpdateReq(
    std::shared_ptr<PeerSession>   peer,
    const std::vector<std::byte>&  body);

// MW_HEROSELECT_REQ — a battle-zone hero was chosen; announced to
// every map peer.
//   Wire: WORD battle_zone, STRING hero_name, INT64 time
boost::asio::awaitable<void> SendMwHeroSelectReq(
    std::shared_ptr<PeerSession> peer,
    std::uint16_t                battle_zone,
    const std::string&           hero_name,
    std::int64_t                 time_hero);

// --- W6-4 recall-mon (summoned creature) sync (senders_recallmon.cpp)
//
// All three forward the inbound body verbatim (the ACK and REQ share
// an identical wire layout); CREATE's caller patches the generated
// recall id into the body before forwarding.
boost::asio::awaitable<void> SendMwCreateRecallMonReq(
    std::shared_ptr<PeerSession>   peer,
    const std::vector<std::byte>&  body);
boost::asio::awaitable<void> SendMwRecallMonDataReq(
    std::shared_ptr<PeerSession>   peer,
    const std::vector<std::byte>&  body);
boost::asio::awaitable<void> SendMwRecallMonDelReq(
    std::shared_ptr<PeerSession>   peer,
    const std::vector<std::byte>&  body);

// --- W6-5 companion-mon (spolecnik) sync — recall-mon's sibling ---
boost::asio::awaitable<void> SendMwCreateSpolecnikMonReq(
    std::shared_ptr<PeerSession>   peer,
    const std::vector<std::byte>&  body);
boost::asio::awaitable<void> SendMwSpolecnikMonDelReq(
    std::shared_ptr<PeerSession>   peer,
    const std::vector<std::byte>&  body);

// --- W4-11 TMS conference channels (senders_tms.cpp) --------------

// MW_TMSRECV_REQ — a conference message delivered to one member.
//   Wire (SSSender.cpp:2173):
//     DWORD char_id, key, tms, STRING sender_name, STRING message
boost::asio::awaitable<void> SendMwTmsRecvReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint32_t                tms,
    const std::string&           sender_name,
    const std::string&           message);

// MW_TMSINVITEASK_REQ — forwarded to a target's map so their client
// pops the "join conference?" dialog (keyed by the requester).
//   Wire (SSSender.cpp:2190):
//     DWORD char_id, key, target_id, target_key, tms, STRING message
boost::asio::awaitable<void> SendMwTmsInviteAskReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint32_t                target_id,
    std::uint32_t                target_key,
    std::uint32_t                tms,
    const std::string&           message);

// One roster row in a MW_TMSINVITE_REQ. Mirrors the per-member
// fields the legacy sender pulls off each LPTCHARACTER.
struct TmsMemberInfo
{
    std::uint32_t char_id = 0;
    std::string   name;
    std::uint8_t  klass   = 0;
    std::uint8_t  level   = 0;
};

// MW_TMSINVITE_REQ — announces the full conference roster to one
// recipient. `inviter` identifies who triggered the change.
//   Wire (SSSender.cpp:2209):
//     DWORD char_id, key, inviter, tms, BYTE member_count,
//     × member_count: DWORD id, STRING name, BYTE klass, BYTE level
boost::asio::awaitable<void> SendMwTmsInviteReq(
    std::shared_ptr<PeerSession>       peer,
    std::uint32_t                      char_id,
    std::uint32_t                      key,
    std::uint32_t                      inviter,
    std::uint32_t                      tms,
    const std::vector<TmsMemberInfo>&  members);

// MW_TMSOUT_REQ — tells a member that someone left the conference.
//   Wire (SSSender.cpp:2235):
//     DWORD char_id, key, tms, STRING target_name
boost::asio::awaitable<void> SendMwTmsOutReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint32_t                tms,
    const std::string&           target_name);

// --- W4-13 mail delivery relay (senders_post.cpp) -----------------

// MW_POSTRECV_REQ — "you have new mail" notification routed to the
// recipient's map. The body is forwarded verbatim (the inbound
// POSTRECV_ACK payload: post_id / sender / target / title / type) —
// world never interprets it, it only routes by the target name.
// Legacy re-tags the inbound packet in place (SSSender.cpp:2250).
boost::asio::awaitable<void> SendMwPostRecvReq(
    std::shared_ptr<PeerSession>   peer,
    const std::vector<std::byte>&  body);

// --- W6-12 GM user-tracking relays (senders_admin.cpp) ------------

// MW_USERPOSITION_REQ — relay a GM's "where is this player" request
// to the target's map.
//   Wire: DWORD char_id, key, STRING gm_name
boost::asio::awaitable<void> SendMwUserPositionReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    const std::string&           gm_name);

// CT_USERMOVE_ACK — relay a GM force-move to the target's map.
//   Wire: DWORD char_id, key, BYTE channel, WORD map_id,
//     FLOAT pos_x, pos_y, pos_z, WORD party_id
boost::asio::awaitable<void> SendCtUserMoveAck(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 channel,
    std::uint16_t                map_id,
    float                        pos_x,
    float                        pos_y,
    float                        pos_z,
    std::uint16_t                party_id);

// --- W6-13 connection-list reconcile (senders_conn.cpp) -----------
//
// The four senders the connection-list reconcile fires. They form
// the smallest complete entry into the legacy connection/teleport
// cluster: the map server reports which servers a char is connected
// to (MW_CONLIST_ACK / MW_MAPSVRLIST_ACK), and world reconciles its
// own view, then either asks the main map to route the char to the
// newly-needed servers (ROUTELIST) or re-confirms the main session
// on every remaining connection (CHECKMAIN). The two error replies
// (DELCHAR / INVALIDCHAR) close a client down when the char is gone
// or its main session can't be found.

// MW_ROUTELIST_REQ — ask the char's main map for the routing info
// (IP/port) of the servers it must additionally connect to. The
// main map answers with MW_ROUTE_ACK, which it forwards down to the
// client so the client opens the extra map connections.
//   Wire (SSHandler.cpp:2113): DWORD char_id, key, BYTE count,
//     × count: BYTE server_id
boost::asio::awaitable<void> SendMwRouteListReq(
    std::shared_ptr<PeerSession>       peer,
    std::uint32_t                      char_id,
    std::uint32_t                      key,
    const std::vector<std::uint8_t>&   server_ids);

// MW_CHECKMAIN_REQ — broadcast to every map the char is connected
// to, asking each "are you the main session?". The one that is
// answers MW_CHECKMAIN_ACK; the rest ignore it. Carries the char's
// current channel / map / position so a stale map can re-sync.
//   Wire (SSSender.cpp:485): DWORD char_id, key, BYTE channel,
//     WORD map_id, FLOAT pos_x, pos_y, pos_z
boost::asio::awaitable<void> SendMwCheckMainReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 channel,
    std::uint16_t                map_id,
    float                        pos_x,
    float                        pos_y,
    float                        pos_z);

// MW_DELCHAR_REQ — tell a map to delete a char it reported on when
// world no longer knows that char. logout=1/save=0 in the reconcile
// error path (legacy SendMW_DELCHAR_REQ(id,key,TRUE,FALSE)).
//   Wire (SSSender.cpp:507): DWORD char_id, key, BYTE logout, BYTE save
boost::asio::awaitable<void> SendMwDelCharReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 logout,
    std::uint8_t                 save);

// MW_INVALIDCHAR_REQ — tell the reporting map the char's main
// session is unknown (suspected stale/hacked connect); the map
// drops the client. release_main is 0 in the reconcile path.
//   Wire (SSSender.cpp:232): DWORD char_id, key, BYTE release_main
boost::asio::awaitable<void> SendMwInvalidCharReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 release_main);

// --- W6-14 main-session confirmation (senders_conn.cpp) -----------

// MW_CONRESULT_REQ — the main map's go-ahead: the char's connection
// set is confirmed, so the client may proceed. Carries the result
// byte (CN_SUCCESS / a TCONNECT_RESULT failure) and the full server
// list the char is connected to.
//   Wire (SSSender.cpp:450): DWORD char_id, key, BYTE result,
//     BYTE count, × count: BYTE server_id
boost::asio::awaitable<void> SendMwConResultReq(
    std::shared_ptr<PeerSession>       peer,
    std::uint32_t                      char_id,
    std::uint32_t                      key,
    std::uint8_t                       result,
    const std::vector<std::uint8_t>&   server_ids);

// MW_CLOSECHAR_REQ — tell a map to drop a connection that's no
// longer part of the char's set (drains TChar::dead_cons; legacy
// ClearDeadCON).
//   Wire (SSSender.cpp:473): DWORD char_id, key
boost::asio::awaitable<void> SendMwCloseCharReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key);

// MW_RELEASEMAIN_REQ — ask the current main map to hand off the main
// session (so a different map can take it over). Carries the char's
// channel / map / position so the new main can resume it.
//   Wire (SSSender.cpp:188): DWORD char_id, key, BYTE channel,
//     WORD map_id, FLOAT pos_x, pos_y, pos_z
boost::asio::awaitable<void> SendMwReleaseMainReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 channel,
    std::uint16_t                map_id,
    float                        pos_x,
    float                        pos_y,
    float                        pos_z);

// MW_ENTERSVR_REQ — forward a released char to the map taking over its
// main session. The body is the inbound RELEASEMAIN_ACK payload
// verbatim (BYTE db_load, DWORD char_id, key, + the char's saved
// state) re-tagged as ENTERSVR_REQ; world doesn't interpret the tail.
// Legacy SendMW_ENTERSVR_REQ(pBUF) (SSSender.cpp:178) copies + re-tags.
boost::asio::awaitable<void> SendMwEnterSvrReq(
    std::shared_ptr<PeerSession>   peer,
    const std::vector<std::byte>&  body);

// MW_MAPSVRLIST_REQ — ask the (new) main map for the full set of map
// servers the char must be connected to. Fired after a main-session
// handoff completes (W6-16 ENTERSVR_ACK); the map answers
// MW_MAPSVRLIST_ACK, which re-enters the W6-13 reconcile.
//   Wire (SSSender.cpp:210): DWORD char_id, key, BYTE channel,
//     WORD map_id, FLOAT pos_x, pos_y, pos_z
boost::asio::awaitable<void> SendMwMapSvrListReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 channel,
    std::uint16_t                map_id,
    float                        pos_x,
    float                        pos_y,
    float                        pos_z);

// MW_STARTTELEPORT_REQ — tell every map the char is connected to that
// a teleport is starting, so each can move the char's avatar to the
// destination channel/map/position. Broadcast from BeginTeleport.
//   Wire (SSSender.cpp:2731): DWORD char_id, key, BYTE channel,
//     WORD map_id, FLOAT pos_x, pos_y, pos_z
boost::asio::awaitable<void> SendMwStartTeleportReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 channel,
    std::uint16_t                map_id,
    float                        pos_x,
    float                        pos_y,
    float                        pos_z);

// --- W6-20 connection-completion sub-flow (senders_conn.cpp) ------

// One pending connection entry in MW_ADDCONNECT_REQ — the (ip/port)
// the client should open + the map server id it leads to.
struct AddConnectEntry
{
    std::uint32_t ip_addr   = 0;   // dwIPAddr (network order on the wire)
    std::uint16_t port      = 0;   // wPort
    std::uint8_t  server_id = 0;   // bServerID
};

// MW_ADDCONNECT_REQ — forwarded to the reporting (main) map so it
// hands the client the list of new map-server (IP/port) endpoints to
// open. Carries the *new* cons that ROUTE_ACK just registered — not
// the live set. The legacy sender is inline in OnMW_ROUTE_ACK
// (SSHandler.cpp:1943); we expose a dedicated free-function builder.
//   Wire: DWORD char_id, key, BYTE count, × count: DWORD ip, WORD port,
//     BYTE server_id
boost::asio::awaitable<void> SendMwAddConnectReq(
    std::shared_ptr<PeerSession>            peer,
    std::uint32_t                           char_id,
    std::uint32_t                           key,
    const std::vector<AddConnectEntry>&     entries);

// MW_CHARDATA_REQ — ask the (main) map to send back the char's
// current level / HP / MP composite. Fired when ROUTE_ACK reports
// count==0, so no new cons are pending; the matching CHARDATA_ACK
// closes the loop by re-confirming the main session across all cons.
//   Wire (SSSender.cpp:246): DWORD char_id, key
boost::asio::awaitable<void> SendMwCharDataReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key);

// --- W6-21 teleport confirm (senders_conn.cpp) --------------------

// MW_TELEPORT_REQ — the teleport outcome relayed back to the
// originating map (which forwards it to the client). `result` is a
// TTELEPORT_RESULT byte (TPR_SUCCESS / TPR_NODESTINATION / …;
// NetCode.h:254).
//   Wire (SSSender.cpp:614): DWORD char_id, key, BYTE channel,
//     WORD map_id, FLOAT pos_x, pos_y, pos_z, BYTE result
boost::asio::awaitable<void> SendMwTeleportReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 channel,
    std::uint16_t                map_id,
    float                        pos_x,
    float                        pos_y,
    float                        pos_z,
    std::uint8_t                 result);

// MW_CONLIST_REQ — ask the destination map for the char's connection
// list (fired after a successful teleport so the new map's reconcile
// re-enters the W6-13 CONLIST/MAPSVRLIST flow).
//   Wire (SSSender.cpp:592): DWORD char_id, key, BYTE channel,
//     WORD map_id, FLOAT pos_x, pos_y, pos_z
boost::asio::awaitable<void> SendMwConListReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 channel,
    std::uint16_t                map_id,
    float                        pos_x,
    float                        pos_y,
    float                        pos_z);

} // namespace tworldsvr::senders
