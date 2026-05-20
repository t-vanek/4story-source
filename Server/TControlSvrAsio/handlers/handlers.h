#pragma once

// Handler dispatch for TControlSvrAsio. F1 wires only the post-login
// flow:
//
//   CT_OPLOGIN_REQ → CT_OPLOGIN_ACK
//                    CT_GROUPLIST_ACK
//                    CT_MACHINELIST_ACK
//                    CT_SVRTYPELIST_ACK
//                    CT_SERVICEAUTOSTART_ACK
//   CT_STLOGIN_REQ → CT_STLOGIN_ACK
//   CT_SERVICEAUTOSTART_REQ → broadcast CT_SERVICEAUTOSTART_ACK
//
// Service / event / admin / patch handlers ship in F2..F5. Unknown
// packet ids are logged and silently dropped so the GUI client
// doesn't crash on packets we haven't implemented yet.

#include "../control_session.h"
#include "../operator_session.h"
#include "../services/operator_auth_service.h"
#include "../services/operator_registry.h"
#include "../services/service_inventory.h"

#include <boost/asio/awaitable.hpp>

#include <cstdint>
#include <memory>
#include <string>

namespace tcontrolsvr {

// Shared per-request context handed to every handler. Owned by the
// ControlServer; non-owning pointers so the context is cheap to copy
// per-packet and so test fakes can substitute parts independently.
struct HandlerContext
{
    IOperatorAuthService*  auth      = nullptr;
    IServiceInventory*     inventory = nullptr;
    OperatorRegistry*      operators = nullptr;

    // Mirror of legacy CTControlSvrModule::m_bAutoStart — whether
    // the cluster scheduler auto-restarts a crashed daemon. Mutated
    // by CT_SERVICEAUTOSTART_REQ broadcast.
    std::uint8_t*          auto_start = nullptr;
};

namespace handlers {

// Top-level dispatch. Returns when the body is unsupported in F1 (no
// reply), or after the corresponding ack chain has been emitted.
boost::asio::awaitable<void> Dispatch(
    std::shared_ptr<OperatorSession> op,
    std::uint16_t wId,
    std::vector<std::byte> body,
    const HandlerContext& ctx);

// Individual handlers — exposed so unit tests can drive each one
// without going through the wire dispatcher.

boost::asio::awaitable<void> OnOpLoginReq(
    std::shared_ptr<OperatorSession> op,
    std::vector<std::byte> body,
    const HandlerContext& ctx);

boost::asio::awaitable<void> OnStLoginReq(
    std::shared_ptr<OperatorSession> op,
    std::vector<std::byte> body,
    const HandlerContext& ctx);

boost::asio::awaitable<void> OnServiceAutoStartReq(
    std::shared_ptr<OperatorSession> op,
    std::vector<std::byte> body,
    const HandlerContext& ctx);

} // namespace handlers
} // namespace tcontrolsvr
