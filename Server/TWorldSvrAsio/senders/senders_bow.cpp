#include "senders/senders.h"
#include "wire_codec.h"

#include "MessageId.h"

namespace tworldsvr::senders {

boost::asio::awaitable<void>
SendMwAddToBowQueueAck(std::shared_ptr<PeerSession> peer,
                       std::uint8_t                 result,
                       std::uint32_t                char_id,
                       std::uint32_t                key,
                       std::uint32_t                tick)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint8_t> (body, result);
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint32_t>(body, tick);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_ADDTOBOWQUEUE_ACK),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwCancelBowQueueAck(std::shared_ptr<PeerSession> peer,
                        std::uint8_t                 result,
                        std::uint32_t                char_id,
                        std::uint32_t                key,
                        std::uint32_t                tick)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint8_t> (body, result);
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint32_t>(body, tick);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_CANCELBOWQUEUE_ACK),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwBattleModeStatusAck(std::shared_ptr<PeerSession> peer,
                          std::uint32_t                char_id,
                          std::uint32_t                key,
                          std::uint8_t                 bow_status,
                          std::uint32_t                bow_start,
                          std::uint8_t                 bow_winner,
                          std::uint8_t                 br_status,
                          std::uint32_t                br_start,
                          std::uint8_t                 br_type)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint8_t> (body, bow_status);
    WritePOD<std::uint32_t>(body, bow_start);
    WritePOD<std::uint8_t> (body, bow_winner);
    WritePOD<std::uint8_t> (body, br_status);
    WritePOD<std::uint32_t>(body, br_start);
    WritePOD<std::uint8_t> (body, br_type);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_BATTLEMODESTATUS_ACK),
        std::move(body));
}

} // namespace tworldsvr::senders
