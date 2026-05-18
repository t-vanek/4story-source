// PCH-free; depends only on TNetLib's portable surface (AsioSession,
// AsioListener, MessageId) plus Boost.Asio.

#include "login_server.h"
#include "handlers.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>

#include <cstdio>
#include <utility>

namespace tloginsvr {

LoginServer::LoginServer(boost::asio::io_context& io, std::uint16_t port)
    : m_io(io)
    , m_listener(io.get_executor(), port)
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
        // PeerType::Server here is a Phase-3 limitation: the legacy
        // client expects PeerType::Client semantics (RC4 over the
        // entire packet on the inbound side). Until Phase E.2.5
        // wires RC4MD5Transform into AsioSession, the new server
        // can only talk to other tnetlib peers, not the legacy
        // client. The integration test uses tnetlib peers on both
        // ends to match.
        auto sess = std::make_shared<tnetlib::AsioSession>(
            std::move(socket), tnetlib::PeerType::Server);

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
        co_await handlers::OnLoginReq(*sess, body);
        break;
    default:
    {
        const auto name = tnetlib::protocol::NameOf(id);
        std::printf("[tloginsvr_asio] unhandled packet id=0x%04X (%s) body=%zu bytes\n",
            packet.wId,
            name.empty() ? "unknown" : std::string(name).c_str(),
            body.size());
        break;
    }
    }
}

} // namespace tloginsvr
