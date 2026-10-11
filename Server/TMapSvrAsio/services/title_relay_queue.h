#pragma once
#include "asio_session.h"
#include <algorithm>
#include <deque>
#include <mutex>
#include <unordered_map>

namespace tmapsvr {
// Correlates unchanged World echoes with this exact local client connection.
// It is notification state only: persistence has already committed, and nothing
// is replayed after reconnect. World and per-client input preserve FIFO order.
class TitleRelayQueue {
    struct Pending {std::uint64_t epoch{};std::weak_ptr<tnetlib::AsioSession> client;std::deque<std::vector<std::byte>> bodies;};
    std::mutex mutex_;
    std::unordered_map<std::uint32_t,Pending> pending_;
public:
    void Add(std::uint32_t id,const std::shared_ptr<tnetlib::AsioSession>& client,std::vector<std::byte> body,std::uint64_t epoch=0) {
        std::lock_guard lock(mutex_);
        std::erase_if(pending_,[](const auto& row){auto s=row.second.client.lock();return !s||!s->IsOpen();});
        auto& p=pending_[id];if(p.client.lock()!=client||p.epoch!=epoch){p.client=client;p.epoch=epoch;p.bodies.clear();}
        p.bodies.push_back(std::move(body));
    }
    void Discard(std::uint32_t id,const std::shared_ptr<tnetlib::AsioSession>& client) {
        std::lock_guard lock(mutex_);auto it=pending_.find(id);
        if(it!=pending_.end()&&it->second.client.lock()==client)pending_.erase(it);
    }
    bool Take(std::uint32_t id,const std::shared_ptr<tnetlib::AsioSession>& client,const std::vector<std::byte>& body,std::uint64_t epoch=0) {
        std::lock_guard lock(mutex_);auto it=pending_.find(id);
        if(it==pending_.end())return false;
        if(it->second.client.lock()!=client||it->second.epoch!=epoch){pending_.erase(it);return false;}
        auto& q=it->second.bodies;if(q.empty()||q.front()!=body)return false;
        q.pop_front();if(q.empty())pending_.erase(it);return true;
    }
};
}
