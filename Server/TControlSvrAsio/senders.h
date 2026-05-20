#pragma once

// Free-function wire builders for the CT_* acks the F1 handlers
// emit. Each one wraps a single legacy CTManager::SendCT_*_ACK call;
// the body layout is reproduced byte-for-byte against
// Server/TControlSvr/Sender.cpp so a legacy TController.exe accepts
// the response. The split into free functions keeps the handlers
// readable and matches the TPatchSvrAsio convention.

#include "control_session.h"
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

} // namespace tcontrolsvr::senders
