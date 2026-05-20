#pragma once

// IPlayerHpRegistry — server-side HP/MP store for live player sessions.
//
// Maintained separately from MapSessionState so monster AI and other
// server-side code can read/modify player vitals without access to
// the per-session coroutine stack.
//
// Lifecycle:
//   Register   — OnConReadyReq (player enters world)
//   Unregister — HandleConnection teardown (session close)
//
// Thread safety: single io_context (no mutex needed).

#include <algorithm>
#include <cstdint>
#include <unordered_map>

namespace tmapsvr {

struct PlayerVitals
{
    std::uint32_t hp     = 0;
    std::uint32_t max_hp = 0;
    std::uint32_t mp     = 0;
    std::uint32_t max_mp = 0;

    bool IsDead()  const { return hp == 0; }
    bool IsAlive() const { return hp > 0; }
};

class IPlayerHpRegistry
{
public:
    virtual ~IPlayerHpRegistry() = default;

    virtual void Register(std::uint32_t char_id,
                          std::uint32_t hp, std::uint32_t max_hp,
                          std::uint32_t mp, std::uint32_t max_mp) = 0;

    virtual void Unregister(std::uint32_t char_id) = 0;

    // Returns nullptr if char_id is not registered.
    virtual const PlayerVitals* Get(std::uint32_t char_id) const = 0;

    // Apply HP delta (negative = damage, positive = heal).
    // Clamps to [0, max_hp]. Returns new HP. Returns 0 if not found.
    virtual std::uint32_t ApplyHpDelta(std::uint32_t char_id,
                                       std::int64_t  delta) = 0;

    virtual std::size_t Size() const = 0;
};

class LocalPlayerHpRegistry : public IPlayerHpRegistry
{
public:
    void Register(std::uint32_t char_id,
                  std::uint32_t hp, std::uint32_t max_hp,
                  std::uint32_t mp, std::uint32_t max_mp) override
    {
        m_vitals[char_id] = { hp, max_hp, mp, max_mp };
    }

    void Unregister(std::uint32_t char_id) override
    {
        m_vitals.erase(char_id);
    }

    const PlayerVitals* Get(std::uint32_t char_id) const override
    {
        auto it = m_vitals.find(char_id);
        return it != m_vitals.end() ? &it->second : nullptr;
    }

    std::uint32_t ApplyHpDelta(std::uint32_t char_id,
                               std::int64_t  delta) override
    {
        auto it = m_vitals.find(char_id);
        if (it == m_vitals.end()) return 0;
        auto& v = it->second;
        const std::int64_t new_hp =
            std::clamp(static_cast<std::int64_t>(v.hp) + delta,
                       std::int64_t{0},
                       static_cast<std::int64_t>(v.max_hp));
        v.hp = static_cast<std::uint32_t>(new_hp);
        return v.hp;
    }

    std::size_t Size() const override { return m_vitals.size(); }

private:
    std::unordered_map<std::uint32_t, PlayerVitals> m_vitals;
};

} // namespace tmapsvr
