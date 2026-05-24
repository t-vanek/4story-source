#include "senders/senders.h"
#include "wire_codec.h"

#include "MessageId.h"

namespace tworldsvr::senders {

boost::asio::awaitable<void>
SendMwChatReq(std::shared_ptr<PeerSession> peer,
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
              const std::string&           talk)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint8_t>(body, channel);
    WritePOD<std::uint32_t>(body, sender_id);
    WriteString(body, sender_name);
    WritePOD<std::uint8_t>(body, country);
    WritePOD<std::uint8_t>(body, war_country);
    WritePOD<std::uint8_t>(body, type);
    WritePOD<std::uint8_t>(body, group);
    WritePOD<std::uint32_t>(body, target_id);
    WriteString(body, talk);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_CHAT_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwChatBanReq(std::shared_ptr<PeerSession> peer,
                 const std::string&           name,
                 std::int64_t                 ban_time,
                 std::uint8_t                 result,
                 std::uint32_t                char_id,
                 std::uint32_t                key)
{
    using namespace wire;
    std::vector<std::byte> body;
    WriteString(body, name);
    WritePOD<std::int64_t>(body, ban_time);
    WritePOD<std::uint8_t>(body, result);
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_CHATBAN_REQ),
        std::move(body));
}

} // namespace tworldsvr::senders
