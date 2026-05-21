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
    // Coroutine hook — Loop awaits the returned awaitable instead of
    // calling the function synchronously. Lets hooks that touch SOCI
    // wrap their blocking calls in fourstory::db::CoOffloadIf so the
    // io_context isn't frozen during a slow DB roundtrip. Mixed sync
    // + coroutine hooks are supported on the same refresher.
    using CoroutineRefreshFn =
        std::function<boost::asio::awaitable<void>()>;

    // `period` of 0 disables the timer (no-op). Typical production
    // value is 30s — matches the legacy update tick.
    static std::shared_ptr<RegistryRefresher> Make(
        boost::asio::io_context& io,
        std::chrono::seconds period);

    // Add a refresh hook. Must be wired before Start().
    void AddHook(RefreshFn fn);

    // Add an awaitable hook — Loop will co_await it each tick. Same
    // wiring rule as AddHook: register before Start().
    void AddCoroutineHook(CoroutineRefreshFn fn);

    // Begin the timer loop. No-op when period is 0.
    void Start();

    // Cancel + stop. Called from io.stop() path. Safe to call multiple
    // times.
    void Stop();

private:
    RegistryRefresher(boost::asio::io_context& io,
                      std::chrono::seconds period);

    boost::asio::awaitable<void> Loop();

    boost::asio::io_context&            m_io;
    std::chrono::seconds                m_period;
    std::vector<RefreshFn>              m_hooks;
    std::vector<CoroutineRefreshFn>     m_coro_hooks;

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace fourstory::ops
