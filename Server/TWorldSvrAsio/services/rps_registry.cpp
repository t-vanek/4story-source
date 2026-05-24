#include "rps_registry.h"

#include <cstdint>

namespace tworldsvr {

namespace {

// Legacy DAY_ONE = 86400 seconds; the 30-day expiry window in
// OnMW_RPSGAME_ACK is `30 * DAY_ONE` (SSHandler.cpp:13208).
constexpr std::int64_t kSecondsPerDay = 86400;
constexpr std::int64_t kExpiryWindowDays = 30;

} // namespace

RpsRegistry::Outcome
RpsRegistry::RecordWin(std::uint8_t type, std::uint8_t win_count,
                       std::uint32_t char_id, std::int64_t now,
                       std::vector<PersistOp>& out_ops)
{
    std::unique_lock g(m_lock);
    auto it = m_games.find(Key(type, win_count));
    if (it == m_games.end()) return Outcome::kNotFound;

    auto& g_entry = it->second;
    if (g_entry.win_keep == 0)
        return Outcome::kAllowed;       // No cap configured.

    // Two-pass over win_dates (legacy SSHandler.cpp:13201 loop):
    //   * expire entries older than 30 days (emit remove op),
    //   * count entries still inside win_period.
    const std::int64_t window =
        static_cast<std::int64_t>(g_entry.win_period) * kSecondsPerDay;
    const std::int64_t expiry_cutoff = kExpiryWindowDays * kSecondsPerDay;

    std::uint16_t fresh_count = 0;
    for (auto wi = g_entry.win_dates.begin(); wi != g_entry.win_dates.end();)
    {
        const std::int64_t age = now - *wi;
        if (age < window)
        {
            ++fresh_count;
            ++wi;
        }
        else if (age > expiry_cutoff)
        {
            out_ops.push_back({/*insert=*/false, /*char_id=*/0, type,
                               win_count, *wi});
            wi = g_entry.win_dates.erase(wi);
        }
        else
        {
            // Stale (between win_period and 30 days) — keep in
            // memory but don't count toward the cap. Legacy parity.
            ++wi;
        }
    }

    if (fresh_count >= g_entry.win_keep) return Outcome::kCapReached;

    g_entry.win_dates.push_back(now);
    out_ops.push_back({/*insert=*/true, char_id, type, win_count, now});
    return Outcome::kAllowed;
}

bool
RpsRegistry::Set(std::uint8_t type, std::uint8_t win_count,
                 std::uint8_t win_prob, std::uint8_t draw_prob,
                 std::uint8_t lose_prob, std::uint16_t win_keep,
                 std::uint16_t win_period)
{
    std::unique_lock g(m_lock);
    auto it = m_games.find(Key(type, win_count));
    if (it == m_games.end()) return false;
    it->second.win_prob   = win_prob;
    it->second.draw_prob  = draw_prob;
    it->second.lose_prob  = lose_prob;
    it->second.win_keep   = win_keep;
    it->second.win_period = win_period;
    return true;
}

bool
RpsRegistry::Insert(const TRpsGame& game)
{
    std::unique_lock g(m_lock);
    return m_games.emplace(Key(game.type, game.win_count), game).second;
}

std::vector<TRpsGame>
RpsRegistry::Snapshot() const
{
    std::shared_lock g(m_lock);
    std::vector<TRpsGame> out;
    out.reserve(m_games.size());
    for (const auto& [_, game] : m_games)
        out.push_back(game);
    return out;
}

std::size_t
RpsRegistry::Size() const
{
    std::shared_lock g(m_lock);
    return m_games.size();
}

} // namespace tworldsvr
