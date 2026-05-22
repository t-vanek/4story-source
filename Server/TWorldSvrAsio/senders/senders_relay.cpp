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

} // namespace tworldsvr::senders
