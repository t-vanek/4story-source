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

// ---- W6-54 scheduler senders ---------------------------------------

boost::asio::awaitable<void>
SendMwBowTimeUpdateAck(std::shared_ptr<PeerSession> peer,
                       std::uint8_t status, std::uint32_t second,
                       std::uint8_t points_d, std::uint8_t points_c)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint8_t>(body, status);
    WritePOD<std::uint32_t>(body, second);
    WritePOD<std::uint8_t>(body, points_d);
    WritePOD<std::uint8_t>(body, points_c);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_BOWTIMEUPDATE_ACK),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwBowCommandExecReq(std::shared_ptr<PeerSession> peer,
                        std::uint8_t command)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint8_t>(body, command);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_BOWCOMMANDEXEC_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwAddBowPlayersAck(std::shared_ptr<PeerSession>   peer,
                       const std::vector<TBowPlayer>& roster)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint8_t>(body,
        static_cast<std::uint8_t>(roster.size()));
    for (const auto& p : roster)
    {
        WritePOD<std::uint32_t>(body, p.char_id);
        WritePOD<std::uint32_t>(body, p.key);
        WritePOD<std::uint8_t>(body, p.country);
    }
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_ADDBOWPLAYERS_ACK),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwPrepareForBowReq(std::shared_ptr<PeerSession> peer,
                       std::uint32_t char_id, std::uint32_t key,
                       std::uint8_t team)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint8_t>(body, team);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_PREPAREFORBOW_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwEndBowWarReq(std::shared_ptr<PeerSession>   peer,
                   std::uint8_t                   winner,
                   const std::vector<TBowPlayer>& roster)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint8_t>(body, winner);
    WritePOD<std::uint8_t>(body,
        static_cast<std::uint8_t>(roster.size()));
    for (const auto& p : roster)
    {
        WritePOD<std::uint32_t>(body, p.char_id);
        WritePOD<std::uint32_t>(body, p.key);
    }
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_ENDBOWWAR_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwReleaseSingleBowPlayerReq(std::shared_ptr<PeerSession> peer,
                                std::uint32_t char_id,
                                std::uint32_t key,
                                std::uint8_t winner)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint8_t>(body, winner);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_RELEASESINGLEBOWPLAYER_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwNotifyNonQueuedPlayerAck(std::shared_ptr<PeerSession> peer,
                               std::uint32_t char_id,
                               std::uint32_t key)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_NOTIFYNONQUEUEDPLAYER_ACK),
        std::move(body));
}

} // namespace tworldsvr::senders
