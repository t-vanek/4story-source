// Integration test for fourstory::cluster::PeerClient — the outbound
// counterpart of CT_PEER_* registration handlers. Stands up a raw TCP
// acceptor that role-plays TControl's wire endpoint, drives a real
// PeerClient against it, and asserts on the bytes that flow between
// them.
//
// Why not against the real OnPeerRegisterReq handler: the production
// handler hard-codes the heartbeat interval at 30s (matching legacy
// WorkTickProc cadence). For a deterministic sub-second test we ship
// hb_interval=1 in the ACK from the stub, which lets us verify a
// heartbeat round-trip in ~1.5 seconds rather than ~30s. The byte
// layout of REGISTER_REQ / REGISTER_ACK / HEARTBEAT_REQ / DEREGISTER_REQ
// is still exactly what the real handlers expect — covered by
// test_peer_registry.cpp at the wire level.

#include "fourstory/cluster/peer_client.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/write.hpp>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <future>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace asio = boost::asio;
using boost::asio::ip::tcp;
using namespace std::chrono_literals;

namespace {

int g_passed = 0;
int g_failed = 0;
void Check(bool ok, const char* label)
{
    if (ok) { ++g_passed; std::printf("  PASS  %s\n", label); }
    else    { ++g_failed; std::printf("  FAIL  %s\n", label); }
}

// Mirror of the wire constants from PeerClient impl. Keep these in
// sync — the whole point of the test is byte compatibility.
constexpr std::uint16_t kPeerRegisterReq    = 0x9F00;
constexpr std::uint16_t kPeerRegisterAck    = 0x9F01;
constexpr std::uint16_t kPeerHeartbeatReq   = 0x9F02;
constexpr std::uint16_t kPeerHeartbeatAck   = 0x9F03;
constexpr std::uint16_t kPeerDeregisterReq  = 0x9F04;

std::uint32_t FoldChecksum(const std::byte* p, std::size_t n)
{
    std::uint32_t acc = 0;
    std::size_t i = 0;
    for (; i + 4 <= n; i += 4)
    {
        std::uint32_t w = 0;
        std::memcpy(&w, p + i, 4);
        acc ^= w;
    }
    for (; i < n; ++i) acc ^= static_cast<std::uint32_t>(p[i]);
    return acc;
}

template <class T>
void PushPOD(std::vector<std::byte>& out, T v)
{
    const auto* p = reinterpret_cast<const std::byte*>(&v);
    out.insert(out.end(), p, p + sizeof(T));
}

struct WireFrame
{
    std::uint16_t          wId;
    std::vector<std::byte> body;
};

asio::awaitable<WireFrame> ReadFrame(tcp::socket& sock)
{
    std::uint8_t hdr[8] = {};
    boost::system::error_code ec;
    co_await asio::async_read(sock, asio::buffer(hdr, sizeof(hdr)),
        asio::redirect_error(asio::use_awaitable, ec));
    if (ec) co_return WireFrame{0, {}};
    std::uint16_t wSize = 0, wId = 0;
    std::uint32_t chk = 0;
    std::memcpy(&wSize, hdr,     2);
    std::memcpy(&wId,   hdr + 2, 2);
    std::memcpy(&chk,   hdr + 4, 4);
    const std::size_t body_size = wSize > sizeof(hdr) ? wSize - sizeof(hdr) : 0;
    std::vector<std::byte> body(body_size);
    if (body_size > 0)
    {
        co_await asio::async_read(sock, asio::buffer(body.data(), body_size),
            asio::redirect_error(asio::use_awaitable, ec));
        if (ec) co_return WireFrame{0, {}};
    }
    (void)chk;
    co_return WireFrame{wId, std::move(body)};
}

asio::awaitable<void> WriteFrame(tcp::socket& sock, std::uint16_t wId,
                                 std::vector<std::byte> body)
{
    const std::size_t total = 8 + body.size();
    std::vector<std::byte> frame(total);
    std::uint16_t wSize = static_cast<std::uint16_t>(total);
    std::uint32_t chk = FoldChecksum(body.data(), body.size());
    std::memcpy(frame.data(),     &wSize, 2);
    std::memcpy(frame.data() + 2, &wId,   2);
    std::memcpy(frame.data() + 4, &chk,   4);
    if (!body.empty())
        std::memcpy(frame.data() + 8, body.data(), body.size());
    boost::system::error_code ec;
    co_await asio::async_write(sock, asio::buffer(frame.data(), total),
        asio::redirect_error(asio::use_awaitable, ec));
}

// ---------------------------------------------------------------------------

// Stub control endpoint that:
//   - accepts one connection
//   - reads REGISTER_REQ + replies REGISTER_ACK(accepted=1, lease, hb=1s)
//   - in a loop: reads HEARTBEAT_REQ + replies HEARTBEAT_ACK
//   - records every REGISTER and HEARTBEAT body for the test to inspect
//
// The stub stops when the client closes the socket (e.g. on Stop()).
struct StubControl
{
    std::uint16_t                       port  = 0;
    std::uint64_t                       lease = 42;
    std::vector<std::vector<std::byte>> registers;   // bodies
    std::vector<std::vector<std::byte>> heartbeats;
    bool                                saw_deregister = false;
    std::uint32_t                       hb_interval = 1;

