// Unit test for fourstory::db::CoOffload. Verifies:
//   1. The lambda runs on a worker thread (not the io_context).
//   2. The coroutine resumes on the original executor with the
//      return value.
//   3. Exceptions thrown inside the lambda propagate back to the
//      caller.
//   4. CoOffloadVoid path works for void-returning callables.

#include "fourstory/db/co_offload.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/thread_pool.hpp>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <stdexcept>
#include <thread>

namespace {

int g_fails = 0;
#define EXPECT(cond) do { \
    if (!(cond)) { \
        std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); \
        ++g_fails; \
    } \
} while (0)

void TestReturnValuePropagates()
{
    boost::asio::io_context io;
    boost::asio::thread_pool pool(2);
    std::atomic<int> result{0};

    boost::asio::co_spawn(io, [&]() -> boost::asio::awaitable<void> {
        const int v = co_await fourstory::db::CoOffload(pool,
            [] {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                return 42;
            });
        result.store(v);
    }, boost::asio::detached);

    io.run();
    pool.stop();
    pool.join();
    EXPECT(result.load() == 42);
}

void TestLambdaRunsOnWorkerThread()
{
    boost::asio::io_context io;
    boost::asio::thread_pool pool(2);
    const auto io_tid = std::this_thread::get_id();
    std::atomic<bool> different_thread{false};
    std::atomic<bool> resumed_on_io_tid{false};

    boost::asio::co_spawn(io, [&]() -> boost::asio::awaitable<void> {
        const std::thread::id worker_tid = co_await
            fourstory::db::CoOffload(pool, [] {
                return std::this_thread::get_id();
            });
        different_thread.store(worker_tid != io_tid);
        resumed_on_io_tid.store(std::this_thread::get_id() == io_tid);
    }, boost::asio::detached);

    io.run();
    pool.stop();
    pool.join();
    EXPECT(different_thread.load());   // lambda ran on a worker
    EXPECT(resumed_on_io_tid.load());  // coroutine resumed on io_context
}

void TestExceptionPropagates()
{
    boost::asio::io_context io;
    boost::asio::thread_pool pool(2);
    std::atomic<bool> caught{false};
    std::atomic<bool> ran{false};

    boost::asio::co_spawn(io, [&]() -> boost::asio::awaitable<void> {
        try
        {
            const int v = co_await fourstory::db::CoOffload(pool,
                [&]() -> int {
                    ran.store(true);
                    throw std::runtime_error("boom from worker");
                });
            (void)v;
        }
        catch (const std::runtime_error& ex)
        {
            caught.store(std::string(ex.what()) == "boom from worker");
        }
    }, boost::asio::detached);

    io.run();
    pool.stop();
    pool.join();
    EXPECT(ran.load());
    EXPECT(caught.load());
}

void TestVoidVariant()
{
    boost::asio::io_context io;
    boost::asio::thread_pool pool(2);
    std::atomic<bool> ran{false};
    std::atomic<bool> resumed{false};

    boost::asio::co_spawn(io, [&]() -> boost::asio::awaitable<void> {
        co_await fourstory::db::CoOffloadVoid(pool, [&] {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            ran.store(true);
        });
        resumed.store(true);
    }, boost::asio::detached);

    io.run();
    pool.stop();
    pool.join();
    EXPECT(ran.load());
    EXPECT(resumed.load());
}

} // namespace

int main()
{
    TestReturnValuePropagates();
    TestLambdaRunsOnWorkerThread();
    TestExceptionPropagates();
    TestVoidVariant();
    if (g_fails) { std::fprintf(stderr, "%d failure(s)\n", g_fails); return 1; }
    std::printf("ok\n");
    return 0;
}
