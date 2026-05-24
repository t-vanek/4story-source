#include "senders/senders.h"
#include "wire_codec.h"

#include "MessageId.h"

namespace tworldsvr::senders {

boost::asio::awaitable<void>
SendMwEventQuarterReq(std::shared_ptr<PeerSession> peer,
                      std::uint8_t                 day,
                      std::uint8_t                 hour,
                      std::uint8_t                 minute,
                      std::uint8_t                 select,
                      const std::string&           present)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint8_t>(body, day);
    WritePOD<std::uint8_t>(body, hour);
    WritePOD<std::uint8_t>(body, minute);
    WritePOD<std::uint8_t>(body, select);
    WriteString(body, present);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_EVENTQUARTER_REQ),
        std::move(body));
}

} // namespace tworldsvr::senders
