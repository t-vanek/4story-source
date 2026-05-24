#include "senders/senders.h"
#include "wire_codec.h"

#include "MessageId.h"

namespace tworldsvr::senders {

boost::asio::awaitable<void>
SendMwFriendAddReq(std::shared_ptr<PeerSession> peer,
                   std::uint32_t                char_id,
                   std::uint32_t                key,
                   std::uint8_t                 result,
                   std::uint32_t                friend_id,
                   const std::string&           name,
                   std::uint8_t                 level,
                   std::uint8_t                 group,
                   std::uint8_t                 klass,
                   std::uint32_t                region)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint8_t>(body, result);
    WritePOD<std::uint32_t>(body, friend_id);
    WriteString(body, name);
    WritePOD<std::uint8_t>(body, level);
    WritePOD<std::uint8_t>(body, group);
    WritePOD<std::uint8_t>(body, klass);
    WritePOD<std::uint32_t>(body, region);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_FRIENDADD_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwFriendAskReq(std::shared_ptr<PeerSession> peer,
                   std::uint32_t                char_id,
                   std::uint32_t                key,
                   const std::string&           inviter_name,
                   std::uint32_t                inviter_id)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WriteString(body, inviter_name);
    WritePOD<std::uint32_t>(body, inviter_id);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_FRIENDASK_REQ),
        std::move(body));
}

} // namespace tworldsvr::senders
