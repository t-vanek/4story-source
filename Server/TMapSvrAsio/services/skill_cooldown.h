#pragma once

// Skill reuse-cooldown — the faithful, certain half of the skill-use gate.
//
// Legacy CTSkill::CanUse(tick) = !GetReuseRemainTick(tick): a skill can be
// used again only once m_dwReuseDelay has elapsed since its last use. The
// MP/HP cost gate (GetRequiredMP/HP) is the *other* half and is deferred —
// its per-level rate (m_f1stRateX^level) and %-of-max-MP variant need the
// char's max-MP, which isn't modelled yet. The cooldown, by contrast, is a
// clean elapsed-time check with no such dependency, so it lands first.
//
// The pure ReuseRemainMs / CanUseSkill are unit-tested without a clock; the
// SkillCooldownTracker holds the per-(char, skill) last-use stamps the live
// handler records. The legacy frame tick becomes a steady_clock millisecond.

#include <cstdint>
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

// Per-(char, skill) last-use stamps. TryUse is the gate the handler calls:
// if the skill is off cooldown it records `now` and returns true; otherwise
// it leaves the stamp untouched and returns false (legacy SKILL_SPEEDYUSE).
// Thread-safe — handlers may run on the io_context or a SOCI worker.
class SkillCooldownTracker
{
public:
    bool TryUse(std::uint32_t char_id, std::uint16_t skill_id,
                std::uint64_t now_ms, std::uint32_t reuse_delay_ms)
    {
        const std::uint64_t key = Key(char_id, skill_id);
        std::lock_guard<std::mutex> lk(m_mtx);
        const auto it = m_last.find(key);
        const std::uint64_t last = it == m_last.end() ? 0 : it->second;
        if (!CanUseSkill(last, now_ms, reuse_delay_ms))
            return false;
        m_last[key] = now_ms;
        return true;
    }

    // Cooldown remaining for a (char, skill) without recording a use.
    std::uint64_t RemainMs(std::uint32_t char_id, std::uint16_t skill_id,
                           std::uint64_t now_ms,
                           std::uint32_t reuse_delay_ms) const
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        const auto it = m_last.find(Key(char_id, skill_id));
        const std::uint64_t last = it == m_last.end() ? 0 : it->second;
        return ReuseRemainMs(last, now_ms, reuse_delay_ms);
    }

    // Drop a char's stamps on disconnect so the map doesn't grow forever.
    void Forget(std::uint32_t char_id)
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        for (auto it = m_last.begin(); it != m_last.end();)
            it = (static_cast<std::uint32_t>(it->first >> 16) == char_id)
                     ? m_last.erase(it) : std::next(it);
    }

private:
    static std::uint64_t Key(std::uint32_t char_id, std::uint16_t skill_id)
    {
        return (static_cast<std::uint64_t>(char_id) << 16) | skill_id;
    }

    mutable std::mutex                              m_mtx;
    std::unordered_map<std::uint64_t, std::uint64_t> m_last;
};

} // namespace tmapsvr
