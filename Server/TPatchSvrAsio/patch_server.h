#pragma once

#include "patch_session.h"
#include "services/patch_repository.h"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

namespace tpatchsvr {

struct PatchServerConfig
{
    std::uint16_t      port = 3715;
    PatchRepository*   repo = nullptr;        // non-owning
    std::string        ftp_url;
    std::string        pre_ftp_url;
    std::string        login_host;
    std::uint16_t      login_port = 0;
};

class PatchServer
{
public:
    PatchServer(boost::asio::io_context& io, PatchServerConfig config);

    boost::asio::awaitable<void> Run();

    std::uint16_t Port() const { return m_port; }

private:
    boost::asio::awaitable<void> HandleConnection(
        std::shared_ptr<PatchSession> session);

    boost::asio::io_context&        m_io;
    boost::asio::ip::tcp::acceptor  m_acceptor;
    std::uint16_t                   m_port;
    PatchServerConfig               m_cfg;
    std::atomic<std::size_t>        m_live_sessions{0};
};

} // namespace tpatchsvr
