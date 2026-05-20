#pragma once

// AsioWorldClient — persistent outbound TCP connection to TWorldSvr.
//
// Dials the configured host:port on Start(), reconnects with
// exponential back-off on loss, dispatches incoming DM_* / MW_*
// packets to handlers_world.h. The same IPlayerService used by the
// standalone path (F2b Part 2) is called when DM_LOADCHAR_REQ arrives.
//
// Wire: server-to-server (PeerType::Server — no RC4 encryption).
//
// Reconnect policy: 1s → 2s → 4s → 8s → cap 30s.
//
// Source: Server/TControlSvrAsio/peer_dialer.cpp — dial pattern.

#include "asio_session.h"
#include "handlers_world.h"
#include "services/world_client.h"
#include "services/player_service.h"
#include "wire_codec.h"
#include "MessageId.h"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/connect.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

#include <spdlog/spdlog.h>

namespace tmapsvr {

struct AsioWorldClientConfig
{
    std::string    world_host;
    std::uint16_t  world_port        = 3815;    // default TWorldSvr port
    std::chrono::milliseconds connect_timeout{5000};
    std::chrono::seconds      reconnect_min{1};
    std::chrono::seconds      reconnect_max{30};
};

class AsioWorldClient : public IWorldClient
{
public:
    AsioWorldClient(boost::asio::io_context& io,
                    AsioWorldClientConfig    cfg,
                    IPlayerService&          player_svc)
        : m_io(io)
        , m_cfg(std::move(cfg))
        , m_player_svc(player_svc)
    {}

    // Spawn the dial+reconnect loop. Non-blocking — returns immediately.
    void Start()
    {
        boost::asio::co_spawn(m_io, Run(), boost::asio::detached);
    }

    // IWorldClient
    void SendMwAddCharAck(std::uint32_t char_id,
                          std::uint32_t dw_key,
                          std::uint32_t client_ip_raw,
                          std::uint16_t client_port,
                          std::uint32_t user_id) override
    {
        auto sess = m_sess;
        if (!sess) { spdlog::warn("SendMwAddCharAck: not connected"); return; }

        std::vector<std::byte> body;
        wire::WritePOD<std::uint32_t>(body, char_id);
        wire::WritePOD<std::uint32_t>(body, dw_key);
        wire::WritePOD<std::uint32_t>(body, client_ip_raw);
        wire::WritePOD<std::uint16_t>(body, client_port);
        wire::WritePOD<std::uint32_t>(body, user_id);

        using tnetlib::protocol::MessageId;
        using tnetlib::protocol::ToUint16;
        boost::asio::co_spawn(m_io,
            [sess, body = std::move(body)]() -> boost::asio::awaitable<void> {
                co_await sess->SendPacket(
                    ToUint16(MessageId::MW_ADDCHAR_ACK),
                    std::span<const std::byte>(body.data(), body.size()));
            },
            boost::asio::detached);
    }

    void SendDmLoadCharAck(std::uint32_t            char_id,
                           std::uint32_t            dw_key,
                           const legacy::CharSnapshot* snap) override
    {
        auto sess = m_sess;
        if (!sess) { spdlog::warn("SendDmLoadCharAck: not connected"); return; }

        // Build body via the shared helper in handlers_world.cpp
        // (re-implemented inline here to avoid exposing BuildDmLoadCharAckBody).
        // The actual ACK body serialization is in BuildDmLoadCharAckBody in
        // handlers_world.cpp — for AsioWorldClient we post a coroutine that
        // calls OnDmLoadCharReq's result path via the handler layer so the
        // serialization isn't duplicated.
        //
        // For now: build a minimal ACK (CN_NOCHAR if snap==null, else
        // CN_SUCCESS + 3-field header only) as a placeholder.
        // F2b Part 4 wires in the full BuildDmLoadCharAckBody path.
        constexpr std::uint8_t CN_SUCCESS = 0;
        constexpr std::uint8_t CN_NOCHAR  = 2;

        std::vector<std::byte> body;
        wire::WritePOD<std::uint32_t>(body, char_id);
        wire::WritePOD<std::uint32_t>(body, dw_key);
        wire::WritePOD<std::uint8_t>(body, snap ? CN_SUCCESS : CN_NOCHAR);

        using tnetlib::protocol::MessageId;
        using tnetlib::protocol::ToUint16;
        boost::asio::co_spawn(m_io,
            [sess, body = std::move(body)]() -> boost::asio::awaitable<void> {
                co_await sess->SendPacket(
                    ToUint16(MessageId::DM_LOADCHAR_ACK),
                    std::span<const std::byte>(body.data(), body.size()));
            },
            boost::asio::detached);
    }

