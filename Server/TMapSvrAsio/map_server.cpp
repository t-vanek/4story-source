#include "services/main_transfer_runtime.h"
#include "map_server.h"

#include "services/channel_presence.h"
#include "services/char_state_store.h"
#include "services/player_service.h"
#include "services/session_validator.h"
#include "services/world_client.h"
#include "services/world_senders.h"
#include "MessageId.h"
#include "services/rate_limiter.h"
#include "services/session_registry.h"
#include "services/skill_cooldown.h"

#include "fourstory/db/co_offload.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <exception>
#include <utility>

namespace tmapsvr {

MapServer::MapServer(boost::asio::io_context& io, MapServerConfig config)
    : m_io(io)
    , m_acceptor(io)
    , m_port(config.port)
    , m_cfg(std::move(config))
{
    using boost::asio::ip::tcp;
    tcp::endpoint ep(tcp::v4(), m_port);
    m_acceptor.open(ep.protocol());
    m_acceptor.set_option(tcp::acceptor::reuse_address(true));
    m_acceptor.bind(ep);
    m_acceptor.listen();
    m_port = m_acceptor.local_endpoint().port();
}

boost::asio::awaitable<void>
MapServer::Run()
{
    using boost::asio::ip::tcp;

    while (m_acceptor.is_open())
    {
        boost::system::error_code ec;
        tcp::socket sock = co_await m_acceptor.async_accept(
            boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        if (ec) break;

        if (m_cfg.require_registered_world &&
            (!m_cfg.handlers.world_client || !m_cfg.handlers.world_client->IsRegistered())) {
            boost::system::error_code ignored; sock.close(ignored);
            continue;
        }

        // max_connections gate — drop the new accept rather than queueing
        // it so the client gets an immediate RST and can retry against
        // another channel instead of waiting on a half-open socket.
        const auto current = m_active_connections.load(std::memory_order_relaxed);
        if (current >= m_cfg.max_connections)
        {
            boost::system::error_code peer_ec;
            const auto peer = sock.remote_endpoint(peer_ec);
            spdlog::warn("map_server: max_connections cap reached ({} >= {}); "
                         "dropping new accept from {}",
                current, m_cfg.max_connections,
                peer_ec ? std::string{"<unknown>"} : peer.address().to_string());
            boost::system::error_code ignored;
            sock.close(ignored);
            continue;
        }

        const auto peer = m_cfg.rc4_secret_key.empty()
            ? tnetlib::PeerType::Server
            : tnetlib::PeerType::Client;
        auto sess = std::make_shared<tnetlib::AsioSession>(
            std::move(sock), peer);

        // Legacy convention (matches CSession::Decrypt RC4 path /
        // CSession::Encrypt XOR-only path): inbound RC4 on, outbound
        // XOR-only. Toggle off entirely for plain-wire test runs.
        if (!m_cfg.rc4_secret_key.empty())
            sess->EnableInboundRC4(m_cfg.rc4_secret_key);

        Register(sess);
        m_active_connections.fetch_add(1, std::memory_order_relaxed);

        // Absorb any exception that escapes the per-connection coroutine
        // so co_spawn's detached-rethrow doesn't terminate the whole
        // io_context.
        boost::asio::co_spawn(
            m_io,
            HandleConnection(sess),
            [this, sess](std::exception_ptr ep) {
                if (ep)
                {
                    try { std::rethrow_exception(ep); }
                    catch (const std::exception& ex) {
                        spdlog::error("map_server: connection coroutine threw: {}",
                            ex.what());
                    }
                }
                m_active_connections.fetch_sub(1, std::memory_order_relaxed);
                Unregister(sess.get());
            });
    }
}

void MapServer::StopAccepting()
{
    boost::system::error_code ignored;
    m_acceptor.close(ignored);
}

void MapServer::Register(std::shared_ptr<tnetlib::AsioSession> session)
{
    std::lock_guard<std::mutex> lk(m_sessions_mtx);
    m_sessions.erase(
        std::remove_if(m_sessions.begin(), m_sessions.end(),
            [](const std::weak_ptr<tnetlib::AsioSession>& w) { return w.expired(); }),
        m_sessions.end());
    m_sessions.emplace_back(std::move(session));
}

void MapServer::Unregister(tnetlib::AsioSession* raw)
{
    std::lock_guard<std::mutex> lk(m_sessions_mtx);
    m_sessions.erase(
        std::remove_if(m_sessions.begin(), m_sessions.end(),
            [raw](const std::weak_ptr<tnetlib::AsioSession>& w)
            {
                auto sp = w.lock();
                return !sp || sp.get() == raw;
            }),
        m_sessions.end());
}

boost::asio::awaitable<void>
MapServer::HandleConnection(std::shared_ptr<tnetlib::AsioSession> session)
{
    auto watchdog = std::make_shared<boost::asio::steady_timer>(m_io);
    if (m_cfg.pre_auth_timeout_seconds > 0) {
        watchdog->expires_after(std::chrono::seconds(m_cfg.pre_auth_timeout_seconds));
        watchdog->async_wait([session, watchdog, reg = m_cfg.handlers.session_reg](auto ec) {
            if (ec || !session->IsOpen()) return;
            const auto id = reg ? reg->Identity(session.get()) : std::nullopt;
            if (!id || (id->phase != SessionPhase::Admitted && id->phase != SessionPhase::Ready))
                session->Close();
        });
    }
    try {
        co_await session->RunPacketsAsync([this, session](tnetlib::DecodedPacket pkt)
            -> boost::asio::awaitable<void> {
            co_await Dispatch(session, pkt.wId,
                std::vector<std::byte>(pkt.body.begin(), pkt.body.end()), m_cfg.handlers);
        });
    } catch (...) {
        spdlog::error("map_server: packet loop failed");
        session->Close();
    }
    watchdog->cancel();

    const auto& ctx = m_cfg.handlers;
    std::optional<SessionIdentity> identity;
    if(ctx.session_reg) {
        boost::asio::steady_timer drain(m_io);
        do {
            identity=ctx.session_reg->BeginClose(session.get());
            if(identity||!ctx.session_reg->Identity(session.get()))break;
            drain.expires_after(std::chrono::milliseconds(5));co_await drain.async_wait(boost::asio::use_awaitable);
        }while(true);
    }
    if (identity) {
        if((identity->phase==SessionPhase::Ready||identity->phase==SessionPhase::TransferOut) && identity->role==MapSessionRole::Primary &&
           (!ctx.char_state||!ctx.player_service||!ctx.char_state->Get(identity->char_id))){
            if(ctx.presence)ctx.presence->UnbindIfMatches(session.get());
            spdlog::error("map_server: ready session has no saveable snapshot; reservation retained");
            std::lock_guard lock(m_sessions_mtx);m_failed_saves.push_back(session);m_failed_save_count.fetch_add(1);co_return;
        }
        // Presence becomes invisible before a potentially slow database write.
        if ((identity->phase == SessionPhase::Ready||identity->phase==SessionPhase::TransferOut) && identity->role == MapSessionRole::Primary && ctx.char_state && ctx.player_service) {
            if (ctx.presence && identity->phase==SessionPhase::Ready) {
                const auto entry = ctx.presence->FindEntry(identity->char_id);
                if (entry) ctx.char_state->Update(identity->char_id, [&entry](CharSnapshot& snap) {
                    snap.fPosX = entry->pos.x; snap.fPosY = entry->pos.y;
                    snap.fPosZ = entry->pos.z; snap.wMapID = entry->map_id;
                });
            }
            if (ctx.presence) ctx.presence->UnbindIfMatches(session.get());
            if (auto snap = ctx.char_state->Get(identity->char_id)) {
                bool saved = false;
                try {
                if(ctx.skill_cooldown&&identity->phase!=SessionPhase::TransferOut)
                    *snap=transfer::PersistenceSnapshot(*snap,identity->key,*ctx.skill_cooldown,SkillClockMs());
                    auto* player = ctx.player_service;
                    co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
                        [player, &snap, claim=identity->Claim(ctx.expected_group)] { player->SaveAuthorized(claim,*snap); });
                    saved = true;
                } catch (...) { spdlog::error("map_server: character save failed; reservation retained char={}", identity->char_id); }
                if (!saved) {
                    // Retain the snapshot and closed-session reservation for operator
                    // recovery. Reconnecting must not load stale data over dirty state.
                    std::lock_guard lock(m_sessions_mtx);
                    m_failed_saves.push_back(session);
                    m_failed_save_count.fetch_add(1);
                    co_return;
                }
            }
        }
        if(identity->role==MapSessionRole::Replica && ctx.presence)ctx.presence->UnbindIfMatches(session.get());
        if (ctx.validator) {
            try {
                auto* validator=ctx.validator;const auto claim=identity->Claim(ctx.expected_group);
                co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,[validator,claim] { validator->ReleaseSession(claim); });
            } catch (...) {
                spdlog::error("map_server: session release failed; reservation retained char={}",identity->char_id);
                std::lock_guard lock(m_sessions_mtx);
                m_failed_saves.push_back(session);m_failed_save_count.fetch_add(1);co_return;
            }
        }
        // Re-read after database suspension: a World retirement may have
        // arrived while local teardown was draining. Failed claims were never
        // announced and cannot close a valid character on another Map.
        const auto current = ctx.session_reg->Identity(session.get());
        if(current && current->world_presence==WorldPresence::Announced &&
           ctx.world_client && ctx.world_client->IsRegistered())
            co_await ctx.world_client->SendPacket(static_cast<std::uint16_t>(tnetlib::protocol::MessageId::MW_CLOSECHAR_ACK),
                EncodeEnterCharAck(identity->char_id,identity->key));
        if (ctx.char_state) ctx.char_state->Remove(identity->char_id);
        if (ctx.skill_cooldown) ctx.skill_cooldown->Forget(identity->char_id);
    }
    if (ctx.presence) ctx.presence->UnbindIfMatches(session.get());
    if (ctx.session_reg) ctx.session_reg->UnbindIfMatches(session.get());
    if (ctx.rate_limiter) ctx.rate_limiter->Remove(reinterpret_cast<std::uint64_t>(session.get()));
}

void MapServer::CloseSessions()
{
    std::lock_guard lock(m_sessions_mtx);
    for (const auto& weak : m_sessions)
        if (auto session = weak.lock()) session->Close();
}

} // namespace tmapsvr
