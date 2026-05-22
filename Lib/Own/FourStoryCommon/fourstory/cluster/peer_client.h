#pragma once

// PeerClient — outbound side of the modern cluster control plane
// (counterpart to the CT_PEER_* handlers in TControlSvrAsio). Each
// peer game server (TLogin / TLog / TPatch / TMap / TWorld) links
// this library, constructs one PeerClient on startup, and co_spawns
// Run() onto its io_context. From that point on the server appears
// in TControl's live registry and stays there as long as its
// heartbeat loop keeps running.
//
// Lifecycle the coroutine drives:
//
//   1. async-connect to control_host:control_port
//   2. send CT_PEER_REGISTER_REQ
//   3. read CT_PEER_REGISTER_ACK
//      - accepted=1 → cache lease_epoch + heartbeat_interval_sec,
//        emit registered_callback, go to step 4
//      - accepted=0 → log reason, close socket, exponential backoff,
//        retry from step 1
//   4. sleep(heartbeat_interval_sec), then send
//      CT_PEER_HEARTBEAT_REQ and read CT_PEER_HEARTBEAT_ACK
//   5. on heartbeat reject or socket error: close, backoff, retry
//      from step 1 (the cached lease is discarded — a re-register
//      always issues a fresh epoch)
//
// Stop() sends CT_PEER_DEREGISTER_REQ when a lease is held, then
// closes the socket. The Run() coroutine returns after Stop() OR
// when the io_context drains.
//
// Wire framing is inlined here (8-byte header WORD+WORD+DWORD + body
// + XOR-fold checksum) to avoid pulling in the TControlSvrAsio
// ControlSession class. The two implementations are independent but
// must stay byte-for-byte compatible — covered by integration tests
// that drive a PeerClient against a real TControl loopback.

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <variant>

namespace fourstory::cluster {

// TOML-friendly slice of PeerClientOptions. Every peer server parses
// a `[cluster]` section into one of these; `MakePeerClientOptions`
// below fills in the per-binary bits (type byte, service name, port)
// and hands back a fully-configured PeerClientOptions.
struct ClusterConfig
{
    std::string   control_host;
    std::uint16_t control_port = 0;
    // service_id = (group<<16) | (type<<8) | server.  The type byte
    // is per-binary (each server hardcodes its own type), the
    // operator picks group + server to match the TSERVER row.
    std::uint8_t  group_id  = 1;
    std::uint8_t  server_id = 1;
    // Peer's externally-reachable IPv4. Falls back to the server's
    // listener bind address when empty.
    std::string   reported_addr;
};

struct PeerClientOptions
{
    // Where TControlSvrAsio is listening for the CT_* protocol.
    std::string   control_host;
    std::uint16_t control_port  = 0;

    // What we're announcing to the registry. service_id MUST match an
    // entry in TControl's static inventory (TSERVER row) — peers that
    // report an unknown id are rejected with reason_code=1.
    std::uint32_t service_id    = 0;
    std::string   reported_name;
    std::string   reported_addr;   // peer's externally-reachable IPv4
    std::uint16_t reported_port   = 0;
    std::string   version;
    std::uint32_t pid            = 0;
    std::int64_t  start_unix     = 0;

    // Reconnect backoff. Starts at initial_backoff, doubles up to
    // max_backoff. Reset to initial_backoff on every successful
    // registration so a flapping link doesn't get permanently stuck
    // on the cap.
    std::chrono::seconds initial_backoff{1};
    std::chrono::seconds max_backoff{30};

    // Optional TLS: when non-null, the PeerClient wraps its socket
    // in boost::asio::ssl::stream<tcp::socket> and drives a client-
    // side TLS handshake after the TCP connect, before the
    // CT_PEER_REGISTER_REQ goes out. Caller owns the context (built
    // via fourstory::security::PeerTlsContextBuilder::BuildClientContext)
    // and must keep it alive for the PeerClient's lifetime.
    //
    // null = plain TCP (current behaviour, default).
    boost::asio::ssl::context* ssl_ctx = nullptr;
};

// Callback to fetch the live cur_users / max_users counts for the
// heartbeat body. Default is constant zero — fine for servers that
// don't track user counts (TLog, TPatch).
using UserCountsFn =
    std::function<std::pair<std::uint32_t, std::uint32_t>()>;

class PeerClient : public std::enable_shared_from_this<PeerClient>
{
public:
    PeerClient(boost::asio::io_context& io, PeerClientOptions opts);

