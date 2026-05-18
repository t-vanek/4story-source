// PCH-free; depends only on TNetLib's portable surface (AsioSession,
// AsioListener, MessageId) plus Boost.Asio.

#include "login_server.h"
#include "handlers.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>

#include <spdlog/spdlog.h>

#include <cstdio>
#include <utility>

namespace tloginsvr {

LoginServer::LoginServer(boost::asio::io_context& io, LoginServerConfig config)
    : m_io(io)
    , m_listener(io.get_executor(), config.port)
    , m_rc4_secret_key(std::move(config.rc4_secret_key))
    , m_auth_service(config.auth_service)
    , m_connection_registry(config.connection_registry)
    , m_map_server_locator(config.map_server_locator)
    , m_session_terminator(config.session_terminator)
    , m_char_service(config.char_service)
{
}

LoginServer::LoginServer(boost::asio::io_context& io, std::uint16_t port)
    : LoginServer(io, LoginServerConfig{ .port = port })
{
}

std::uint16_t LoginServer::Port() const
{
    return m_listener.Port();
}

boost::asio::awaitable<void>
LoginServer::Run()
{
    co_await m_listener.Run([this](boost::asio::ip::tcp::socket socket) {
        // PeerType::Client if we're running with the legacy-client
        // secret configured (Phase 2.5 wire compat); PeerType::Server
        // for the modernized peer test mode. The peer type label
        // doesn't change behavior here — we toggle RC4 explicitly
        // via EnableInboundRC4 — but keeping it semantically right
        // means future code reading sess->Type() gets the truthful
        // answer about "what kind of peer is on the other side".
        const auto peer = m_rc4_secret_key.empty()
            ? tnetlib::PeerType::Server
            : tnetlib::PeerType::Client;
        auto sess = std::make_shared<tnetlib::AsioSession>(
            std::move(socket), peer);

        if (!m_rc4_secret_key.empty())
        {
            // Server-side: legacy convention is RC4 inbound only,
            // XOR-only outbound (matches CSession::Decrypt RC4 path
            // vs CSession::Encrypt XOR-only path).
            sess->EnableInboundRC4(m_rc4_secret_key);
        }

        boost::asio::co_spawn(
            m_io,
            HandleConnection(sess),
            boost::asio::detached);
    });
}

boost::asio::awaitable<void>
LoginServer::HandleConnection(std::shared_ptr<tnetlib::AsioSession> sess)
{
    // RunPackets fires the handler synchronously per packet, but the
    // handler we want to invoke is awaitable (it issues SendPacket).
    // Spawn a per-packet coroutine instead of trying to do it inline,
    // so the codec read loop isn't blocked by send completions.
    co_await sess->RunPackets(
        [this, sess](const tnetlib::DecodedPacket& packet) {
            boost::asio::co_spawn(
                m_io,
                Dispatch(sess, packet),
                boost::asio::detached);
        });

    // Connection closing — drive the per-session cleanup chain:
    // 1. Look up the entry stamped at LOGIN-success time (if any).
    //    Unauthenticated sessions have no entry → skip everything.
    // 2. If a terminator is wired, call Terminate with the entry's
    //    user_id + session_key. The MapHandoff flag flips the reason
    //    so the impl can preserve TCURRENTUSER for Map's dwKEY
    //    validation (legacy CSHandler.cpp:1428 behavior).
    // 3. Always unregister from the connection registry.
    if (m_connection_registry)
    {
        const auto entry = m_connection_registry->Lookup(sess);
        if (entry && m_session_terminator)
        {
            const auto reason = entry->handoff_to_map
                ? services::TerminationReason::MapHandoff
                : services::TerminationReason::Disconnect;
            m_session_terminator->Terminate(
                entry->user_id, entry->session_key, reason);
        }
        m_connection_registry->Unregister(sess);
    }
}

boost::asio::awaitable<void>
LoginServer::Dispatch(std::shared_ptr<tnetlib::AsioSession> sess,
                      const tnetlib::DecodedPacket& packet)
{
    using tnetlib::protocol::MessageId;
    using tnetlib::protocol::ToMessageId;

    // Copy the body once — `packet.body` is only valid for the
    // duration of the RunPackets callback that produced this Dispatch
    // call, and we've already returned from it via the co_spawn.
    std::vector<std::byte> body(packet.body.begin(), packet.body.end());
    const auto id = ToMessageId(packet.wId);

    switch (id)
    {
    case MessageId::CS_LOGIN_REQ:
        co_await handlers::OnLoginReq(sess, body, m_auth_service, m_connection_registry);
        break;
    case MessageId::CS_GROUPLIST_REQ:
        co_await handlers::OnGroupListReq(sess, body, m_map_server_locator, m_connection_registry);
        break;
    case MessageId::CS_CHANNELLIST_REQ:
        co_await handlers::OnChannelListReq(*sess, body, m_map_server_locator);
        break;
    case MessageId::CS_CHARLIST_REQ:
        co_await handlers::OnCharListReq(sess, body, m_char_service, m_connection_registry);
        break;
    case MessageId::CS_CREATECHAR_REQ:
        co_await handlers::OnCreateCharReq(sess, body, m_char_service, m_connection_registry);
        break;
    case MessageId::CS_DELCHAR_REQ:
        co_await handlers::OnDelCharReq(sess, body, m_char_service, m_connection_registry);
        break;
    case MessageId::CS_START_REQ:
        co_await handlers::OnStartReq(sess, body, m_map_server_locator, m_connection_registry);
        break;
    case MessageId::CS_AGREEMENT_REQ:
        co_await handlers::OnAgreementReq(sess, body, m_auth_service, m_connection_registry);
        break;
    case MessageId::CS_HOTSEND_REQ:
        co_await handlers::OnHotsendReq(*sess, body);
        break;
    case MessageId::CS_VETERAN_REQ:
        co_await handlers::OnVeteranReq(*sess, body, m_char_service);
        break;
    case MessageId::CS_TERMINATE_REQ:
        co_await handlers::OnTerminateReq(*sess, body);
        break;
    case MessageId::CS_SECURITYCONFIRM_ACK:
        co_await handlers::OnSecurityConfirmAck(*sess, body);
        break;
    default:
    {
        const auto name = tnetlib::protocol::NameOf(id);
        spdlog::warn("unhandled packet id=0x{:04X} ({}) body={} bytes",
            packet.wId,
            name.empty() ? std::string_view{"unknown"} : name,
            body.size());
        break;
    }
    }
}

} // namespace tloginsvr
