#pragma once

// RegistryRefresher — periodic refresh tick that the login server
// fires every N seconds to give long-lived caches a chance to
// reload. Mirrors legacy `CTLoginSvrModule::UpdateData` which
// re-reads TGROUP / TCHANNEL / TIPADDR every N seconds inside
// ControlThread.
//
// Most of our SOCI services already query live on each request so
// caches don't drift — but TVETERANCHART is cached at SociCharService
// construction time and would otherwise need a restart to reflect a
// data change. The refresher gives operators a way to update it
// without a restart.
//
// Single-shot interface: the caller wires whichever services should
// participate, and the refresher runs them on its asio steady_timer.

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>

#include <chrono>
#include <functional>
#include <memory>
#include <vector>

namespace fourstory::ops {

class RegistryRefresher
    : public std::enable_shared_from_this<RegistryRefresher>
{
public:
    using RefreshFn = std::function<void()>;

    // `period` of 0 disables the timer (no-op). Typical production
    // value is 30s — matches the legacy update tick.
    static std::shared_ptr<RegistryRefresher> Make(
        boost::asio::io_context& io,
        std::chrono::seconds period);

    // Add a refresh hook. Must be wired before Start().
    void AddHook(RefreshFn fn);

    // Begin the timer loop. No-op when period is 0.
    void Start();

    // Cancel + stop. Called from io.stop() path. Safe to call multiple
    // times.
    void Stop();

private:
    RegistryRefresher(boost::asio::io_context& io,
                      std::chrono::seconds period);

    boost::asio::awaitable<void> Loop();

    boost::asio::io_context&       m_io;
    std::chrono::seconds           m_period;
    std::vector<RefreshFn>         m_hooks;

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace fourstory::ops
