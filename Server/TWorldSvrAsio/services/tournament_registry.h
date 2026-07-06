#pragma once

// TournamentRegistry — the in-memory tournament state core (W6-50).
// Mirrors the legacy CTWorldSvrModule members:
//
//   m_mapTournament         → catalogue (tournament id → entry map)
//   m_mapTournamentSchedule → schedules_ (id → enable + step map)
//   m_mapTournamentTime     → times_ (id → BattleTime)
//   m_wTournamentID         → next_id_
//   m_TournamentSchedule    → active_id_ (the next-up schedule the
//                             timer walks; W6-51 consumes it)
//   m_tournament            → current_ (TOURNAMENTSTATUS)
//   m_mapTNMTPlayer         → players_
//
// The legacy split these across the timer thread (schedules/times)
// and the batch thread (catalogue/current/players) and bridged them
// with SM_TOURNAMENTEVENT_REQ/_ACK posts; the asio port runs both
// halves on the io_context strand, so one registry owns everything
// and the SM legs collapse into direct calls (the wire handlers for
// the SM messages still exist for parity).
//
// Legacy side effects (SM ACK posts, DM persists, ctrl-svr echoes)
// are NOT fired from here — mutators return the affected ids and the
// wire layer (handlers_tournament.cpp) drives the fan-out, matching
// how BowRegistry / BrRegistry / MonthRankRegistry are factored.
//
// Step-map keys are MAKEWORD(step, group) = step | group << 8 — the
// main tournament carries per-group ladders, operator lucky events
// are always group 0 (their key degenerates to the bare step, which
// is exactly the legacy behaviour: main inserts MAKEWORD'd keys,
// TET_SCHEDULEADD inserts bare-step keys).

