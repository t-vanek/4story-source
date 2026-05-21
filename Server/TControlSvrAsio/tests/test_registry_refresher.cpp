// Unit test for fourstory::ops::RegistryRefresher::AddCoroutineHook.
// Verifies that:
//   1. Sync hooks added via AddHook still run on each tick.
//   2. Coroutine hooks added via AddCoroutineHook are co_awaited on
//      each tick.
//   3. Sync + coroutine hooks coexist on the same refresher.
//   4. An exception in a coroutine hook doesn't kill the loop —
//      subsequent ticks fire the hook again, mirroring the existing
//      sync-hook contract.

#include "fourstory/ops/registry_refresher.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <stdexcept>

namespace {

int g_fails = 0;
#define EXPECT(cond) do { \
    if (!(cond)) { \
        std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); \
        ++g_fails; \
    } \
} while (0)

// Helper: run io_context until `t` ms elapse, then Stop the
// refresher so the Loop coroutine exits and io.run() returns.
void RunForMs(boost::asio::io_context& io,
              std::shared_ptr<fourstory::ops::RegistryRefresher> r,
              int ms)
{
    boost::asio::steady_timer stopper(io);
    stopper.expires_after(std::chrono::milliseconds(ms));
    stopper.async_wait([r](auto) { r->Stop(); });
    io.run();
}

void TestCoroutineHookFires()
{
    boost::asio::io_context io;
    auto r = fourstory::ops::RegistryRefresher::Make(io,
        std::chrono::milliseconds(20));
    std::atomic<int> count{0};
    r->AddCoroutineHook([&count]() -> boost::asio::awaitable<void> {
        count.fetch_add(1, std::memory_order_relaxed);
        co_return;
    });
    r->Start();
    RunForMs(io, r, 65); // expect 3 ticks at 20ms intervals
    EXPECT(count.load() >= 2);
}

void TestSyncAndCoroutineHooksBothRun()
{
    boost::asio::io_context io;
    auto r = fourstory::ops::RegistryRefresher::Make(io,
        std::chrono::milliseconds(20));
    std::atomic<int> sync_count{0};
    std::atomic<int> coro_count{0};
    r->AddHook([&] { sync_count.fetch_add(1, std::memory_order_relaxed); });
    r->AddCoroutineHook([&]() -> boost::asio::awaitable<void> {
        coro_count.fetch_add(1, std::memory_order_relaxed);
        co_return;
    });
    r->Start();
    RunForMs(io, r, 65);
    EXPECT(sync_count.load() >= 2);
    EXPECT(coro_count.load() >= 2);
    // Counts should be roughly equal — both fire each tick.
    EXPECT(std::abs(sync_count.load() - coro_count.load()) <= 1);
}

void TestCoroutineHookExceptionDoesntKillLoop()
{
    boost::asio::io_context io;
    auto r = fourstory::ops::RegistryRefresher::Make(io,
        std::chrono::milliseconds(20));
    std::atomic<int> fired{0};
    r->AddCoroutineHook([&fired]() -> boost::asio::awaitable<void> {
        fired.fetch_add(1, std::memory_order_relaxed);
        throw std::runtime_error("hook went boom");
        co_return; // unreachable, keeps the awaitable signature happy
    });
    r->Start();
    RunForMs(io, r, 65);
    // Loop must keep firing even though every hook invocation throws.
    EXPECT(fired.load() >= 2);
}

} // namespace

int main()
{
    TestCoroutineHookFires();
    TestSyncAndCoroutineHooksBothRun();
    TestCoroutineHookExceptionDoesntKillLoop();
    if (g_fails) { std::fprintf(stderr, "%d failure(s)\n", g_fails); return 1; }
    std::printf("ok\n");
    return 0;
}
