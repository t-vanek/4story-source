#include "senders/senders.h"
#include "wire_codec.h"

#include "MessageId.h"

namespace tworldsvr::senders {

boost::asio::awaitable<void>
SendMwCorpsAskReq(std::shared_ptr<PeerSession> peer,
                  std::uint32_t                char_id,
                  std::uint32_t                key,
                  std::uint32_t                inviter_char_id,
                  const std::string&           inviter_name)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint32_t>(body, inviter_char_id);
    WriteString(body, inviter_name);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_CORPSASK_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwCorpsReplyReq(std::shared_ptr<PeerSession> peer,
                    std::uint32_t                char_id,
                    std::uint32_t                key,
                    std::uint8_t                 result,
                    const std::string&           name)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint8_t>(body, result);
    WriteString(body, name);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_CORPSREPLY_REQ),
        std::move(body));
}

} // namespace tworldsvr::senders
