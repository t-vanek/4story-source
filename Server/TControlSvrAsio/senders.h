#pragma once

// Free-function wire builders for the CT_* acks the F1 handlers
// emit. Each one wraps a single legacy CTManager::SendCT_*_ACK call;
// the body layout is reproduced byte-for-byte against
// Server/TControlSvr/Sender.cpp so a legacy TController.exe accepts
// the response. The split into free functions keeps the handlers
// readable and matches the TPatchSvrAsio convention.

#include "control_session.h"
#include "services/event_types.h"
#include "services/peer_registry.h"
#include "services/service_inventory.h"

#include <boost/asio/awaitable.hpp>

#include <cstdint>
#include <memory>

namespace tcontrolsvr::senders {

// CT_OPLOGIN_ACK = { BYTE bRet, BYTE bAuthority, DWORD dwID }
//   bRet=0 on success, 1 on failure
//   bAuthority=0 unless bRet==0
//   dwID is the operator's manager_seq (0 on failure)
boost::asio::awaitable<void> SendOpLoginAck(
    const std::shared_ptr<ControlSession>& sess,
    std::uint8_t  ret,
    std::uint8_t  authority,
    std::uint32_t manager_seq);

// CT_STLOGIN_ACK = { BYTE bRet, BYTE bAuthority }
boost::asio::awaitable<void> SendStLoginAck(
    const std::shared_ptr<ControlSession>& sess,
    std::uint8_t ret,
    std::uint8_t authority);

// CT_AUTHORITY_ACK — body is empty, just announces the operator
// lacks privileges for the most-recent command.
boost::asio::awaitable<void> SendAuthorityAck(
    const std::shared_ptr<ControlSession>& sess);

// CT_GROUPLIST_ACK = { DWORD count, [ BYTE bGroupID, CString name ] * count }
boost::asio::awaitable<void> SendGroupListAck(
    const std::shared_ptr<ControlSession>& sess,
    const std::vector<Group>& groups);

// CT_MACHINELIST_ACK = { DWORD count, [ BYTE bMachineID, CString name ] * count }
boost::asio::awaitable<void> SendMachineListAck(
    const std::shared_ptr<ControlSession>& sess,
    const std::vector<Machine>& machines);

// CT_SVRTYPELIST_ACK = { DWORD count, [ BYTE bType, CString name ] * count }
boost::asio::awaitable<void> SendSvrTypeListAck(
    const std::shared_ptr<ControlSession>& sess,
    const std::vector<ServerType>& types);

// CT_SERVICEAUTOSTART_ACK = { BYTE bAutoStart }
boost::asio::awaitable<void> SendServiceAutoStartAck(
    const std::shared_ptr<ControlSession>& sess,
    std::uint8_t auto_start);

// CT_SERVICESTAT_ACK = {
//   DWORD count,
//   [ BYTE bGroupID, BYTE bType, BYTE bServerID, CString szName,
//     BYTE bMachineID, DWORD dwStatus ] * count
// }
struct ServiceStatRow
{
    std::uint8_t   group_id;
    std::uint8_t   type_id;
    std::uint8_t   server_id;
    std::string    name;
    std::uint8_t   machine_id;
    std::uint32_t  status;
};
boost::asio::awaitable<void> SendServiceStatAck(
    const std::shared_ptr<ControlSession>& sess,
    const std::vector<ServiceStatRow>& rows);

// CT_SERVICECONTROL_ACK = { BYTE bRet }
boost::asio::awaitable<void> SendServiceControlAck(
    const std::shared_ptr<ControlSession>& sess,
    std::uint8_t ret);

// CT_SERVICECHANGE_ACK = { DWORD dwID, DWORD dwStatus }
boost::asio::awaitable<void> SendServiceChangeAck(
    const std::shared_ptr<ControlSession>& sess,
    std::uint32_t service_id,
    std::uint32_t status);

// CT_SERVICEDATA_ACK = {
//   DWORD dwID, DWORD dwSession, DWORD dwCurUser, DWORD dwMaxUser,
//   DWORD dwPing, INT64 nPickTime, DWORD dwStopCount,
//   INT64 nLatestStop, DWORD dwActiveUser
// }
boost::asio::awaitable<void> SendServiceDataAck(
    const std::shared_ptr<ControlSession>& sess,
    std::uint32_t service_id,
    std::uint32_t session_count,
    std::uint32_t cur_users,
    std::uint32_t max_users,
    std::uint32_t ping_ms,
    std::int64_t  peak_time_unix,
    std::uint32_t stop_count,
    std::int64_t  latest_stop_unix,
    std::uint32_t active_users);

// Peer-side: CT_SERVICEMONITOR_ACK = { DWORD dwTick }
boost::asio::awaitable<void> SendServiceMonitorAck(
    const std::shared_ptr<ControlSession>& sess,
    std::uint32_t tick);

// Peer-side: CT_CTRLSVR_REQ — body empty, peer learns "you're
// talking to a control server" and flips its inbound socket type.
boost::asio::awaitable<void> SendCtrlSvrReq(
    const std::shared_ptr<ControlSession>& sess);

// --- F3: admin operations (operator → control → peer) ----------------

// CT_USERPROTECTED_ACK = { BYTE bRet }
boost::asio::awaitable<void> SendUserProtectedAck(
    const std::shared_ptr<ControlSession>& sess,
    std::uint8_t ret);

// CT_CHATBAN_ACK = { BYTE bRet }
boost::asio::awaitable<void> SendChatBanAck(
    const std::shared_ptr<ControlSession>& sess,
    std::uint8_t ret);

// CT_CHATBANLIST_ACK = {
//   WORD count,
//   [ DWORD dwID, CString target, INT64 created, WORD minutes,
//     CString reason, CString operator ] * count
// }
// Layout matches legacy CTManager::SendCT_CHATBANLIST_ACK in
// Server/TControlSvr/Sender.cpp:233 byte-for-byte (WORD count,
// dwID first, then target name, then time, then minutes, then
// reason, then operator name).
struct ChatBanRow
{
    std::uint32_t  id;
    std::string    operator_id;
    std::string    target_user;
    std::uint16_t  minutes;
    std::string    reason;
    std::int64_t   created_unix;
};
boost::asio::awaitable<void> SendChatBanListAck(
    const std::shared_ptr<ControlSession>& sess,
    const std::vector<ChatBanRow>& rows);

// Peer-side forwarders. Same wire shape as the legacy
// CTServer::SendCT_*_ACK / ::SendCT_*_REQ methods.

// CT_ANNOUNCEMENT_ACK = { CString message }
boost::asio::awaitable<void> SendAnnouncementAck(
    const std::shared_ptr<ControlSession>& sess,
    const std::string& message);

// CT_USERKICKOUT_ACK = { CString user }
boost::asio::awaitable<void> SendUserKickoutAck(
    const std::shared_ptr<ControlSession>& sess,
    const std::string& user);

// CT_USERMOVE_ACK = { CString user, BYTE channel, WORD map_id,
//                     float x, float y, float z }
boost::asio::awaitable<void> SendUserMoveAck(
    const std::shared_ptr<ControlSession>& sess,
    const std::string& user,
    std::uint8_t channel,
    std::uint16_t map_id,
    float x, float y, float z);

// CT_USERPOSITION_ACK = { CString mover, CString target }
boost::asio::awaitable<void> SendUserPositionAck(
    const std::shared_ptr<ControlSession>& sess,
    const std::string& mover,
    const std::string& target);

// CT_CHARMSG_ACK = { CString user, CString message }
boost::asio::awaitable<void> SendCharMsgAck(
    const std::shared_ptr<ControlSession>& sess,
    const std::string& user,
    const std::string& message);

// CT_CHATBAN_REQ (control → peer) = {
//   CString user, WORD minutes, DWORD ban_seq, DWORD manager_id
// }
boost::asio::awaitable<void> SendChatBanReq(
    const std::shared_ptr<ControlSession>& sess,
    const std::string& user,
    std::uint16_t minutes,
    std::uint32_t ban_seq,
    std::uint32_t manager_id);

// CT_MONSPAWNFIND_ACK (control → peer) = {
//   DWORD manager_id, BYTE channel, WORD map_id, WORD spawn_id
// }
boost::asio::awaitable<void> SendMonSpawnFindAck(
    const std::shared_ptr<ControlSession>& sess,
    std::uint32_t manager_id,
    std::uint8_t channel,
    std::uint16_t map_id,
    std::uint16_t spawn_id);

// --- F4: event manager (operator ↔ control ↔ peer) -----------------

// CT_EVENTCHANGE_ACK = { BYTE bRet, BYTE bType, EventInfo serialized }
//   bRet is event_result::* (Success / Fail / InvalidTime / …).
//   bType is event_op::* — the CRUD op the operator requested.
boost::asio::awaitable<void> SendEventChangeAck(
    const std::shared_ptr<ControlSession>& sess,
    std::uint8_t  ret,
    std::uint8_t  op,
    const EventInfo& ev);

// CT_EVENTLIST_ACK = { DWORD count, EventInfo * count }
boost::asio::awaitable<void> SendEventListAck(
    const std::shared_ptr<ControlSession>& sess,
    const std::vector<EventInfo>& events);

// CT_CASHITEMLIST_ACK = { DWORD count, [ WORD wID, CString szName ] }
boost::asio::awaitable<void> SendCashItemListAck(
    const std::shared_ptr<ControlSession>& sess,
    const std::vector<CashItem>& items);

// Peer-side event forwarders (control → World/Map/Login):

// CT_EVENTUPDATE_REQ = { BYTE kind, WORD value, EventInfo body }
//   Matches legacy CTServer::SendCT_EVENTUPDATE_REQ
//   (Server/TControlSvr/TServer.cpp:190) byte-for-byte: the kind
//   + value header is followed by the full EventInfo body via
//   the legacy WrapPacketIn writer. Without the trailing body
//   the peer's matching On*EVENTUPDATE_REQ reads garbage past the
//   header and either crashes or misapplies the event state.
boost::asio::awaitable<void> SendEventUpdateReq(
    const std::shared_ptr<ControlSession>& sess,
    std::uint8_t  kind,
    std::uint16_t value,
    const EventInfo& ev);

// CT_EVENTMSG_REQ = { BYTE kind, BYTE msg_type, CString msg }
//   msg_type = 0 → start announcement, 1 → end announcement.
boost::asio::awaitable<void> SendEventMsgReq(
    const std::shared_ptr<ControlSession>& sess,
    std::uint8_t kind,
    std::uint8_t msg_type,
    const std::string& msg);

// CT_CASHITEMSALE_REQ = {
//   DWORD index, WORD value, WORD count, [ WORD wID, BYTE bSale ] * count
// }
boost::asio::awaitable<void> SendCashItemSaleReq(
    const std::shared_ptr<ControlSession>& sess,
    std::uint32_t index,
    std::uint16_t value,
    const std::vector<CashItemSale>& items);

// CT_CASHSHOPSTOP_REQ = { BYTE bType }
boost::asio::awaitable<void> SendCashShopStopReq(
    const std::shared_ptr<ControlSession>& sess,
    std::uint8_t type);

// Raw passthrough — packet body verbatim, repacked with the given
// wId. Used by EVENTQUARTER*, TOURNAMENTEVENT, HELPMESSAGE,
// RPSGAME*, CMGIFT* forwarders.
boost::asio::awaitable<void> SendRawForward(
    const std::shared_ptr<ControlSession>& sess,
    std::uint16_t wId,
    const std::vector<std::byte>& body);

// --- F5: patch metadata + castle ----------------------------------

// CT_PREVERSIONTABLE_ACK = { DWORD count,
//   [ DWORD beta, CString path, CString name, DWORD size ] * count }
struct PreVersionAckRow
{
    std::uint32_t  beta_ver;
    std::string    path;
    std::string    name;
    std::uint32_t  size;
};
boost::asio::awaitable<void> SendPreVersionTableAck(
    const std::shared_ptr<ControlSession>& sess,
    const std::vector<PreVersionAckRow>& rows);

// CT_CASTLEINFO_REQ (control → peer) = { DWORD manager_id }
boost::asio::awaitable<void> SendCastleInfoReq(
    const std::shared_ptr<ControlSession>& sess,
    std::uint32_t manager_id);

// CT_CASTLEGUILDCHG_REQ (control → peer) = {
//   WORD castle_id, DWORD def_guild, DWORD atk_guild,
//   DWORD manager_id, INT64 time
// }
boost::asio::awaitable<void> SendCastleGuildChangeReq(
    const std::shared_ptr<ControlSession>& sess,
    std::uint16_t castle_id,
    std::uint32_t def_guild_id,
    std::uint32_t atk_guild_id,
    std::uint32_t manager_id,
    std::int64_t  time_unix);

// Battle status — legacy SM_BATTLESTATUS_REQ = {
//   BYTE type, BYTE status, DWORD reserved, DWORD seconds
// }
// Used for CT_CASTLEENABLE_REQ (operator → control → World).
boost::asio::awaitable<void> SendBattleStatusReq(
    const std::shared_ptr<ControlSession>& sess,
    std::uint8_t  battle_type,
    std::uint8_t  status,
    std::uint32_t seconds);

// --- Round-2 audit fixes: ITEMFIND/STATE, MONACTION, platform,
// service-change, service-data-clear ---------------------------------

// CT_ITEMFIND_REQ (control → World) = {
//   DWORD manager_id, WORD item_id, CString user_name
// }
boost::asio::awaitable<void> SendItemFindReq(
    const std::shared_ptr<ControlSession>& sess,
    std::uint32_t manager_id,
    std::uint16_t item_id,
    const std::string& user_name);

// CT_ITEMSTATE_REQ (control → World) — body is repacked verbatim
// from the operator request after the world filter is stripped.
boost::asio::awaitable<void> SendItemStateReq(
    const std::shared_ptr<ControlSession>& sess,
    const std::vector<std::byte>& body);

// CT_MONACTION_ACK (control → Map) = {
//   BYTE channel, WORD map_id, DWORD mon_id, BYTE action,
//   DWORD trigger_id, DWORD host_id, DWORD rh_id, BYTE rh_type,
//   WORD spawn_id
// }
boost::asio::awaitable<void> SendMonActionAck(
    const std::shared_ptr<ControlSession>& sess,
    std::uint8_t  channel,
    std::uint16_t map_id,
    std::uint32_t mon_id,
    std::uint8_t  action,
    std::uint32_t trigger_id,
    std::uint32_t host_id,
    std::uint32_t rh_id,
    std::uint8_t  rh_type,
    std::uint16_t spawn_id);

// CT_PLATFORM_ACK = { BYTE machine_id, DWORD cpu, DWORD mem,
//                     float net }
// Legacy fan-out to every MANAGER_SERVICE operator. F1+ ships a
// no-op zero-filled body — PDH counters were Windows-only and the
// modernization plan explicitly defers monitoring to /metrics.
boost::asio::awaitable<void> SendPlatformAck(
    const std::shared_ptr<ControlSession>& sess,
    std::uint8_t machine_id,
    std::uint32_t cpu,
    std::uint32_t mem,
    float net);

// CT_SERVICEDATACLEAR_ACK (control → peer) — body empty.
// Resets the per-peer max-user / stop-count counters.
boost::asio::awaitable<void> SendServiceDataClearAck(
    const std::shared_ptr<ControlSession>& sess);

// CT_SERVICEUPLOADEND_ACK = { BYTE bRet }
//   1 = upload-in-progress (someone else holds the slot)
//   2 = machine ID not found
//   3 = local file open error
//   4 = backup file create error
// Legacy uses these as failure codes; the modern server fires
// bRet=2 across the board to indicate "feature unavailable" without
// adding a new wire enum value.
boost::asio::awaitable<void> SendServiceUploadEndAck(
    const std::shared_ptr<ControlSession>& sess,
    std::uint8_t ret);

// CT_SERVICEUPLOADSTART_ACK = { BYTE bRet }
//   ACK_SUCCESS (= 1) when the file slot is reserved; any other
//   value is a failure with the same numbering as UPLOADEND_ACK.
boost::asio::awaitable<void> SendServiceUploadStartAck(
    const std::shared_ptr<ControlSession>& sess,
    std::uint8_t ret);

} // namespace tcontrolsvr::senders
