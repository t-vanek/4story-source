#include "services/tournament_boot.h"

#include "services/tournament_constants.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <map>
#include <utility>
#include <vector>

namespace tworldsvr {

namespace {

// Attach reward rows to their entries by entry id.
void AttachRewards(std::vector<TournamentEntrySeed>&  entries,
                   const std::vector<TnmtRewardRow>&  rewards)
{
    for (const auto& r : rewards)
        for (auto& e : entries)
            if (e.entry_id == r.entry_id)
            {
                e.rewards.push_back(r.reward);
                break;
            }
}

// Persisted step watermark → per-match result array
// (TWorldSvr.cpp:1831): a row that reached SFINAL implies the
// QFINAL was won, FINAL implies both.
void MapPersistedResult(const TnmtPlayerRow& row,
                        std::uint8_t (&result)[tournament::kMatchCount])
{
    namespace tnmt = tournament;
    if (row.step == tnmt::kStepQFinal)
        result[tnmt::kMatchQFinal] = row.result;
    else if (row.step == tnmt::kStepSFinal)
    {
        result[tnmt::kMatchQFinal] = tnmt::kWinWin;
        result[tnmt::kMatchSFinal] = row.result;
    }
    else if (row.step == tnmt::kStepFinal)
    {
        result[tnmt::kMatchQFinal] = tnmt::kWinWin;
        result[tnmt::kMatchSFinal] = tnmt::kWinWin;
        result[tnmt::kMatchFinal]  = row.result;
    }
}

// Boot-shaped eviction fan-out: catalogue drop + persist delete.
// (No peers are connected during boot, so the legacy SM ACK /
// TournamentUpdate broadcast legs have no observable effect beyond
// these two.)
void HandleBootEvictions(TournamentRegistry&               registry,
                         ITournamentRepository&            repo,
                         const std::vector<std::uint16_t>& evicted)
{
    for (const std::uint16_t id : evicted)
    {
        registry.DropTournament(id);
        repo.DeleteEvent(id);
        spdlog::info("tournament boot: schedule {} evicted", id);
    }
}

} // namespace

void LoadTournamentState(TournamentRegistry&    registry,
                         ITournamentRepository& repo,
                         std::int64_t           now,
                         const TnmtFirstGroupFn& is_first_group)
{
    namespace tnmt = tournament;

    // CSPGetLimitedLevel (TWorldSvr.cpp:1574). The legacy treats a
    // failure as fatal; the asio port degrades to 0 with a warning.
    if (const auto lvl = repo.LoadMaxLevel())
        registry.SetMaxLevel(*lvl);
    else
        spdlog::warn("tournament boot: TGetLimitedLevel unavailable "
                     "- max level defaults to 0");

    // Persisted mid-flight step (CTBLTournamentCurrentStep).
    TnmtCurrentStep cur{};
    if (const auto c = repo.LoadCurrentStep())
        cur = *c;

    // ---- main tournament (id 1) ---------------------------------
    TournamentBattleTime bt{};
    if (const auto b = repo.LoadBattleTime())
        bt = *b;

    TournamentRegistry::StepMap steps;
    for (const auto& row : repo.LoadMainSchedule())
    {
        TournamentStep s{};
        s.group  = row.group;
        s.step   = row.step;
        s.period = row.period;
        steps.emplace(
            TournamentRegistry::StepKey(s.step, s.group), s);
    }

    std::uint8_t first_group_count = 0;
    if (!steps.empty())
    {
        const auto res = registry.SetTournamentTime(std::move(steps),
            bt, tnmt::kMainTournamentId, /*month_base=*/false,
            /*enable=*/true, now, cur.id, cur.group, cur.step);
        HandleBootEvictions(registry, repo, res.evicted);
        if (res.id)
        {
            auto entries = repo.LoadMainEntries();
            AttachRewards(entries, repo.LoadMainRewards());
            if (!entries.empty())
                first_group_count = static_cast<std::uint8_t>(
                    std::min(static_cast<float>(entries.size()) *
                                     4.0f / 3.0f + 1.999f,
                             static_cast<float>(
                                 tnmt::kFirstGroupCap)));
            registry.SeedEntries(res.id, std::move(entries));
        }
    }
    registry.SetFirstGroupCount(first_group_count);

    // ---- operator lucky events ----------------------------------
    //
    // The legacy inserts the time rows into MAPTOURNAMENTTIME first
    // and iterates that (ascending ids) — the resume graft keys off
    // next_id_, so the order matters.
    auto event_times = repo.LoadEventTimes();
    std::sort(event_times.begin(), event_times.end(),
        [](const TnmtEventTimeRow& a, const TnmtEventTimeRow& b)
        { return a.tour_id < b.tour_id; });
    for (const auto& t : event_times)
    {
        TournamentRegistry::StepMap es;
        for (const auto& row : repo.LoadEventSchedule(t.tour_id))
        {
            TournamentStep s{};
            s.step   = row.step;
            s.period = row.period;
            es.emplace(TournamentRegistry::StepKey(s.step, 0), s);
        }
        if (es.empty())
            continue;

        const auto res = registry.SetTournamentTime(std::move(es),
            t.time, t.tour_id, /*month_base=*/false, /*enable=*/true,
            now, cur.id, cur.group, cur.step);
        HandleBootEvictions(registry, repo, res.evicted);
        if (res.id)
        {
            auto entries = repo.LoadEventEntries(t.tour_id);
            AttachRewards(entries, repo.LoadEventRewards(t.tour_id));
            registry.SeedEntries(res.id, std::move(entries));
        }
    }

    // ---- resume follow-up (TWorldSvr.cpp:1787) -------------------
    //
    // The legacy gates the player reload only on the catalogue row
    // existing (GetTournament != NULL); the group-filtered entry
    // count feeds nothing but the base prize.
    if (registry.CurrentId() &&
        registry.HasTournament(registry.CurrentId()))
    {
        const std::uint8_t ec = registry.RecomputeCurrentBase();
        {
            // TVIEW_TOURNAMENTPLAYER reload (TWorldSvr.cpp:1807):
            // chief rows re-register into the 1st / normal pool
            // (fame first-grade membership picks 1st when the
            // group-0 1st step exists with a period), member rows
            // stash for the second pass and re-attach to their
            // chief's party (dropped when the chief vanished —
            // legacy parity). The persisted step watermark maps
            // onto the per-match result array. Guild names deferred
            // (legacy m_mapCharGuild boots from the guild-member
            // tables; the port's chars connect later — display-only
            // gap until the char interacts). The legacy
            // step>=PARTY TournamentSelectPlayer() re-run is the
            // match-engine slice.
            const bool b1st = registry.CurrentHasFirstStep();
            std::vector<TnmtPlayerRow> members;
            std::size_t chiefs = 0;
            for (const auto& row : repo.LoadPlayers())
            {
                if (row.char_id != row.chief_id)
                {
                    members.push_back(row);
                    continue;
                }
                TournamentRegistry::TnmtPlayerSeed seed{};
                seed.brief = TnmtPlayerBrief{row.char_id, row.country,
                    row.name, row.level, row.cls};
                seed.hwid    = row.hwid;
                seed.ip_addr = row.ip_addr;
                MapPersistedResult(row, seed.result);
                const std::uint8_t step =
                    (b1st && is_first_group &&
                     is_first_group(row.country, row.char_id))
                        ? tournament::kStep1st
                        : tournament::kStepNormal;
                if (registry.AddPlayerAtStep(row.entry_id, seed,
                        step, row.char_id))
                    ++chiefs;
            }
            if (registry.CurrentStep() >= tournament::kStepParty)
            {
                // TWorldSvr.cpp:1867: re-seed the bracket before the
                // member pass re-attaches parties. Fee-back paybacks
                // persist; the mail leg is a no-op at boot (no chars
                // online — legacy m_mapTCHAR is empty here too).
                for (const auto& due : registry.SelectPlayers())
                {
                    const std::uint32_t copper = due.fee_back % 1000;
                    const std::uint32_t silver =
                        (due.fee_back / 1000) % 1000;
                    const std::uint32_t gold =
                        due.fee_back / 1000 / 1000;
                    repo.Payback(due.char_id, gold, silver, copper);
                }
            }
            std::size_t attached = 0;
            for (const auto& row : members)
            {
                TournamentRegistry::TnmtPlayerSeed seed{};
                seed.brief = TnmtPlayerBrief{row.char_id, row.country,
                    row.name, row.level, row.cls};
                seed.hwid    = row.hwid;
                seed.ip_addr = row.ip_addr;
                MapPersistedResult(row, seed.result);
                if (registry.AddPlayerAtStep(row.entry_id, seed,
                        tournament::kStepParty, row.chief_id))
                    ++attached;
            }
            spdlog::info("tournament boot: resumed tournament {} at "
                         "step {} ({} entrie(s), {} chief(s) + {} "
                         "member(s) reloaded)", registry.CurrentId(),
                registry.CurrentStep(), ec, chiefs, attached);
        }
    }

    // ---- initial election (TWorldSvr.cpp:230) --------------------
    //
    // The legacy timer thread runs TournamentUpdate() once at boot.
    // No peers are connected yet, so the SM_TOURNAMENTUPDATE /
    // TournamentInfo broadcast has no receiver — only the rebuild
    // matters here.
    const auto er = registry.ElectNextSchedule(0);
    if (er.changed)
        registry.RebuildCurrent(er.id, er.steps);

    spdlog::info("tournament boot: {} schedule(s), active={}, "
                 "current={} step={}", registry.ScheduleList().size(),
        registry.ActiveScheduleId(), registry.CurrentId(),
        registry.CurrentStep());
}

} // namespace tworldsvr
