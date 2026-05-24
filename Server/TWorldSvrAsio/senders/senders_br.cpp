#include "senders/senders.h"
#include "wire_codec.h"

#include "MessageId.h"

namespace tworldsvr::senders {

boost::asio::awaitable<void>
SendMwAddToBrQueueAck(std::shared_ptr<PeerSession> peer,
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
            tnetlib::protocol::MessageId::MW_ADDTOBRQUEUE_ACK),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwBrTeamMateAddAck(std::shared_ptr<PeerSession> peer,
                       std::uint8_t                 result,
                       std::uint32_t                char_id,
                       std::uint32_t                key,
                       const std::string&           name)
{
    using namespace wire;
    std::vector<std::byte> body;
    // Legacy SSSender.cpp:3814 emits result FIRST, then the
    // (char_id, key, name) triple — distinct from the BOW family
    // shape which has the recipient ids first.
    WritePOD<std::uint8_t> (body, result);
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WriteString            (body, name);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_BRTEAMMATEADD_ACK),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwUpdateBrTeamAck(std::shared_ptr<PeerSession>          peer,
                      std::uint32_t                         char_id,
                      std::uint32_t                         key,
                      const std::string&                    chief_name,
                      std::uint8_t                          team_ready,
                      const std::vector<UpdateBrTeamRow>&   members)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WriteString            (body, chief_name);
    WritePOD<std::uint8_t> (body, team_ready);
    WritePOD<std::uint8_t> (body,
        static_cast<std::uint8_t>(members.size()));
    for (const auto& m : members)
    {
        WritePOD<std::uint32_t>(body, m.char_id);
        WriteString            (body, m.name);
        WritePOD<std::uint8_t> (body, m.ready);
    }
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_UPDATEBRTEAM_ACK),
        std::move(body));
}

} // namespace tworldsvr::senders
