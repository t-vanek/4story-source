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

boost::asio::awaitable<void>
SendMwCastleApplyReq(std::shared_ptr<PeerSession> peer,
                     std::uint32_t                char_id,
                     std::uint32_t                key,
                     std::uint8_t                 result,
                     std::uint16_t                castle_id,
                     std::uint32_t                target,
                     std::uint8_t                 camp)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint8_t>(body, result);
    WritePOD<std::uint16_t>(body, castle_id);
    WritePOD<std::uint32_t>(body, target);
    WritePOD<std::uint8_t>(body, camp);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_CASTLEAPPLY_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwCastleApplicantCountReq(std::shared_ptr<PeerSession> peer,
                              std::uint16_t                castle_id,
                              std::uint32_t                guild_id,
                              std::uint16_t                count)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint16_t>(body, castle_id);
    WritePOD<std::uint32_t>(body, guild_id);
    WritePOD<std::uint8_t>(body, static_cast<std::uint8_t>((count >> 8) & 0xFF));
    WritePOD<std::uint8_t>(body, static_cast<std::uint8_t>(count & 0xFF));
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_CASTLEAPPLICANTCOUNT_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwLocalEnableReq(std::shared_ptr<PeerSession> peer,
                     std::uint8_t                 status,
                     std::uint32_t                second,
                     std::uint32_t                local_start,
                     std::uint8_t                 castle_day,
                     std::uint32_t                castle_start)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint8_t>(body, status);
    WritePOD<std::uint32_t>(body, second);
    WritePOD<std::uint32_t>(body, local_start);
    WritePOD<std::uint8_t>(body, castle_day);
    WritePOD<std::uint32_t>(body, castle_start);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_LOCALENABLE_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwCastleEnableReq(std::shared_ptr<PeerSession> peer,
                      std::uint8_t                 status,
                      std::uint32_t                second)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint8_t>(body, status);
    WritePOD<std::uint32_t>(body, second);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_CASTLEENABLE_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwMissionEnableReq(std::shared_ptr<PeerSession> peer,
                       std::uint8_t                 status,
                       std::uint32_t                start,
                       std::uint32_t                second)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint8_t>(body, status);
    WritePOD<std::uint32_t>(body, start);
    WritePOD<std::uint32_t>(body, second);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_MISSIONENABLE_REQ),
        std::move(body));
}

} // namespace tworldsvr::senders
