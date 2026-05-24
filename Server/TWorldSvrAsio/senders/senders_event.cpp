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

boost::asio::awaitable<void>
SendMwEventMsgReq(std::shared_ptr<PeerSession> peer,
                  std::uint8_t                 event_id,
                  std::uint8_t                 msg_type,
                  const std::string&           msg)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint8_t>(body, event_id);
    WritePOD<std::uint8_t>(body, msg_type);
    WriteString(body, msg);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_EVENTMSG_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwEventUpdateReq(std::shared_ptr<PeerSession>   peer,
                     std::uint8_t                   event_id,
                     std::uint16_t                  value,
                     const std::vector<std::byte>&  event_body)
{
    using namespace wire;
    std::vector<std::byte> body;
    body.reserve(sizeof(std::uint8_t) + sizeof(std::uint16_t)
        + event_body.size());
    WritePOD<std::uint8_t>(body, event_id);
    WritePOD<std::uint16_t>(body, value);
    body.insert(body.end(), event_body.begin(), event_body.end());
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_EVENTUPDATE_REQ),
        std::move(body));
}

} // namespace tworldsvr::senders
