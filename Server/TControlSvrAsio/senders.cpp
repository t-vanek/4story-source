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

} // namespace tcontrolsvr::senders
