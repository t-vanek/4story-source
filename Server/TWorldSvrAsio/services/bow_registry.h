#pragma once

// BowRegistry — minimal in-memory state for the Bow battleground
// (legacy CTBowSystem). W6-24 ships only what the three player-
// driven handlers (ADDTOBOWQUEUE / CANCELBOWQUEUE / BOWPOINTSUPDATE)
// touch:
//
//   * a queue of `TBowEntry` rows keyed by char_id (the players
//     currently waiting for a match);
//   * per-country point counters (legacy m_bPoints[2], cleared at
//     match-end);
//   * a `Tick()` accessor reporting the timestamp of the last
//     status change (always 0 today — the scheduler / status state
//     machine isn't ported yet).
//
// Deferred until the scheduler ports:
//   * BS_PEACE / BS_ALARM status gating (legacy
//     `m_bStatus != BS_ALARM` rejects everything outside the
//     pre-match window);
//   * match creation + team balancing (CreateMatch);
//   * teleportation into the battleground map;
//   * the per-guild queue grouping (legacy m_mapGuildMember vs
//     m_mapBOWREG split — we store guild_id on the entry but don't
//     yet group on it).
//
// Single shared_mutex protecting the whole struct. Bow churn is
// low-volume (per-player queue events, not per-packet) so per-shard
// partitioning isn't worth it here.

#include "bow_constants.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

namespace tworldsvr {

struct TBowEntry
{
    std::uint32_t char_id  = 0;
    std::uint32_t key      = 0;
    std::uint8_t  country  = 0;
    std::uint32_t guild_id = 0;  // tactics first, else guild, else 0
};

class BowRegistry
{
public:
    BowRegistry() = default;
    BowRegistry(const BowRegistry&) = delete;
    BowRegistry& operator=(const BowRegistry&) = delete;

    // Add a char to the Bow queue. Returns the BOWREG_* result byte
    // mirroring legacy CTBowSystem::AddPlayerToQueue (NetCode.h):
    //   kSuccess         — accepted (entry inserted)
    //   kCountry         — `country` past TCONTRY_C (caller already
    //                      substituted aid_country when needed)
    //   kAlreadyInQueue  — char already has a queue entry
    //   kFail            — never returned here today (left for the
    //                      scheduler / status-gate slice)
    std::uint8_t AddPlayer(std::uint32_t char_id, std::uint32_t key,
                           std::uint8_t country, std::uint32_t guild_id);

    // Remove a char from the queue. Returns BOWREG_* — kSuccess on
    // erase, kFail when no matching entry (legacy parity).
    std::uint8_t RemovePlayer(std::uint32_t char_id, std::uint32_t key);

    // Per-country Bow score bump. Legacy m_bPoints[country]++ plus
    // the "if the other side was at max, reset it" wrap that the
    // legacy UpdatePoints implements (kept for parity even though
    // the BOW_MAX_POINTS constant isn't ported — we treat the
    // single-side cap as 255 and clamp instead of wrap, since the
    // legacy `if (... == BOW_MAX_POINTS / BOW_MAX_POINTS)` literal
    // expression always equals 1 and was clearly a bug; we model
    // the intent: clamp at 255, no other-side wipe).
    void UpdatePoints(std::uint8_t country);

    std::uint8_t  Points(std::uint8_t country) const;
    std::uint32_t Tick() const { return m_tick; }
    std::size_t   QueueSize() const;
    bool          Contains(std::uint32_t char_id) const;

private:
    mutable std::shared_mutex m_lock;

    std::unordered_map<std::uint32_t, TBowEntry> m_queue;
    std::array<std::uint8_t, 2> m_points{};   // [TCONTRY_D, TCONTRY_C]

    // Wall-clock second of the last status-state transition. Legacy
    // sets this in SetStatus; with no scheduler ported it stays 0
    // and the queue replies carry 0 (clients tolerate that — same
    // as a quiescent server with no pending battle).
    std::uint32_t m_tick = 0;
};

} // namespace tworldsvr