    bool IsConnected() const override { return m_sess != nullptr; }

    // F2b Part 4: pending session registry + pre-load cache.
    // All methods run on the same io_context thread — no mutex needed.

    void RegisterPendingSession(std::uint32_t char_id,
                                std::shared_ptr<tnetlib::AsioSession> sess,
                                std::weak_ptr<MapSessionState> state_weak) override
    {
        m_pending[char_id] = { std::move(sess), state_weak };
    }

    void CancelPendingSession(std::uint32_t char_id) override
    {
        m_pending.erase(char_id);
    }

    void StorePreloadedChar(legacy::CharSnapshot snap) override
    {
        const auto id = snap.char_id;
        m_preloaded[id] = std::move(snap);
    }

    std::optional<legacy::CharSnapshot>
    TakePreloadedChar(std::uint32_t char_id,
                      std::uint32_t user_id,
                      std::uint32_t dw_key) override
    {
        auto it = m_preloaded.find(char_id);
        if (it == m_preloaded.end()) return std::nullopt;
        const auto& s = it->second;
        if (s.user_id != user_id || s.dw_key != dw_key) return std::nullopt;
        auto snap = std::move(it->second);
        m_preloaded.erase(it);
        return snap;
    }

private:
    struct PendingEntry {
        std::shared_ptr<tnetlib::AsioSession> sess;
        std::weak_ptr<MapSessionState>        state_weak;
    };

    std::unordered_map<std::uint32_t, PendingEntry>          m_pending;
    std::unordered_map<std::uint32_t, legacy::CharSnapshot>  m_preloaded;
    boost::asio::awaitable<void> Run()
    {
        auto delay = m_cfg.reconnect_min;
        while (true)
        {
            co_await Dial();
            if (m_sess)
            {
                delay = m_cfg.reconnect_min;    // reset on success
                co_await RunSession();
            }

            m_sess.reset();
            spdlog::info("world_client: disconnected — retry in {}s",
                delay.count());

            boost::asio::steady_timer t(m_io);
            t.expires_after(delay);
            boost::system::error_code ec;
            co_await t.async_wait(
                boost::asio::redirect_error(boost::asio::use_awaitable, ec));

            delay = std::min(delay * 2, m_cfg.reconnect_max);
        }
    }

    boost::asio::awaitable<void> Dial()
    {
        using tcp = boost::asio::ip::tcp;
        tcp::resolver resolver(m_io);
        boost::system::error_code ec;

        auto endpoints = co_await resolver.async_resolve(
            m_cfg.world_host,
            std::to_string(m_cfg.world_port),
            boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        if (ec)
        {
            spdlog::warn("world_client: resolve {}:{} failed: {}",
                m_cfg.world_host, m_cfg.world_port, ec.message());
            co_return;
        }

        tcp::socket sock(m_io);
        boost::asio::steady_timer deadline(m_io);
        deadline.expires_after(m_cfg.connect_timeout);
        bool timed_out = false;
        deadline.async_wait([&sock, &timed_out](auto err) {
            if (err) return;
            timed_out = true;
            boost::system::error_code ig;
            sock.close(ig);
        });

        co_await boost::asio::async_connect(
            sock, endpoints,
            boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        deadline.cancel();

        if (ec || timed_out)
        {
            spdlog::warn("world_client: connect {}:{} failed: {}",
                m_cfg.world_host, m_cfg.world_port,
                timed_out ? "timeout" : ec.message());
            co_return;
        }

        m_sess = std::make_shared<tnetlib::AsioSession>(
            std::move(sock), tnetlib::PeerType::Server);
        spdlog::info("world_client: connected to {}:{}",
            m_cfg.world_host, m_cfg.world_port);
    }

    boost::asio::awaitable<void> RunSession()
    {
        WorldHandlerContext ctx{};
        ctx.player_service = &m_player_svc;
        ctx.world_client   = this;

        try
        {
            co_await m_sess->RunPackets(
                [this, &ctx](const tnetlib::DecodedPacket& pkt) {
                    auto s = m_sess;
                    boost::asio::co_spawn(m_io,
                        DispatchWorld(s, pkt, ctx),
                        boost::asio::detached);
                });
        }
        catch (const std::exception& ex)
        {
            spdlog::warn("world_client: session error: {}", ex.what());
        }
    }

    boost::asio::io_context&                    m_io;
    AsioWorldClientConfig                       m_cfg;
    IPlayerService&                             m_player_svc;
    std::shared_ptr<tnetlib::AsioSession>       m_sess;
};

} // namespace tmapsvr
