#pragma once

// ControlServer — accept loop + per-session coroutine wiring. F1
// only handles inbound operator sockets (OperatorSession). F2 adds
// the outbound peer dialer and the SERVICEMONITOR-driven flip from
// "looks like an operator" to "this is a PeerSession".

#include "control_session.h"
#include "operator_session.h"
#include "handlers/handlers.h"
#include "services/operator_registry.h"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <cstdint>
#include <memory>

namespace tcontrolsvr {

struct ControlServerConfig
{
    std::uint16_t          port = 0;
    IOperatorAuthService*  auth      = nullptr;
    IServiceInventory*     inventory = nullptr;
    std::uint8_t           auto_start = 0;
};

class ControlServer
{
public:
    ControlServer(boost::asio::io_context& io, ControlServerConfig cfg);

    boost::asio::awaitable<void> Run();

    std::uint16_t Port() const { return m_port; }

    // Test hook — drive a single session through the dispatch loop
    // without binding a socket. The session is constructed by the
    // caller, then handed off here.
    boost::asio::awaitable<void> Drive(std::shared_ptr<ControlSession> sess);

    std::size_t LiveOperators() const { return m_operators.Size(); }

private:
    boost::asio::awaitable<void> HandleConnection(
        std::shared_ptr<ControlSession> sess);

    boost::asio::io_context&        m_io;
    boost::asio::ip::tcp::acceptor  m_acceptor;
    std::uint16_t                   m_port;
    ControlServerConfig             m_cfg;
    OperatorRegistry                m_operators;
    std::uint8_t                    m_auto_start = 0;
};

} // namespace tcontrolsvr
