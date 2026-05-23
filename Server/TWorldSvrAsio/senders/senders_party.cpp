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

} // namespace tworldsvr::senders
