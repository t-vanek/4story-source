#include "senders/senders.h"
#include "wire_codec.h"

#include "MessageId.h"

namespace tworldsvr::senders {

boost::asio::awaitable<void>
SendRwRelaysvrAck(std::shared_ptr<PeerSession>      peer,
                  std::uint8_t                       nation,
                  const std::vector<std::uint32_t>&  operators,
                  const std::map<std::string, std::string>& svr_msgs)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint8_t>(body, nation);
    WritePOD<std::uint16_t>(body, static_cast<std::uint16_t>(operators.size()));
    for (auto op_id : operators)
        WritePOD<std::uint32_t>(body, op_id);
    WritePOD<std::uint16_t>(body, static_cast<std::uint16_t>(svr_msgs.size()));
    for (const auto& [k, v] : svr_msgs)
    {
        WriteString(body, k);
        WriteString(body, v);
    }
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::RW_RELAYSVR_ACK),
        std::move(body));
}

boost::asio::awaitable<void>
SendRwEntercharAck(std::shared_ptr<PeerSession> peer,
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
                   std::uint16_t                unit_id)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WriteString(body, name);
    WritePOD<std::uint8_t>(body, result);
    WritePOD<std::uint8_t>(body, country);
    WritePOD<std::uint8_t>(body, aid_country);
    WritePOD<std::uint32_t>(body, guild_id);
    WritePOD<std::uint32_t>(body, guild_chief);
    WritePOD<std::uint8_t>(body, duty);
    WritePOD<std::uint16_t>(body, party_id);
    WritePOD<std::uint32_t>(body, party_chief_id);
    WritePOD<std::uint16_t>(body, corps_id);
    WritePOD<std::uint32_t>(body, general_id);
    WritePOD<std::uint32_t>(body, tactics_id);
    WritePOD<std::uint32_t>(body, tactics_chief);
    WritePOD<std::uint16_t>(body, map_id);
    WritePOD<std::uint16_t>(body, unit_id);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::RW_ENTERCHAR_ACK),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwCharStatInfoAnsReq(std::shared_ptr<PeerSession> peer,
                         std::uint32_t                req_char_id,
                         std::uint32_t                char_id)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, req_char_id);
    WritePOD<std::uint32_t>(body, char_id);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_CHARSTATINFOANS_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwCharStatInfoReq(std::shared_ptr<PeerSession>  peer,
                      const std::vector<std::byte>& body)
{
    std::vector<std::byte> copy(body);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_CHARSTATINFO_REQ),
        std::move(copy));
}

boost::asio::awaitable<void>
SendMwLevelUpReq(std::shared_ptr<PeerSession> peer,
                 std::uint32_t                char_id,
                 std::uint32_t                key,
                 std::uint8_t                 level)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint8_t>(body, level);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_LEVELUP_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwPetRidingReq(std::shared_ptr<PeerSession> peer,
                   std::uint32_t                char_id,
                   std::uint32_t                key,
                   std::uint32_t                riding)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint32_t>(body, riding);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_PETRIDING_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwHelmetHideReq(std::shared_ptr<PeerSession> peer,
                    std::uint32_t                char_id,
                    std::uint32_t                key,
                    std::uint8_t                 hide)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint8_t>(body, hide);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_HELMETHIDE_REQ),
        std::move(body));
}

// --- W4-22 fresh-login ENTERSVR completion ------------------------

