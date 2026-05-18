#pragma once

// AsioSession — modern C++20 + Boost.Asio replacement for the legacy
// CSession class in Session.h. Lives alongside CSession during the
// migration; servers can opt in one at a time.
//
// Why a separate class instead of refitting CSession:
//   * CSession's API is shaped around external IOCP loops
//     (CreateIoCompletionPort + GetQueuedCompletionStatus in each
//     server's main loop). Asio owns the event loop internally; a
//     drop-in replacement would force every consumer to rewrite its
//     dispatch anyway. Cleaner to introduce a new abstraction with the
//     right shape from the start and migrate callers gradually.
//   * AsioSession is portable C++20: no winsock.h, no OVERLAPPED, no
//     CRITICAL_SECTION, no TCHAR/CString. Compiles under MSVC and
//     g++/clang on Linux against the same boost-asio dependency
//     declared in /vcpkg.json.
//
// Phase 1 (this file): raw byte stream — accept + async_read + async_write.
//   No packet framing yet. That arrives in Phase 2 once the codec
//   helpers (CPacket::Encrypt/Decrypt/EncryptHeader/DecryptHeader) are
//   extracted into a PCH-free module that doesn't drag in winsock.h.
//
// Phase 3 will retire CSession entirely once every server has migrated.

#include <boost/asio/awaitable.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <vector>

namespace tnetlib {

// Peer role determines whether the session participates in the
// RC4+XOR wire encryption. Server-to-server links run plaintext;
// client-facing links run encrypted (see CSession::m_bUseCrypt).
enum class PeerType
{
    Server,
    Client
};

// One TCP connection. Owns the socket; the caller owns the executor /
// io_context that drives it. RAII-managed via shared_ptr so async
// continuations can hold weak refs without the session outliving the
// last completion.
class AsioSession : public std::enable_shared_from_this<AsioSession>
{
public:
    // Called once per fully-read buffer of size `bytes_read` (Phase 1
    // hands raw bytes; Phase 2 will switch to decoded packets).
    using BytesHandler = std::function<void(std::span<const std::byte>)>;

    AsioSession(boost::asio::ip::tcp::socket socket, PeerType type);
    ~AsioSession();

    AsioSession(const AsioSession&) = delete;
    AsioSession& operator=(const AsioSession&) = delete;

    // Start the read loop. Returns when the peer closes or an error
    // occurs. Pass a handler that is invoked for each chunk of bytes
    // received. Phase 2: this method will instead decode CPackets and
    // invoke a PacketHandler.
    boost::asio::awaitable<void> Run(BytesHandler on_bytes);

    // Write `bytes` to the socket. Awaitable; completes when all bytes
    // are flushed (or throws on error). Phase 2 will also frame +
    // encrypt before writing.
    boost::asio::awaitable<void> Send(std::span<const std::byte> bytes);

    // Close the socket. Idempotent. Safe to call from any thread that
    // owns the executor.
    void Close();

    boost::asio::ip::tcp::socket&       Socket()       { return m_socket; }
    const boost::asio::ip::tcp::socket& Socket() const { return m_socket; }
    PeerType                            Type()   const { return m_type;   }

private:
    boost::asio::ip::tcp::socket m_socket;
    PeerType                     m_type;
    std::vector<std::byte>       m_recv_buffer;
};

// Accept loop. Binds to `port` on all interfaces (INADDR_ANY) and
// invokes `on_accept` with each connected socket. The handler is
// expected to construct an AsioSession from the socket and spawn its
// Run() coroutine on the executor.
class AsioListener
{
public:
    using AcceptHandler = std::function<void(boost::asio::ip::tcp::socket)>;

    AsioListener(boost::asio::any_io_executor exec, std::uint16_t port);

    // Run the accept loop until the executor is stopped.
    boost::asio::awaitable<void> Run(AcceptHandler on_accept);

    std::uint16_t Port() const { return m_port; }

private:
    boost::asio::ip::tcp::acceptor m_acceptor;
    std::uint16_t                  m_port;
};

} // namespace tnetlib
