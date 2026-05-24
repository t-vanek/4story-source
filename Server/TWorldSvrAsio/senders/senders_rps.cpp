#include "senders/senders.h"
#include "wire_codec.h"

#include "MessageId.h"

namespace tworldsvr::senders {

boost::asio::awaitable<void>
SendMwRpsGameReq(std::shared_ptr<PeerSession> peer,
                 std::uint32_t                char_id,
                 std::uint32_t                key,
                 std::uint8_t                 result,
                 std::uint8_t                 player_rps)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint8_t> (body, result);
    WritePOD<std::uint8_t> (body, player_rps);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_RPSGAME_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwRpsGameChangeReq(std::shared_ptr<PeerSession>   peer,
                       const std::vector<std::byte>&  body)
{
    // Legacy SSSender.cpp:3649 copies the inbound CT_RPSGAMECHANGE_REQ
    // packet whole and re-tags the message id. We forward the body
    // verbatim — both packets emit the same row layout.
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_RPSGAMECHANGE_REQ),
        body);
}

boost::asio::awaitable<void>
SendCtRpsGameDataAck(std::shared_ptr<PeerSession>      peer,
                     std::uint8_t                      change,
                     std::uint8_t                      group,
                     const std::vector<TRpsGame>&      games)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint8_t> (body, change);
    WritePOD<std::uint8_t> (body, group);
    WritePOD<std::uint16_t>(body, static_cast<std::uint16_t>(games.size()));
    for (const auto& g : games)
    {
        WritePOD<std::uint8_t> (body, g.type);
        WritePOD<std::uint8_t> (body, g.win_count);
        WritePOD<std::uint8_t> (body, g.win_prob);
        WritePOD<std::uint8_t> (body, g.draw_prob);
        WritePOD<std::uint8_t> (body, g.lose_prob);
        WritePOD<std::uint16_t>(body, g.win_keep);
        WritePOD<std::uint16_t>(body, g.win_period);
    }
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::CT_RPSGAMEDATA_ACK),
        std::move(body));
}

} // namespace tworldsvr::senders