    // Run the connect → register → heartbeat → reconnect state
    // machine. Returns when Stop() is called (or the io_context
    // executor goes away). co_spawn this from the host server's main.
    boost::asio::awaitable<void> Run();

    void SetUserCountsFn(UserCountsFn fn) { m_user_counts = std::move(fn); }

    // Initiate graceful shutdown — sends CT_PEER_DEREGISTER_REQ if a
    // lease is currently held, then closes the socket. Run() returns
    // shortly after.
    void Stop();

    bool IsRegistered() const { return m_registered.load(); }
    std::uint64_t LeaseEpoch() const { return m_lease_epoch.load(); }

    // Compute the synthetic service_id used in the registry. Pure
    // helper exposed so tests can recompute it without going through
    // a full PeerClient construction.
    static std::uint32_t MakeServiceId(std::uint8_t group_id,
                                       std::uint8_t type_id,
                                       std::uint8_t server_id)
    {
        return (static_cast<std::uint32_t>(group_id) << 16) |
               (static_cast<std::uint32_t>(type_id)  <<  8) |
                static_cast<std::uint32_t>(server_id);
    }

private:
    boost::asio::awaitable<bool> ConnectAndRegister();
    boost::asio::awaitable<void> HeartbeatLoop();
    boost::asio::awaitable<bool> SendOneFrame(std::uint16_t wId,
                                              std::vector<std::byte> body);
    boost::asio::awaitable<bool> RecvOneFrame(std::uint16_t expected_wId,
                                              std::vector<std::byte>& out_body);

    using PlainSocket = boost::asio::ip::tcp::socket;
    using TlsStream   = boost::asio::ssl::stream<PlainSocket>;

    // Either-or transport: m_socket holds a plain tcp::socket
    // (m_opts.ssl_ctx == nullptr) or a TLS stream wrapping one
    // (m_opts.ssl_ctx != nullptr). Re-emplaced on every reconnect so
    // a torn-down socket can be replaced without juggling state in
    // the variant boundary.
    using SocketVariant = std::variant<PlainSocket, TlsStream>;

    // Helpers that pick the right async path based on which arm of
    // the variant is active. Read / write go through the active arm
    // directly (Boost.Asio's async_read / async_write accept any
    // AsyncStream); connect / close / cancel operate on the
    // underlying tcp::socket via next_layer() for the TLS arm.
    PlainSocket& UnderlyingTcp();

    // Build the SocketVariant in its initial (disconnected) state.
    // Called once from the ctor and again on every reconnect so the
    // active variant arm matches the configured transport. ssl_ctx is
    // the same pointer stored in m_opts (just passed explicitly here
    // because the helper is also called during member init).
    static SocketVariant
    MakeInitialSocket(boost::asio::io_context& io,
                      boost::asio::ssl::context* ssl_ctx);

    boost::asio::io_context&        m_io;
    PeerClientOptions               m_opts;
    SocketVariant                   m_socket;
    UserCountsFn                    m_user_counts;
    std::atomic<bool>               m_registered{false};
    std::atomic<std::uint64_t>      m_lease_epoch{0};
    std::atomic<std::uint32_t>      m_heartbeat_interval_sec{30};
    std::atomic<bool>               m_stop{false};
};

// One-liner used by every peer server's main.cpp to translate a
// parsed TOML [cluster] block into a registry-ready options bag.
// Centralized so the byte-fiddling (group/type/server composition,
// reported_addr fallback, service-name format) stays consistent
// across all five binaries.
inline PeerClientOptions
MakePeerClientOptions(const ClusterConfig& cfg,
                      std::uint8_t  type_id,
                      const std::string& name_prefix,
                      const std::string& listener_bind,
                      std::uint16_t      listener_port,
                      const std::string& version)
{
    PeerClientOptions o;
    o.control_host  = cfg.control_host;
    o.control_port  = cfg.control_port;
    o.service_id    = PeerClient::MakeServiceId(cfg.group_id, type_id,
                                                cfg.server_id);
    o.reported_name = name_prefix + "-" + std::to_string(cfg.group_id)
                    + "-" + std::to_string(cfg.server_id);
    o.reported_addr = cfg.reported_addr.empty() ? listener_bind
                                                : cfg.reported_addr;
    o.reported_port = listener_port;
    o.version       = version;
    return o;
}

} // namespace fourstory::cluster
