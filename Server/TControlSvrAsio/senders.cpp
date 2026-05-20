#include "senders.h"

#include "wire_codec.h"
#include "MessageId.h"

#include <cstdint>

namespace tcontrolsvr::senders {

namespace {
using tnetlib::protocol::MessageId;
using tnetlib::protocol::ToUint16;
} // namespace

boost::asio::awaitable<void> SendOpLoginAck(
    const std::shared_ptr<ControlSession>& sess,
    std::uint8_t ret, std::uint8_t authority, std::uint32_t manager_seq)
{
    std::vector<std::byte> body;
    body.reserve(1 + 1 + 4);
    wire::WritePOD<std::uint8_t >(body, ret);
    wire::WritePOD<std::uint8_t >(body, authority);
    wire::WritePOD<std::uint32_t>(body, manager_seq);
    co_await sess->SendPacket(ToUint16(MessageId::CT_OPLOGIN_ACK),
                              std::move(body));
}

boost::asio::awaitable<void> SendStLoginAck(
    const std::shared_ptr<ControlSession>& sess,
    std::uint8_t ret, std::uint8_t authority)
{
    std::vector<std::byte> body;
    body.reserve(2);
    wire::WritePOD<std::uint8_t>(body, ret);
    wire::WritePOD<std::uint8_t>(body, authority);
    co_await sess->SendPacket(ToUint16(MessageId::CT_STLOGIN_ACK),
                              std::move(body));
}

boost::asio::awaitable<void> SendAuthorityAck(
    const std::shared_ptr<ControlSession>& sess)
{
    co_await sess->SendPacket(ToUint16(MessageId::CT_AUTHORITY_ACK), {});
}

boost::asio::awaitable<void> SendGroupListAck(
    const std::shared_ptr<ControlSession>& sess,
    const std::vector<Group>& groups)
{
    std::vector<std::byte> body;
    wire::WritePOD<std::uint32_t>(body, static_cast<std::uint32_t>(groups.size()));
    for (const auto& g : groups)
    {
        wire::WritePOD<std::uint8_t>(body, g.id);
        wire::WriteString(body, g.name);
    }
    co_await sess->SendPacket(ToUint16(MessageId::CT_GROUPLIST_ACK),
                              std::move(body));
}

boost::asio::awaitable<void> SendMachineListAck(
    const std::shared_ptr<ControlSession>& sess,
    const std::vector<Machine>& machines)
{
    std::vector<std::byte> body;
    wire::WritePOD<std::uint32_t>(body, static_cast<std::uint32_t>(machines.size()));
    for (const auto& m : machines)
    {
        wire::WritePOD<std::uint8_t>(body, m.id);
        wire::WriteString(body, m.name);
    }
    co_await sess->SendPacket(ToUint16(MessageId::CT_MACHINELIST_ACK),
                              std::move(body));
}

boost::asio::awaitable<void> SendSvrTypeListAck(
    const std::shared_ptr<ControlSession>& sess,
    const std::vector<ServerType>& types)
{
    std::vector<std::byte> body;
    wire::WritePOD<std::uint32_t>(body, static_cast<std::uint32_t>(types.size()));
    for (const auto& t : types)
    {
        wire::WritePOD<std::uint8_t>(body, t.id);
        wire::WriteString(body, t.name);
    }
    co_await sess->SendPacket(ToUint16(MessageId::CT_SVRTYPELIST_ACK),
                              std::move(body));
}

boost::asio::awaitable<void> SendServiceAutoStartAck(
    const std::shared_ptr<ControlSession>& sess,
    std::uint8_t auto_start)
{
    std::vector<std::byte> body;
    wire::WritePOD<std::uint8_t>(body, auto_start);
    co_await sess->SendPacket(ToUint16(MessageId::CT_SERVICEAUTOSTART_ACK),
                              std::move(body));
}

boost::asio::awaitable<void> SendServiceStatAck(
    const std::shared_ptr<ControlSession>& sess,
    const std::vector<ServiceStatRow>& rows)
{
    std::vector<std::byte> body;
    wire::WritePOD<std::uint32_t>(body, static_cast<std::uint32_t>(rows.size()));
    for (const auto& r : rows)
    {
        wire::WritePOD<std::uint8_t >(body, r.group_id);
        wire::WritePOD<std::uint8_t >(body, r.type_id);
        wire::WritePOD<std::uint8_t >(body, r.server_id);
        wire::WriteString(body, r.name);
        wire::WritePOD<std::uint8_t >(body, r.machine_id);
        wire::WritePOD<std::uint32_t>(body, r.status);
    }
    co_await sess->SendPacket(ToUint16(MessageId::CT_SERVICESTAT_ACK),
                              std::move(body));
}

boost::asio::awaitable<void> SendServiceControlAck(
    const std::shared_ptr<ControlSession>& sess,
    std::uint8_t ret)
{
    std::vector<std::byte> body;
    wire::WritePOD<std::uint8_t>(body, ret);
    co_await sess->SendPacket(ToUint16(MessageId::CT_SERVICECONTROL_ACK),
                              std::move(body));
}

boost::asio::awaitable<void> SendServiceChangeAck(
    const std::shared_ptr<ControlSession>& sess,
    std::uint32_t service_id, std::uint32_t status)
{
    std::vector<std::byte> body;
    wire::WritePOD<std::uint32_t>(body, service_id);
    wire::WritePOD<std::uint32_t>(body, status);
    co_await sess->SendPacket(ToUint16(MessageId::CT_SERVICECHANGE_ACK),
                              std::move(body));
}

boost::asio::awaitable<void> SendServiceDataAck(
    const std::shared_ptr<ControlSession>& sess,
    std::uint32_t service_id, std::uint32_t session_count,
    std::uint32_t cur_users, std::uint32_t max_users,
    std::uint32_t ping_ms, std::int64_t peak_time_unix,
    std::uint32_t stop_count, std::int64_t latest_stop_unix,
    std::uint32_t active_users)
{
    std::vector<std::byte> body;
    wire::WritePOD<std::uint32_t>(body, service_id);
    wire::WritePOD<std::uint32_t>(body, session_count);
    wire::WritePOD<std::uint32_t>(body, cur_users);
    wire::WritePOD<std::uint32_t>(body, max_users);
    wire::WritePOD<std::uint32_t>(body, ping_ms);
    wire::WritePOD<std::int64_t >(body, peak_time_unix);
    wire::WritePOD<std::uint32_t>(body, stop_count);
    wire::WritePOD<std::int64_t >(body, latest_stop_unix);
    wire::WritePOD<std::uint32_t>(body, active_users);
    co_await sess->SendPacket(ToUint16(MessageId::CT_SERVICEDATA_ACK),
                              std::move(body));
}

boost::asio::awaitable<void> SendServiceMonitorAck(
    const std::shared_ptr<ControlSession>& sess,
    std::uint32_t tick)
{
    std::vector<std::byte> body;
    wire::WritePOD<std::uint32_t>(body, tick);
    co_await sess->SendPacket(ToUint16(MessageId::CT_SERVICEMONITOR_ACK),
                              std::move(body));
}

boost::asio::awaitable<void> SendCtrlSvrReq(
    const std::shared_ptr<ControlSession>& sess)
{
    co_await sess->SendPacket(ToUint16(MessageId::CT_CTRLSVR_REQ), {});
}

// --- F3: admin operations ------------------------------------------

boost::asio::awaitable<void> SendUserProtectedAck(
    const std::shared_ptr<ControlSession>& sess, std::uint8_t ret)
{
    std::vector<std::byte> body;
    wire::WritePOD<std::uint8_t>(body, ret);
    co_await sess->SendPacket(ToUint16(MessageId::CT_USERPROTECTED_ACK),
                              std::move(body));
}

boost::asio::awaitable<void> SendChatBanAck(
    const std::shared_ptr<ControlSession>& sess, std::uint8_t ret)
{
    std::vector<std::byte> body;
    wire::WritePOD<std::uint8_t>(body, ret);
    co_await sess->SendPacket(ToUint16(MessageId::CT_CHATBAN_ACK),
                              std::move(body));
}

boost::asio::awaitable<void> SendChatBanListAck(
    const std::shared_ptr<ControlSession>& sess,
    const std::vector<ChatBanRow>& rows)
{
    std::vector<std::byte> body;
    wire::WritePOD<std::uint32_t>(body, static_cast<std::uint32_t>(rows.size()));
    for (const auto& r : rows)
    {
        wire::WritePOD<std::uint32_t>(body, r.id);
        wire::WriteString(body, r.operator_id);
        wire::WriteString(body, r.target_user);
        wire::WritePOD<std::uint16_t>(body, r.minutes);
        wire::WriteString(body, r.reason);
        wire::WritePOD<std::int64_t >(body, r.created_unix);
    }
    co_await sess->SendPacket(ToUint16(MessageId::CT_CHATBANLIST_ACK),
                              std::move(body));
}

boost::asio::awaitable<void> SendAnnouncementAck(
    const std::shared_ptr<ControlSession>& sess,
    const std::string& message)
{
    std::vector<std::byte> body;
    wire::WriteString(body, message);
    co_await sess->SendPacket(ToUint16(MessageId::CT_ANNOUNCEMENT_ACK),
                              std::move(body));
}

boost::asio::awaitable<void> SendUserKickoutAck(
    const std::shared_ptr<ControlSession>& sess,
    const std::string& user)
{
    std::vector<std::byte> body;
    wire::WriteString(body, user);
    co_await sess->SendPacket(ToUint16(MessageId::CT_USERKICKOUT_ACK),
                              std::move(body));
}

boost::asio::awaitable<void> SendUserMoveAck(
    const std::shared_ptr<ControlSession>& sess,
    const std::string& user,
    std::uint8_t channel, std::uint16_t map_id,
    float x, float y, float z)
{
    std::vector<std::byte> body;
    wire::WriteString(body, user);
    wire::WritePOD<std::uint8_t >(body, channel);
    wire::WritePOD<std::uint16_t>(body, map_id);
    wire::WritePOD<float>(body, x);
    wire::WritePOD<float>(body, y);
    wire::WritePOD<float>(body, z);
    co_await sess->SendPacket(ToUint16(MessageId::CT_USERMOVE_ACK),
                              std::move(body));
}

boost::asio::awaitable<void> SendUserPositionAck(
    const std::shared_ptr<ControlSession>& sess,
    const std::string& mover, const std::string& target)
{
    std::vector<std::byte> body;
    wire::WriteString(body, mover);
    wire::WriteString(body, target);
    co_await sess->SendPacket(ToUint16(MessageId::CT_USERPOSITION_ACK),
                              std::move(body));
}

boost::asio::awaitable<void> SendCharMsgAck(
    const std::shared_ptr<ControlSession>& sess,
    const std::string& user, const std::string& message)
{
    std::vector<std::byte> body;
    wire::WriteString(body, user);
    wire::WriteString(body, message);
    co_await sess->SendPacket(ToUint16(MessageId::CT_CHARMSG_ACK),
                              std::move(body));
}

boost::asio::awaitable<void> SendChatBanReq(
    const std::shared_ptr<ControlSession>& sess,
    const std::string& user,
    std::uint16_t minutes,
    std::uint32_t ban_seq,
    std::uint32_t manager_id)
{
    std::vector<std::byte> body;
    wire::WriteString(body, user);
    wire::WritePOD<std::uint16_t>(body, minutes);
    wire::WritePOD<std::uint32_t>(body, ban_seq);
    wire::WritePOD<std::uint32_t>(body, manager_id);
    co_await sess->SendPacket(ToUint16(MessageId::CT_CHATBAN_REQ),
                              std::move(body));
}

boost::asio::awaitable<void> SendMonSpawnFindAck(
    const std::shared_ptr<ControlSession>& sess,
    std::uint32_t manager_id,
    std::uint8_t channel,
    std::uint16_t map_id,
    std::uint16_t spawn_id)
{
    std::vector<std::byte> body;
    wire::WritePOD<std::uint32_t>(body, manager_id);
    wire::WritePOD<std::uint8_t >(body, channel);
    wire::WritePOD<std::uint16_t>(body, map_id);
    wire::WritePOD<std::uint16_t>(body, spawn_id);
    co_await sess->SendPacket(ToUint16(MessageId::CT_MONSPAWNFIND_ACK),
                              std::move(body));
}

} // namespace tcontrolsvr::senders
