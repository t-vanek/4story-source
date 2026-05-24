#include "senders/senders.h"
#include "wire_codec.h"

#include "MessageId.h"

namespace tworldsvr::senders {

boost::asio::awaitable<void>
SendMwCorpsAskReq(std::shared_ptr<PeerSession> peer,
                  std::uint32_t                char_id,
                  std::uint32_t                key,
                  std::uint32_t                inviter_char_id,
                  const std::string&           inviter_name)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint32_t>(body, inviter_char_id);
    WriteString(body, inviter_name);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_CORPSASK_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwCorpsReplyReq(std::shared_ptr<PeerSession> peer,
                    std::uint32_t                char_id,
                    std::uint32_t                key,
                    std::uint8_t                 result,
                    const std::string&           name)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint8_t>(body, result);
    WriteString(body, name);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_CORPSREPLY_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwAddSquadReq(std::shared_ptr<PeerSession>        peer,
                  std::uint32_t                       recipient_char_id,
                  std::uint32_t                       recipient_key,
                  std::uint32_t                       chief_id,
                  std::uint16_t                       party_id,
                  const std::vector<SquadMemberInfo>& members)
{
    using namespace wire;
    constexpr std::uint16_t kMoveNone = 1800; // NetCode.h:34
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, recipient_char_id);
    WritePOD<std::uint32_t>(body, recipient_key);
    WritePOD<std::uint32_t>(body, chief_id);
    WritePOD<std::uint16_t>(body, party_id);
    WritePOD<std::uint8_t>(body, static_cast<std::uint8_t>(members.size()));
    for (const auto& m : members)
    {
        WritePOD<std::uint32_t>(body, m.char_id);
        WriteString(body, m.name);
        WritePOD<float>(body, 1.0f);
        WritePOD<std::uint32_t>(body, 0);          // m_command.m_dwTgObjID
        WritePOD<std::uint32_t>(body, m.max_hp);
        WritePOD<std::uint32_t>(body, m.hp);
        WritePOD<std::uint16_t>(body, 0);          // m_command.m_wTgPosX
        WritePOD<std::uint16_t>(body, 0);          // m_command.m_wTgPosZ
        WritePOD<std::uint16_t>(body, m.map_id);
        WritePOD<std::uint16_t>(body, m.pos_x);
        WritePOD<std::uint16_t>(body, m.pos_z);
        WritePOD<std::uint16_t>(body, kMoveNone);
        WritePOD<std::uint8_t>(body, 0);           // m_command.m_bTgType
        WritePOD<std::uint8_t>(body, m.level);
        WritePOD<std::uint8_t>(body, m.klass);
        WritePOD<std::uint8_t>(body, m.race);
        WritePOD<std::uint8_t>(body, m.sex);
        WritePOD<std::uint8_t>(body, m.face);
        WritePOD<std::uint8_t>(body, m.hair);
        WritePOD<std::uint8_t>(body, 0);           // m_command.m_bCommand
    }
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_ADDSQUAD_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwCorpsJoinReq(std::shared_ptr<PeerSession> peer,
                   std::uint32_t                char_id,
                   std::uint32_t                key,
                   std::uint16_t                corps_id,
                   std::uint16_t                commander_party_id)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint16_t>(body, corps_id);
    WritePOD<std::uint16_t>(body, commander_party_id);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_CORPSJOIN_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwDelSquadReq(std::shared_ptr<PeerSession> peer,
                  std::uint32_t                char_id,
                  std::uint32_t                key,
                  std::uint16_t                squad_party_id)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint16_t>(body, squad_party_id);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_DELSQUAD_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwChgCorpsCommanderReq(std::shared_ptr<PeerSession> peer,
                           std::uint32_t                char_id,
                           std::uint32_t                key,
                           std::uint8_t                 result)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint8_t>(body, result);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_CHGCORPSCOMMANDER_REQ),
        std::move(body));
}

} // namespace tworldsvr::senders
