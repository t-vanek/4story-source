#include "senders/senders.h"
#include "wire_codec.h"

#include "MessageId.h"

namespace tworldsvr::senders {

boost::asio::awaitable<void>
SendMwRouteListReq(std::shared_ptr<PeerSession>     peer,
                   std::uint32_t                    char_id,
                   std::uint32_t                    key,
                   const std::vector<std::uint8_t>& server_ids)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint8_t>(body, static_cast<std::uint8_t>(server_ids.size()));
    for (std::uint8_t sid : server_ids)
        WritePOD<std::uint8_t>(body, sid);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_ROUTELIST_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwCheckMainReq(std::shared_ptr<PeerSession> peer,
                   std::uint32_t                char_id,
                   std::uint32_t                key,
                   std::uint8_t                 channel,
                   std::uint16_t                map_id,
                   float                        pos_x,
                   float                        pos_y,
                   float                        pos_z)
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
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_CHECKMAIN_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwDelCharReq(std::shared_ptr<PeerSession> peer,
                 std::uint32_t                char_id,
                 std::uint32_t                key,
                 std::uint8_t                 logout,
                 std::uint8_t                 save)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint8_t>(body, logout);
    WritePOD<std::uint8_t>(body, save);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_DELCHAR_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwInvalidCharReq(std::shared_ptr<PeerSession> peer,
                     std::uint32_t                char_id,
                     std::uint32_t                key,
                     std::uint8_t                 release_main)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint8_t>(body, release_main);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_INVALIDCHAR_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwConResultReq(std::shared_ptr<PeerSession>     peer,
                   std::uint32_t                    char_id,
                   std::uint32_t                    key,
                   std::uint8_t                     result,
                   const std::vector<std::uint8_t>& server_ids)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint8_t>(body, result);
    WritePOD<std::uint8_t>(body, static_cast<std::uint8_t>(server_ids.size()));
    for (std::uint8_t sid : server_ids)
        WritePOD<std::uint8_t>(body, sid);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_CONRESULT_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwCloseCharReq(std::shared_ptr<PeerSession> peer,
                   std::uint32_t                char_id,
                   std::uint32_t                key)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_CLOSECHAR_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwReleaseMainReq(std::shared_ptr<PeerSession> peer,
                     std::uint32_t                char_id,
                     std::uint32_t                key,
                     std::uint8_t                 channel,
                     std::uint16_t                map_id,
                     float                        pos_x,
                     float                        pos_y,
                     float                        pos_z)
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
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_RELEASEMAIN_REQ),
        std::move(body));
}

} // namespace tworldsvr::senders
