#pragma once

// SociTournamentRepository — ITournamentRepository against the
// tournament chart/table family (TTOURNAMENTSTATUSTABLE,
// TTOURNAMENTSCHEDULECHART, TTOURNAMENTCHART(+REWARD),
// TTNMTEVENT*TABLE, TBATTLETIMECHART, TCHARTABLE) and the
// TTnmtEvent* / TTournamentApply SPs on the world pool.

#include "services/tournament_repository.h"

#include "fourstory/db/session_pool.h"

namespace tworldsvr {

class SociTournamentRepository : public ITournamentRepository
{
public:
    explicit SociTournamentRepository(fourstory::db::SessionPool& pool)
        : m_pool(pool) {}

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

private:
    fourstory::db::SessionPool& m_pool;
};

} // namespace tworldsvr
