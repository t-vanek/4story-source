// W6-50 tournament senders.

#include "senders.h"

#include "../wire_codec.h"

#include "MessageId.h"

namespace tworldsvr::senders {

boost::asio::awaitable<void>
SendMwTournamentInfoReq(std::shared_ptr<PeerSession>            peer,
                        const TournamentRegistry::InfoSnapshot& info)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint8_t>(body, info.first_group_count);
    WritePOD<std::uint8_t>(body, info.group);
    WritePOD<std::uint8_t>(body, info.step);
    WritePOD<std::uint8_t>(body,
        static_cast<std::uint8_t>(info.entries.size()));
    for (const auto& e : info.entries)
    {
        WritePOD<std::uint8_t>(body, e.group);
        WritePOD<std::uint8_t>(body, e.entry_id);
        WriteString(body, e.name);
        WritePOD<std::uint8_t>(body, e.type);
        WritePOD<std::uint32_t>(body, e.cls);
        WritePOD<std::uint32_t>(body, e.fee);
        WritePOD<std::uint32_t>(body, e.fee_back);
        WritePOD<std::uint16_t>(body, e.permit_item_id);
        WritePOD<std::uint8_t>(body, e.permit_count);
        WritePOD<std::uint8_t>(body, e.min_level);
        WritePOD<std::uint8_t>(body, e.max_level);
        WritePOD<std::uint8_t>(body,
            static_cast<std::uint8_t>(e.rewards.size()));
        for (const auto& w : e.rewards)
        {
            WritePOD<std::uint8_t>(body, w.chart_type);
            WritePOD<std::uint16_t>(body, w.item_id);
            WritePOD<std::uint8_t>(body, w.count);
        }
    }
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_TOURNAMENTINFO_REQ),
        std::move(body));
}

} // namespace tworldsvr::senders
