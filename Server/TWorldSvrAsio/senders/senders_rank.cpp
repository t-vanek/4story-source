#include "senders/senders.h"
#include "wire_codec.h"
#include "services/month_rank_codec.h"

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

boost::asio::awaitable<void>
SendMwMonthRankListReq(std::shared_ptr<PeerSession>   peer,
                       std::uint8_t                   rank_month,
                       const MonthRankRegistry&       registry)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint8_t>(body, rank_month);
    WritePOD<std::uint8_t>(body,
        static_cast<std::uint8_t>(kMonthRankCount));
    for (std::uint8_t c = 0; c < kMonthRankCountryCount; ++c)
    {
        const auto row = registry.SnapshotCountry(c);
        for (const auto& ranker : row)
            WriteMonthRanker(body, ranker);
    }
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_MONTHRANKLIST_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwMonthRankUpdateReq(std::shared_ptr<PeerSession>     peer,
                         std::uint8_t                     month,
                         std::uint8_t                     country,
                         std::uint8_t                     start_rank,
                         std::uint8_t                     end_rank,
                         const std::vector<MonthRanker>&  slots,
                         bool                             new_warlord,
                         const MonthRanker&               warlord)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint8_t>(body, month);
    WritePOD<std::uint8_t>(body, country);
    WritePOD<std::uint8_t>(body, start_rank);
    WritePOD<std::uint8_t>(body, end_rank);
    if (start_rank && end_rank)
        for (const auto& ranker : slots)
            WriteMonthRanker(body, ranker);
    WritePOD<std::uint8_t>(body, new_warlord ? 1 : 0);
    if (new_warlord)
        WriteMonthRanker(body, warlord);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_MONTHRANKUPDATE_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwMonthRankResetCharReq(std::shared_ptr<PeerSession>   peer,
                            std::uint32_t                  char_id)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_MONTHRANKRESETCHAR_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwWarLordSayReq(std::shared_ptr<PeerSession>   peer,
                    std::uint8_t                   type,
                    std::uint8_t                   rank_month,
                    std::uint32_t                  char_id,
                    const std::string&             say)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint8_t>(body, type);
    WritePOD<std::uint8_t>(body, rank_month);
    WritePOD<std::uint32_t>(body, char_id);
    WriteString(body, say);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_WARLORDSAY_REQ),
        std::move(body));
}

} // namespace tworldsvr::senders
