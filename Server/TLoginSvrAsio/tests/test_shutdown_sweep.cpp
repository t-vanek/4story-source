// Exercise the actual LoginServer::Stop path over live TCP sessions. Lobby
// sessions are terminated, successful Map handoffs retain their reason, and
// Stop completes only after connection cleanup (also with no connection cap).
#include "login_server.h"
#include "services/fake_auth_service.h"
#include "services/fake_map_server_locator.h"
#include "services/fake_session_terminator.h"
#include "services/local_connection_registry.h"
#include "asio_session.h"
#include "MessageId.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace asio = boost::asio;
using tnetlib::protocol::MessageId;
using tnetlib::protocol::ToUint16;
namespace {
int failed = 0;
void Check(bool ok, const char* label)
{
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", label);
    if (!ok) ++failed;
}

std::vector<std::byte> LoginBody(const std::string& user)
{
    std::vector<std::byte> body;
    const auto append = [&](const auto& value) {
        const auto* p = reinterpret_cast<const std::byte*>(&value);
        body.insert(body.end(), p, p + sizeof(value));
    };
    append(std::uint16_t{0x2918});
    for (const auto& s : {std::string{}, std::string{"pw"}, std::string{}, std::string{}, user})
    {
        append(static_cast<std::int32_t>(s.size()));
        const auto* p = reinterpret_cast<const std::byte*>(s.data());
        body.insert(body.end(), p, p + s.size());
    }
    append(std::uint64_t{0});
    append(std::uint64_t{0xcdb0ebafdc6a7c5cULL}); // fixed original-client version checksum
    return body;
}

void TestStop(std::size_t connection_cap)
{
    asio::io_context io;
    tloginsvr::services::FakeAuthService auth;
    auth.AddUser("lobby", "pw", 101);
    auth.AddUser("handoff", "pw", 202);
    tloginsvr::services::LocalConnectionRegistry registry;
    tloginsvr::services::FakeSessionTerminator terminator;
    tloginsvr::services::FakeMapServerLocator locator;
    locator.AddMapServer(1, {{127, 0, 0, 1}, 5815, 1});
    tloginsvr::LoginServerConfig cfg{};
    cfg.port = 0;
    cfg.max_connections = connection_cap;
    cfg.auth_service = &auth;
    cfg.connection_registry = &registry;
    cfg.session_terminator = &terminator;
    cfg.map_server_locator = &locator;
    tloginsvr::LoginServer server(io, cfg);
    bool stopped = false;
    bool timed_out = false;
    int ready = 0;
    asio::steady_timer deadline(io, std::chrono::seconds(3));
    deadline.async_wait([&](boost::system::error_code ec) {
        if (!ec) { timed_out = true; io.stop(); }
    });
    asio::co_spawn(io, server.Run(), asio::detached);

    std::vector<std::shared_ptr<tnetlib::AsioSession>> clients;
    for (bool handoff : {false, true})
    {
        asio::ip::tcp::socket socket(io);
        socket.connect({asio::ip::address_v4::loopback(), server.Port()});
        auto session = std::make_shared<tnetlib::AsioSession>(std::move(socket), tnetlib::PeerType::Server);
        clients.push_back(session);
        asio::co_spawn(io, [&, session, handoff]() -> asio::awaitable<void> {
            co_await session->RunPackets([&, handoff, seen = 0](const tnetlib::DecodedPacket& packet) mutable {
                Check(!packet.body.empty() && packet.body[0] == std::byte{0}, "successful client ACK");
                if (++seen == (handoff ? 2 : 1) && ++ready == 2)
                    asio::co_spawn(io, server.Stop(), [&](std::exception_ptr error) {
                        Check(!error, "Stop completes without exception");
                        stopped = !error;
                        deadline.cancel();
                        io.stop();
                    });
            });
        }, asio::detached);
        asio::co_spawn(io, [session, handoff]() -> asio::awaitable<void> {
            const auto login = LoginBody(handoff ? "handoff" : "lobby");
            co_await session->SendPacket(ToUint16(MessageId::CS_LOGIN_REQ), login);
            if (handoff)
            {
                // Pipeline START immediately behind LOGIN, as an original peer may.
                std::vector<std::byte> start{std::byte{1}, std::byte{0}, std::byte{42},
                                             std::byte{0}, std::byte{0}, std::byte{0}};
                co_await session->SendPacket(ToUint16(MessageId::CS_START_REQ), start);
            }
        }, asio::detached);
    }
    io.run();
    Check(stopped && !timed_out, "graceful stop drains actual accepted sessions before deadline");
    Check(registry.Count() == 0, "registry empty when Stop completes");
    auto calls = terminator.History();
    std::sort(calls.begin(), calls.end(), [](const auto& a, const auto& b) { return a.user_id < b.user_id; });
    Check(calls.size() == 2, "each authenticated session cleaned exactly once");
    if (calls.size() == 2)
    {
        Check(calls[0].user_id == 101 && calls[0].session_key != 0 &&
              calls[0].reason == tloginsvr::services::TerminationReason::Disconnect,
              "lobby session terminates on shutdown");
        Check(calls[1].user_id == 202 && calls[1].session_key != 0 && calls[1].char_id == 42 &&
              calls[1].reason == tloginsvr::services::TerminationReason::MapHandoff,
              "shutdown preserves Map handoff reason and selected character");
    }
}
}
int main()
{
    TestStop(10);
    TestStop(0);
    return failed == 0 ? 0 : 1;
}
