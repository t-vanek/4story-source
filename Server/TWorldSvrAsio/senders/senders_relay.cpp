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

} // namespace tworldsvr::senders
