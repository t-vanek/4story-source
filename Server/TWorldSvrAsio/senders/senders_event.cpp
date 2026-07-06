#include "senders/senders.h"
#include "wire_codec.h"
#include "services/lucky_event_codec.h"

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

boost::asio::awaitable<void>
SendCtEventQuarterListAck(std::shared_ptr<PeerSession>     peer,
                          std::uint32_t                    manager,
                          const std::vector<LuckyEvent>&   rows)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, manager);
    WritePOD<std::uint16_t>(body,
        static_cast<std::uint16_t>(rows.size()));
    for (const auto& e : rows)
        WriteLuckyEvent(body, e);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::CT_EVENTQUARTERLIST_ACK),
        std::move(body));
}

boost::asio::awaitable<void>
SendCtEventQuarterUpdateAck(std::shared_ptr<PeerSession>     peer,
                            std::uint8_t                     ret,
                            std::uint32_t                    manager,
                            std::uint8_t                     type,
                            const LuckyEvent&                event)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint8_t>(body, ret);
    WritePOD<std::uint32_t>(body, manager);
    WritePOD<std::uint8_t>(body, type);
    WriteLuckyEvent(body, event);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::CT_EVENTQUARTERUPDATE_ACK),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwWorldPostLotItemReq(std::shared_ptr<PeerSession>   peer,
                          std::uint8_t                   type,
                          std::uint32_t                  recv_id,
                          const std::string&             recver,
                          const std::string&             title,
                          const std::string&             message,
                          std::uint16_t                  item_id,
                          std::uint8_t                   item_num,
                          std::uint16_t                  use_time)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint8_t>(body, type);
    WritePOD<std::uint32_t>(body, recv_id);
    WriteString(body, recver);
    WriteString(body, title);
    WriteString(body, message);
    WritePOD<std::uint16_t>(body, item_id);
    WritePOD<std::uint8_t>(body, item_num);
    WritePOD<std::uint16_t>(body, use_time);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_WORLDPOSTSEND_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwEventMsgLotteryReq(std::shared_ptr<PeerSession>         peer,
                         const std::string&                   event_title,
                         const std::vector<LotteryBoardRow>&  rows)
{
    using namespace wire;
    std::vector<std::byte> body;
    WriteString(body, event_title);
    WritePOD<std::uint16_t>(body,
        static_cast<std::uint16_t>(rows.size()));
    for (const auto& r : rows)
    {
        WritePOD<std::uint16_t>(body, r.item_id);
        WritePOD<std::uint8_t>(body, r.num);
        WritePOD<std::uint16_t>(body,
            static_cast<std::uint16_t>(r.names.size()));
        for (const auto& n : r.names)
            WriteString(body, n);
    }
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_EVENTMSGLOTTERY_REQ),
        std::move(body));
}

} // namespace tworldsvr::senders
