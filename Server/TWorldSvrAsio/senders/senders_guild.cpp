#include "senders/senders.h"
#include "wire_codec.h"

#include "MessageId.h"

namespace tworldsvr::senders {

boost::asio::awaitable<void>
SendMwGuildEstablishReq(std::shared_ptr<PeerSession> peer,
                        std::uint32_t                char_id,
                        std::uint32_t                key,
                        std::uint8_t                 result,
                        std::uint32_t                guild_id,
                        const std::string&           name,
                        std::uint8_t                 establish)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint8_t>(body, result);
    WritePOD<std::uint32_t>(body, guild_id);
    WriteString(body, name);
    WritePOD<std::uint8_t>(body, establish);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_GUILDESTABLISH_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwRelayconnectReq(std::shared_ptr<PeerSession> peer,
                      std::uint32_t                char_id,
                      std::uint8_t                 relay_on)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint8_t>(body, relay_on);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_RELAYCONNECT_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwGuildLeaveReq(std::shared_ptr<PeerSession> peer,
                    std::uint32_t                char_id,
                    std::uint32_t                key,
                    const std::string&           name,
                    std::uint8_t                 leave_reason,
                    std::uint32_t                time_unix)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WriteString(body, name);
    WritePOD<std::uint8_t>(body, leave_reason);
    WritePOD<std::uint32_t>(body, time_unix);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_GUILDLEAVE_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwGuildDisorganizationReq(std::shared_ptr<PeerSession> peer,
                              std::uint32_t                char_id,
                              std::uint32_t                key,
                              std::uint8_t                 disorg)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint8_t>(body, disorg);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_GUILDDISORGANIZATION_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwGuildDutyReq(std::shared_ptr<PeerSession> peer,
                   std::uint32_t                char_id,
                   std::uint32_t                key,
                   const std::string&           target_name,
                   std::uint8_t                 duty)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WriteString(body, target_name);
    WritePOD<std::uint8_t>(body, duty);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_GUILDDUTY_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwGuildFameReq(std::shared_ptr<PeerSession> peer,
                   std::uint32_t                char_id,
                   std::uint32_t                key,
                   std::uint8_t                 result,
                   std::uint32_t                originator_char_id,
                   std::uint32_t                fame,
                   std::uint32_t                fame_color)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint8_t>(body, result);
    WritePOD<std::uint32_t>(body, originator_char_id);
    WritePOD<std::uint32_t>(body, fame);
    WritePOD<std::uint32_t>(body, fame_color);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_GUILDFAME_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwGuildContributionReq(std::shared_ptr<PeerSession> peer,
                           std::uint32_t                char_id,
                           std::uint32_t                key,
                           std::uint8_t                 result,
                           std::uint32_t                exp,
                           std::uint32_t                gold,
                           std::uint32_t                silver,
                           std::uint32_t                cooper,
                           std::uint32_t                pvp_point)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint8_t>(body, result);
    WritePOD<std::uint32_t>(body, exp);
    WritePOD<std::uint32_t>(body, gold);
    WritePOD<std::uint32_t>(body, silver);
    WritePOD<std::uint32_t>(body, cooper);
    WritePOD<std::uint32_t>(body, pvp_point);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_GUILDCONTRIBUTION_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwGuildVolunteeringReq(std::shared_ptr<PeerSession> peer,
                           std::uint32_t                char_id,
                           std::uint32_t                key,
                           std::uint8_t                 result)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint8_t>(body, result);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_GUILDVOLUNTEERING_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwGuildVolunteeringDelReq(std::shared_ptr<PeerSession> peer,
                              std::uint32_t                char_id,
                              std::uint32_t                key,
                              std::uint8_t                 result)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint8_t>(body, result);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_GUILDVOLUNTEERINGDEL_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwGuildVolunteerListReq(std::shared_ptr<PeerSession>          peer,
                            std::uint32_t                         char_id,
                            std::uint32_t                         key,
                            const std::vector<GuildVolunteerRow>& applicants)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint32_t>(body,
        static_cast<std::uint32_t>(applicants.size()));
    for (const auto& a : applicants)
    {
        WritePOD<std::uint32_t>(body, a.char_id);
        WriteString(body, a.name);
        WritePOD<std::uint8_t>(body, a.level);
        WritePOD<std::uint8_t>(body, a.klass);
        WritePOD<std::uint32_t>(body, a.region);
    }
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_GUILDVOLUNTEERLIST_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwGuildVolunteerReplyReq(std::shared_ptr<PeerSession> peer,
                             std::uint32_t                char_id,
                             std::uint32_t                key,
                             std::uint8_t                 result)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint8_t>(body, result);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_GUILDVOLUNTEERREPLY_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwGuildWantedAddReq(std::shared_ptr<PeerSession> peer,
                        std::uint32_t                char_id,
                        std::uint32_t                key,
                        std::uint8_t                 result)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint8_t>(body, result);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_GUILDWANTEDADD_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwGuildWantedDelReq(std::shared_ptr<PeerSession> peer,
                        std::uint32_t                char_id,
                        std::uint32_t                key,
                        std::uint8_t                 result)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint8_t>(body, result);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_GUILDWANTEDDEL_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwGuildWantedListReq(std::shared_ptr<PeerSession>       peer,
                         std::uint32_t                      char_id,
                         std::uint32_t                      key,
                         const std::vector<GuildWantedRow>& entries)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint32_t>(body, static_cast<std::uint32_t>(entries.size()));
    for (const auto& e : entries)
    {
        WritePOD<std::uint32_t>(body, e.guild_id);
        WriteString(body, e.name);
        WriteString(body, e.title);
        WriteString(body, e.text);
        WritePOD<std::uint8_t>(body, e.min_level);
        WritePOD<std::uint8_t>(body, e.max_level);
        WritePOD<std::int64_t>(body, e.end_time_unix);
        WritePOD<std::uint8_t>(body, e.already_applied);
    }
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_GUILDWANTEDLIST_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwGuildInfoReq(std::shared_ptr<PeerSession> peer,
                   std::uint32_t                char_id,
                   std::uint32_t                key,
                   std::uint8_t                 result,
                   const GuildInfoPayload*      payload)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint8_t>(body, result);
    // Legacy SSSender.cpp:1030 — only emit payload on success.
    if (result == 0 /*kSuccess*/ && payload != nullptr)
    {
        WritePOD<std::uint32_t>(body, payload->guild_id);
        WriteString(body, payload->name);
        WritePOD<std::int64_t>(body, payload->establish_time);
        WritePOD<std::uint16_t>(body, payload->member_count);
        WritePOD<std::uint16_t>(body, payload->max_member);
        WriteString(body, payload->chief_name);
        WritePOD<std::uint8_t>(body, payload->chief_peer);
        // Vice-chief slots: always emit 2 entries even when fewer
        // exist (NAME_NULL = empty string per legacy parity).
        for (const auto& name : payload->vice_chief_names)
            WriteString(body, name);
        WritePOD<std::uint8_t>(body, payload->level);
        WritePOD<std::uint32_t>(body, payload->fame);
        WritePOD<std::uint32_t>(body, payload->fame_color);
        WritePOD<std::uint32_t>(body, payload->gi);
        WritePOD<std::uint32_t>(body, payload->exp);
        WritePOD<std::uint32_t>(body, payload->level_exp);
        WritePOD<std::uint8_t>(body, payload->guild_points);
        WritePOD<std::uint8_t>(body, payload->status);
        WritePOD<std::uint32_t>(body, payload->gold);
        WritePOD<std::uint32_t>(body, payload->silver);
        WritePOD<std::uint32_t>(body, payload->cooper);
        WritePOD<std::uint8_t>(body, payload->requester_duty);
        WritePOD<std::uint8_t>(body, payload->requester_peer);
        WriteString(body, payload->article_title);
        WritePOD<std::uint32_t>(body, payload->pvp_total_point);
        WritePOD<std::uint32_t>(body, payload->pvp_useable_point);
        WritePOD<std::uint32_t>(body, payload->pvp_month_point);
        WritePOD<std::uint32_t>(body, payload->rank_total);
        WritePOD<std::uint32_t>(body, payload->rank_month);
        WritePOD<std::uint8_t>(body, payload->stat_level);
        WritePOD<std::uint8_t>(body, payload->stat_point);
        WritePOD<std::uint32_t>(body, payload->stat_exp);
    }
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_GUILDINFO_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwGuildArticleListReq(std::shared_ptr<PeerSession>           peer,
                          std::uint32_t                          char_id,
                          std::uint32_t                          key,
                          const std::vector<GuildArticleRow>&    articles)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint8_t>(body, static_cast<std::uint8_t>(articles.size()));
    for (const auto& a : articles)
    {
        WritePOD<std::uint32_t>(body, a.id);
        WritePOD<std::uint8_t>(body, a.duty);
        WriteString(body, a.writer);
        WriteString(body, a.title);
        WriteString(body, a.body);
        WriteString(body, a.date);
    }
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_GUILDARTICLELIST_REQ),
        std::move(body));
}

