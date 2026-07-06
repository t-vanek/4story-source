// W6-50 tournament senders.

#include "senders.h"

#include "../wire_codec.h"

#include "MessageId.h"

namespace tworldsvr::senders {

boost::asio::awaitable<void>
SendMwTournamentReq(std::shared_ptr<PeerSession> peer,
                    std::vector<std::byte>       body)
{
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_TOURNAMENT_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwTournamentEnableReq(std::shared_ptr<PeerSession> peer,
                          std::uint8_t                 group,
                          std::uint8_t                 step,
                          std::uint32_t                period,
                          std::int64_t                 next_step_start)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint8_t>(body, group);
    WritePOD<std::uint8_t>(body, step);
    WritePOD<std::uint32_t>(body, period);
    WritePOD<std::int64_t>(body, next_step_start);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_TOURNAMENTENABLE_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwTournamentMatchReq(
    std::shared_ptr<PeerSession>                              peer,
    const std::vector<TournamentRegistry::MatchBroadcastRow>& rows)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint8_t>(body,
        static_cast<std::uint8_t>(rows.size()));
    for (const auto& r : rows)
    {
        WritePOD<std::uint8_t>(body, r.entry_id);
        WritePOD<std::uint8_t>(body, r.slot_id);
        WritePOD<std::uint32_t>(body, r.row.char_id);
        WritePOD<std::uint8_t>(body, r.row.country);
        WriteString(body, r.row.name);
        WritePOD<std::uint8_t>(body, r.row.level);
        WritePOD<std::uint8_t>(body, r.row.cls);
        WritePOD<std::uint32_t>(body, r.chief_id);
        WritePOD<std::uint8_t>(body, r.result[0]);
        WritePOD<std::uint8_t>(body, r.result[1]);
        WritePOD<std::uint8_t>(body, r.result[2]);
    }
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_TOURNAMENTMATCH_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwTournamentResultReq(std::shared_ptr<PeerSession>       peer,
                          std::uint16_t                      tournament_id,
                          std::uint8_t                       step,
                          std::uint8_t                       ret,
                          std::uint32_t                      win_id,
                          std::uint32_t                      lose_id,
                          std::uint32_t                      blue_hide_tick,
                          std::uint32_t                      red_hide_tick,
                          const std::vector<std::uint32_t>&  party_members)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint16_t>(body, tournament_id);
    WritePOD<std::uint8_t>(body, step);
    WritePOD<std::uint8_t>(body, ret);
    WritePOD<std::uint32_t>(body, win_id);
    WritePOD<std::uint32_t>(body, lose_id);
    WritePOD<std::uint32_t>(body, blue_hide_tick);
    WritePOD<std::uint32_t>(body, red_hide_tick);
    WritePOD<std::uint8_t>(body,
        static_cast<std::uint8_t>(party_members.size()));
    for (const std::uint32_t id : party_members)
        WritePOD<std::uint32_t>(body, id);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_TOURNAMENTRESULT_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwTournamentBatPointReq(std::shared_ptr<PeerSession> peer,
                            std::uint32_t                char_id,
                            const std::string&           name,
                            std::uint32_t                amount)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WriteString(body, name);
    WritePOD<std::uint32_t>(body, amount);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_TOURNAMENTBATPOINT_REQ),
        std::move(body));
}

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
