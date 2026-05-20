#include "senders.h"

#include "event_codec.h"
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
    // Legacy CTManager::SendCT_CHATBANLIST_ACK shape (Sender.cpp:233):
    // WORD count, [ DWORD id, CString target, INT64 created,
    //   WORD minutes, CString reason, CString operator ] * count.
    std::vector<std::byte> body;
    wire::WritePOD<std::uint16_t>(body,
        static_cast<std::uint16_t>(rows.size()));
    for (const auto& r : rows)
    {
        wire::WritePOD<std::uint32_t>(body, r.id);
        wire::WriteString(body, r.target_user);
        wire::WritePOD<std::int64_t >(body, r.created_unix);
        wire::WritePOD<std::uint16_t>(body, r.minutes);
        wire::WriteString(body, r.reason);
        wire::WriteString(body, r.operator_id);
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

// --- F4: event manager ---------------------------------------------

boost::asio::awaitable<void> SendEventChangeAck(
    const std::shared_ptr<ControlSession>& sess,
    std::uint8_t ret, std::uint8_t op, const EventInfo& ev)
{
    std::vector<std::byte> body;
    wire::WritePOD<std::uint8_t>(body, ret);
    wire::WritePOD<std::uint8_t>(body, op);
    event_codec::Write(body, ev);
    co_await sess->SendPacket(ToUint16(MessageId::CT_EVENTCHANGE_ACK),
                              std::move(body));
}

boost::asio::awaitable<void> SendEventListAck(
    const std::shared_ptr<ControlSession>& sess,
    const std::vector<EventInfo>& events)
{
    // Legacy CTManager::SendCT_EVENTLIST_ACK (Sender.cpp:284): WORD
    // count, EventInfo * count (the legacy CPacket cast is
    // `(WORD)pMapEvent->size()`).
    std::vector<std::byte> body;
    wire::WritePOD<std::uint16_t>(body,
        static_cast<std::uint16_t>(events.size()));
    for (const auto& ev : events)
        event_codec::Write(body, ev);
    co_await sess->SendPacket(ToUint16(MessageId::CT_EVENTLIST_ACK),
                              std::move(body));
}

boost::asio::awaitable<void> SendCashItemListAck(
    const std::shared_ptr<ControlSession>& sess,
    const std::vector<CashItem>& items)
{
    // Legacy CTManager::SendCT_CASHITEMLIST_ACK (Sender.cpp:297):
    // WORD count, [ WORD id, CString name ] * count.
    std::vector<std::byte> body;
    wire::WritePOD<std::uint16_t>(body,
        static_cast<std::uint16_t>(items.size()));
    for (const auto& ci : items)
    {
        wire::WritePOD<std::uint16_t>(body, ci.id);
        wire::WriteString(body, ci.name);
    }
    co_await sess->SendPacket(ToUint16(MessageId::CT_CASHITEMLIST_ACK),
                              std::move(body));
}

boost::asio::awaitable<void> SendEventUpdateReq(
    const std::shared_ptr<ControlSession>& sess,
    std::uint8_t kind, std::uint16_t value, const EventInfo& ev)
{
    std::vector<std::byte> body;
    wire::WritePOD<std::uint8_t >(body, kind);
    wire::WritePOD<std::uint16_t>(body, value);
    event_codec::Write(body, ev);   // legacy WrapPacketIn payload
    co_await sess->SendPacket(ToUint16(MessageId::CT_EVENTUPDATE_REQ),
                              std::move(body));
}

boost::asio::awaitable<void> SendEventMsgReq(
    const std::shared_ptr<ControlSession>& sess,
    std::uint8_t kind, std::uint8_t msg_type, const std::string& msg)
{
    std::vector<std::byte> body;
    wire::WritePOD<std::uint8_t>(body, kind);
    wire::WritePOD<std::uint8_t>(body, msg_type);
    wire::WriteString(body, msg);
    co_await sess->SendPacket(ToUint16(MessageId::CT_EVENTMSG_REQ),
                              std::move(body));
}

boost::asio::awaitable<void> SendCashItemSaleReq(
    const std::shared_ptr<ControlSession>& sess,
    std::uint32_t index, std::uint16_t value,
    const std::vector<CashItemSale>& items)
{
    std::vector<std::byte> body;
    wire::WritePOD<std::uint32_t>(body, index);
    wire::WritePOD<std::uint16_t>(body, value);
    wire::WritePOD<std::uint16_t>(body,
        static_cast<std::uint16_t>(items.size()));
    for (const auto& s : items)
    {
        wire::WritePOD<std::uint16_t>(body, s.item_id);
        wire::WritePOD<std::uint8_t >(body, s.sale_value);
    }
    co_await sess->SendPacket(ToUint16(MessageId::CT_CASHITEMSALE_REQ),
                              std::move(body));
}

boost::asio::awaitable<void> SendCashShopStopReq(
    const std::shared_ptr<ControlSession>& sess,
    std::uint8_t type)
{
    std::vector<std::byte> body;
    wire::WritePOD<std::uint8_t>(body, type);
    co_await sess->SendPacket(ToUint16(MessageId::CT_CASHSHOPSTOP_REQ),
                              std::move(body));
}

boost::asio::awaitable<void> SendRawForward(
    const std::shared_ptr<ControlSession>& sess,
    std::uint16_t wId,
    const std::vector<std::byte>& body)
{
    co_await sess->SendPacket(wId, body);
}

// --- F5: patch metadata + castle ----------------------------------

boost::asio::awaitable<void> SendPreVersionTableAck(
    const std::shared_ptr<ControlSession>& sess,
    const std::vector<PreVersionAckRow>& rows)
{
    // Legacy CTManager::SendCT_PREVERSIONTABLE_ACK (Sender.cpp:347):
    // WORD count, [ DWORD beta, CString path, CString name,
    //   DWORD size ] * count.
    std::vector<std::byte> body;
    wire::WritePOD<std::uint16_t>(body,
        static_cast<std::uint16_t>(rows.size()));
    for (const auto& r : rows)
    {
        wire::WritePOD<std::uint32_t>(body, r.beta_ver);
        wire::WriteString(body, r.path);
        wire::WriteString(body, r.name);
        wire::WritePOD<std::uint32_t>(body, r.size);
    }
    co_await sess->SendPacket(ToUint16(MessageId::CT_PREVERSIONTABLE_ACK),
                              std::move(body));
}

boost::asio::awaitable<void> SendCastleInfoReq(
    const std::shared_ptr<ControlSession>& sess,
    std::uint32_t manager_id)
{
    std::vector<std::byte> body;
    wire::WritePOD<std::uint32_t>(body, manager_id);
    co_await sess->SendPacket(ToUint16(MessageId::CT_CASTLEINFO_REQ),
                              std::move(body));
}

boost::asio::awaitable<void> SendCastleGuildChangeReq(
    const std::shared_ptr<ControlSession>& sess,
    std::uint16_t castle_id,
    std::uint32_t def_guild_id,
    std::uint32_t atk_guild_id,
    std::uint32_t manager_id,
    std::int64_t  time_unix)
{
    std::vector<std::byte> body;
    wire::WritePOD<std::uint16_t>(body, castle_id);
    wire::WritePOD<std::uint32_t>(body, def_guild_id);
    wire::WritePOD<std::uint32_t>(body, atk_guild_id);
    wire::WritePOD<std::uint32_t>(body, manager_id);
    wire::WritePOD<std::int64_t >(body, time_unix);
    co_await sess->SendPacket(ToUint16(MessageId::CT_CASTLEGUILDCHG_REQ),
                              std::move(body));
}

boost::asio::awaitable<void> SendBattleStatusReq(
    const std::shared_ptr<ControlSession>& sess,
    std::uint8_t  battle_type,
    std::uint8_t  status,
    std::uint32_t seconds)
{
    std::vector<std::byte> body;
    wire::WritePOD<std::uint8_t >(body, battle_type);
    wire::WritePOD<std::uint8_t >(body, status);
    wire::WritePOD<std::uint32_t>(body, 0);          // reserved
    wire::WritePOD<std::uint32_t>(body, seconds);
    co_await sess->SendPacket(ToUint16(MessageId::SM_BATTLESTATUS_REQ),
                              std::move(body));
}

// --- Round-2 audit fixes ------------------------------------------

boost::asio::awaitable<void> SendItemFindReq(
    const std::shared_ptr<ControlSession>& sess,
    std::uint32_t manager_id,
    std::uint16_t item_id,
    const std::string& user_name)
{
    std::vector<std::byte> body;
    wire::WritePOD<std::uint32_t>(body, manager_id);
    wire::WritePOD<std::uint16_t>(body, item_id);
    wire::WriteString(body, user_name);
    co_await sess->SendPacket(ToUint16(MessageId::CT_ITEMFIND_REQ),
                              std::move(body));
}

boost::asio::awaitable<void> SendItemStateReq(
    const std::shared_ptr<ControlSession>& sess,
    const std::vector<std::byte>& body)
{
    co_await sess->SendPacket(ToUint16(MessageId::CT_ITEMSTATE_REQ), body);
}

boost::asio::awaitable<void> SendMonActionAck(
    const std::shared_ptr<ControlSession>& sess,
    std::uint8_t channel, std::uint16_t map_id,
    std::uint32_t mon_id, std::uint8_t action,
    std::uint32_t trigger_id, std::uint32_t host_id,
    std::uint32_t rh_id, std::uint8_t rh_type,
    std::uint16_t spawn_id)
{
    std::vector<std::byte> body;
    wire::WritePOD<std::uint8_t >(body, channel);
    wire::WritePOD<std::uint16_t>(body, map_id);
    wire::WritePOD<std::uint32_t>(body, mon_id);
    wire::WritePOD<std::uint8_t >(body, action);
    wire::WritePOD<std::uint32_t>(body, trigger_id);
    wire::WritePOD<std::uint32_t>(body, host_id);
    wire::WritePOD<std::uint32_t>(body, rh_id);
    wire::WritePOD<std::uint8_t >(body, rh_type);
    wire::WritePOD<std::uint16_t>(body, spawn_id);
    co_await sess->SendPacket(ToUint16(MessageId::CT_MONACTION_ACK),
                              std::move(body));
}

boost::asio::awaitable<void> SendPlatformAck(
    const std::shared_ptr<ControlSession>& sess,
    std::uint8_t machine_id, std::uint32_t cpu,
    std::uint32_t mem, float net)
{
    std::vector<std::byte> body;
    wire::WritePOD<std::uint8_t >(body, machine_id);
    wire::WritePOD<std::uint32_t>(body, cpu);
    wire::WritePOD<std::uint32_t>(body, mem);
    wire::WritePOD<float        >(body, net);
    co_await sess->SendPacket(ToUint16(MessageId::CT_PLATFORM_ACK),
                              std::move(body));
}

boost::asio::awaitable<void> SendServiceDataClearAck(
    const std::shared_ptr<ControlSession>& sess)
{
    co_await sess->SendPacket(
        ToUint16(MessageId::CT_SERVICEDATACLEAR_ACK), {});
}

boost::asio::awaitable<void> SendServiceUploadEndAck(
    const std::shared_ptr<ControlSession>& sess,
    std::uint8_t ret)
{
    std::vector<std::byte> body;
    wire::WritePOD<std::uint8_t>(body, ret);
    co_await sess->SendPacket(
        ToUint16(MessageId::CT_SERVICEUPLOADEND_ACK), std::move(body));
}

boost::asio::awaitable<void> SendServiceUploadStartAck(
    const std::shared_ptr<ControlSession>& sess,
    std::uint8_t ret)
{
    std::vector<std::byte> body;
    wire::WritePOD<std::uint8_t>(body, ret);
    co_await sess->SendPacket(
        ToUint16(MessageId::CT_SERVICEUPLOADSTART_ACK), std::move(body));
}

} // namespace tcontrolsvr::senders