namespace {
// Shared by ADD/DEL/UPDATE: identical 3-byte reply (char_id, key,
// result). The wID is the only difference, so we factor the wire
// build behind a single helper.
boost::asio::awaitable<void>
SendArticleResultReply(std::shared_ptr<PeerSession> peer,
                       tnetlib::protocol::MessageId wid,
                       std::uint32_t                char_id,
                       std::uint32_t                key,
                       std::uint8_t                 result)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint8_t>(body, result);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(wid), std::move(body));
}
} // namespace

boost::asio::awaitable<void>
SendMwGuildArticleAddReq(std::shared_ptr<PeerSession> peer,
                         std::uint32_t                char_id,
                         std::uint32_t                key,
                         std::uint8_t                 result)
{
    co_await SendArticleResultReply(peer,
        tnetlib::protocol::MessageId::MW_GUILDARTICLEADD_REQ,
        char_id, key, result);
}

boost::asio::awaitable<void>
SendMwGuildArticleDelReq(std::shared_ptr<PeerSession> peer,
                         std::uint32_t                char_id,
                         std::uint32_t                key,
                         std::uint8_t                 result)
{
    co_await SendArticleResultReply(peer,
        tnetlib::protocol::MessageId::MW_GUILDARTICLEDEL_REQ,
        char_id, key, result);
}

