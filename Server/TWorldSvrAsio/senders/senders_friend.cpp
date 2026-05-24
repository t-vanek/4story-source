#include "senders/senders.h"
#include "wire_codec.h"

#include "MessageId.h"

namespace tworldsvr::senders {

boost::asio::awaitable<void>
SendMwFriendAddReq(std::shared_ptr<PeerSession> peer,
                   std::uint32_t                char_id,
                   std::uint32_t                key,
                   std::uint8_t                 result,
                   std::uint32_t                friend_id,
                   const std::string&           name,
                   std::uint8_t                 level,
                   std::uint8_t                 group,
                   std::uint8_t                 klass,
                   std::uint32_t                region)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint8_t>(body, result);
    WritePOD<std::uint32_t>(body, friend_id);
    WriteString(body, name);
    WritePOD<std::uint8_t>(body, level);
    WritePOD<std::uint8_t>(body, group);
    WritePOD<std::uint8_t>(body, klass);
    WritePOD<std::uint32_t>(body, region);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_FRIENDADD_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwFriendAskReq(std::shared_ptr<PeerSession> peer,
                   std::uint32_t                char_id,
                   std::uint32_t                key,
                   const std::string&           inviter_name,
                   std::uint32_t                inviter_id)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WriteString(body, inviter_name);
    WritePOD<std::uint32_t>(body, inviter_id);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_FRIENDASK_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwFriendConnectionReq(std::shared_ptr<PeerSession> peer,
                          std::uint32_t                char_id,
                          std::uint32_t                key,
                          std::uint8_t                 result,
                          const std::string&           name,
                          std::uint32_t                region)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint8_t>(body, result);
    WriteString(body, name);
    WritePOD<std::uint32_t>(body, region);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_FRIENDCONNECTION_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwFriendEraseReq(std::shared_ptr<PeerSession> peer,
                     std::uint32_t                char_id,
                     std::uint32_t                key,
                     std::uint8_t                 result,
                     std::uint32_t                target_id)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint8_t>(body, result);
    WritePOD<std::uint32_t>(body, target_id);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_FRIENDERASE_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwFriendListReq(std::shared_ptr<PeerSession>                             peer,
                    std::uint32_t                                            char_id,
                    std::uint32_t                                            key,
                    std::uint32_t                                            soulmate_target,
                    const std::vector<std::pair<std::uint8_t, std::string>>& groups,
                    const std::vector<FriendListRow>&                        friends)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    // W4-23: the legacy soulmate slot — 0 when unpaired (NO soulmate).
    WritePOD<std::uint32_t>(body, soulmate_target);
    WritePOD<std::uint8_t>(body, static_cast<std::uint8_t>(groups.size()));
    for (const auto& [id, name] : groups)
    {
        WritePOD<std::uint8_t>(body, id);
        WriteString(body, name);
    }
    WritePOD<std::uint8_t>(body, static_cast<std::uint8_t>(friends.size()));
    for (const auto& f : friends)
    {
        WritePOD<std::uint32_t>(body, f.id);
        WriteString(body, f.name);
        WritePOD<std::uint8_t>(body, f.level);
        WritePOD<std::uint8_t>(body, f.group);
        WritePOD<std::uint8_t>(body, f.klass);
        WritePOD<std::uint8_t>(body, f.connected);
        WritePOD<std::uint32_t>(body, f.region);
    }
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_FRIENDLIST_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwFriendGroupMakeReq(std::shared_ptr<PeerSession> peer,
                         std::uint32_t char_id, std::uint32_t key,
                         std::uint8_t result, std::uint8_t group,
                         const std::string& name)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint8_t>(body, result);
    WritePOD<std::uint8_t>(body, group);
    WriteString(body, name);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_FRIENDGROUPMAKE_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwFriendGroupNameReq(std::shared_ptr<PeerSession> peer,
                         std::uint32_t char_id, std::uint32_t key,
                         std::uint8_t result, std::uint8_t group,
                         const std::string& name)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint8_t>(body, result);
    WritePOD<std::uint8_t>(body, group);
    WriteString(body, name);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_FRIENDGROUPNAME_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwFriendGroupDeleteReq(std::shared_ptr<PeerSession> peer,
                           std::uint32_t char_id, std::uint32_t key,
                           std::uint8_t result, std::uint8_t group)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint8_t>(body, result);
    WritePOD<std::uint8_t>(body, group);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_FRIENDGROUPDELETE_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwFriendGroupChangeReq(std::shared_ptr<PeerSession> peer,
                           std::uint32_t char_id, std::uint32_t key,
                           std::uint8_t result, std::uint8_t group,
                           std::uint32_t friend_id)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint8_t>(body, result);
    WritePOD<std::uint8_t>(body, group);
    WritePOD<std::uint32_t>(body, friend_id);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_FRIENDGROUPCHANGE_REQ),
        std::move(body));
}

} // namespace tworldsvr::senders
