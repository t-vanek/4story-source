#include "senders/senders.h"
#include "wire_codec.h"

#include "MessageId.h"

namespace tworldsvr::senders {

boost::asio::awaitable<void>
SendMwGuildEstablishReq(std::shared_ptr<PeerSession> peer,
                        std::uint32_t                char_id,
                        std::uint32_t                key,
                        std::uint8_t                 result,
                        std::uint32_t                guild_id,
                        const std::string&           name,
                        std::uint8_t                 establish)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint8_t>(body, result);
    WritePOD<std::uint32_t>(body, guild_id);
    WriteString(body, name);
    WritePOD<std::uint8_t>(body, establish);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_GUILDESTABLISH_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwRelayconnectReq(std::shared_ptr<PeerSession> peer,
                      std::uint32_t                char_id,
                      std::uint8_t                 relay_on)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint8_t>(body, relay_on);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_RELAYCONNECT_REQ),
        std::move(body));
}

} // namespace tworldsvr::senders