boost::asio::awaitable<void>
SendMwGuildArticleUpdateReq(std::shared_ptr<PeerSession> peer,
                            std::uint32_t                char_id,
                            std::uint32_t                key,
                            std::uint8_t                 result)
{
    co_await SendArticleResultReply(peer,
        tnetlib::protocol::MessageId::MW_GUILDARTICLEUPDATE_REQ,
        char_id, key, result);
}

boost::asio::awaitable<void>
SendMwGuildMemberListReq(std::shared_ptr<PeerSession>           peer,
                         std::uint32_t                          char_id,
                         std::uint32_t                          key,
                         std::uint8_t                           result,
                         std::uint32_t                          guild_id,
                         const std::string&                     guild_name,
                         const std::vector<GuildMemberListRow>& members)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint8_t>(body, result);
    // Legacy SSSender.cpp:984 — only emit the guild meta on the
    // success branch; error replies are 9 bytes total (header +
    // 1-byte result + nothing else after the wKey field).
    if (result == 0 /*kSuccess*/)
    {
        WritePOD<std::uint32_t>(body, guild_id);
        WriteString(body, guild_name);
        WritePOD<std::uint16_t>(body, static_cast<std::uint16_t>(
            members.size()));
        for (const auto& m : members)
        {
            WritePOD<std::uint32_t>(body, m.char_id);
            WriteString(body, m.name);
            WritePOD<std::uint8_t>(body, m.level);
            WritePOD<std::uint8_t>(body, m.klass);
            WritePOD<std::uint8_t>(body, m.duty);
            WritePOD<std::uint8_t>(body, m.peer);
            WritePOD<std::uint8_t>(body, m.online);
            WritePOD<std::uint32_t>(body, m.region);
            WritePOD<std::uint16_t>(body, m.castle);
            WritePOD<std::uint8_t>(body, m.camp);
            WritePOD<std::uint32_t>(body, m.tactics);
            WritePOD<std::uint8_t>(body, m.war_country);
            WritePOD<std::int64_t>(body, m.connected_date_unix);
        }
    }
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_GUILDMEMBERLIST_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwGuildInviteReq(std::shared_ptr<PeerSession> peer,
                     std::uint32_t                target_char_id,
                     std::uint32_t                target_key,
                     const std::string&           guild_name,
                     std::uint32_t                inviter_char_id,
                     const std::string&           inviter_name)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, target_char_id);
    WritePOD<std::uint32_t>(body, target_key);
    WriteString(body, guild_name);
    WritePOD<std::uint32_t>(body, inviter_char_id);
    WriteString(body, inviter_name);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_GUILDINVITE_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwGuildJoinReq(std::shared_ptr<PeerSession> peer,
                   std::uint32_t                char_id,
                   std::uint32_t                key,
                   std::uint8_t                 result,
                   std::uint32_t                guild_id,
                   std::uint32_t                fame,
                   std::uint32_t                fame_color,
                   const std::string&           guild_name,
                   std::uint32_t                member_id,
                   const std::string&           member_name,
                   std::uint8_t                 max_member)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint8_t>(body, result);
    WritePOD<std::uint32_t>(body, guild_id);
    WritePOD<std::uint32_t>(body, fame);
    WritePOD<std::uint32_t>(body, fame_color);
    WriteString(body, guild_name);
    WritePOD<std::uint32_t>(body, member_id);
    WriteString(body, member_name);
    WritePOD<std::uint8_t>(body, max_member);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_GUILDJOIN_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwGuildPeerReq(std::shared_ptr<PeerSession> peer,
                   std::uint32_t                char_id,
                   std::uint32_t                key,
                   std::uint8_t                 result,
                   const std::string&           target_name,
                   std::uint8_t                 new_peer,
                   std::uint8_t                 old_peer)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint8_t>(body, result);
    WriteString(body, target_name);
    WritePOD<std::uint8_t>(body, new_peer);
    WritePOD<std::uint8_t>(body, old_peer);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_GUILDPEER_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwGuildCabinetMaxReq(std::shared_ptr<PeerSession> peer,
                         std::uint32_t                char_id,
                         std::uint32_t                key,
                         std::uint8_t                 max_cabinet)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint8_t>(body, max_cabinet);
    // Legacy doesn't have a dedicated wID for this; the map side
    // re-reads the cabinet count from the guild's next state push.
    // We emit it on the closest matching wID — the same
    // MW_GUILDCABINETPUTIN_ACK channel the map uses for cabinet
    // delta notices — so future protocol cleanup can pick a real
    // dedicated ID without breaking the in-place handler.
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_GUILDCABINETPUTIN_ACK),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwGuildPvPRecordReq(std::shared_ptr<PeerSession>           peer,
                        std::uint32_t                          char_id,
                        std::uint32_t                          key,
                        const std::vector<GuildPvPRecordRow>&  members)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint16_t>(body, static_cast<std::uint16_t>(
        members.size()));
    for (const auto& m : members)
    {
        WritePOD<std::uint32_t>(body, m.char_id);
        WritePOD<std::uint16_t>(body, m.kill_count);
        WritePOD<std::uint16_t>(body, m.die_count);
        for (std::size_t i = 0; i < m.points.size(); ++i)
            WritePOD<std::uint32_t>(body, m.points[i]);
        // "Last record" slot — always zeros until per-day
        // vRecord history ports. Legacy emits the last vRecord
        // entry when its dwDate >= dwRecentRecordDate; the
        // structure is the same as the weekrecord slot above
        // (WORD kill / WORD die / DWORD points[6]).
        WritePOD<std::uint16_t>(body, 0);
        WritePOD<std::uint16_t>(body, 0);
        for (std::size_t i = 0; i < m.points.size(); ++i)
            WritePOD<std::uint32_t>(body, 0);
    }
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_GUILDPVPRECORD_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwGuildTacticsWantedAddReq(std::shared_ptr<PeerSession> peer,
                               std::uint32_t                char_id,
                               std::uint32_t                key,
                               std::uint8_t                 result)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint8_t>(body, result);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_GUILDTACTICSWANTEDADD_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwGuildTacticsWantedDelReq(std::shared_ptr<PeerSession> peer,
                               std::uint32_t                char_id,
                               std::uint32_t                key,
                               std::uint8_t                 result)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint8_t>(body, result);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_GUILDTACTICSWANTEDDEL_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwGuildTacticsWantedListReq(
    std::shared_ptr<PeerSession>              peer,
    std::uint32_t                             char_id,
    std::uint32_t                             key,
    const std::vector<GuildTacticsWantedRow>& entries)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint32_t>(body, static_cast<std::uint32_t>(
        entries.size()));
    for (const auto& e : entries)
    {
        WritePOD<std::uint32_t>(body, e.id);
        WritePOD<std::uint32_t>(body, e.guild_id);
        WriteString(body, e.name);
        WriteString(body, e.title);
        WriteString(body, e.text);
        WritePOD<std::uint8_t>(body, e.day);
        WritePOD<std::uint8_t>(body, e.min_level);
        WritePOD<std::uint8_t>(body, e.max_level);
        WritePOD<std::uint32_t>(body, e.point);
        WritePOD<std::uint32_t>(body, e.gold);
        WritePOD<std::uint32_t>(body, e.silver);
        WritePOD<std::uint32_t>(body, e.cooper);
        WritePOD<std::int64_t>(body, e.end_time_unix);
        WritePOD<std::uint8_t>(body, e.already_applied);
    }
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_GUILDTACTICSWANTEDLIST_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwGuildTacticsVolunteeringReq(std::shared_ptr<PeerSession> peer,
                                  std::uint32_t                char_id,
                                  std::uint32_t                key,
                                  std::uint8_t                 result)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint8_t>(body, result);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_GUILDTACTICSVOLUNTEERING_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwGuildTacticsVolunteeringDelReq(std::shared_ptr<PeerSession> peer,
                                     std::uint32_t                char_id,
                                     std::uint32_t                key,
                                     std::uint8_t                 result)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint8_t>(body, result);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId
                ::MW_GUILDTACTICSVOLUNTEERINGDEL_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwGuildTacticsVolunteerListReq(
    std::shared_ptr<PeerSession>                 peer,
    std::uint32_t                                char_id,
    std::uint32_t                                key,
    const std::vector<GuildTacticsVolunteerRow>& applicants)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint32_t>(body, static_cast<std::uint32_t>(
        applicants.size()));
    for (const auto& a : applicants)
    {
        WritePOD<std::uint32_t>(body, a.char_id);
        WriteString(body, a.name);
        WritePOD<std::uint8_t>(body, a.level);
        WritePOD<std::uint8_t>(body, a.klass);
        WritePOD<std::uint32_t>(body, a.region);
        WritePOD<std::uint8_t>(body, a.day);
        WritePOD<std::uint32_t>(body, a.point);
        WritePOD<std::uint32_t>(body, a.gold);
        WritePOD<std::uint32_t>(body, a.silver);
        WritePOD<std::uint32_t>(body, a.cooper);
    }
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId
                ::MW_GUILDTACTICSVOLUNTEERLIST_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwGainPvPointReq(std::shared_ptr<PeerSession> peer,
                     std::uint32_t                owner_id,
                     std::uint32_t                point,
                     std::uint8_t                 event,
                     std::uint8_t                 type,
                     std::uint8_t                 gain,
                     const std::string&           name,
                     std::uint8_t                 klass,
                     std::uint8_t                 level)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, owner_id);
    WritePOD<std::uint32_t>(body, point);
    WritePOD<std::uint8_t>(body, event);
    WritePOD<std::uint8_t>(body, type);
    WritePOD<std::uint8_t>(body, gain);
    WriteString(body, name);
    WritePOD<std::uint8_t>(body, klass);
    WritePOD<std::uint8_t>(body, level);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_GAINPVPPOINT_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwGuildPointLogReq(std::shared_ptr<PeerSession>           peer,
                       std::uint32_t                          char_id,
                       std::uint32_t                          key,
                       const std::vector<GuildPointLogEntry>& entries)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint16_t>(body, static_cast<std::uint16_t>(
        entries.size()));
    for (const auto& e : entries)
    {
        WritePOD<std::int64_t>(body, e.date_unix);
        WriteString(body, e.recipient_name);
        WritePOD<std::uint32_t>(body, e.point);
    }
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_GUILDPOINTLOG_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwGuildCabinetListReq(std::shared_ptr<PeerSession> peer,
                          std::uint32_t                char_id,
                          std::uint32_t                key,
                          std::uint8_t                 max_cabinet)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint8_t>(body, max_cabinet);
    // W3a-26 stub: item_count = 0. The per-item loop body
    // (DWORD itemID + WrapItem 18-field codec) lands when the
    // cabinet PUTIN/TAKEOUT handlers port — those need the full
    // TItem state model first.
    WritePOD<std::uint8_t>(body, 0);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_GUILDCABINETLIST_REQ),
        std::move(body));
}

} // namespace tworldsvr::senders
