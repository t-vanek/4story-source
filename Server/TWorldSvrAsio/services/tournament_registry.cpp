#include "services/tournament_registry.h"

#include <ctime>
#include <utility>

namespace tworldsvr {

namespace {

std::tm LocalTm(std::int64_t unix_sec)
{
    std::time_t t = static_cast<std::time_t>(unix_sec);
    std::tm     lt{};
#ifdef _WIN32
    localtime_s(&lt, &t);
#else
    localtime_r(&t, &lt);
#endif
    return lt;
}

// CTime(year, month, day, 0, 0, 0).GetTime() — mktime-normalized
// (a day past the month's end rolls forward, the release-MFC
// behaviour the legacy scan relies on when it probes day 30/31).
std::int64_t MakeLocalMidnight(int year, int month, int day)
{
    std::tm tm{};
    tm.tm_year  = year - 1900;
    tm.tm_mon   = month - 1;
    tm.tm_mday  = day;
    tm.tm_isdst = -1;
    const std::time_t t = std::mktime(&tm);
    return t == static_cast<std::time_t>(-1) ? -1
        : static_cast<std::int64_t>(t);
}

// CTime::GetDayOfWeek(): 1 = Sunday .. 7 = Saturday.
int WeekdayOf(std::int64_t unix_sec)
{
    return LocalTm(unix_sec).tm_wday + 1;
}

} // namespace

// ---- SetTournamentTime (TWorldSvr.cpp:5659) ---------------------

TournamentRegistry::SetTimeResult
TournamentRegistry::SetTournamentTime(StepMap steps,
                                      const TournamentBattleTime& bt,
                                      std::uint16_t tour_id,
                                      bool month_base, bool enable,
                                      std::int64_t now,
                                      std::uint16_t tnmt_id,
                                      std::uint8_t tnmt_group,
                                      std::uint8_t tnmt_step)
{
    std::lock_guard g(m_lock);
    SetTimeResult res{};

    if (!bt.week || steps.empty())
        return res;

    // Existing ids are overwritten in place (the legacy handler
    // clears + refills the stored map before validation runs, so a
    // failing re-add still leaves the new steps behind).
    StepMap* work = &steps;
    auto it_sched = schedules_.end();
    if (tour_id)
        it_sched = schedules_.find(tour_id);
    if (it_sched != schedules_.end())
    {
        it_sched->second.steps = std::move(steps);
        work = &it_sched->second.steps;
    }

    const std::tm cur   = LocalTm(now);
    const int cur_year  = cur.tm_year + 1900;
    const int cur_month = cur.tm_mon + 1;

    std::int64_t dstart    = 0;
    std::int64_t start_day = 0;   // legacy `CTime start(0)`

    for (int m = month_base ? 1 : 0; m <= 1; ++m)
    {
        int year  = cur_year;
        int month = cur_month + m;
        if (month > 12)
        {
            year  = cur_year + 1;
            month = 1;
        }

        // Find the week-th `day` weekday of that month.
        int hits = 0;
        for (int i = 1;
             i <= static_cast<int>(bt.week) * 7; )
        {
            if (i > 31)
                break;
            const std::int64_t t = MakeLocalMidnight(year, month, i);
            if (t < 0)
                break;
            if (WeekdayOf(t) == bt.day)
            {
                if (++hits == bt.week)
                {
                    start_day = t;
                    break;
                }
                i += 7;
            }
            else
                ++i;
        }

        if (start_day)
        {
            dstart = start_day + bt.start_sec;
            for (auto& [key, sc] : *work)
            {
                sc.start = dstart;
                sc.end   = dstart + sc.period;
                dstart   = sc.end;
            }
            if (dstart > now)
                break;
        }
    }

    if (dstart <= now)
    {
        // Window already past. A registered schedule evicts itself
        // (legacy pointer-matches pTour against the schedule map and
        // DelTournamentSchedule's it); a fresh map just fails.
        if (work != &steps)
        {
            const std::uint16_t evict = it_sched->first;
            times_.erase(evict);
            schedules_.erase(it_sched);
            res.evicted.push_back(evict);
        }
        return res;
    }

    // Overlap scan against every other schedule. The main
    // tournament (id 1) evicts the loser; anyone else just fails.
    const std::int64_t ns = work->begin()->second.start;
    const std::int64_t ne = work->rbegin()->second.end;
    for (auto it = schedules_.begin(); it != schedules_.end(); ++it)
    {
        if (it->second.steps.empty() || it->first == tour_id)
            continue;
        const std::int64_t cs = it->second.steps.begin()->second.start;
        const std::int64_t ce = it->second.steps.rbegin()->second.end;
        if ((ns >= cs && ns <= ce) || (ne >= cs && ne <= ce))
        {
            if (tour_id == tournament::kMainTournamentId)
            {
                res.evicted.push_back(it->first);
                times_.erase(it->first);
                schedules_.erase(it);
            }
            return res;
        }
    }

    // Register.
    std::uint16_t id = tour_id;
    if (!id)
    {
        id = ++next_id_;
        ScheduleInfo si;
        si.enable = enable;
        si.steps  = std::move(steps);
        work = &schedules_.emplace(id, std::move(si)).first->second.steps;
        times_[id] = bt;
    }
    else if (it_sched == schedules_.end())
    {
        ScheduleInfo si;
        si.enable = enable;
        si.steps  = std::move(steps);
        work = &schedules_.emplace(id, std::move(si)).first->second.steps;
        times_[id] = bt;
        if (next_id_ < id)
            next_id_ = id;
    }
    res.id = id;

    // Boot-resume graft (TWorldSvr.cpp:5799): re-adopt the persisted
    // current step, compressing the ladder back to the nearest
    // re-enter step and re-basing its clock on `now` (+10 min for
    // the step being resumed).
    if (id == tnmt_id && tnmt_step != tournament::kStepReady)
    {
        bool         edit = false;
        std::uint8_t step = tnmt_step;
        if (step >= tournament::kStepFEnter)
        {
            step = tournament::kStepFEnter;
            edit = true;
        }
        else if (step >= tournament::kStepSFEnter)
        {
            step = tournament::kStepSFEnter;
            edit = true;
        }
        else if (step == tournament::kStepEnter)
            edit = true;
        if (edit)
            dstart = now;

        current_          = Status{};
        current_.id       = next_id_;    // legacy uses m_wTournamentID
        current_.group    = tnmt_group;
        current_.step     = step;
        active_id_        = next_id_;

        for (auto& [key, sc] : *work)
        {
            std::uint16_t add_time = 0;
            if (edit)
            {
                if (StepKey(sc.step, sc.group) >=
                        StepKey(step, tnmt_group) && sc.period)
                {
                    if (sc.group == tnmt_group && sc.step == step)
                        add_time = 60 * 10;
                    sc.start = dstart;
                    sc.end   = dstart + sc.period + add_time;
                    dstart   = sc.end;
                }
                else
                {
                    sc.start = 0;
                    sc.end   = 0;
                }
            }

            TournamentStep s{};
            s.group  = sc.group;
            s.step   = sc.step;
            s.period = sc.period + add_time;
            s.start  = sc.start;
            current_.steps.emplace(StepKey(sc.step, sc.group), s);
        }
    }

    return res;
}

bool TournamentRegistry::ReplaceTime(std::uint16_t id,
                                     const TournamentBattleTime& bt)
{
    std::lock_guard g(m_lock);
    if (schedules_.find(id) == schedules_.end())
        return false;
    times_[id] = bt;
    return true;
}

bool TournamentRegistry::HasSchedule(std::uint16_t id) const
{
    std::lock_guard g(m_lock);
    return schedules_.find(id) != schedules_.end();
}

void TournamentRegistry::DeleteSchedule(std::uint16_t id)
{
    std::lock_guard g(m_lock);
    times_.erase(id);
    schedules_.erase(id);
}

// ---- TournamentUpdate election (TWorldSvr.cpp:6934) -------------

TournamentRegistry::ElectResult
TournamentRegistry::ElectNextSchedule(std::uint16_t updated_tour_id)
{
    std::lock_guard g(m_lock);
    ElectResult res{};

    const StepMap* elected_steps = nullptr;
    std::uint16_t  elected_id    = 0;
    for (auto& [id, si] : schedules_)
    {
        if (si.steps.empty())
            continue;
        if (id == updated_tour_id)
            si.enable = true;
        if (!elected_steps ||
            si.steps.begin()->second.start <
                elected_steps->begin()->second.start)
        {
            elected_steps = &si.steps;
            elected_id    = id;
        }
    }

    if (!elected_steps)
    {
        active_id_  = 0;
        res.changed = true;
        res.id      = 0;
        return res;
    }

    if (updated_tour_id == elected_id || elected_id != current_.id)
    {
        active_id_  = elected_id;
        res.changed = true;
        res.id      = elected_id;
        for (const auto& [key, sc] : *elected_steps)
            res.steps.push_back(sc);
    }
    return res;
}

// ---- scheduler tick (TWorldSvr.cpp:4012) --------------------------

TournamentRegistry::DueAction
TournamentRegistry::PopDueTick(std::int64_t now)
{
    std::lock_guard g(m_lock);
    DueAction none{};

    if (!active_id_)
        return none;
    auto it = schedules_.find(active_id_);
    if (it == schedules_.end() || it->second.steps.empty())
        return none;

    StepMap& steps = it->second.steps;
    const std::uint8_t last_group = steps.rbegin()->second.group;

    for (auto& [key, sc] : steps)
    {
        if (!sc.period)
            continue;
        if (sc.start > now)
            break;
        if (sc.start && sc.start <= now)
        {
            sc.start = 0;
            DueAction a{};
            a.kind   = DueAction::Kind::kStep;
            a.id     = active_id_;
            a.group  = sc.group;
            a.step   = sc.step;
            a.period = sc.period;
            return a;
        }
        if (sc.step == tournament::kStepEnd && sc.end &&
            sc.end <= now && last_group == sc.group)
        {
            sc.end = 0;
            DueAction a{};
            a.kind = DueAction::Kind::kReschedule;
            a.id   = active_id_;
            return a;
        }
    }
    return none;
}

// ---- OnSM_TOURNAMENT_REQ state leg (SSHandler.cpp:11406) ----------

std::optional<TournamentRegistry::AdvanceResult>
TournamentRegistry::AdvanceStep(std::uint16_t id, std::uint8_t group,
                                std::uint8_t step)
{
    std::lock_guard g(m_lock);

    auto it = catalogue_.find(id);
    if (it == catalogue_.end() || it->second.empty())
        return std::nullopt;

    if (current_.group != group)
    {
        current_.group = group;
        current_.sum   = 0;
        const std::uint8_t ec = CurrentEntryCountLocked();
        current_.base =
            ec ? static_cast<std::uint8_t>(
                     tournament::kTournamentBasePrize / ec)
               : 0;
    }
    current_.step = step;

    AdvanceResult res{};
    auto it_next = current_.steps.find(
        StepKey(static_cast<std::uint8_t>(step + 1), group));
    if (it_next != current_.steps.end())
        res.next_step_start = it_next->second.start;
    return res;
}

std::optional<TournamentBattleTime>
TournamentRegistry::TimeFor(std::uint16_t id) const
{
    std::lock_guard g(m_lock);
    auto it = times_.find(id);
    if (it == times_.end())
        return std::nullopt;
    return it->second;
}

TournamentRegistry::StepMap
TournamentRegistry::StepsFor(std::uint16_t id) const
{
    std::lock_guard g(m_lock);
    auto it = schedules_.find(id);
    return it == schedules_.end() ? StepMap{} : it->second.steps;
}

// ---- current-tournament rebuild (SSHandler.cpp:11468) -----------

void TournamentRegistry::ClearCurrent()
{
    std::lock_guard g(m_lock);
    const std::uint16_t old_id = current_.id;

    current_ = Status{};
    players_.clear();

    auto it = catalogue_.find(old_id);
    if (it == catalogue_.end())
        return;
    for (auto& [entry_id, entry] : it->second)
    {
        entry.first.clear();
        entry.normal.clear();
        entry.player.clear();
    }
}

bool TournamentRegistry::RebuildCurrent(
    std::uint16_t id, const std::vector<TournamentStep>& steps)
{
    std::lock_guard g(m_lock);

    // TournamentClear (inline — same lock).
    {
        const std::uint16_t old_id = current_.id;
        current_ = Status{};
        players_.clear();
        auto it = catalogue_.find(old_id);
        if (it != catalogue_.end())
            for (auto& [entry_id, entry] : it->second)
            {
                entry.first.clear();
                entry.normal.clear();
                entry.player.clear();
            }
    }

    auto it = catalogue_.find(id);
    if (id == 0 || it == catalogue_.end())
        return false;

    current_.id    = id;
    current_.group = 0;
    for (const auto& s : steps)
        current_.steps.emplace(StepKey(s.step, s.group), s);

    const std::uint8_t ec = CurrentEntryCountLocked();
    current_.base =
        ec ? static_cast<std::uint8_t>(
                 tournament::kTournamentBasePrize / ec)
           : 0;
    return true;
}

// ---- catalogue ---------------------------------------------------

std::vector<std::uint32_t> TournamentRegistry::ReplaceEntries(
    std::uint16_t tid, std::vector<TournamentEntrySeed> entries)
{
    std::lock_guard g(m_lock);
    std::vector<std::uint32_t> removed;

    auto it = catalogue_.find(tid);
    if (it != catalogue_.end())
    {
        while (!it->second.empty())
        {
            auto ids = DeleteEntryLocked(
                tid, it->second.begin()->first);
            removed.insert(removed.end(), ids.begin(), ids.end());
        }
        catalogue_.erase(it);
    }

    EntryMap em;
    for (auto& seed : entries)
    {
        Entry e;
        e.seed = std::move(seed);
        const std::uint8_t eid = e.seed.entry_id;
        em.emplace(eid, std::move(e));
    }
    catalogue_.emplace(tid, std::move(em));
    return removed;
}

std::vector<std::uint32_t>
TournamentRegistry::DeleteEntry(std::uint16_t tid,
                                std::uint8_t entry_id)
{
    std::lock_guard g(m_lock);
    return DeleteEntryLocked(tid, entry_id);
}

std::vector<std::uint32_t>
TournamentRegistry::DeleteEntryLocked(std::uint16_t tid,
                                      std::uint8_t entry_id)
{
    std::vector<std::uint32_t> removed;

    auto it_t = catalogue_.find(tid);
    if (it_t == catalogue_.end())
        return removed;
    auto it_e = it_t->second.find(entry_id);
    if (it_e == it_t->second.end())
        return removed;

    if (tid == current_.id)
    {
        std::vector<std::shared_ptr<TnmtPlayer>> victims;
        for (const auto& [cid, p] : players_)
            if (p->entry_id == entry_id)
                victims.push_back(p);
        for (const auto& p : victims)
        {
            removed.push_back(p->char_id);
            RemovePlayerLocked(it_e->second, p);
        }
    }

    it_t->second.erase(it_e);
    return removed;
}

bool TournamentRegistry::DropTournament(std::uint16_t tid)
{
    std::lock_guard g(m_lock);
    return catalogue_.erase(tid) > 0;
}

bool TournamentRegistry::HasTournament(std::uint16_t tid) const
{
    std::lock_guard g(m_lock);
    return catalogue_.find(tid) != catalogue_.end();
}

void TournamentRegistry::SeedEntries(
    std::uint16_t tid, std::vector<TournamentEntrySeed> entries)
{
    std::lock_guard g(m_lock);
    EntryMap em;
    for (auto& seed : entries)
    {
        Entry e;
        e.seed = std::move(seed);
        const std::uint8_t eid = e.seed.entry_id;
        em.emplace(eid, std::move(e));
    }
    catalogue_[tid] = std::move(em);
}

std::uint8_t TournamentRegistry::RecomputeCurrentBase()
{
    std::lock_guard g(m_lock);
    const std::uint8_t ec = CurrentEntryCountLocked();
    current_.base =
        ec ? static_cast<std::uint8_t>(
                 tournament::kTournamentBasePrize / ec)
           : 0;
    return ec;
}

// ---- players (operator path) -------------------------------------

TournamentRegistry::Entry*
TournamentRegistry::CurrentEntryLocked(std::uint8_t entry_id)
{
    auto it_t = catalogue_.find(current_.id);
    if (current_.id == 0 || it_t == catalogue_.end())
        return nullptr;
    auto it_e = it_t->second.find(entry_id);
    return it_e == it_t->second.end() ? nullptr : &it_e->second;
}

const TournamentRegistry::Entry*
TournamentRegistry::CurrentEntryLocked(std::uint8_t entry_id) const
{
    return const_cast<TournamentRegistry*>(this)
        ->CurrentEntryLocked(entry_id);
}

void TournamentRegistry::RemovePlayerLocked(
    Entry& entry, const std::shared_ptr<TnmtPlayer>& p)
{
    // DelTNMTPlayer (TWorldSvr.cpp:5933).
    entry.first.erase(p->char_id);
    entry.normal.erase(p->char_id);
    entry.player.erase(p->char_id);

    if (entry.seed.type == tournament::kEntryParty)
    {
        auto it_chief = players_.find(p->chief_id);
        if (it_chief != players_.end())
            it_chief->second->party.erase(p->char_id);
    }

    players_.erase(p->char_id);
}

std::uint8_t TournamentRegistry::CurrentEntryCountLocked() const
{
    auto it = catalogue_.find(current_.id);
    if (current_.id == 0 || it == catalogue_.end())
        return 0;
    std::uint8_t count = 0;
    for (const auto& [eid, entry] : it->second)
        if (entry.seed.group == current_.group)
            ++count;
    return count;
}

bool TournamentRegistry::CurrentEntryExists(std::uint8_t entry_id) const
{
    std::lock_guard g(m_lock);
    return CurrentEntryLocked(entry_id) != nullptr;
}

bool TournamentRegistry::HasPlayer(std::uint32_t char_id) const
{
    std::lock_guard g(m_lock);
    return players_.find(char_id) != players_.end();
}

bool TournamentRegistry::AddPlayer1st(std::uint8_t entry_id,
                                      const TnmtPlayerBrief& info,
                                      const std::string& guild_name)
{
    TnmtPlayerSeed seed{};
    seed.brief      = info;
    seed.guild_name = guild_name;
    return AddPlayerAtStep(entry_id, seed, tournament::kStep1st,
        info.char_id);
}

bool TournamentRegistry::AddPlayerAtStep(std::uint8_t entry_id,
                                         const TnmtPlayerSeed& seed,
                                         std::uint8_t step,
                                         std::uint32_t chief_id)
{
    std::lock_guard g(m_lock);
    return AddPlayerAtStepLocked(entry_id, seed, step, chief_id);
}

bool TournamentRegistry::AddPlayerAtStepLocked(
    std::uint8_t entry_id, const TnmtPlayerSeed& seed,
    std::uint8_t step, std::uint32_t chief_id)
{
    Entry* entry = CurrentEntryLocked(entry_id);
    if (!entry ||
        players_.find(seed.brief.char_id) != players_.end())
        return false;

    auto p = std::make_shared<TnmtPlayer>();
    p->char_id    = seed.brief.char_id;
    p->country    = seed.brief.country;
    p->name       = seed.brief.name;
    p->level      = seed.brief.level;
    p->cls        = seed.brief.cls;
    p->guild_name = seed.guild_name;
    p->hwid       = seed.hwid;
    p->ip_addr    = seed.ip_addr;
    for (std::size_t i = 0; i < tournament::kMatchCount; ++i)
        p->result[i] = seed.result[i];
    // rank / month_rank stay 0 — the legacy GetRanking maps
    // (m_mapRank / m_mapMonthRank) are never populated by the
    // shipped binary, so every roster carries zeros on the wire.

    // AddTNMTPlayer (TWorldSvr.cpp:5913).
    std::shared_ptr<TnmtPlayer> chief;
    if (chief_id == p->char_id)
        chief = p;
    else
    {
        auto it_chief = players_.find(chief_id);
        if (it_chief == players_.end())
            return false;
        chief = it_chief->second;
    }
    p->entry_id = entry->seed.entry_id;
    p->chief_id = chief->char_id;
    p->slot_id  = chief->slot_id;

    if (step == tournament::kStep1st)
        entry->first.emplace(p->char_id, p);
    else if (step == tournament::kStepNormal)
        entry->normal.emplace(p->char_id, p);
    else if (step == tournament::kStepParty)
        chief->party.emplace(p->char_id, p);
    else if (step == tournament::kStepMatch)
        entry->player.emplace(p->char_id, p);
    else
        return false;

    players_.emplace(p->char_id, p);
    return true;
}

// ---- W6-52 player vertical -----------------------------------------

bool TournamentRegistry::CanDoTournamentLocked(std::uint8_t step,
                                               std::uint8_t group) const
{
    if (catalogue_.find(current_.id) == catalogue_.end())
        return false;
    return current_.step == step &&
           (!group || current_.group == group);
}

bool TournamentRegistry::CanDoTournament(std::uint8_t step,
                                         std::uint8_t group) const
{
    std::lock_guard g(m_lock);
    return CanDoTournamentLocked(step, group);
}

std::uint8_t TournamentRegistry::FirstGroupCount() const
{
    std::lock_guard g(m_lock);
    return first_group_count_;
}

std::uint8_t
TournamentRegistry::EntryOfLocked(std::uint32_t char_id) const
{
    auto it = players_.find(char_id);
    return it == players_.end() ? 0 : it->second->entry_id;
}

TournamentRegistry::PlayerRosterRow
TournamentRegistry::RosterOf(const TnmtPlayer& p)
{
    PlayerRosterRow row;
    row.char_id    = p.char_id;
    row.country    = p.country;
    row.name       = p.name;
    row.level      = p.level;
    row.cls        = p.cls;
    row.rank       = p.rank;
    row.month_rank = p.month_rank;
    return row;
}

TournamentRegistry::ApplyOutcome
TournamentRegistry::TryApply(const TnmtPlayerBrief& info,
                             const std::string& guild_name,
                             std::uint8_t entry_id,
                             const std::string& hwid,
                             std::uint32_t ip_addr,
                             bool in_first_grade)
{
    std::lock_guard g(m_lock);
    ApplyOutcome out{};

    // TournamentApply (TWorldSvr.cpp:6290) in legacy order.
    if (info.country > tournament::kCountryB)
    {
        out.result = tournament::kResultFail;
        return out;
    }
    Entry* entry = CurrentEntryLocked(entry_id);
    if (!entry)
    {
        out.result = tournament::kResultFail;
        return out;
    }

    std::uint8_t result = tournament::kResultDisqualify;
    std::uint8_t step   = tournament::kStepNormal;
    if (CanDoTournamentLocked(tournament::kStep1st, 0))
    {
        if (in_first_grade)
        {
            result = tournament::kResultSuccess;
            step   = tournament::kStep1st;
        }
    }
    else if (!CanDoTournamentLocked(tournament::kStepNormal, 0))
        result = tournament::kResultTimeout;
    else
        result = tournament::kResultSuccess;

    if (result != tournament::kResultSuccess)
    {
        out.result = result;
        return out;
    }

    if (players_.find(info.char_id) != players_.end())
    {
        // Legacy quirk: a registered char echoes the short-form
        // SUCCESS without re-adding.
        out.result  = tournament::kResultSuccess;
        out.already = true;
        return out;
    }

    // FindTNMTPlayerApply (TWorldSvr.cpp:5959) — HWID/IP dup scan.
    for (const auto& [cid, p] : players_)
        if (p->hwid == hwid || p->ip_addr == ip_addr)
        {
            out.result = tournament::kResultFail;
            return out;
        }

    if (entry->first.size() >= tournament::kTournamentSlot)
    {
        out.result = tournament::kResultFull;
        return out;
    }

    TnmtPlayerSeed seed{};
    seed.brief      = info;
    seed.guild_name = guild_name;
    seed.hwid       = hwid;
    seed.ip_addr    = ip_addr;
    if (!AddPlayerAtStepLocked(entry_id, seed, step, info.char_id))
    {
        out.result = tournament::kResultFail;   // unreachable guard
        return out;
    }
    out.result = tournament::kResultSuccess;
    out.added  = true;
    return out;
}

TournamentRegistry::PartyAddOutcome
TournamentRegistry::TryPartyAdd(std::uint32_t chief_char_id,
                                std::uint8_t chief_country,
                                const TnmtPlayerBrief& target,
                                const std::string& target_guild)
{
    std::lock_guard g(m_lock);
    PartyAddOutcome out{};

    // TournamentPartyAdd (TWorldSvr.cpp:6537) in legacy order.
    if (!CanDoTournamentLocked(tournament::kStepParty, 0))
        return out;
    auto it_t = catalogue_.find(current_.id);
    if (current_.id == 0 || it_t == catalogue_.end())
        return out;

    Entry* entry = nullptr;
    for (auto& [eid, e] : it_t->second)
        if (e.seed.type == tournament::kEntryParty)
        {
            entry = &e;
            break;
        }
    if (!entry)
        return out;

    auto it_chief_probe = players_.find(chief_char_id);

    out.silent = false;
    if (!target.char_id || chief_country != target.country)
    {
        out.result = tournament::kResultNotFound;
        return out;
    }
    if (players_.find(target.char_id) != players_.end())
    {
        out.result = tournament::kResultAlreadyReg;
        return out;
    }
    if (entry->seed.max_level < target.level ||
        entry->seed.min_level > target.level)
    {
        out.result = tournament::kResultLevel;
        return out;
    }
    if (it_chief_probe == players_.end())
    {
        out.silent = true;                       // legacy silent
        return out;
    }
    if (it_chief_probe->second->party.size() >= 6)
    {
        out.result = tournament::kResultFull;
        return out;
    }

    TnmtPlayerSeed seed{};
    seed.brief      = target;
    seed.guild_name = target_guild;
    if (!AddPlayerAtStepLocked(entry->seed.entry_id, seed,
            tournament::kStepParty, chief_char_id))
    {
        out.silent = true;
        return out;
    }
    out.result   = tournament::kResultSuccess;
    out.added    = true;
    out.entry_id = entry->seed.entry_id;
    out.chief_id = chief_char_id;
    return out;
}

TournamentRegistry::PartyDelOutcome
TournamentRegistry::PartyDel(std::uint32_t requester_char_id,
                             std::uint32_t target_char_id)
{
    std::lock_guard g(m_lock);
    PartyDelOutcome out{};

    // TournamentPartyDel (TWorldSvr.cpp:6634).
    if (!CanDoTournamentLocked(tournament::kStepParty, 0))
        return out;
    auto it = players_.find(target_char_id);
    if (it == players_.end())
        return out;
    const auto player = it->second;
    if (player->chief_id == target_char_id ||
        (player->chief_id != requester_char_id &&
         target_char_id != requester_char_id))
        return out;

    out.chief_id = player->chief_id;
    Entry* entry = CurrentEntryLocked(player->entry_id);
    if (entry)
        RemovePlayerLocked(*entry, player);
    else
    {
        // Legacy DelTNMTPlayer(NULL, …) would crash; the pools are
        // consistent here so the entry always resolves — belt and
        // braces: still drop the global row.
        players_.erase(target_char_id);
    }
    out.removed = true;
    return out;
}

TournamentRegistry::ScheduleSnapshot
TournamentRegistry::ScheduleReply() const
{
    std::lock_guard g(m_lock);
    ScheduleSnapshot snap{};
    if (current_.steps.empty())
        return snap;
    snap.ok    = true;
    snap.group = current_.group;
    snap.step  = current_.step;
    for (const auto& [key, sc] : current_.steps)
        snap.steps.push_back(sc);
    return snap;
}

TournamentRegistry::ApplyInfoSnapshot
TournamentRegistry::ApplyInfoReply(std::uint32_t char_id) const
{
    std::lock_guard g(m_lock);
    ApplyInfoSnapshot snap{};
    snap.max_level = max_level_;

    auto it_t = catalogue_.find(current_.id);
    if (current_.id == 0 || it_t == catalogue_.end())
        return snap;
    if (current_.step > tournament::kStepNormal)
        return snap;
    snap.ok = true;

    const std::uint8_t my_entry = EntryOfLocked(char_id);
    for (const auto& [eid, entry] : it_t->second)
    {
        ApplyInfoEntry row;
        row.seed    = entry.seed;
        row.applied = entry.seed.entry_id == my_entry;
        row.free_first = static_cast<std::uint8_t>(
            tournament::kTournamentSlot - entry.first.size());
        row.normal_count =
            static_cast<std::uint16_t>(entry.normal.size());
        for (const auto& [cid, p] : entry.first)
            row.first_pool.push_back(RosterOf(*p));
        snap.entries.push_back(std::move(row));
    }
    return snap;
}

TournamentRegistry::JoinListSnapshot
TournamentRegistry::JoinListReply(std::uint32_t char_id) const
{
    std::lock_guard g(m_lock);
    JoinListSnapshot snap{};

    auto it_t = catalogue_.find(current_.id);
    if (current_.id == 0 || it_t == catalogue_.end())
        return snap;
    if (!CanDoTournamentLocked(tournament::kStepParty, 0))
        return snap;
    snap.ok = true;

    const std::uint8_t my_entry = EntryOfLocked(char_id);
    for (const auto& [eid, entry] : it_t->second)
    {
        JoinListEntry row;
        row.seed    = entry.seed;
        row.applied = entry.seed.entry_id == my_entry;
        for (const auto& [cid, p] : entry.player)
            row.players.push_back(RosterOf(*p));
        snap.entries.push_back(std::move(row));
    }
    return snap;
}

TournamentRegistry::PartyListSnapshot
TournamentRegistry::PartyListReply(std::uint32_t chief_id) const
{
    std::lock_guard g(m_lock);
    PartyListSnapshot snap{};
    auto it = players_.find(chief_id);
    if (it == players_.end())
        return snap;
    snap.ok       = true;
    snap.chief_id = chief_id;
    for (const auto& [cid, p] : it->second->party)
        snap.members.push_back(RosterOf(*p));
    return snap;
}

TournamentRegistry::MatchListSnapshot
TournamentRegistry::MatchListReply(std::uint32_t char_id) const
{
    std::lock_guard g(m_lock);
    MatchListSnapshot snap{};

    auto it_t = catalogue_.find(current_.id);
    if (current_.id == 0 || it_t == catalogue_.end())
        return snap;
    snap.ok = true;

    const std::uint8_t my_entry = EntryOfLocked(char_id);
    for (const auto& [eid, entry] : it_t->second)
    {
        MatchListEntry row;
        row.seed    = entry.seed;
        row.applied = entry.seed.entry_id == my_entry;
        for (const auto& [cid, p] : entry.player)
        {
            MatchRosterRow m;
            m.slot_id = p->slot_id;
            m.row     = RosterOf(*p);
            for (std::size_t i = 0; i < tournament::kMatchCount; ++i)
                m.result[i] = p->result[i];
            row.players.push_back(std::move(m));
        }
        snap.entries.push_back(std::move(row));
    }
    return snap;
}

bool TournamentRegistry::CurrentHasFirstStep() const
{
    std::lock_guard g(m_lock);
    auto it = current_.steps.find(
        StepKey(tournament::kStep1st, 0));
    return it != current_.steps.end() && it->second.period != 0;
}

std::optional<std::uint32_t> TournamentRegistry::RemovePlayerByName(
    std::uint16_t tid, std::uint8_t entry_id, const std::string& name)
{
    std::lock_guard g(m_lock);
    if (tid != current_.id)
        return std::nullopt;
    Entry* entry = CurrentEntryLocked(entry_id);
    if (!entry)
        return std::nullopt;

    for (const auto& [cid, p] : players_)
    {
        if (p->name != name)
            continue;
        const std::uint32_t char_id = cid;
        const auto victim = p;             // keep alive past erase
        RemovePlayerLocked(*entry, victim);
        return char_id;
    }
    return std::nullopt;
}

std::size_t TournamentRegistry::PlayerCount() const
{
    std::lock_guard g(m_lock);
    return players_.size();
}

// ---- TET_LIST snapshots ------------------------------------------

std::vector<TournamentRegistry::ScheduleListRow>
TournamentRegistry::ScheduleList() const
{
    std::lock_guard g(m_lock);
    std::vector<ScheduleListRow> out;
    for (const auto& [id, bt] : times_)
    {
        ScheduleListRow row;
        row.id   = id;
        row.time = bt;
        auto it = schedules_.find(id);
        if (it != schedules_.end())
            for (const auto& [key, sc] : it->second.steps)
                row.steps.push_back(sc);
        out.push_back(std::move(row));
    }
    return out;
}

std::vector<TournamentRegistry::TournamentListRow>
TournamentRegistry::CatalogueList() const
{
    std::lock_guard g(m_lock);
    std::vector<TournamentListRow> out;
    for (const auto& [tid, entries] : catalogue_)
    {
        TournamentListRow row;
        row.id = tid;
        for (const auto& [eid, entry] : entries)
        {
            EntryListRow er;
            er.seed = entry.seed;
            if (current_.step < tournament::kStepParty)
            {
                for (const auto& [cid, p] : entry.first)
                    er.players.push_back(TnmtPlayerBrief{
                        p->char_id, p->country, p->name, p->level,
                        p->cls});
            }
            else
            {
                for (const auto& [cid, p] : entry.player)
                    if (p->char_id == p->chief_id)
                        er.players.push_back(TnmtPlayerBrief{
                            p->char_id, p->country, p->name,
                            p->level, p->cls});
            }
            row.entries.push_back(std::move(er));
        }
        out.push_back(std::move(row));
    }
    return out;
}

TournamentRegistry::InfoSnapshot TournamentRegistry::Info() const
{
    std::lock_guard g(m_lock);
    InfoSnapshot snap;
    snap.first_group_count = first_group_count_;
    snap.group             = current_.group;
    snap.step              = current_.step;
    snap.max_level         = max_level_;
    auto it = catalogue_.find(current_.id);
    if (current_.id != 0 && it != catalogue_.end())
        for (const auto& [eid, entry] : it->second)
            snap.entries.push_back(entry.seed);
    return snap;
}

// ---- status scalars ----------------------------------------------

std::uint16_t TournamentRegistry::CurrentId() const
{
    std::lock_guard g(m_lock);
    return current_.id;
}

std::uint8_t TournamentRegistry::CurrentStep() const
{
    std::lock_guard g(m_lock);
    return current_.step;
}

std::uint8_t TournamentRegistry::CurrentGroup() const
{
    std::lock_guard g(m_lock);
    return current_.group;
}

std::uint16_t TournamentRegistry::ActiveScheduleId() const
{
    std::lock_guard g(m_lock);
    return active_id_;
}

std::uint16_t TournamentRegistry::NextScheduleId() const
{
    std::lock_guard g(m_lock);
    return next_id_;
}

void TournamentRegistry::SetFirstGroupCount(std::uint8_t v)
{
    std::lock_guard g(m_lock);
    first_group_count_ = v;
}

void TournamentRegistry::SetMaxLevel(std::uint8_t v)
{
    std::lock_guard g(m_lock);
    max_level_ = v;
}

} // namespace tworldsvr
