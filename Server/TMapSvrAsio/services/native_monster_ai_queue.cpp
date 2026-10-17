#include "native_monster_ai_queue.h"
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace tmapsvr {
NativeMonsterAiQueue::NativeMonsterAiQueue(std::size_t capacity):m_capacity(capacity) {
    if(!capacity)throw std::invalid_argument("Native AI queue capacity is zero");
}
void NativeMonsterAiQueue::Publish(NativeAiInvocation call) {
    if(m_stopped)throw std::runtime_error("Native AI queue retired");
    if(!call.binding||!call.generation||!call.delay)
        throw std::runtime_error("Invalid native delayed AI request identity/delay");
    if(Pending()>=m_capacity)throw std::runtime_error("Native AI outstanding-job limit");
    m_requests.push_back(std::move(call));
}
void NativeMonsterAiQueue::Turn(std::uint32_t tick) {
    if(m_stopped)throw std::runtime_error("Native AI queue retired");
    const std::uint32_t elapsed=m_tick?tick-*m_tick:0; // source DWORD subtraction/wrap
    m_tick=tick;
    if(elapsed) {
        std::size_t retained=0;
        for(std::size_t index=0;index<m_timers.size();++index) {
            auto& timer=m_timers[index];
            timer.elapsed+=elapsed; // source DWORD accumulation, not a saturating deadline
            if(timer.elapsed>=timer.call.delay)m_batch.push_back(std::move(timer.call));
            else {
                if(retained!=index)m_timers[retained]=std::move(timer);
                ++retained;
            }
        }
        m_timers.resize(retained); // stable due/remaining order; no invalidated source iterator
    }
    if(!m_requests.empty()) {
        m_timers.push_back({std::move(m_requests.front()),0});
        m_requests.pop_front();
    }
}
std::optional<NativeAiInvocation> NativeMonsterAiQueue::TakeAcknowledgement() {
    if(m_stopped||m_batch.empty())return {};
    auto call=std::move(m_batch.front());m_batch.pop_front();return call;
}
void NativeMonsterAiQueue::CancelObject(const NativeAiInvocation& identity) {
    const auto matches=[&](const NativeAiInvocation& call) {
        return call.map==identity.map&&call.party==identity.party&&call.channel==identity.channel&&
            call.monster==identity.monster&&call.generation==identity.generation;
    };
    std::erase_if(m_requests,matches);std::erase_if(m_batch,matches);
    std::erase_if(m_timers,[&](const Timer& timer){return matches(timer.call);});
}
void NativeMonsterAiQueue::Stop() {
    m_stopped=true;m_requests.clear();m_timers.clear();m_batch.clear();
}
} // namespace tmapsvr
