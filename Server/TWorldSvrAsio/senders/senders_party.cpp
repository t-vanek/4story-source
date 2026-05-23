#include "senders/senders.h"
#include "wire_codec.h"

#include "MessageId.h"

namespace tworldsvr::senders {

boost::asio::awaitable<void>
SendMwPartyAddReq(std::shared_ptr<PeerSession> peer,
                  std::uint32_t                char_id,
                  std::uint32_t                key,
                  const std::string&           request_name,
                  const std::string&           target_name,
                  std::uint8_t                 obtain_type,
                  std::uint8_t                 result,
                  std::uint32_t                request_char_id)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WriteString(body, request_name);
    WriteString(body, target_name);
    WritePOD<std::uint8_t>(body, obtain_type);
    WritePOD<std::uint8_t>(body, result);
    WritePOD<std::uint32_t>(body, request_char_id);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_PARTYADD_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwPartyJoinReq(std::shared_ptr<PeerSession> peer,
                   std::uint32_t                recipient_char_id,
                   std::uint32_t                recipient_key,
                   std::uint16_t                party_id,
                   std::uint32_t                chief_id,
                   std::uint16_t                commander_id,
                   std::uint8_t                 obtain_type,
                   const PartyMemberInfo&       member)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, recipient_char_id);
    WritePOD<std::uint32_t>(body, recipient_key);
    WritePOD<std::uint16_t>(body, party_id);
    WriteString(body, member.name);
    WritePOD<std::uint32_t>(body, member.char_id);
    WritePOD<std::uint32_t>(body, chief_id);
    WritePOD<std::uint16_t>(body, commander_id);
    WriteString(body, member.guild_name);
    WritePOD<std::uint8_t>(body, member.level);
    WritePOD<std::uint32_t>(body, member.max_hp);
    WritePOD<std::uint32_t>(body, member.hp);
    WritePOD<std::uint32_t>(body, member.max_mp);
    WritePOD<std::uint32_t>(body, member.mp);
    WritePOD<std::uint8_t>(body, member.race);
    WritePOD<std::uint8_t>(body, member.sex);
    WritePOD<std::uint8_t>(body, member.face);
    WritePOD<std::uint8_t>(body, member.hair);
    WritePOD<std::uint8_t>(body, obtain_type);
    WritePOD<std::uint8_t>(body, member.klass);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_PARTYJOIN_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwPartyAttrReq(std::shared_ptr<PeerSession> peer,
                   std::uint32_t                char_id,
                   std::uint32_t                key,
                   std::uint16_t                party_id,
                   std::uint8_t                 party_type,
                   std::uint32_t                chief_id,
                   std::uint16_t                commander_id)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint16_t>(body, party_id);
    WritePOD<std::uint8_t>(body, party_type);
    WritePOD<std::uint32_t>(body, chief_id);
    WritePOD<std::uint16_t>(body, commander_id);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_PARTYATTR_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwPartyDelReq(std::shared_ptr<PeerSession> peer,
                  std::uint32_t                recipient_char_id,
                  std::uint32_t                recipient_key,
                  std::uint32_t                leaver_char_id,
                  std::uint32_t                chief_id,
                  std::uint16_t                commander_id,
                  std::uint16_t                party_id,
                  std::uint8_t                 kick)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, recipient_char_id);
    WritePOD<std::uint32_t>(body, recipient_key);
    WritePOD<std::uint32_t>(body, leaver_char_id);
    WritePOD<std::uint32_t>(body, chief_id);
    WritePOD<std::uint16_t>(body, commander_id);
    WritePOD<std::uint16_t>(body, party_id);
    WritePOD<std::uint8_t>(body, kick);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_PARTYDEL_REQ),
        std::move(body));
}

} // namespace tworldsvr::senders
