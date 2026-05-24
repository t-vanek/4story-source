#include "senders/senders.h"
#include "wire_codec.h"

#include "MessageId.h"

namespace tworldsvr::senders {

boost::asio::awaitable<void>
SendMwMagicMirrorReq(std::shared_ptr<PeerSession>  peer,
                     const std::vector<std::byte>& body)
{
    std::vector<std::byte> copy(body);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_MAGICMIRROR_REQ),
        std::move(copy));
}

boost::asio::awaitable<void>
SendMwMonTemptReq(std::shared_ptr<PeerSession> peer,
                  std::uint32_t                char_id,
                  std::uint32_t                key,
                  std::uint16_t                mon_id)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint16_t>(body, mon_id);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_MONTEMPT_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwMonTemptEvoReq(std::shared_ptr<PeerSession> peer,
                     std::uint32_t                char_id,
                     std::uint32_t                key,
                     std::uint32_t                host_id,
                     std::uint8_t                 host_type)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint32_t>(body, host_id);
    WritePOD<std::uint8_t>(body, host_type);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_MONTEMPTEVO_REQ),
        std::move(body));
}

} // namespace tworldsvr::senders
