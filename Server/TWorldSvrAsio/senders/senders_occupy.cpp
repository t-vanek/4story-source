#include "senders/senders.h"
#include "wire_codec.h"

#include "MessageId.h"

namespace tworldsvr::senders {

boost::asio::awaitable<void>
SendMwCastleOccupyReq(std::shared_ptr<PeerSession> peer,
                      std::uint8_t                 type,
                      std::uint16_t                castle_id,
                      std::uint32_t                guild_id,
                      std::uint8_t                 country,
                      const std::string&           guild_name)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint8_t>(body, type);
    WritePOD<std::uint16_t>(body, castle_id);
    WritePOD<std::uint32_t>(body, guild_id);
    WritePOD<std::uint8_t>(body, country);
    WriteString(body, guild_name);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_CASTLEOCCUPY_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwLocalOccupyReq(std::shared_ptr<PeerSession> peer,
                     std::uint8_t                 type,
                     std::uint16_t                local_id,
                     std::uint8_t                 country,
                     std::uint32_t                guild_id,
                     const std::string&           guild_name)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint8_t>(body, type);
    WritePOD<std::uint16_t>(body, local_id);
    WritePOD<std::uint8_t>(body, country);
    WritePOD<std::uint32_t>(body, guild_id);
    WriteString(body, guild_name);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_LOCALOCCUPY_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwMissionOccupyReq(std::shared_ptr<PeerSession> peer,
                       std::uint8_t                 type,
                       std::uint16_t                local_id,
                       std::uint8_t                 country)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint8_t>(body, type);
    WritePOD<std::uint16_t>(body, local_id);
    WritePOD<std::uint8_t>(body, country);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_MISSIONOCCUPY_REQ),
        std::move(body));
}

} // namespace tworldsvr::senders
