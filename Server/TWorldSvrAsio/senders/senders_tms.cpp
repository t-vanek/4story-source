#include "senders/senders.h"
#include "wire_codec.h"

#include "MessageId.h"

namespace tworldsvr::senders {

boost::asio::awaitable<void>
SendMwTmsRecvReq(std::shared_ptr<PeerSession> peer,
                 std::uint32_t                char_id,
                 std::uint32_t                key,
                 std::uint32_t                tms,
                 const std::string&           sender_name,
                 const std::string&           message)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint32_t>(body, tms);
    WriteString(body, sender_name);
    WriteString(body, message);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_TMSRECV_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwTmsInviteAskReq(std::shared_ptr<PeerSession> peer,
                      std::uint32_t                char_id,
                      std::uint32_t                key,
                      std::uint32_t                target_id,
                      std::uint32_t                target_key,
                      std::uint32_t                tms,
                      const std::string&           message)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint32_t>(body, target_id);
    WritePOD<std::uint32_t>(body, target_key);
    WritePOD<std::uint32_t>(body, tms);
    WriteString(body, message);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_TMSINVITEASK_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwTmsInviteReq(std::shared_ptr<PeerSession>      peer,
                   std::uint32_t                     char_id,
                   std::uint32_t                     key,
                   std::uint32_t                     inviter,
                   std::uint32_t                     tms,
                   const std::vector<TmsMemberInfo>& members)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint32_t>(body, inviter);
    WritePOD<std::uint32_t>(body, tms);
    WritePOD<std::uint8_t>(body, static_cast<std::uint8_t>(members.size()));
    for (const auto& m : members)
    {
        WritePOD<std::uint32_t>(body, m.char_id);
        WriteString(body, m.name);
        WritePOD<std::uint8_t>(body, m.klass);
        WritePOD<std::uint8_t>(body, m.level);
    }
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_TMSINVITE_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwTmsOutReq(std::shared_ptr<PeerSession> peer,
                std::uint32_t                char_id,
                std::uint32_t                key,
                std::uint32_t                tms,
                const std::string&           target_name)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint32_t>(body, tms);
    WriteString(body, target_name);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_TMSOUT_REQ),
        std::move(body));
}

} // namespace tworldsvr::senders
