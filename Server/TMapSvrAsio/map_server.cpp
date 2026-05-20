#include "map_server.h"
#include "handlers_map.h"
#include "spawn_manager.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <spdlog/spdlog.h>

#include <exception>
#include <utility>

namespace tmapsvr {

MapServer::MapServer(boost::asio::io_context& io, MapServerConfig cfg)
    : m_io(io)
    , m_listener(io.get_executor(), cfg.port)
    , m_cfg(std::move(cfg))
{
    m_ctx.validator          = m_cfg.validator;
    m_ctx.accepted_versions  = &m_cfg.accepted_versions;
    m_ctx.on_quit_request    = m_cfg.on_quit_request;
    m_ctx.player_service     = m_cfg.player_service;
    m_ctx.world_client       = m_cfg.world_client;
    m_ctx.map_state          = m_cfg.map_state;
    m_ctx.session_registry   = m_cfg.session_registry;
    m_ctx.monster_registry   = m_cfg.monster_registry;
    m_ctx.level_chart        = m_cfg.level_chart;
    m_ctx.spawn_manager      = m_cfg.spawn_manager;
    m_ctx.live_session_count = [this]() -> std::uint32_t {
        return LiveSessions();
    };
}

boost::asio::awaitable<void>
MapServer::Run()
{
    // F4: start monster spawn manager after the listener is ready.
    if (m_cfg.spawn_manager)
        m_cfg.spawn_manager->Start();

    co_await m_listener.Run([this](boost::asio::ip::tcp::socket socket) {
        // Pre-auth flood gate. Counter is decremented in HandleConnection
        // after the per-session coroutine finishes.
        if (m_cfg.max_connections > 0)
        {
            const auto current = m_active_connections.load(std::memory_order_relaxed);
            if (current >= m_cfg.max_connections)
            {
                boost::system::error_code peer_ec;
                const auto peer = socket.remote_endpoint(peer_ec);
                spdlog::warn("max_connections cap reached ({} >= {}); "
                    "dropping accept from {}",
                    current, m_cfg.max_connections,
                    peer_ec ? std::string{"<unknown>"} : peer.address().to_string());
                boost::system::error_code ignored;
                socket.close(ignored);
                return;
            }
        }

        // Per-IP rate limit on inbound CONNECT. The token bucket is
        // shared across sessions; tripping the limit closes the new
        // socket before the per-session coroutine starts (and before
        // the rate-limiter exposes a session-state side effect).
        if (m_cfg.connect_rate)
        {
            boost::system::error_code peer_ec;
            const auto peer = socket.remote_endpoint(peer_ec);
            const std::string peer_ip = peer_ec ? std::string{}
                : peer.address().to_string();
            if (!peer_ip.empty() && !m_cfg.connect_rate->Allow(peer_ip))
            {
                spdlog::warn("connect rate limit tripped for {}; dropping",
                    peer_ip);
                boost::system::error_code ignored;
                socket.close(ignored);
                return;
            }
        }

        m_active_connections.fetch_add(1, std::memory_order_relaxed);

        const auto peer_type = m_cfg.rc4_secret_key.empty()
            ? tnetlib::PeerType::Server
            : tnetlib::PeerType::Client;
        auto sess = std::make_shared<tnetlib::AsioSession>(
            std::move(socket), peer_type);
        if (!m_cfg.rc4_secret_key.empty())
            sess->EnableInboundRC4(m_cfg.rc4_secret_key);

        boost::asio::co_spawn(m_io,
            HandleConnection(sess),
            [this](std::exception_ptr ep) {
                m_active_connections.fetch_sub(1, std::memory_order_relaxed);
                if (ep)
                {
                    try { std::rethrow_exception(ep); }
                    catch (const std::exception& ex)
                    {
                        spdlog::error("session terminated with exception: {}",
                            ex.what());
                    }
                }
            });
    });
}

boost::asio::awaitable<void>
MapServer::HandleConnection(std::shared_ptr<tnetlib::AsioSession> sess)
{
    // F2b Part 4: heap-allocate state so AsioWorldClient can hold a
    // weak_ptr for routing MW_CONRESULT_REQ back to this session.
    // All handlers still take MapSessionState& via the reference below
    // — no handler signature changes needed.
    auto state_holder = std::make_shared<MapSessionState>();
    MapSessionState& state = *state_holder;

    // Per-session copy of the server context so we can inject the
    // weak_ptr without touching the shared m_ctx.
    HandlerContext local_ctx       = m_ctx;
    local_ctx.session_state_weak   = std::weak_ptr<MapSessionState>(state_holder);

    auto exec = co_await boost::asio::this_coro::executor;

    // Pre-auth watchdog. If we don't see a successful CS_CONNECT_REQ
    // within the timeout, close the socket so the read loop tears down
    // naturally. The watchdog only fires before `state.connected` flips
    // — once the player is in the world, idle is allowed.
    auto watchdog = std::make_shared<boost::asio::steady_timer>(exec);
    bool timer_fired = false;
    if (m_cfg.pre_auth_timeout.count() > 0)
    {
        watchdog->expires_after(m_cfg.pre_auth_timeout);
        boost::asio::co_spawn(exec,
            [sess, watchdog, &state, &timer_fired]() -> boost::asio::awaitable<void> {
                boost::system::error_code ec;
                co_await watchdog->async_wait(
                    boost::asio::redirect_error(boost::asio::use_awaitable, ec));
                if (ec) co_return;          // cancelled by success path
                if (state.connected) co_return;  // raced past us
                timer_fired = true;
                spdlog::info("pre-auth timeout — closing {}", sess->RemoteIPv4());
                sess->Close();
            },
            boost::asio::detached);
    }

    // Read loop. Each decoded packet routes through Dispatch. Errors
    // (framing, RC4 desync, checksum mismatch) terminate the loop;
    // AsioSession::Close() is idempotent so the watchdog branch is
    // safe.
    try
    {
        co_await sess->RunPackets(
            [this, sess, &state, &local_ctx](const tnetlib::DecodedPacket& pkt) {
                boost::asio::co_spawn(sess->Socket().get_executor(),
                    Dispatch(sess, state, pkt, local_ctx),
                    boost::asio::detached);
            });
    }
    catch (const std::exception& ex)
    {
        spdlog::warn("read loop terminated for uid={}: {}",
            state.user_id, ex.what());
    }

    if (m_cfg.pre_auth_timeout.count() > 0)
        watchdog->cancel();

    // F3 teardown: remove from AOI cell grid + session registry.
    // Broadcast CS_LEAVE_ACK to all AOI neighbours so they can
    // despawn the departing player from their client.
    if (state.in_world)
    {
        if (m_cfg.map_state)
        {
            const auto notify_ids = m_cfg.map_state->LeaveMap(state.char_id);
            for (std::uint32_t nid : notify_ids)
            {
                auto nbr = m_cfg.session_registry
                           ? m_cfg.session_registry->Get(nid)
                           : std::shared_ptr<tnetlib::AsioSession>{};
                if (nbr)
                {
                    const auto cid = state.char_id;
                    boost::asio::co_spawn(exec,
                        [nbr, cid]() -> boost::asio::awaitable<void> {
                            co_await SendLeaveAck(nbr, cid);
                        },
                        boost::asio::detached);
                }
            }
        }
        if (m_cfg.session_registry)
            m_cfg.session_registry->Unregister(state.char_id);
    }

    // F2b: cancel any pending world-client registration.
    if (m_cfg.world_client && state.char_id != 0)
        m_cfg.world_client->CancelPendingSession(state.char_id);

    spdlog::info("session closed uid={} char={} in_world={} ({})",
        state.user_id, state.char_id, state.in_world,
        timer_fired ? "pre-auth-timeout" : "peer-closed");
}

} // namespace tmapsvr
