#include "bow_registry.h"

namespace tworldsvr {

std::uint8_t
BowRegistry::AddPlayer(std::uint32_t char_id, std::uint32_t key,
                       std::uint8_t country, std::uint32_t guild_id)
{
    if (country > bow::kCountryC) return bow::kCountry;

    std::unique_lock g(m_lock);
    auto [it, inserted] = m_queue.emplace(char_id,
        TBowEntry{char_id, key, country, guild_id});
    return inserted ? bow::kSuccess : bow::kAlreadyInQueue;
}

std::uint8_t
BowRegistry::RemovePlayer(std::uint32_t char_id, std::uint32_t key)
{
    std::unique_lock g(m_lock);
    auto it = m_queue.find(char_id);
    if (it == m_queue.end() || it->second.key != key)
        return bow::kFail;
    m_queue.erase(it);
    return bow::kSuccess;
}

void
BowRegistry::UpdatePoints(std::uint8_t country)
{
    if (country > bow::kCountryC) return;

    std::unique_lock g(m_lock);
    auto& self = m_points[country];
    if (self != 255) ++self;
    // Legacy UpdatePoints also touches the *other* side's counter
    // on a wrap (the BOW_MAX_POINTS reset), but the legacy
    // expression `m_bPoints[X] == BOW_MAX_POINTS / BOW_MAX_POINTS`
    // is a `== 1` literal — a bug. We honour the intent (own side
    // clamps; no spurious wipe) and leave a TODO if a future slice
    // ports the scheduler and decides what the real cap is.
}

std::uint8_t
BowRegistry::Points(std::uint8_t country) const
{
    if (country > bow::kCountryC) return 0;
    std::shared_lock g(m_lock);
    return m_points[country];
}

std::size_t
BowRegistry::QueueSize() const
{
    std::shared_lock g(m_lock);
    return m_queue.size();
}

bool
BowRegistry::Contains(std::uint32_t char_id) const
{
    std::shared_lock g(m_lock);
    return m_queue.find(char_id) != m_queue.end();
}

} // namespace tworldsvr
