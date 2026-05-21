// Unit test for fourstory::db::CoOffload + the optional-pool wrappers.
// Verifies:
//   1. The lambda runs on a worker thread (not the io_context).
//   2. The coroutine resumes on the original executor with the
//      return value.
//   3. Exceptions thrown inside the lambda propagate back to the
//      caller.
//   4. CoOffloadVoid path works for void-returning callables.
//   5. CoOffloadIf  (R-returning, nullable pool):
//        a. null pool  → callable runs inline, return value preserved
//        b. valid pool → callable runs on worker thread
//        c. null pool + throw → exception propagates
//   6. CoOffloadVoidIf:
//        a. null pool  → callable runs inline
//        b. valid pool → callable runs on worker thread

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

// ---- CoOffloadIf / CoOffloadVoidIf coverage ----------------------

void TestCoOffloadIfNullPoolInline()
{
    boost::asio::io_context io;
    const auto io_thread_id = std::this_thread::get_id();
    std::thread::id ran_on_thread{};
    int result = -1;

    boost::asio::co_spawn(io, [&]() -> boost::asio::awaitable<void> {
        result = co_await fourstory::db::CoOffloadIf(nullptr,
            [&io_thread_id, &ran_on_thread] {
                ran_on_thread = std::this_thread::get_id();
                return 42;
            });
    }, boost::asio::detached);

    io.run();
    EXPECT(result == 42);
    // Null pool MUST run inline — on the same thread as io.run().
    EXPECT(ran_on_thread == io_thread_id);
}

void TestCoOffloadIfWithPoolOffloads()
{
    boost::asio::io_context io;
    boost::asio::thread_pool pool(2);
    const auto io_thread_id = std::this_thread::get_id();
    std::thread::id ran_on_thread{};
    int result = -1;

    boost::asio::co_spawn(io, [&]() -> boost::asio::awaitable<void> {
        result = co_await fourstory::db::CoOffloadIf(&pool,
            [&ran_on_thread] {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                ran_on_thread = std::this_thread::get_id();
                return 7;
            });
    }, boost::asio::detached);

    io.run();
    pool.stop();
    pool.join();
    EXPECT(result == 7);
    // Valid pool MUST hop to a worker thread.
    EXPECT(ran_on_thread != io_thread_id);
}

void TestCoOffloadIfNullPoolPropagatesException()
{
    boost::asio::io_context io;
    bool caught = false;

    boost::asio::co_spawn(io, [&]() -> boost::asio::awaitable<void> {
        try {
            co_await fourstory::db::CoOffloadIf(nullptr,
                []() -> int { throw std::runtime_error("boom"); });
        } catch (const std::runtime_error&) {
            caught = true;
        }
    }, boost::asio::detached);

    io.run();
    EXPECT(caught);
}

void TestCoOffloadVoidIfNullPoolInline()
{
    boost::asio::io_context io;
    const auto io_thread_id = std::this_thread::get_id();
    std::thread::id ran_on_thread{};

    boost::asio::co_spawn(io, [&]() -> boost::asio::awaitable<void> {
        co_await fourstory::db::CoOffloadVoidIf(nullptr,
            [&ran_on_thread] {
                ran_on_thread = std::this_thread::get_id();
            });
    }, boost::asio::detached);

    io.run();
    EXPECT(ran_on_thread == io_thread_id);
}

void TestCoOffloadVoidIfWithPoolOffloads()
{
    boost::asio::io_context io;
    boost::asio::thread_pool pool(2);
    const auto io_thread_id = std::this_thread::get_id();
    std::thread::id ran_on_thread{};

    boost::asio::co_spawn(io, [&]() -> boost::asio::awaitable<void> {
        co_await fourstory::db::CoOffloadVoidIf(&pool,
            [&ran_on_thread] {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                ran_on_thread = std::this_thread::get_id();
            });
    }, boost::asio::detached);

    io.run();
    pool.stop();
    pool.join();
    EXPECT(ran_on_thread != io_thread_id);
}

} // namespace

int main()
{
    TestReturnValuePropagates();
    TestLambdaRunsOnWorkerThread();
    TestExceptionPropagates();
    TestVoidVariant();
    TestCoOffloadIfNullPoolInline();
    TestCoOffloadIfWithPoolOffloads();
    TestCoOffloadIfNullPoolPropagatesException();
    TestCoOffloadVoidIfNullPoolInline();
    TestCoOffloadVoidIfWithPoolOffloads();
    if (g_fails) { std::fprintf(stderr, "%d failure(s)\n", g_fails); return 1; }
    std::printf("ok\n");
    return 0;
}
