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

boost::asio::awaitable<void>
SendMwEndWarReq(std::shared_ptr<PeerSession> peer, std::uint16_t castle)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint16_t>(body, castle);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_ENDWAR_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwSkyGardenOccupyReq(std::shared_ptr<PeerSession> peer,
                         std::uint8_t type, std::uint16_t id,
                         std::uint8_t country)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint8_t>(body, type);
    WritePOD<std::uint16_t>(body, id);
    WritePOD<std::uint8_t>(body, country);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_SKYGARDENOCCUPY_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwWarCountryBalanceReq(std::shared_ptr<PeerSession> peer,
                           std::uint32_t char_id, std::uint32_t key,
                           std::uint32_t count_d, std::uint32_t count_c,
                           std::uint8_t gap)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint32_t>(body, count_d);
    WritePOD<std::uint32_t>(body, count_c);
    WritePOD<std::uint8_t>(body, gap);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_WARCOUNTRYBALANCE_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwCastleGuildChgReq(std::shared_ptr<PeerSession> peer,
                        std::uint16_t castle, std::uint32_t def_id,
                        const std::string& def_name,
                        std::uint32_t atk_id,
                        const std::string& atk_name, std::int64_t when)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint16_t>(body, castle);
    WritePOD<std::uint32_t>(body, def_id);
    WriteString(body, def_name);
    WritePOD<std::uint32_t>(body, atk_id);
    WriteString(body, atk_name);
    WritePOD<std::int64_t>(body, when);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_CASTLEGUILDCHG_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendCtCastleGuildChgAck(std::shared_ptr<PeerSession> peer,
                        std::uint32_t manager, std::uint8_t ret,
                        std::uint16_t castle, std::uint32_t def_id,
                        const std::string& def_name,
                        std::uint32_t atk_id,
                        const std::string& atk_name, std::int64_t when)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, manager);
    WritePOD<std::uint8_t>(body, ret);
    WritePOD<std::uint16_t>(body, castle);
    WritePOD<std::uint32_t>(body, def_id);
    WriteString(body, def_name);
    WritePOD<std::uint32_t>(body, atk_id);
    WriteString(body, atk_name);
    WritePOD<std::int64_t>(body, when);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::CT_CASTLEGUILDCHG_ACK),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwCastleWarInfoReq(std::shared_ptr<PeerSession>     peer,
                       const CastleWarBroadcastRow&     row)
{
    using namespace wire;
    const auto& c = row.info;
    std::vector<std::byte> body;
    WritePOD<std::uint16_t>(body, c.id);
    WritePOD<std::uint32_t>(body, row.def_id);
    WriteString(body, row.def_name);
    WritePOD<std::uint8_t>(body, c.def_country);
    WritePOD<std::uint16_t>(body, c.country_point[0]);
    WritePOD<std::uint32_t>(body, row.atk_id);
    WriteString(body, row.atk_name);
    WritePOD<std::uint16_t>(body, c.country_point[1]);
    WritePOD<std::uint32_t>(body,
        static_cast<std::uint32_t>(c.guild_points.size()));
    for (const auto& [gid, points] : c.guild_points)
    {
        WritePOD<std::uint32_t>(body, gid);
        WritePOD<std::uint32_t>(body, points);
    }
    WritePOD<std::uint8_t>(body, static_cast<std::uint8_t>(
        c.top3[0].size() + c.top3[1].size()));
    for (const auto& [rank, entry] : c.top3[0])
    {
        WritePOD<std::uint8_t>(body, 0);
        WriteString(body, entry.name);
        WritePOD<std::uint16_t>(body, entry.point);
    }
    for (const auto& [rank, entry] : c.top3[1])
    {
        WritePOD<std::uint8_t>(body, 1);
        WriteString(body, entry.name);
        WritePOD<std::uint16_t>(body, entry.point);
    }
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_CASTLEWARINFO_REQ),
        std::move(body));
}

} // namespace tworldsvr::senders
