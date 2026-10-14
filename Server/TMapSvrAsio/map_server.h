#pragma once

// TMapSvrAsio top-level server. Owns the TCP acceptor, spins up an
// AsioSession per inbound connection, and dispatches each decoded
// packet to handlers.cpp (added in F4).
//
// Modeled on tloginsvr::LoginServer + tpatchsvr::PatchServer. The
// session-registry pattern (weak_ptr vector + sweep) is copied
// directly from PatchServer so the F17 control / sweep paths drop
// into the same shape later.

#include "asio_session.h"
#include "handlers.h"
#include "services/control_session.h"
#include "fourstory/security/ip_allowlist.h"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <mutex>
#include <vector>

namespace tmapsvr {

struct MapServerConfig
{
    std::uint16_t           port = 5815;          // DEF_MAPPORT
    // RC4 key shared with the legacy client; empty disables crypto
    // (dev / netcat smoke tests run plain wire). Same convention as
    // tloginsvr::LoginServerConfig::rc4_secret_key.
    std::vector<std::byte>  rc4_secret_key;
    std::uint32_t           max_connections = 8000;

    // Native clients require an acknowledged World registration.
    bool require_registered_world = false;
    // Source fixed-Control-IP role selection, explicitly configured and
    // denied by default. It never depends on a client opcode/claimed role.
    std::vector<std::string> control_ip_allowlist;
    std::function<boost::asio::awaitable<bool>()> check_control_owner;

    // T5 pre-auth watchdog. After this many seconds, sessions that
    // haven't yet completed accepted CS_CONNECT identity/claim get their
    // socket closed. Accepted World loading is excluded. Zero disables.
    std::uint32_t           pre_auth_timeout_seconds = 30;

    // Per-packet handler context — owns no state, just non-owning
    // pointers to services (validator, …) main built. Copied into
    // MapServer at construction; the caller's struct lifetime must
    // dominate the io_context's run().
    HandlerContext          handlers;
};

class MapServer
{
public:
    MapServer(boost::asio::io_context& io, MapServerConfig config);

    boost::asio::awaitable<void> Run();

    // T5 graceful shutdown — closes the acceptor so the Run() loop
    // exits after the current async_accept returns. Existing
    // per-connection coroutines keep running until they finish or
    // hit the drain timeout from main().
    void StopAccepting();
    void CloseSessions();

    std::uint16_t Port() const { return m_port; }

    // Session-registry hooks. Public so F17 / tests can drive the
    // sweep without standing up the full accept loop.
    void Register(std::shared_ptr<tnetlib::AsioSession> session);
    void Unregister(tnetlib::AsioSession* raw);

    std::uint32_t FailedSaves() const { return m_failed_save_count.load(); }

    std::uint32_t LiveSessions() const { return m_active_connections.load(); }
    std::uint32_t ControlOperations() const {return m_control_operations.load();}
    bool IsCurrentControl(const std::shared_ptr<ControlSession>&) const;
    boost::asio::awaitable<bool> CheckControl(const std::shared_ptr<ControlSession>&);
    void SweepControlClients();
    std::uint32_t SourceSessionCount() const {return LiveSessions()+ControlOperations();}

private:
    boost::asio::awaitable<void> HandleConnection(
        std::shared_ptr<tnetlib::AsioSession> session);
    boost::asio::awaitable<void> HandleControl(std::shared_ptr<ControlSession>);

    boost::asio::io_context&        m_io;
    boost::asio::ip::tcp::acceptor  m_acceptor;
    std::uint16_t                   m_port;
    MapServerConfig                 m_cfg;
    std::atomic<std::uint32_t>      m_active_connections{0};
    std::atomic<std::uint32_t>      m_failed_save_count{0};
    std::atomic<std::uint32_t> m_control_operations{0};
    fourstory::security::IpAllowlist m_control_allowlist;
    std::shared_ptr<ControlSession> m_control;
    std::optional<std::uint32_t> m_last_control_sweep;

    std::vector<std::shared_ptr<tnetlib::AsioSession>> m_failed_saves;
    std::mutex                                            m_sessions_mtx;
    struct ClientTransport {std::weak_ptr<tnetlib::AsioSession> session;std::uint32_t accepted_at{},closing_at{};};
    std::vector<ClientTransport> m_sessions;
};

} // namespace tmapsvr
