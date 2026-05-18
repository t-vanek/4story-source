// PCH-free (no stdafx.h) — compiles on Linux without TNetLib.h → winsock2.h.
// MSVC vcxproj marks this file with PrecompiledHeader=NotUsing.

#include "asio_session.h"

#include <boost/asio/buffer.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>

#include <utility>

namespace tnetlib {

namespace {
// Reasonable starting buffer for a single async_read_some. Packets
// rarely exceed a few KB on this wire protocol; this just bounds
// per-recv memory.
constexpr std::size_t kRecvChunkBytes = 4096;
} // namespace

// ===== AsioSession ==========================================================

AsioSession::AsioSession(boost::asio::ip::tcp::socket socket, PeerType type)
    : m_socket(std::move(socket))
    , m_type(type)
    , m_recv_buffer(kRecvChunkBytes)
{
}

AsioSession::~AsioSession()
{
    Close();
}

boost::asio::awaitable<void>
AsioSession::Run(BytesHandler on_bytes)
{
    boost::system::error_code ec;
    while (m_socket.is_open())
    {
        const auto n = co_await m_socket.async_read_some(
            boost::asio::buffer(m_recv_buffer.data(), m_recv_buffer.size()),
            boost::asio::redirect_error(boost::asio::use_awaitable, ec));

        if (ec || n == 0)
        {
            // Peer closed (EOF) or hard error. Both terminate the loop;
            // the handler caller decides whether to log.
            break;
        }

        if (on_bytes)
        {
            on_bytes(std::span<const std::byte>(m_recv_buffer.data(), n));
        }
    }
    Close();
}

boost::asio::awaitable<void>
AsioSession::Send(std::span<const std::byte> bytes)
{
    if (!m_socket.is_open() || bytes.empty())
        co_return;

    co_await boost::asio::async_write(
        m_socket,
        boost::asio::buffer(bytes.data(), bytes.size()),
        boost::asio::use_awaitable);
}

void AsioSession::Close()
{
    if (!m_socket.is_open())
        return;
    boost::system::error_code ec;
    m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    m_socket.close(ec);
}

// ===== AsioListener =========================================================

AsioListener::AsioListener(boost::asio::any_io_executor exec, std::uint16_t port)
    : m_acceptor(exec,
        boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port))
    , m_port(port)
{
    m_acceptor.set_option(boost::asio::socket_base::reuse_address(true));
    // If port was 0 (ephemeral), reflect the actual bound port back.
    if (m_port == 0)
    {
        m_port = m_acceptor.local_endpoint().port();
    }
}

boost::asio::awaitable<void>
AsioListener::Run(AcceptHandler on_accept)
{
    boost::system::error_code ec;
    while (m_acceptor.is_open())
    {
        auto socket = co_await m_acceptor.async_accept(
            boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        if (ec)
        {
            // operation_aborted == acceptor was closed by Stop; clean exit.
            break;
        }
        if (on_accept)
        {
            on_accept(std::move(socket));
        }
    }
}

} // namespace tnetlib
