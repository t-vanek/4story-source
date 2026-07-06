#pragma once

// FakeTournamentRepository — in-memory ITournamentRepository with a
// seeded read side and a call trace on the write side.

#include "services/tournament_repository.h"

#include <map>
#include <mutex>

namespace tworldsvr {

class FakeTournamentRepository : public ITournamentRepository
{
public:
    // ---- test seeding ---------------------------------------------
    void SeedBattleTime(const TournamentBattleTime& bt);
    void SeedMaxLevel(std::uint8_t v);
    void SeedCurrentStep(const TnmtCurrentStep& s);
    void SeedMainSchedule(std::vector<TnmtScheduleRow> rows);
    void SeedMainEntries(std::vector<TournamentEntrySeed> rows);
    void SeedMainRewards(std::vector<TnmtRewardRow> rows);
    void SeedEventTime(const TnmtEventTimeRow& row);
    void SeedEventSchedule(std::uint16_t tour_id,
                           std::vector<TnmtEventStepRow> rows);
    void SeedEventEntries(std::uint16_t tour_id,
                          std::vector<TournamentEntrySeed> rows);
    void SeedEventRewards(std::uint16_t tour_id,
                          std::vector<TnmtRewardRow> rows);
    void SeedChar(const TnmtPlayerBrief& info);
    void SeedPlayerRow(const TnmtPlayerRow& row);

    // ---- ITournamentRepository ------------------------------------
    std::optional<TournamentBattleTime> LoadBattleTime() override;
    std::optional<std::uint8_t>         LoadMaxLevel() override;
    std::optional<TnmtCurrentStep>      LoadCurrentStep() override;
    std::vector<TnmtScheduleRow>        LoadMainSchedule() override;
    std::vector<TournamentEntrySeed>    LoadMainEntries() override;
    std::vector<TnmtRewardRow>          LoadMainRewards() override;
    std::vector<TnmtEventTimeRow>       LoadEventTimes() override;
    std::vector<TnmtEventStepRow>
        LoadEventSchedule(std::uint16_t tour_id) override;
    std::vector<TournamentEntrySeed>
        LoadEventEntries(std::uint16_t tour_id) override;
    std::vector<TnmtRewardRow>
        LoadEventRewards(std::uint16_t tour_id) override;
    std::vector<TnmtPlayerRow>          LoadPlayers() override;

    void SaveEventSchedule(
        std::uint16_t tour_id, const TournamentBattleTime& time,
        const std::vector<TnmtEventStepRow>& steps) override;
    void DeleteEvent(std::uint16_t tour_id) override;
    void SaveEventEntries(
        std::uint16_t tour_id,
        const std::vector<TournamentEntrySeed>& entries) override;
    std::vector<TnmtPlayerBrief>
        FindCharInfoByName(const std::string& name) override;
    void ApplyPlayer(const TnmtApplyOp& op) override;
    void ClearPersisted() override;
    void SaveStatus(std::uint16_t id, std::uint8_t group,
                    std::uint8_t step) override;
    std::optional<std::uint32_t> Payback(
        std::uint32_t char_id, std::uint32_t gold,
        std::uint32_t silver, std::uint32_t copper) override;
    void SaveResult(std::uint8_t step, std::uint8_t ret,
                    std::uint32_t win_id,
                    std::uint32_t lose_id) override;

    // Next post id handed out by Payback (0 = fail/no post).
    void SetNextPostId(std::uint32_t id);

    // ---- write-side call traces ------------------------------------
    struct SaveScheduleCall
    {
        std::uint16_t tour_id = 0;
        TournamentBattleTime time;
        std::vector<TnmtEventStepRow> steps;
    };
    struct SaveEntriesCall
    {
        std::uint16_t tour_id = 0;
        std::vector<TournamentEntrySeed> entries;
    };
    std::vector<SaveScheduleCall> SaveScheduleCalls() const;
    std::vector<std::uint16_t>    DeleteEventCalls() const;
    std::vector<SaveEntriesCall>  SaveEntriesCalls() const;
    std::vector<TnmtApplyOp>      ApplyCalls() const;
    int                           ClearCalls() const;
    std::vector<TnmtCurrentStep>  SaveStatusCalls() const;
    struct PaybackCall
    {
        std::uint32_t char_id = 0;
        std::uint32_t gold = 0, silver = 0, copper = 0;
    };
    std::vector<PaybackCall>      PaybackCalls() const;
    struct ResultCall
    {
        std::uint8_t  step = 0, ret = 0;
        std::uint32_t win_id = 0, lose_id = 0;
    };
    std::vector<ResultCall>       ResultCalls() const;

private:
    mutable std::mutex m_mtx;

    std::optional<TournamentBattleTime> m_battle_time;
    std::optional<std::uint8_t>         m_max_level;
    std::optional<TnmtCurrentStep>      m_current_step;
    std::vector<TnmtScheduleRow>        m_main_schedule;
    std::vector<TournamentEntrySeed>    m_main_entries;
    std::vector<TnmtRewardRow>          m_main_rewards;
    std::vector<TnmtEventTimeRow>       m_event_times;
    std::map<std::uint16_t, std::vector<TnmtEventStepRow>>
        m_event_schedules;
    std::map<std::uint16_t, std::vector<TournamentEntrySeed>>
        m_event_entries;
    std::map<std::uint16_t, std::vector<TnmtRewardRow>>
        m_event_rewards;
    std::vector<TnmtPlayerBrief>        m_chars;
    std::vector<TnmtPlayerRow>          m_player_rows;

    std::vector<SaveScheduleCall> m_save_schedule_calls;
    std::vector<std::uint16_t>    m_delete_event_calls;
    std::vector<SaveEntriesCall>  m_save_entries_calls;
    std::vector<TnmtApplyOp>      m_apply_calls;
    int                           m_clear_calls = 0;
    std::vector<TnmtCurrentStep>  m_save_status_calls;
    std::vector<PaybackCall>      m_payback_calls;
    std::vector<ResultCall>       m_result_calls;
    std::uint32_t                 m_next_post_id = 9000;
};

} // namespace tworldsvr
