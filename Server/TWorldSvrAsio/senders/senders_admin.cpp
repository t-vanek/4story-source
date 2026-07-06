#include "senders/senders.h"
#include "wire_codec.h"

#include "MessageId.h"

namespace tworldsvr::senders {

boost::asio::awaitable<void>
SendMwUserPositionReq(std::shared_ptr<PeerSession> peer,
                      std::uint32_t                char_id,
                      std::uint32_t                key,
                      const std::string&           gm_name)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WriteString(body, gm_name);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_USERPOSITION_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendCtUserMoveAck(std::shared_ptr<PeerSession> peer,
                  std::uint32_t                char_id,
                  std::uint32_t                key,
                  std::uint8_t                 channel,
                  std::uint16_t                map_id,
                  float                        pos_x,
                  float                        pos_y,
                  float                        pos_z,
                  std::uint16_t                party_id)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint8_t>(body, channel);
    WritePOD<std::uint16_t>(body, map_id);
    WritePOD<float>(body, pos_x);
    WritePOD<float>(body, pos_y);
    WritePOD<float>(body, pos_z);
    WritePOD<std::uint16_t>(body, party_id);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::CT_USERMOVE_ACK),
        std::move(body));
}

boost::asio::awaitable<void>
SendCtServiceMonitorReq(std::shared_ptr<PeerSession>   peer,
                        std::uint32_t                  tick,
                        std::uint32_t                  sessions,
                        std::uint32_t                  chars,
                        std::uint32_t                  active_users)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, tick);
    WritePOD<std::uint32_t>(body, sessions);
    WritePOD<std::uint32_t>(body, chars);
    WritePOD<std::uint32_t>(body, active_users);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::CT_SERVICEMONITOR_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwHelpMessageReq(std::shared_ptr<PeerSession>   peer,
                     std::uint8_t                   id,
                     std::int64_t                   start_unix,
                     std::int64_t                   end_unix,
                     const std::string&             message)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint8_t>(body, id);
    WritePOD<std::int64_t>(body, start_unix);
    WritePOD<std::int64_t>(body, end_unix);
    WriteString(body, message);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_HELPMESSAGE_REQ),
        std::move(body));
}

} // namespace tworldsvr::senders
