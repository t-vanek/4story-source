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

} // namespace tworldsvr::senders