boost::asio::awaitable<void>
SendMwCharInfoReq(std::shared_ptr<PeerSession> peer,
                  std::uint32_t                char_id,
                  std::uint32_t                key,
                  const CharInfoPayload&       p)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    // Guild block (legacy emits zeros + TCONTRY_N + "" when guildless).
    WritePOD<std::uint32_t>(body, p.guild_id);
    WritePOD<std::uint8_t> (body, p.guild_country);
    WriteString            (body, p.guild_name);
    WritePOD<std::uint32_t>(body, p.fame);
    WritePOD<std::uint32_t>(body, p.fame_color);
    // Tactics block.
    WritePOD<std::uint32_t>(body, p.tactics_id);
    WriteString            (body, p.tactics_name);
    // Per-guild member fields + party.
    WritePOD<std::uint8_t> (body, p.duty);
    WritePOD<std::uint8_t> (body, p.peer);
    WritePOD<std::uint16_t>(body, p.castle);
    WritePOD<std::uint8_t> (body, p.camp);
    WritePOD<std::uint16_t>(body, p.party_id);
    WritePOD<std::uint8_t> (body, p.party_obtain_type);
    WritePOD<std::uint32_t>(body, p.party_chief_id);
    // Trailer.
    WritePOD<std::uint16_t>(body, p.title_id);
    WritePOD<std::uint32_t>(body, p.rank_point);
    WritePOD<std::uint8_t> (body, p.bow_release);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_CHARINFO_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwRouteReq(std::shared_ptr<PeerSession> peer,
               std::uint32_t                char_id,
               std::uint32_t                key,
               std::uint8_t                 channel,
               std::uint16_t                map_id,
               float                        pos_x,
               float                        pos_y,
               float                        pos_z)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint8_t> (body, channel);
    WritePOD<std::uint16_t>(body, map_id);
    WritePOD<float>        (body, pos_x);
    WritePOD<float>        (body, pos_y);
    WritePOD<float>        (body, pos_z);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_ROUTE_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwEnterCharReq(std::shared_ptr<PeerSession>      peer,
                   std::uint32_t                     char_id,
                   std::uint32_t                     key,
                   const EnterCharReqPayload&        p,
                   const std::vector<std::byte>&     opaque_tail)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint8_t> (body, p.start_act);
    WriteString            (body, p.name);
    WritePOD<std::uint16_t>(body, p.map_id);
    WritePOD<float>        (body, p.pos_x);
    WritePOD<float>        (body, p.pos_y);
    WritePOD<float>        (body, p.pos_z);
    WritePOD<std::uint32_t>(body, p.guild_id);
    WritePOD<std::uint32_t>(body, p.fame);
    WritePOD<std::uint32_t>(body, p.fame_color);
    WriteString            (body, p.guild_name);
    WritePOD<std::uint8_t> (body, p.duty);
    WritePOD<std::uint8_t> (body, p.peer);
    WritePOD<std::uint16_t>(body, p.castle);
    WritePOD<std::uint8_t> (body, p.camp);
    WritePOD<std::uint32_t>(body, p.tactics_id);
    WriteString            (body, p.tactics_name);
    WritePOD<std::uint16_t>(body, p.party_id);
    WritePOD<std::uint8_t> (body, p.party_type);
    WritePOD<std::uint32_t>(body, p.party_chief_id);
    WritePOD<std::uint16_t>(body, p.commander);
    WritePOD<std::uint8_t> (body, p.level);
    WritePOD<std::uint8_t> (body, p.helmet_hide);
    WritePOD<std::uint8_t> (body, p.country);
    WritePOD<std::uint8_t> (body, p.aid_country);
    WritePOD<std::uint8_t> (body, p.mode);
    WritePOD<std::uint32_t>(body, p.riding);
    WritePOD<std::int64_t> (body, p.chat_ban_time);
    WritePOD<std::uint32_t>(body, p.soulmate);
    WritePOD<std::uint32_t>(body, p.soul_silence);
    WriteString            (body, p.soulmate_name);
    WritePOD<std::uint8_t> (body, p.klass);
    // Recall-mon table + comment string come straight from the inbound
    // CHARDATA_ACK body (offset past the 10 named header fields). The
    // legacy parses + re-encodes them field-by-field; both packets
    // emit the same structured shape, so an opaque byte copy is
    // wire-equivalent and avoids re-introducing the parsing bug that
    // an early prototype of this sender hit on the recall-skill list.
    body.insert(body.end(), opaque_tail.begin(), opaque_tail.end());
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_ENTERCHAR_REQ),
        std::move(body));
}

} // namespace tworldsvr::senders
