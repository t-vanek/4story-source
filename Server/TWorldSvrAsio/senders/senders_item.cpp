#include "senders/senders.h"
#include "wire_codec.h"

#include "MessageId.h"

namespace tworldsvr::senders {

boost::asio::awaitable<void>
SendMwDealItemErrorReq(std::shared_ptr<PeerSession> peer,
                       const std::string&           target,
                       const std::string&           error_char,
                       std::uint8_t                 error)
{
    using namespace wire;
    std::vector<std::byte> body;
    WriteString(body, target);
    WriteString(body, error_char);
    WritePOD<std::uint8_t>(body, error);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_DEALITEMERROR_REQ),
        std::move(body));
}

} // namespace tworldsvr::senders