    asio::awaitable<void> Run(tcp::acceptor& acc)
    {
        boost::system::error_code ec;
        auto sock = co_await acc.async_accept(
            asio::redirect_error(asio::use_awaitable, ec));
        if (ec) co_return;

        while (sock.is_open())
        {
            auto frame = co_await ReadFrame(sock);
            if (frame.wId == 0) co_return;

            if (frame.wId == kPeerRegisterReq)
            {
                registers.push_back(frame.body);
                std::vector<std::byte> ack;
                PushPOD<std::uint8_t >(ack, 1);          // accepted
                PushPOD<std::uint32_t>(ack, 0);          // reason
                PushPOD<std::uint64_t>(ack, lease);
                PushPOD<std::uint32_t>(ack, hb_interval);
                co_await WriteFrame(sock, kPeerRegisterAck, std::move(ack));
            }
            else if (frame.wId == kPeerHeartbeatReq)
            {
                heartbeats.push_back(frame.body);
                std::vector<std::byte> ack;
                PushPOD<std::uint8_t >(ack, 1);
                PushPOD<std::uint64_t>(ack, lease);
                co_await WriteFrame(sock, kPeerHeartbeatAck, std::move(ack));
            }
            else if (frame.wId == kPeerDeregisterReq)
            {
                saw_deregister = true;
                co_return;
            }
        }
    }
};

fourstory::cluster::PeerClientOptions MakeOpts(std::uint16_t port)
{
    fourstory::cluster::PeerClientOptions o;
    o.control_host    = "127.0.0.1";
    o.control_port    = port;
    o.service_id      = 0x010204;
    o.reported_name   = "tloginsvr-1";
    o.reported_addr   = "10.0.0.42";
    o.reported_port   = 4816;
    o.version         = "5.0.0-test";
    o.pid             = 31337;
    o.start_unix      = 1700000000;
    // Tight retries so tests don't hang for 30s on failure paths.
    o.initial_backoff = 1s;
    o.max_backoff     = 2s;
    return o;
}

// ---------------------------------------------------------------------------

void TestRegisterAndHeartbeatRoundTrip()
{
    std::printf("[peer_client — register + heartbeat round-trip]\n");
    asio::io_context io;
    tcp::acceptor acc(io, tcp::endpoint(tcp::v4(), 0));
    const auto port = acc.local_endpoint().port();

    StubControl stub;
    asio::co_spawn(io, stub.Run(acc), asio::detached);

    auto opts = MakeOpts(port);
    auto pc = std::make_shared<fourstory::cluster::PeerClient>(io, opts);
    asio::co_spawn(io, pc->Run(), asio::detached);

    // Run on a worker thread for ~1.8s so we get one heartbeat then
    // stop the client cleanly. The 1s hb_interval + 0.8s slack gives
    // the heartbeat coroutine time to send + recv + record.
    std::thread runner([&] { io.run(); });
    std::this_thread::sleep_for(1800ms);
    pc->Stop();
    runner.join();

    Check(stub.registers.size() == 1, "control stub saw one REGISTER");
    Check(!stub.heartbeats.empty(),
        "control stub saw at least one HEARTBEAT before Stop()");
    Check(stub.saw_deregister,
        "control stub saw DEREGISTER on Stop() (graceful)");

    // Verify the REGISTER body layout. Layout:
    //   DWORD service_id, CString name, CString addr, WORD port,
    //   CString version, DWORD pid, QWORD start_unix
    const auto& body = stub.registers.front();
    std::uint32_t sid = 0;
    std::memcpy(&sid, body.data(), 4);
    Check(sid == 0x010204, "REGISTER body service_id matches");

    std::int32_t name_len = 0;
    std::memcpy(&name_len, body.data() + 4, 4);
    Check(name_len == 11, "REGISTER name length (=='tloginsvr-1'.size())");
    const std::string name(
        reinterpret_cast<const char*>(body.data() + 8), name_len);
    Check(name == "tloginsvr-1", "REGISTER name bytes verbatim");
}

void TestRejectedRegistrationTriggersReconnect()
{
    std::printf("[peer_client — rejected REGISTER triggers reconnect]\n");
    asio::io_context io;
    tcp::acceptor acc(io, tcp::endpoint(tcp::v4(), 0));
    const auto port = acc.local_endpoint().port();

    // Stub flow: first connect → reply REJECTED. Second connect →
    // reply ACCEPTED. Verifies PeerClient retries.
    auto attempts = std::make_shared<int>(0);
    auto reject_then_accept = [&acc, attempts]() -> asio::awaitable<void>
    {
        for (int i = 0; i < 2; ++i)
        {
            boost::system::error_code ec;
            auto sock = co_await acc.async_accept(
                asio::redirect_error(asio::use_awaitable, ec));
            if (ec) co_return;
            auto frame = co_await ReadFrame(sock);
            if (frame.wId != kPeerRegisterReq) continue;
            std::vector<std::byte> ack;
            const std::uint8_t accepted = (i == 0) ? 0 : 1;
            const std::uint32_t reason  = (i == 0) ? 1 : 0;
            const std::uint64_t lease   = (i == 0) ? 0 : 99;
            PushPOD<std::uint8_t >(ack, accepted);
            PushPOD<std::uint32_t>(ack, reason);
            PushPOD<std::uint64_t>(ack, lease);
            PushPOD<std::uint32_t>(ack, 10);  // long hb so no heartbeat fires
            co_await WriteFrame(sock, kPeerRegisterAck, std::move(ack));
            ++(*attempts);
            if (accepted == 1)
            {
                // Hold the connection open briefly so the client
                // observes the success before we tear down.
                asio::steady_timer t(co_await asio::this_coro::executor);
                t.expires_after(200ms);
                co_await t.async_wait(
                    asio::redirect_error(asio::use_awaitable, ec));
                co_return;
            }
        }
    };
    asio::co_spawn(io, reject_then_accept(), asio::detached);

    auto opts = MakeOpts(port);
    opts.initial_backoff = 100ms;  // fast retry
    opts.max_backoff     = 200ms;
    auto pc = std::make_shared<fourstory::cluster::PeerClient>(io, opts);
    asio::co_spawn(io, pc->Run(), asio::detached);

    std::thread runner([&] { io.run(); });
    // Wait for: reject attempt (~0ms), backoff (~100ms), success attempt,
    // observation window (~200ms). 800ms is plenty.
    std::this_thread::sleep_for(800ms);
    Check(pc->IsRegistered(), "PeerClient became registered after retry");
    Check(pc->LeaseEpoch() == 99,
        "PeerClient cached the second (successful) lease epoch");
    pc->Stop();
    runner.join();
    Check(*attempts == 2, "stub saw exactly two REGISTER attempts");
}

void TestStaleHeartbeatDropsToReconnect()
{
    std::printf("[peer_client — stale-lease HEARTBEAT triggers reconnect]\n");
    asio::io_context io;
    tcp::acceptor acc(io, tcp::endpoint(tcp::v4(), 0));
    const auto port = acc.local_endpoint().port();

    // Stub flow: accept REGISTER (issue lease=7), then on first
    // HEARTBEAT reply rejected (accepted=0). Then close socket. The
    // PeerClient should treat this as 'must re-register' and dial
    // back in for a second REGISTER. We count both registrations.
    auto registers = std::make_shared<int>(0);
    auto stub = [&acc, registers]() -> asio::awaitable<void>
    {
        for (int i = 0; i < 2; ++i)
        {
            boost::system::error_code ec;
            auto sock = co_await acc.async_accept(
                asio::redirect_error(asio::use_awaitable, ec));
            if (ec) co_return;

            auto reg = co_await ReadFrame(sock);
            if (reg.wId != kPeerRegisterReq) co_return;
            ++(*registers);

            // Send accepted register-ack with 1s heartbeat.
            std::vector<std::byte> ack;
            PushPOD<std::uint8_t >(ack, 1);
            PushPOD<std::uint32_t>(ack, 0);
            PushPOD<std::uint64_t>(ack, 7);
            PushPOD<std::uint32_t>(ack, 1);
            co_await WriteFrame(sock, kPeerRegisterAck, std::move(ack));

            if (i == 0)
            {
                // First time: reject the next heartbeat to push the
                // client into re-register.
                auto hb = co_await ReadFrame(sock);
                if (hb.wId == kPeerHeartbeatReq)
                {
                    std::vector<std::byte> hb_ack;
                    PushPOD<std::uint8_t >(hb_ack, 0);
                    PushPOD<std::uint64_t>(hb_ack, 0);
                    co_await WriteFrame(sock, kPeerHeartbeatAck,
                        std::move(hb_ack));
                }
                sock.close(ec);
            }
            else
            {
                // Second time: hold for a moment so the test can
                // assert before we tear down.
                asio::steady_timer t(co_await asio::this_coro::executor);
                t.expires_after(200ms);
                co_await t.async_wait(
                    asio::redirect_error(asio::use_awaitable, ec));
                co_return;
            }
        }
    };
    asio::co_spawn(io, stub(), asio::detached);

    auto opts = MakeOpts(port);
    opts.initial_backoff = 100ms;
    opts.max_backoff     = 200ms;
    auto pc = std::make_shared<fourstory::cluster::PeerClient>(io, opts);
    asio::co_spawn(io, pc->Run(), asio::detached);

    std::thread runner([&] { io.run(); });
    // Need: first register (immediate), one heartbeat (~1s), reject,
    // backoff (~100ms), second register. 2.0s total.
    std::this_thread::sleep_for(2000ms);
    Check(*registers == 2, "stub saw two REGISTER attempts");
    Check(pc->IsRegistered(), "client is registered after re-register");
    pc->Stop();
    runner.join();
}

} // namespace

int main()
{
    std::printf("=== peer_client integration test ===\n");
    try
    {
        TestRegisterAndHeartbeatRoundTrip();
        TestRejectedRegistrationTriggersReconnect();
        TestStaleHeartbeatDropsToReconnect();
    }
    catch (const std::exception& ex)
    {
        std::printf("  FAIL  unexpected exception: %s\n", ex.what());
        ++g_failed;
    }
    std::printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
