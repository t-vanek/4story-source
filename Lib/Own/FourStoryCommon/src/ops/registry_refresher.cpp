#include "fourstory/ops/registry_refresher.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <spdlog/spdlog.h>

namespace fourstory::ops {

struct RegistryRefresher::Impl
{
    boost::asio::steady_timer timer;
    bool cancelled = false;
    explicit Impl(boost::asio::io_context& io) : timer(io) {}
};

RegistryRefresher::RegistryRefresher(boost::asio::io_context& io,
                                     std::chrono::seconds period)
    : m_io(io)
    , m_period(period)
    , m_impl(std::make_unique<Impl>(io))
{
}

std::shared_ptr<RegistryRefresher>
RegistryRefresher::Make(boost::asio::io_context& io,
                        std::chrono::seconds period)
{
    return std::shared_ptr<RegistryRefresher>(
        new RegistryRefresher(io, period));
}

void RegistryRefresher::AddHook(RefreshFn fn)
{
    if (fn) m_hooks.push_back(std::move(fn));
}

void RegistryRefresher::AddCoroutineHook(CoroutineRefreshFn fn)
{
    if (fn) m_coro_hooks.push_back(std::move(fn));
}

void RegistryRefresher::Start()
{
    if (m_period.count() <= 0 ||
        (m_hooks.empty() && m_coro_hooks.empty()))
        return;
    boost::asio::co_spawn(m_io, shared_from_this()->Loop(),
                          boost::asio::detached);
    spdlog::info("registry_refresher: tick every {}s ({} sync hook(s), "
                 "{} coroutine hook(s))",
        m_period.count(), m_hooks.size(), m_coro_hooks.size());
}

void RegistryRefresher::Stop()
{
    m_impl->cancelled = true;
    m_impl->timer.cancel();
}

boost::asio::awaitable<void> RegistryRefresher::Loop()
{
    auto self = shared_from_this();
    while (!m_impl->cancelled)
    {
        m_impl->timer.expires_after(m_period);
        boost::system::error_code ec;
        co_await m_impl->timer.async_wait(
            boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        if (ec || m_impl->cancelled) break;
        for (const auto& fn : m_hooks)
        {
            try { fn(); }
            catch (const std::exception& ex)
            {
                spdlog::warn("registry_refresher: sync hook failed: {}",
                    ex.what());
            }
        }
        // Awaitable hooks run after sync hooks. Each one's blocking
        // SOCI work is expected to be wrapped in fourstory::db::
        // CoOffloadIf so the io_context isn't stuck during the
        // roundtrip.
        for (const auto& fn : m_coro_hooks)
        {
            try { co_await fn(); }
            catch (const std::exception& ex)
            {
                spdlog::warn("registry_refresher: coroutine hook failed: {}",
                    ex.what());
            }
        }
        spdlog::debug("registry_refresher: tick complete ({}+{} hooks ran)",
            m_hooks.size(), m_coro_hooks.size());
    }
}

} // namespace fourstory::ops
