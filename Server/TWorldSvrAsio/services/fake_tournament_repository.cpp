#include "services/fake_tournament_repository.h"

namespace tworldsvr {

// ---- seeding ------------------------------------------------------

void FakeTournamentRepository::SeedBattleTime(
    const TournamentBattleTime& bt)
{
    std::lock_guard g(m_mtx);
    m_battle_time = bt;
}

void FakeTournamentRepository::SeedMaxLevel(std::uint8_t v)
{
    std::lock_guard g(m_mtx);
    m_max_level = v;
}

void FakeTournamentRepository::SeedCurrentStep(const TnmtCurrentStep& s)
{
    std::lock_guard g(m_mtx);
    m_current_step = s;
}

void FakeTournamentRepository::SeedMainSchedule(
    std::vector<TnmtScheduleRow> rows)
{
    std::lock_guard g(m_mtx);
    m_main_schedule = std::move(rows);
}

void FakeTournamentRepository::SeedMainEntries(
    std::vector<TournamentEntrySeed> rows)
{
    std::lock_guard g(m_mtx);
    m_main_entries = std::move(rows);
}

void FakeTournamentRepository::SeedMainRewards(
    std::vector<TnmtRewardRow> rows)
{
    std::lock_guard g(m_mtx);
    m_main_rewards = std::move(rows);
}

void FakeTournamentRepository::SeedEventTime(const TnmtEventTimeRow& row)
{
    std::lock_guard g(m_mtx);
    m_event_times.push_back(row);
}

void FakeTournamentRepository::SeedEventSchedule(
    std::uint16_t tour_id, std::vector<TnmtEventStepRow> rows)
{
    std::lock_guard g(m_mtx);
    m_event_schedules[tour_id] = std::move(rows);
}

void FakeTournamentRepository::SeedEventEntries(
    std::uint16_t tour_id, std::vector<TournamentEntrySeed> rows)
{
    std::lock_guard g(m_mtx);
    m_event_entries[tour_id] = std::move(rows);
}

void FakeTournamentRepository::SeedEventRewards(
    std::uint16_t tour_id, std::vector<TnmtRewardRow> rows)
{
    std::lock_guard g(m_mtx);
    m_event_rewards[tour_id] = std::move(rows);
}

void FakeTournamentRepository::SeedChar(const TnmtPlayerBrief& info)
{
    std::lock_guard g(m_mtx);
    m_chars.push_back(info);
}

void FakeTournamentRepository::SeedPlayerRow(const TnmtPlayerRow& row)
{
    std::lock_guard g(m_mtx);
    m_player_rows.push_back(row);
}

// ---- reads --------------------------------------------------------

std::optional<TournamentBattleTime>
FakeTournamentRepository::LoadBattleTime()
{
    std::lock_guard g(m_mtx);
    return m_battle_time;
}

std::optional<std::uint8_t> FakeTournamentRepository::LoadMaxLevel()
{
    std::lock_guard g(m_mtx);
    return m_max_level;
}

std::optional<TnmtCurrentStep>
FakeTournamentRepository::LoadCurrentStep()
{
    std::lock_guard g(m_mtx);
    return m_current_step;
}

std::vector<TnmtScheduleRow>
FakeTournamentRepository::LoadMainSchedule()
{
    std::lock_guard g(m_mtx);
    return m_main_schedule;
}

std::vector<TournamentEntrySeed>
FakeTournamentRepository::LoadMainEntries()
{
    std::lock_guard g(m_mtx);
    return m_main_entries;
}

std::vector<TnmtRewardRow> FakeTournamentRepository::LoadMainRewards()
{
    std::lock_guard g(m_mtx);
    return m_main_rewards;
}

std::vector<TnmtEventTimeRow>
FakeTournamentRepository::LoadEventTimes()
{
    std::lock_guard g(m_mtx);
    return m_event_times;
}

std::vector<TnmtEventStepRow>
FakeTournamentRepository::LoadEventSchedule(std::uint16_t tour_id)
{
    std::lock_guard g(m_mtx);
    auto it = m_event_schedules.find(tour_id);
    return it == m_event_schedules.end()
        ? std::vector<TnmtEventStepRow>{} : it->second;
}

std::vector<TournamentEntrySeed>
FakeTournamentRepository::LoadEventEntries(std::uint16_t tour_id)
{
    std::lock_guard g(m_mtx);
    auto it = m_event_entries.find(tour_id);
    return it == m_event_entries.end()
        ? std::vector<TournamentEntrySeed>{} : it->second;
}

std::vector<TnmtRewardRow>
FakeTournamentRepository::LoadEventRewards(std::uint16_t tour_id)
{
    std::lock_guard g(m_mtx);
    auto it = m_event_rewards.find(tour_id);
    return it == m_event_rewards.end()
        ? std::vector<TnmtRewardRow>{} : it->second;
}

std::vector<TnmtPlayerRow> FakeTournamentRepository::LoadPlayers()
{
    std::lock_guard g(m_mtx);
    return m_player_rows;
}

// ---- writes -------------------------------------------------------

void FakeTournamentRepository::SaveEventSchedule(
    std::uint16_t tour_id, const TournamentBattleTime& time,
    const std::vector<TnmtEventStepRow>& steps)
{
    std::lock_guard g(m_mtx);
    m_save_schedule_calls.push_back(
        SaveScheduleCall{tour_id, time, steps});
}

void FakeTournamentRepository::DeleteEvent(std::uint16_t tour_id)
{
    std::lock_guard g(m_mtx);
    m_delete_event_calls.push_back(tour_id);
}

void FakeTournamentRepository::SaveEventEntries(
    std::uint16_t tour_id,
    const std::vector<TournamentEntrySeed>& entries)
{
    std::lock_guard g(m_mtx);
    m_save_entries_calls.push_back(SaveEntriesCall{tour_id, entries});
}

std::vector<TnmtPlayerBrief>
FakeTournamentRepository::FindCharInfoByName(const std::string& name)
{
    std::lock_guard g(m_mtx);
    std::vector<TnmtPlayerBrief> out;
    for (const auto& c : m_chars)
        if (c.name == name)
            out.push_back(c);
    return out;
}

void FakeTournamentRepository::ApplyPlayer(const TnmtApplyOp& op)
{
    std::lock_guard g(m_mtx);
    m_apply_calls.push_back(op);
}

void FakeTournamentRepository::ClearPersisted()
{
    std::lock_guard g(m_mtx);
    ++m_clear_calls;
}

void FakeTournamentRepository::SaveStatus(std::uint16_t id,
                                          std::uint8_t group,
                                          std::uint8_t step)
{
    std::lock_guard g(m_mtx);
    m_save_status_calls.push_back(TnmtCurrentStep{id, group, step});
}

// ---- traces -------------------------------------------------------

std::vector<FakeTournamentRepository::SaveScheduleCall>
FakeTournamentRepository::SaveScheduleCalls() const
{
    std::lock_guard g(m_mtx);
    return m_save_schedule_calls;
}

std::vector<std::uint16_t>
FakeTournamentRepository::DeleteEventCalls() const
{
    std::lock_guard g(m_mtx);
    return m_delete_event_calls;
}

std::vector<FakeTournamentRepository::SaveEntriesCall>
FakeTournamentRepository::SaveEntriesCalls() const
{
    std::lock_guard g(m_mtx);
    return m_save_entries_calls;
}

std::vector<TnmtApplyOp> FakeTournamentRepository::ApplyCalls() const
{
    std::lock_guard g(m_mtx);
    return m_apply_calls;
}

int FakeTournamentRepository::ClearCalls() const
{
    std::lock_guard g(m_mtx);
    return m_clear_calls;
}

std::vector<TnmtCurrentStep>
FakeTournamentRepository::SaveStatusCalls() const
{
    std::lock_guard g(m_mtx);
    return m_save_status_calls;
}

void FakeTournamentRepository::SetNextPostId(std::uint32_t id)
{
    std::lock_guard g(m_mtx);
    m_next_post_id = id;
}

std::optional<std::uint32_t> FakeTournamentRepository::Payback(
    std::uint32_t char_id, std::uint32_t gold, std::uint32_t silver,
    std::uint32_t copper)
{
    std::lock_guard g(m_mtx);
    m_payback_calls.push_back(
        PaybackCall{char_id, gold, silver, copper});
    if (!m_next_post_id)
        return std::nullopt;
    return m_next_post_id++;
}

void FakeTournamentRepository::SaveResult(std::uint8_t step,
                                          std::uint8_t ret,
                                          std::uint32_t win_id,
                                          std::uint32_t lose_id)
{
    std::lock_guard g(m_mtx);
    m_result_calls.push_back(ResultCall{step, ret, win_id, lose_id});
}

std::vector<FakeTournamentRepository::PaybackCall>
FakeTournamentRepository::PaybackCalls() const
{
    std::lock_guard g(m_mtx);
    return m_payback_calls;
}

std::vector<FakeTournamentRepository::ResultCall>
FakeTournamentRepository::ResultCalls() const
{
    std::lock_guard g(m_mtx);
    return m_result_calls;
}

} // namespace tworldsvr
