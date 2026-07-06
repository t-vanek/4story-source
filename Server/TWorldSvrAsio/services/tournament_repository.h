#pragma once

// ITournamentRepository — DB surface for the W6-50 tournament core:
// the boot loads (TWorldSvr.cpp:1580-1889 minus the player table —
// that reload belongs to the player vertical), the operator-tool
// persists the legacy routed through DM_TNMTEVENT* / DM_TOURNAMENT*
// round-trips (repository-collapsed per README §D), and the
// CTBLGetCharInfo name lookup behind TET_PLAYERADD.

#include "tournament_registry.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace tworldsvr {

// TTOURNAMENTSTATUSTABLE row (CTBLTournamentCurrentStep,
// DBAccess.h:1023) — the persisted mid-flight step for boot resume.
struct TnmtCurrentStep
{
    std::uint16_t id    = 0;
    std::uint8_t  group = 0;
    std::uint8_t  step  = 0;
};

// TTOURNAMENTSCHEDULECHART row (CTBLTournamentSchedule,
// DBAccess.h:1039) — the main tournament's step ladder.
struct TnmtScheduleRow
{
    std::uint8_t  group  = 0;
    std::uint8_t  step   = 0;
    std::uint32_t period = 0;
};

// TTNMTEVENTTIMETABLE row (CTBLTnmtEventTime, DBAccess.h:1127).
struct TnmtEventTimeRow
{
    std::uint16_t tour_id = 0;
    TournamentBattleTime time;
};

// TTNMTEVENTSCHEDULETABLE row (CTBLTnmtEventSchedule,
// DBAccess.h:1150) — group is always 0 for operator events.
struct TnmtEventStepRow
{
    std::uint8_t  step   = 0;
    std::uint32_t period = 0;
};

// TTOURNAMENTREWARDCHART / TTNMTEVENTREWARDTABLE row.
struct TnmtRewardRow
{
    std::uint8_t entry_id = 0;
    TnmtReward   reward;
};

// TVIEW_TOURNAMENTPLAYER row (CTBLTournamentPlayer, DBAccess.h:1286)
// — the mid-flight registration reload for boot resume.
struct TnmtPlayerRow
{
    std::uint32_t char_id  = 0;
    std::string   name;
    std::uint32_t chief_id = 0;
    std::uint8_t  cls      = 0;
    std::uint8_t  country  = 0;
    std::uint8_t  level    = 0;
    std::uint8_t  entry_id = 0;
    std::uint8_t  step     = 0;   // persisted TNMTSTEP_* watermark
    std::uint8_t  result   = 0;   // that step's TNMTWIN_*
    std::string   hwid;
    std::uint32_t ip_addr  = 0;
};

// TTournamentApply parameters (CSPTournamentApply, DBAccess.h:2712).
// The remove path uses the legacy defaults (entry/chief/hwid/ip all
// zero — DMSender.cpp:551 default args).
struct TnmtApplyOp
{
    bool          add      = false;
    std::uint32_t char_id  = 0;
    std::uint8_t  entry_id = 0;
    std::uint32_t chief_id = 0;
    std::string   hwid;
    std::uint32_t ip_addr  = 0;
};

class ITournamentRepository
{
public:
    virtual ~ITournamentRepository() = default;

    // ---- boot loads ---------------------------------------------

    // TBATTLETIMECHART WHERE bType = 2 (BT_TOURNAMENT) — only the
    // week / day / start fields the schedule math consumes.
    virtual std::optional<TournamentBattleTime> LoadBattleTime() = 0;

    // TGetLimitedLevel SP (CSPGetLimitedLevel, DBAccess.h:2902).
    virtual std::optional<std::uint8_t> LoadMaxLevel() = 0;

    virtual std::optional<TnmtCurrentStep> LoadCurrentStep() = 0;
    virtual std::vector<TnmtScheduleRow>   LoadMainSchedule() = 0;

    // TTOURNAMENTCHART WHERE bEnable = 1 (min_level/max_level fixed
    // at 0/0xFF by the boot loader, DBAccess.h:1060) + the reward
    // chart (bItemCount > 0).
    virtual std::vector<TournamentEntrySeed> LoadMainEntries() = 0;
    virtual std::vector<TnmtRewardRow>       LoadMainRewards() = 0;

    virtual std::vector<TnmtEventTimeRow> LoadEventTimes() = 0;
    virtual std::vector<TnmtEventStepRow>
        LoadEventSchedule(std::uint16_t tour_id) = 0;
    virtual std::vector<TournamentEntrySeed>
        LoadEventEntries(std::uint16_t tour_id) = 0;
    virtual std::vector<TnmtRewardRow>
        LoadEventRewards(std::uint16_t tour_id) = 0;

    // TVIEW_TOURNAMENTPLAYER (boot resume of the mid-flight
    // registrations, TWorldSvr.cpp:1807).
    virtual std::vector<TnmtPlayerRow> LoadPlayers() = 0;

    // ---- operator persists (repository-collapsed DM legs) --------

    // OnDM_TNMTEVENTSCHEDULEADD_REQ (SSHandler.cpp:12679):
    // TTnmtEventTime + one TTnmtEventSchedule call per step.
    virtual void SaveEventSchedule(
        std::uint16_t tour_id, const TournamentBattleTime& time,
        const std::vector<TnmtEventStepRow>& steps) = 0;

    // OnDM_TNMTEVENTSCHEDULEDEL_REQ (SSHandler.cpp:12721):
    // TTnmtEventDel.
    virtual void DeleteEvent(std::uint16_t tour_id) = 0;

    // OnDM_TNMTEVENTENTRYADD_REQ (SSHandler.cpp:12735): the
    // clear-all call (entry 0 + empty name) followed by one
    // TTnmtEventEntry + per-reward TTnmtEventReward per entry.
    virtual void SaveEventEntries(
        std::uint16_t tour_id,
        const std::vector<TournamentEntrySeed>& entries) = 0;

    // OnDM_TOURNAMENTEVENTCHARINFO_REQ (SSHandler.cpp:11731):
    // CTBLGetCharInfo — TCHARTABLE WHERE szName = :name.
    virtual std::vector<TnmtPlayerBrief>
        FindCharInfoByName(const std::string& name) = 0;

    // DM_TOURNAMENTAPPLY_REQ (SSHandler.cpp:12010): TTournamentApply.
    virtual void ApplyPlayer(const TnmtApplyOp& op) = 0;

    // DM_TOURNAMENTCLEAR_REQ (SSHandler.cpp:12041):
    // TTournamentClear(1) — fired whenever TournamentUpdate re-points
    // the current tournament.
    virtual void ClearPersisted() = 0;

    // DM_TOURNAMENTSTATUS_REQ (SSHandler.cpp:12051):
    // TTournamentStatus(id, group, step) — the mid-flight step
    // persist the tick fires alongside each SM_TOURNAMENT_REQ (what
    // LoadCurrentStep reads back at boot).
    virtual void SaveStatus(std::uint16_t id, std::uint8_t group,
                            std::uint8_t step) = 0;
};

} // namespace tworldsvr
