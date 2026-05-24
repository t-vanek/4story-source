#include "senders/senders.h"
#include "wire_codec.h"

#include "MessageId.h"

namespace tworldsvr::senders {

boost::asio::awaitable<void>
SendMwFameRankUpdateReq(std::shared_ptr<PeerSession>  peer,
                        const std::vector<std::byte>& body)
{
    std::vector<std::byte> copy(body);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_FAMERANKUPDATE_REQ),
        std::move(copy));
}

boost::asio::awaitable<void>
SendMwHeroSelectReq(std::shared_ptr<PeerSession> peer,
                    std::uint16_t                battle_zone,
                    const std::string&           hero_name,
                    std::int64_t                 time_hero)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint16_t>(body, battle_zone);
    WriteString(body, hero_name);
    WritePOD<std::int64_t>(body, time_hero);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_HEROSELECT_REQ),
        std::move(body));
}

} // namespace tworldsvr::senders
