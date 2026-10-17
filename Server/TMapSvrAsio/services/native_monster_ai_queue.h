#pragma once
#include "native_monster_ai.h"
#include <deque>
#include <optional>
#include <vector>

namespace tmapsvr {
// Owned equivalents of SayToAI, TAIBUF and SayToBATCH. The caller serializes
// access; no borrowed packet, executor or legacy command-pointer handle survives.
class NativeMonsterAiQueue {
public:
    explicit NativeMonsterAiQueue(std::size_t capacity=65536);
    void Publish(NativeAiInvocation);
    // AIThread: advance existing timers BEFORE accepting one queued request.
    // First sample establishes the thread's clock; startup time is not elapsed
    // time for requests which have not reached OnSM_AICMD_REQ yet.
    void Turn(std::uint32_t tick);
    std::optional<NativeAiInvocation> TakeAcknowledgement();
    void CancelObject(const NativeAiInvocation& identity);
    void Stop();
    bool ImmediateWork() const {return !m_requests.empty()||!m_batch.empty();}
    std::size_t Pending() const {return m_requests.size()+m_timers.size()+m_batch.size();}
    std::uint32_t Tick() const {return m_tick.value();}
private:
    struct Timer {NativeAiInvocation call;std::uint32_t elapsed{};};
    std::size_t m_capacity;
    bool m_stopped{};
    std::optional<std::uint32_t> m_tick;
    std::deque<NativeAiInvocation> m_requests,m_batch;
    std::vector<Timer> m_timers;
};
} // namespace tmapsvr
