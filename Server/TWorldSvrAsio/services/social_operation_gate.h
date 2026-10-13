#pragma once
#include <boost/asio/awaitable.hpp>
#include <boost/asio/experimental/concurrent_channel.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <memory>
#include <mutex>

namespace tworldsvr {
// The source World batch queue processes social decisions sequentially. Keep
// that property across DB offloads: validate the next request only after the
// preceding transaction and its cache projection, including requests from a
// different Map socket. This is a coroutine permit, never a held thread lock.
class SocialOperationGate {
    struct State {
        using Channel=boost::asio::experimental::concurrent_channel<void(boost::system::error_code,int)>;
        explicit State(boost::asio::any_io_executor ex):permit(ex,1) {
            permit.try_send(boost::system::error_code{},0);
        }
        Channel permit;
    };
public:
    class Lease {
    public:
        explicit Lease(std::shared_ptr<State> state):m_state(std::move(state)){}
        ~Lease(){m_state->permit.try_send(boost::system::error_code{},0);}
        Lease(const Lease&)=delete;
        Lease& operator=(const Lease&)=delete;
    private:
        std::shared_ptr<State> m_state;
    };
    boost::asio::awaitable<std::unique_ptr<Lease>> Acquire() {
        const auto ex=co_await boost::asio::this_coro::executor;
        std::shared_ptr<State> state;
        {std::lock_guard guard(m_mutex);if(!m_state)m_state=std::make_shared<State>(ex);state=m_state;}
        co_await state->permit.async_receive(boost::asio::use_awaitable);
        co_return std::make_unique<Lease>(std::move(state));
    }
private:
    std::mutex m_mutex;
    std::shared_ptr<State> m_state;
};
}
