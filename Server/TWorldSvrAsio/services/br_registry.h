#pragma once

// BrRegistry — minimal in-memory state for the Battle Royale subsystem
// (legacy CBRSystem). W6-25 ships only what the five player-driven
// handlers (ADDTOBRQUEUE / BRTEAMMATEADD / BRTEAMMATEDEL /
// BRTEAMMATEADDRESULT / VOTEFORBRMAP) touch:
//
//   * the solo queue (legacy m_mapBRREG) — chars waiting to be
//     matched into a fresh team;
//   * premade teams (legacy m_mapPremadeTeam) keyed by chief char_id,
//     each with a member roster + a per-member `ready` flag + a
//     team-wide `ready` flag (FlagTeamReady's gate);
//   * per-user map + mode vote tallies (legacy m_mapMapVote /
//     m_mapVoteMode — first vote wins, no changes allowed);
//   * a `Tick()` timestamp accessor (always 0 today — the scheduler
//     isn't ported yet, same as BowRegistry).
//
// Deferred until the scheduler ports:
//   * `BS_PEACE` / `BS_ALARM` status gating (legacy `m_bStatus !=
//     BS_ALARM` rejects everything outside the pre-match window);
//   * UpdatePlayerQueue auto-fill (drains the solo queue into teams
//     once the size threshold is reached);
//   * CreateMatch / team balancing;
//   * BR_TEAM vs BR_SOLO type switch (`m_bType` / SwitchType);
//   * map-name → vote count tally exposure (we record raw per-user
//     votes; the scheduler picks the winning entry at match time).
//
// One shared_mutex protecting all maps. BR churn is low-volume so
// per-shard partitioning isn't worth it.

#include "br_constants.h"

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace tworldsvr {

struct TBrPlayer
{
    std::uint32_t char_id = 0;
    std::uint32_t key     = 0;
    std::uint8_t  klass   = 0;
    std::string   name;
    bool          ready   = false;
};

struct TBrTeam
{
    // Chief is whoever AddPlayerToQueue / JoinPremadeTeam first
    // identifies as the team owner — used as the map key.
    std::uint32_t chief_id = 0;
    bool          ready    = false;
    // Members keyed by char_id (insertion order doesn't matter — the
    // legacy iterates in map order too).
    std::unordered_map<std::uint32_t, TBrPlayer> members;
};

class BrRegistry
{
public:
    BrRegistry() = default;
    BrRegistry(const BrRegistry&) = delete;
    BrRegistry& operator=(const BrRegistry&) = delete;

    // --- Solo queue --------------------------------------------------

    // Add a char to the solo queue. Returns BOWREG_* (BR reuses the
    // BOWREG enum — legacy BRSystem.cpp:225):
    //   kSuccess         — accepted
    //   kAlreadyInQueue  — already in solo queue OR in a premade team
    //   kFail            — left for the scheduler / status-gate slice
    std::uint8_t AddPlayerToQueue(std::uint32_t char_id, std::uint32_t key,
                                  std::uint8_t klass, const std::string& name);

    // Remove from solo queue. Returns BOWREG_*.
    std::uint8_t ErasePlayerFromQueue(std::uint32_t char_id,
                                      std::uint32_t key);

    // --- Premade teams ----------------------------------------------

    // Chief accepts a mate's join. If the chief has no team yet, one
    // is seeded with the chief as the first member. Caller must have
    // already run the FindPlayerInPremade + count-cap gates.
    void JoinPremadeTeam(std::uint32_t chief_id, std::uint32_t chief_key,
                         std::uint8_t chief_class, const std::string& chief_name,
                         std::uint32_t mate_id, std::uint32_t mate_key,
                         std::uint8_t mate_class, const std::string& mate_name);

    // Drop a member by char_id. If the leaver is the chief, the team
    // itself is destroyed (legacy parity — chief-leave nukes the team).
    void ErasePlayerFromPremade(std::uint32_t char_id);

    // True if char is anywhere in a premade team (chief or mate).
    bool FindPlayerInPremade(std::uint32_t char_id) const;

    // Count of mates *plus the chief* in the team owned by chief_id.
    // 0 when chief has no team yet (legacy returns 0 too; the
    // ADDRESULT cap check uses `count + 1 > kTeamMaxCount3v3`).
    std::uint8_t GetPremadePlayerCountByChief(std::uint32_t chief_id) const;

    // chief_id of the team containing char_id, or 0. char_id may be
    // the chief itself (returns chief_id) or any member.
    std::uint32_t GetChiefIdByMateId(std::uint32_t char_id) const;

    // Mark a single mate ready (legacy FlagPlayerReady — scans all
    // premade teams; returns true on a state change). The legacy
    // sets `ready=true` whether it was already true or not but
    // returns TRUE — we report a real state change so handler logs
    // are accurate.
    bool FlagPlayerReady(std::uint32_t char_id, std::uint32_t key);

    // Mark a team ready when every *non-chief* mate is ready. Sets
    // the team's `ready` flag, leaves the per-mate flags untouched
    // (legacy parity). Returns false if no such team or any mate
    // not ready yet.
    bool FlagTeamReady(std::uint32_t chief_id);

    // Snapshot of a team's members for the UPDATEBRTEAM broadcast.
    // Returns an empty vector if no such team. `chief_name_out` is
    // filled when the chief is found in the roster.
    std::vector<TBrPlayer>
    SnapshotTeam(std::uint32_t chief_id, std::string& chief_name_out,
                 bool& ready_out) const;

    // --- Votes -------------------------------------------------------

    // First vote wins; subsequent calls from the same user are
    // silently ignored (legacy m_mapMapVote.find != end → return).
    void VoteForMap(std::uint32_t user_id, const std::string& map_name);
    void VoteForMode(std::uint32_t user_id, std::uint8_t mode);

    // --- Accessors (for tests + introspection) ----------------------

    std::uint32_t Tick() const { return m_tick; }
    std::size_t   QueueSize() const;
    std::size_t   TeamCount() const;
    std::size_t   MapVoteCount() const;
    std::size_t   ModeVoteCount() const;

    // W6-26 — opportunistic cleanup on LEAVEBATTLEFIELD (legacy
    // CBRSystem::ReleaseSinglePlayer; SSHandler.cpp:14125). Legacy
    // operates on the active-match roster (m_mapBRTeam) and teleports
    // the player home; we don't model the active match yet, so this
    // is a best-effort drop from the solo queue + any premade team
    // entry. Silent no-op when not found.
    void ReleaseSinglePlayer(std::uint32_t char_id, std::uint32_t key);

private:
    mutable std::shared_mutex m_lock;

    std::unordered_map<std::uint32_t, TBrPlayer> m_solo_queue;
    std::unordered_map<std::uint32_t, TBrTeam>   m_premade_teams;
    std::unordered_map<std::uint32_t, std::string> m_map_votes;   // user → map
    std::unordered_map<std::uint32_t, std::uint8_t> m_mode_votes; // user → mode

    std::uint32_t m_tick = 0;
};

} // namespace tworldsvr