#include "tournament_constants.h"

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace tworldsvr {

// One TTOURNAMENTREWARDCHART / TTNMTEVENTREWARDTABLE row (legacy
// TNMTREWARD, TWorldType.h:1231). Field order == wire order.
struct TnmtReward
{
    std::uint8_t  chart_type   = 0;
    std::uint16_t item_id      = 0;
    std::uint8_t  count        = 0;
    std::uint32_t cls          = 0;   // m_dwClass
    std::uint8_t  check_shield = 0;
};

// Entry chart fields (legacy TOURNAMENTENTRY minus the player pools,
// TWorldType.h:1240). Wire order of the TET_ENTRYADD payload.
struct TournamentEntrySeed
{
    std::uint8_t  entry_id       = 0;
    std::uint8_t  group          = 0;     // main chart only; events = 0
    std::string   name;
    std::uint8_t  type           = 0;     // ENTRY_PARTY / ENTRY_PRIVATE
    std::uint32_t cls            = 0;     // m_dwClass
    std::uint32_t fee            = 0;
    std::uint32_t fee_back       = 0;
    std::uint16_t permit_item_id = 0;
    std::uint8_t  permit_count   = 0;
    std::uint8_t  min_level      = 0;
    std::uint8_t  max_level      = 0xFF;
    std::vector<TnmtReward> rewards;
};

// One schedule step (legacy TOURNAMENTSTEP, TWorldType.h:1264).
struct TournamentStep
{
    std::uint8_t  group  = 0;
    std::uint8_t  step   = 0;    // TNMTSTEP_*
    std::uint32_t period = 0;    // seconds
    std::int64_t  start  = 0;    // unix; 0 = already fired
    std::int64_t  end    = 0;
};

// Battle-time slot (legacy TBATTLETIME fields the tournament math
// consumes: m_bWeek / m_bDay / m_dwBattleStart).
struct TournamentBattleTime
{
    std::uint8_t  week      = 0;   // n-th weekday of the month; 0 = off
    std::uint8_t  day       = 0;   // 1=Sunday .. 7=Saturday (CTime)
    std::uint32_t start_sec = 0;   // seconds past midnight
};

// Registered player (legacy TNMTPLAYER, TWorldType.h:1191). The
// batting ledger fields stay legacy-only until the match/result
// slice lands.
struct TnmtPlayer
{
    std::uint32_t char_id    = 0;
    std::uint8_t  country    = 0;
    std::string   name;
    std::uint8_t  level      = 0;
    std::uint8_t  cls        = 0;
    std::uint32_t rank       = 0;
    std::uint32_t month_rank = 0;
    std::uint8_t  entry_id   = 0;
    std::uint32_t chief_id   = 0;
    std::string   hwid;
    std::uint32_t ip_addr    = 0;
    std::string   guild_name;
    std::uint8_t  slot_id    = tournament::kTournamentSlot;
    std::uint8_t  result[tournament::kMatchCount] = {};

    std::map<std::uint32_t, std::shared_ptr<TnmtPlayer>> party;
};

// (char_id, country, name, level, class) — the CTBLGetCharInfo row
// shape shared by the TET_PLAYERADD lookup and its wire echo.
struct TnmtPlayerBrief
{
    std::uint32_t char_id = 0;
    std::uint8_t  country = 0;
    std::string   name;
    std::uint8_t  level   = 0;
    std::uint8_t  cls     = 0;
};

class TournamentRegistry
{
public:
    using StepMap = std::map<std::uint16_t, TournamentStep>;

    TournamentRegistry() = default;
    TournamentRegistry(const TournamentRegistry&) = delete;
    TournamentRegistry& operator=(const TournamentRegistry&) = delete;

    static constexpr std::uint16_t StepKey(std::uint8_t step,
                                           std::uint8_t group)
    { return static_cast<std::uint16_t>(step)
           | static_cast<std::uint16_t>(group) << 8; }

    // ---- SetTournamentTime (TWorldSvr.cpp:5659) -----------------
    //
    // Computes per-step start/end from the battle-time slot (the
    // week-th `day` weekday of the current month, or next month
    // when `month_base`), registers the schedule, and — on resume
    // (tour_id == tnmt_id && tnmt_step != READY) — grafts the
    // persisted current step back into `current_`.
    //
    // Returns id=0 when the slot is off (week==0), empty steps, the
    // computed window is already past, or it overlaps another
    // schedule. `evicted` carries schedules the legacy would
    // SCHEDULEDEL on the way (its SM ACK + DelTournamentSchedule +
    // DM persist fan-out is the caller's job):
    //   - past-window re-adds evict the schedule itself;
    //   - a main-tournament (id 1) overlap evicts the other side.
    // Legacy parity: an existing tour_id's stored steps are
    // overwritten (in place) even when the call then fails.
    struct SetTimeResult
    {
        std::uint16_t id = 0;
        std::vector<std::uint16_t> evicted;
    };
    SetTimeResult SetTournamentTime(StepMap steps,
                                    const TournamentBattleTime& bt,
                                    std::uint16_t tour_id,
                                    bool month_base, bool enable,
                                    std::int64_t now,
                                    std::uint16_t tnmt_id = 0,
                                    std::uint8_t tnmt_group = 0,
                                    std::uint8_t tnmt_step =
                                        tournament::kStepReady);

    // TET_SCHEDULEADD pre-step (SSHandler.cpp:12367): an existing
    // id's battle time is replaced before SetTournamentTime runs.
    // Returns false when the id is unknown (legacy inserts nothing).
    bool ReplaceTime(std::uint16_t id, const TournamentBattleTime& bt);

    bool HasSchedule(std::uint16_t id) const;

    // DelTournamentSchedule minus its fan-out (TWorldSvr.cpp:5866):
    // erases the time + schedule rows. The legacy follow-ups
    // (TournamentUpdate + DM_TNMTEVENTSCHEDULEDEL persist) stay with
    // the caller.
    void DeleteSchedule(std::uint16_t id);

    // ---- TournamentUpdate election (TWorldSvr.cpp:6934) ---------
    //
    // Flips `enable` on updated_tour_id (when registered), elects
    // the schedule with the earliest first step, and re-points the
    // active slot. `changed` mirrors the legacy "post
    // SM_TOURNAMENTUPDATE_REQ + DM_TOURNAMENTCLEAR" condition; the
    // caller then runs RebuildCurrent(id, steps) + the
    // MW_TOURNAMENTINFO broadcast + the persist.
    struct ElectResult
    {
        bool changed = false;
        std::uint16_t id = 0;               // 0 = nothing scheduled
        std::vector<TournamentStep> steps;  // payload snapshot
    };
    ElectResult ElectNextSchedule(std::uint16_t updated_tour_id);

    // ---- OnSM_TOURNAMENTUPDATE_REQ state leg (SSHandler:11468) --
    //
    // TournamentClear + re-point current_ at `id` (when the
    // catalogue knows it) + adopt the step list + recompute the
    // per-entry base prize. Returns false when the catalogue miss
    // zeroed current_ (the legacy early-return branch) — the caller
    // broadcasts TournamentInfo either way.
    bool RebuildCurrent(std::uint16_t id,
                        const std::vector<TournamentStep>& steps);

    // TournamentClear (TWorldSvr.cpp:6254). Drops every player and
    // empties the current tournament's pools. (The legacy also
    // clears each online char's batting map — batting lands with
    // the match/result slice.)
    void ClearCurrent();

    // ---- catalogue ----------------------------------------------
    //
    // TET_ENTRYADD replace (SSHandler.cpp:12142): evicts the old
    // entry set (TNMTEntryDelete per entry — removed registered
    // players are returned so the caller can persist the removals)
    // and adopts the new one.
    std::vector<std::uint32_t> ReplaceEntries(
        std::uint16_t tid, std::vector<TournamentEntrySeed> entries);

    // TNMTEntryDelete (TWorldSvr.cpp:7058). Returns the char ids
    // whose registration the caller must persist-remove (only
    // populated when tid is the current tournament).
    std::vector<std::uint32_t> DeleteEntry(std::uint16_t tid,
                                           std::uint8_t entry_id);

    // TET_SCHEDULEDEL ACK-side catalogue drop (SSHandler.cpp:12562).
    bool DropTournament(std::uint16_t tid);

    bool HasTournament(std::uint16_t tid) const;

    // Boot-time seed: insert a tournament's entry chart wholesale
    // (TWorldSvr.cpp:1652 / 1756). Replaces any existing set.
    void SeedEntries(std::uint16_t tid,
                     std::vector<TournamentEntrySeed> entries);

    // Boot follow-up (TWorldSvr.cpp:1787-1798): once the resumed
    // current tournament's entries are seeded, recompute the
    // per-entry base prize. Returns the entry count (0 when the
    // current id has no catalogue entry — the caller skips the
    // player reload then, once that slice exists).
    std::uint8_t RecomputeCurrentBase();

    // ---- players (operator path) --------------------------------

    bool CurrentEntryExists(std::uint8_t entry_id) const;
    bool HasPlayer(std::uint32_t char_id) const;

    // TET_PLAYERADD ACK-side add (SSHandler.cpp:12629): guards
    // (current entry exists, char not registered) + AddTNMTPlayer
    // with chief=self into the 1st pool. Returns false when a guard
    // failed (the caller echoes char_id=0).
    bool AddPlayer1st(std::uint8_t entry_id,
                      const TnmtPlayerBrief& info,
                      const std::string& guild_name);

    // TET_PLAYERDEL (SSHandler.cpp:12233): name scan over the
    // registered players, gated on tid being current + the entry
    // existing. Returns the removed char id (caller persists).
    std::optional<std::uint32_t> RemovePlayerByName(
        std::uint16_t tid, std::uint8_t entry_id,
        const std::string& name);

    std::size_t PlayerCount() const;

    // ---- TET_LIST snapshots --------------------------------------

    // SM REQ leg (SSHandler.cpp:12302): every battle-time row plus
    // its schedule's steps (empty when the id has no schedule —
    // legacy emits count 0).
    struct ScheduleListRow
    {
        std::uint16_t id = 0;
        TournamentBattleTime time;
        std::vector<TournamentStep> steps;
    };
    std::vector<ScheduleListRow> ScheduleList() const;

    // SM ACK leg (SSHandler.cpp:12452): the catalogue with, per
    // entry, the roster the operator console shows — the 1st pool
    // before the party step, party chiefs from it on.
    struct EntryListRow
    {
        TournamentEntrySeed seed;
        std::vector<TnmtPlayerBrief> players;
    };
    struct TournamentListRow
    {
        std::uint16_t id = 0;
        std::vector<EntryListRow> entries;
    };
    std::vector<TournamentListRow> CatalogueList() const;

    // ---- MW_TOURNAMENTINFO_REQ payload (SSSender.cpp:3512) -------
    struct InfoSnapshot
    {
        std::uint8_t first_group_count = 0;
        std::uint8_t group             = 0;
        std::uint8_t step              = 0;
        std::uint8_t max_level         = 0;
        std::vector<TournamentEntrySeed> entries;  // current only
    };
    InfoSnapshot Info() const;

    // ---- status scalars ------------------------------------------

    std::uint16_t CurrentId() const;
    std::uint8_t  CurrentStep() const;
    std::uint8_t  CurrentGroup() const;
    std::uint16_t ActiveScheduleId() const;   // m_TournamentSchedule
    std::uint16_t NextScheduleId() const;     // m_wTournamentID

    // Boot-only cluster scalars the info broadcast carries.
    void SetFirstGroupCount(std::uint8_t v);
    void SetMaxLevel(std::uint8_t v);

private:
    struct ScheduleInfo
    {
        bool    enable = false;
        StepMap steps;
    };

    struct Entry
    {
        TournamentEntrySeed seed;
        std::map<std::uint32_t, std::shared_ptr<TnmtPlayer>> first;
        std::map<std::uint32_t, std::shared_ptr<TnmtPlayer>> normal;
        std::map<std::uint32_t, std::shared_ptr<TnmtPlayer>> player;
    };
    using EntryMap = std::map<std::uint8_t, Entry>;

    // TOURNAMENTSTATUS (TWorldType.h:1279). `entries` resolves live
    // through catalogue_ (the legacy stored a raw pointer).
    struct Status
    {
        std::uint16_t id       = 0;
        std::uint8_t  group    = 0;
        std::uint8_t  step     = tournament::kStepReady;
        bool          selected = false;
        std::uint8_t  base     = 0;
        std::uint32_t sum      = 0;
        StepMap       steps;
    };

    // Locked helpers (callers hold m_lock).
    Entry*       CurrentEntryLocked(std::uint8_t entry_id);
    const Entry* CurrentEntryLocked(std::uint8_t entry_id) const;
    std::vector<std::uint32_t> DeleteEntryLocked(std::uint16_t tid,
                                                 std::uint8_t entry_id);
    void RemovePlayerLocked(Entry& entry,
                            const std::shared_ptr<TnmtPlayer>& p);
    std::uint8_t CurrentEntryCountLocked() const;

    mutable std::mutex m_lock;

    std::map<std::uint16_t, EntryMap>     catalogue_;
    std::map<std::uint16_t, ScheduleInfo> schedules_;
    std::map<std::uint16_t, TournamentBattleTime> times_;
    std::map<std::uint32_t, std::shared_ptr<TnmtPlayer>> players_;

    Status        current_{};
    std::uint16_t active_id_ = 0;
    std::uint16_t next_id_   = 0;

    std::uint8_t first_group_count_ = 0;
    std::uint8_t max_level_         = 0;
};

} // namespace tworldsvr
