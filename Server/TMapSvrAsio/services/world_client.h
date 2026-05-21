#pragma once

// AsioWorldClient — persistent outbound TCP link to TWorldSvr.
//
// The legacy CTMapSvrModule held a single `CSession m_world` that was
// connected once at boot and reused for every map↔world packet (mostly
// MW_/DM_/SS_ family). This is the modern equivalent: a coroutine that
// connects, reads inbound packets, and on disconnect schedules a
// reconnect with exponential backoff (1s → 30s). The mode is
// server-to-server so the wire codec runs plain (no RC4 layer; just
// the XOR header + body codec that AsioSession applies by default).
//
// Thread safety: assumes a single-threaded io_context (the default
// in main.cpp), so SendPacket calls from multiple handler coroutines
// don't race because the executor only ever runs one at a time. A
// production multi-threaded io_context would need a send strand or
// queue — left as a TODO for the relevant phase.

#include "asio_session.h"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace tmapsvr {

class IWorldClient
{
public:
    virtual ~IWorldClient() = default;

    // Fire-and-forget packet send. Returns true if the bytes were
    // handed off to the AsioSession (does NOT wait for ACK on the
    // wire); false when the session is currently disconnected (caller
    // can choose to retry / buffer / drop).
    virtual boost::asio::awaitable<bool>
        SendPacket(std::uint16_t wId, std::vector<std::byte> body) = 0;

    // Lifecycle gate — handlers use this to decide whether to even
    // attempt SendPacket (no buffering yet in F5).
    virtual bool IsConnected() const = 0;
};

class AsioWorldClient final : public IWorldClient
{
public:
    AsioWorldClient(boost::asio::io_context& io,
                    std::string host,
                    std::uint16_t port,
                    std::chrono::milliseconds backoff_initial = std::chrono::seconds(1),
                    std::chrono::milliseconds backoff_max     = std::chrono::seconds(30));

    // Main coroutine. Loops connect → read → on disconnect, sleep +
    // retry with doubling backoff. Returns only when the io_context
    // stops (clean shutdown).
    boost::asio::awaitable<void> Run();

    boost::asio::awaitable<bool>
        SendPacket(std::uint16_t wId, std::vector<std::byte> body) override;

    bool IsConnected() const override;

private:
    // One connect attempt with the configured timeout. Returns a
    // fresh AsioSession on success, nullptr on failure.
    boost::asio::awaitable<std::shared_ptr<tnetlib::AsioSession>>
        DialOnce();

    boost::asio::io_context&    m_io;
    std::string                 m_host;
    std::uint16_t               m_port;
    std::chrono::milliseconds   m_backoff_initial;
    std::chrono::milliseconds   m_backoff_max;

    // Active session, or nullptr when disconnected. shared_ptr so the
    // read loop and SendPacket callers can hold weak refs across
    // co_awaits without resurrecting a dead socket.
    std::shared_ptr<tnetlib::AsioSession> m_session;
};

} // namespace tmapsvr
