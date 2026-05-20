#pragma once

// CoOffload — bridge a synchronous SOCI call onto a worker thread
// without blocking the calling coroutine's executor.
//
// Motivation: SOCI's `session::operator<<` blocks the calling
// thread until the DB returns a row / SP completes. Calling it
// from a handler coroutine on the single io_context thread freezes
// every other operator + peer for the duration of the call. The
// asio convention to fix this is `co_await async_initiate(pool,
// …, use_awaitable)`, but the boilerplate for "run a sync lambda
// on a pool, return the result on the original executor" is verbose
// enough that every server's been skipping it. This header is the
// shared one-call helper.
//
// Usage:
//
//   boost::asio::thread_pool db_pool(4);
//   auto result = co_await fourstory::db::CoOffload(db_pool,
//       [&] { return repo.HeavyLookup(arg); });
//
// The lambda runs on a worker thread; the coroutine resumes on the
// original executor (typically the io_context) with the result.
// Exceptions thrown inside the lambda propagate to the awaiter.
//
// Void-returning lambdas are supported via the
// `CoOffloadVoid` overload — C++20 can't constexpr-if on
// `co_return co_await` cleanly when R is void.

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <exception>
#include <type_traits>
#include <utility>

namespace fourstory::db {

// Offload a synchronous callable onto `pool`. Returns whatever the
// callable returns; rethrows whatever it threw.
//
// The caller must keep `pool` alive at least until the awaiter
// completes — typically a member of the same scope that owns the
// io_context (e.g. main).
//
// Implementation detail: the completion signature is
// `void(std::exception_ptr, R)`, the canonical asio convention for
// fallible async ops. `use_awaitable` rethrows the exception_ptr
// into the calling coroutine when set, otherwise yields R into the
// co_await expression. Rethrowing from inside the posted handler
// (instead of via the completion signature) would unwind the post
// continuation, never reaching the coroutine's try/catch.
template <class F>
boost::asio::awaitable<std::invoke_result_t<std::decay_t<F>>>
CoOffload(boost::asio::thread_pool& pool, F&& func)
{
    using Func = std::decay_t<F>;
    using R    = std::invoke_result_t<Func>;
    static_assert(!std::is_same_v<R, void>,
        "CoOffload<R=void> not supported here — use CoOffloadVoid");

    auto current_exec = co_await boost::asio::this_coro::executor;

    co_return co_await boost::asio::async_initiate<
        const boost::asio::use_awaitable_t<>&,
        void(std::exception_ptr, R)>(
        [&pool, current_exec, func = std::forward<F>(func)]
        (auto handler) mutable {
            boost::asio::post(pool,
                [current_exec,
                 handler = std::move(handler),
                 func    = std::move(func)]() mutable {
                    std::exception_ptr eptr;
                    R result{};
                    try { result = func(); }
                    catch (...) { eptr = std::current_exception(); }
                    boost::asio::post(current_exec,
                        [handler = std::move(handler),
                         eptr,
                         result = std::move(result)]() mutable {
                            std::move(handler)(eptr, std::move(result));
                        });
                });
        },
        boost::asio::use_awaitable);
}

// Void-returning specialization. Same exception propagation
// contract; just no result value.
template <class F>
boost::asio::awaitable<void>
CoOffloadVoid(boost::asio::thread_pool& pool, F&& func)
{
    static_assert(std::is_same_v<std::invoke_result_t<std::decay_t<F>>, void>,
        "CoOffloadVoid expects a void-returning callable");

    auto current_exec = co_await boost::asio::this_coro::executor;
    co_await boost::asio::async_initiate<
        const boost::asio::use_awaitable_t<>&,
        void(std::exception_ptr)>(
        [&pool, current_exec, func = std::forward<F>(func)]
        (auto handler) mutable {
            boost::asio::post(pool,
                [current_exec,
                 handler = std::move(handler),
                 func    = std::move(func)]() mutable {
                    std::exception_ptr eptr;
                    try { func(); }
                    catch (...) { eptr = std::current_exception(); }
                    boost::asio::post(current_exec,
                        [handler = std::move(handler), eptr]() mutable {
                            std::move(handler)(eptr);
                        });
                });
        },
        boost::asio::use_awaitable);
}

} // namespace fourstory::db
