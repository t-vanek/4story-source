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

#include "packet_codec.h"

namespace tnetlib {

// Decoded view of one inbound packet. `body` points into the session's
// internal recv buffer and is valid for the duration of the
// PacketHandler invocation only — copy before storing.
struct DecodedPacket
{
    std::uint16_t wId;
    std::uint32_t dwNumber;
    std::span<const std::byte> body;
};

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
    // Phase-1 byte-level handler — fires for each `async_read_some`
    // result. Bypasses framing/decryption. Kept for callers that want
    // a low-level stream (debug tooling, manual codec testing).
    using BytesHandler = std::function<void(std::span<const std::byte>)>;

    // Phase-2 packet-level handler — fires once per fully-decoded
    // wire packet (header verified, body XOR-decrypted, checksum OK,
    // sequence number matches).
    using PacketHandler = std::function<void(const DecodedPacket&)>;

    AsioSession(boost::asio::ip::tcp::socket socket, PeerType type);
    ~AsioSession();

    AsioSession(const AsioSession&) = delete;
    AsioSession& operator=(const AsioSession&) = delete;

    // Byte-level read loop. Returns when peer closes or error.
    boost::asio::awaitable<void> Run(BytesHandler on_bytes);

    // Packet-level read loop. Drives the wire-format codec end-to-end:
    //   read 16-byte header (wSize plaintext) → read body → decrypt
    //   header (XOR keyed by expected sequence) → validate dwNumber →
    //   decrypt body (XOR + checksum verify) → dispatch DecodedPacket.
    //
    // Any framing error, checksum mismatch, or sequence mismatch
    // terminates the loop and the session closes (matches legacy
    // CSession::CheckMessage → PACKET_INVALID path).
    //
    // Does NOT yet run the RC4-over-entire-packet client → server
    // pre-pass. That layer is added in a follow-up commit and only
    // applies when `Type() == PeerType::Client`.
    boost::asio::awaitable<void> RunPackets(PacketHandler on_packet);

    // Byte-level send.
    boost::asio::awaitable<void> Send(std::span<const std::byte> bytes);

    // Packet-level send: assigns the next outbound sequence number,
    // builds a 16-byte header + `body` payload in an internal scratch
    // buffer, encrypts body + header per the wire codec, async_writes
    // the whole frame.
    //
    // THREAD-SAFETY: NOT thread-safe. Two coroutines calling SendPacket
    // concurrently on the same AsioSession will interleave their
    // sequence-number assignments and async_writes — both fatal for
    // the wire codec (sequence-number mismatch on the receiver, and
    // async_write is documented as not safe to call concurrently on
    // the same socket). Caller must serialize through a strand, a
    // single coroutine, or an explicit send queue. Phase-3 server
    // migration will add an internal send queue here.
    boost::asio::awaitable<void> SendPacket(std::uint16_t wId,
                                            std::span<const std::byte> body);

    // Close the socket. Idempotent. Safe to call from any thread that
    // owns the executor.
    void Close();

    // Enable RC4-over-entire-packet on the RECV side. After each inbound
    // packet is fully read (header + body), the entire buffer is
    // RC4-decrypted with a key derived from MD5(`secret_key`) — matching
    // what tnetlib_crypto::RC4MD5Transform does, which mirrors the
    // Win32 CryptDeriveKey(CALG_RC4) chain the legacy client encrypts
    // with. The 2-byte wSize field is preserved across RC4 because it
    // was used pre-RC4 for framing (legacy convention; see Session.cpp's
    // Decrypt).
    //
    // Server-side AsioSessions hosting a real legacy client should
    // enable this. Server-server links should NOT (legacy convention is
    // m_bUseCrypt=FALSE for server peers).
    void EnableInboundRC4(std::vector<std::byte> secret_key);

    // Enable RC4-over-entire-packet on the SEND side. Mirror of the
    // inbound case: each outbound packet's full frame is RC4-encrypted
    // with the MD5-derived key after the XOR header/body codec runs,
    // wSize preserved as plaintext on the wire.
    //
    // Used by tooling acting as a CLIENT against a legacy server (e.g.
    // a future packet-replay test bench). Server-side sessions hosting
    // a legacy client should NOT enable this — legacy server Encrypt is
    // XOR-only, no RC4 outbound.
    void EnableOutboundRC4(std::vector<std::byte> secret_key);

    boost::asio::ip::tcp::socket&       Socket()       { return m_socket; }
    const boost::asio::ip::tcp::socket& Socket() const { return m_socket; }
    PeerType                            Type()   const { return m_type;   }

    // Cached peer IPv4 (dotted-decimal). Captured at construction time
    // — calling remote_endpoint() after the socket has been shutdown
    // throws, and IP-banlist + audit-log lookups happen after that
    // point on some code paths. Empty string if the socket wasn't
    // connected (constructed from a default tcp::socket — only happens
    // in tests).
    const std::string& RemoteIPv4() const { return m_remote_ipv4; }

private:
    boost::asio::ip::tcp::socket m_socket;
    PeerType                     m_type;
    std::vector<std::byte>       m_recv_buffer;     // byte-level Run scratch
    std::vector<std::byte>       m_packet_buffer;   // RunPackets header+body scratch
    std::vector<std::byte>       m_send_buffer;     // SendPacket scratch
    std::uint32_t                m_recv_sequence = 0;
    std::uint32_t                m_send_sequence = 0;
    std::vector<std::byte>       m_rc4_inbound_key;   // empty = no RC4 on recv
    std::vector<std::byte>       m_rc4_outbound_key;  // empty = no RC4 on send
    std::string                  m_remote_ipv4;       // captured at ctor, empty if not connected
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
