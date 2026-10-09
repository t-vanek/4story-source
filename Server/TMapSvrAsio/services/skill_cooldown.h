#pragma once

// Skill reuse gate and transferable remaining durations. References:
// TSkill.cpp:73–117, TObjBase.cpp:4568 and SSHandler.cpp:4880–4915.

#include "domain/skill.h"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <unordered_set>
#include <vector>
#include <mutex>
#include <unordered_map>

namespace tmapsvr {

// Milliseconds remaining on a skill's cooldown. last_use_ms == 0 means
// "never used" → ready. A non-monotonic now (now < last) is treated as
// ready rather than underflowing. Pure (legacy GetReuseRemainTick).
inline std::uint64_t ReuseRemainMs(std::uint64_t last_use_ms,
                                   std::uint64_t now_ms,
                                   std::uint32_t reuse_delay_ms)
{
    if (last_use_ms == 0 || now_ms < last_use_ms)
        return 0;
    const std::uint64_t elapsed = now_ms - last_use_ms;
    return elapsed >= reuse_delay_ms ? 0 : (reuse_delay_ms - elapsed);
}

// Legacy CanUse — ready iff nothing remains on the cooldown.
inline bool CanUseSkill(std::uint64_t last_use_ms, std::uint64_t now_ms,
                        std::uint32_t reuse_delay_ms)
{
    return ReuseRemainMs(last_use_ms, now_ms, reuse_delay_ms) == 0;
}

// Imported remaining durations are independent of a template's base delay.
// Legacy OnDM_LOADCHAR_ACK installs dwRemainTick as m_dwDelayTick (SSHandler
// :4904); no process clock is sent over the wire. Store a duration and its local
// origin, avoiding deadline overflow and the old last_use==0 sentinel ambiguity.
inline std::uint64_t SkillClockMs() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
class SkillCooldownTracker
{
public:
    bool TryUse(std::uint32_t char_id, std::uint16_t skill_id,
                std::uint64_t now_ms, std::uint32_t reuse_delay_ms,
                std::span<const std::uint16_t> same_kind={},std::uint32_t kind_delay_ms=0)
    {
        std::lock_guard lock(m_mtx);
        const auto key=Key(char_id,skill_id);
        const auto it=m_timers.find(key);
        if(it!=m_timers.end() && Remaining(it->second,now_ms))return false;
        if(reuse_delay_ms)m_timers[key]={now_ms,reuse_delay_ms};
        else m_timers.erase(key);
        // CTSkill::Use only extends a running duration. SDELAY_KIND returns
        // the supplied kind delay directly, without attack-speed scaling.
        if(kind_delay_ms)for(const auto id:same_kind) {
            const auto other=Key(char_id,id);const auto found=m_timers.find(other);
            if(found==m_timers.end()||Remaining(found->second,now_ms)<kind_delay_ms)
                m_timers[other]={now_ms,kind_delay_ms};
        }
        return true;
    }
    std::uint32_t RemainMs(std::uint32_t char_id,std::uint16_t skill_id,std::uint64_t now_ms) const {
        std::lock_guard lock(m_mtx);
        const auto it=m_timers.find(Key(char_id,skill_id));
        return it==m_timers.end()?0:Remaining(it->second,now_ms);
    }
    // Compatibility for existing callers: duration was fixed when armed; a
    // changed template cannot shorten an imported/live outstanding cooldown.
    std::uint32_t RemainMs(std::uint32_t char_id,std::uint16_t skill_id,
                           std::uint64_t now_ms,std::uint32_t) const {
        return RemainMs(char_id,skill_id,now_ms);
    }
    void Restore(std::uint32_t char_id,std::span<const SkillRow> skills,std::uint64_t now_ms) {
        std::unordered_set<std::uint16_t> seen;
        for(const auto& skill:skills)
            if(!seen.insert(skill.wSkillID).second)throw std::invalid_argument("Duplicate restored skill cooldown");
        std::lock_guard lock(m_mtx);
        ForgetLocked(char_id);
        for(const auto& skill:skills)if(skill.dwRemainTick)
            m_timers.emplace(Key(char_id,skill.wSkillID),Timer{now_ms,skill.dwRemainTick});
    }
    // Snapshot all active entries under one lock at one caller-supplied instant.
    // Export never mutates or rearms timers and is ordered independently of the
    // hash table. Caller merges these remaining durations into learned ranks.
    std::vector<std::pair<std::uint16_t,std::uint32_t>> Snapshot(std::uint32_t char_id,std::uint64_t now_ms) const {
        std::lock_guard lock(m_mtx);
        std::vector<std::pair<std::uint16_t,std::uint32_t>> out;
        for(const auto& [key,timer]:m_timers)if(static_cast<std::uint32_t>(key>>16)==char_id)
            if(const auto remaining=Remaining(timer,now_ms))out.emplace_back(static_cast<std::uint16_t>(key),remaining);
        std::sort(out.begin(),out.end());return out;
    }
    void Forget(std::uint32_t char_id) {std::lock_guard lock(m_mtx);ForgetLocked(char_id);}
private:
    struct Timer {std::uint64_t started{};std::uint32_t duration{};};
    static std::uint32_t Remaining(const Timer& t,std::uint64_t now) {
        // A backwards injected clock cannot bypass a live gate.
        const auto elapsed=now<t.started?0:now-t.started;
        return elapsed>=t.duration?0:t.duration-static_cast<std::uint32_t>(elapsed);
    }
    static std::uint64_t Key(std::uint32_t c,std::uint16_t s){return (std::uint64_t(c)<<16)|s;}
    void ForgetLocked(std::uint32_t c){std::erase_if(m_timers,[c](const auto& row){return static_cast<std::uint32_t>(row.first>>16)==c;});}
    mutable std::mutex m_mtx;
    std::unordered_map<std::uint64_t,Timer> m_timers;
};

} // namespace tmapsvr
