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
    std::lock_guard g(m_lock);
    Entry* entry = CurrentEntryLocked(entry_id);
    if (!entry || players_.find(info.char_id) != players_.end())
        return false;

    auto p = std::make_shared<TnmtPlayer>();
    p->char_id    = info.char_id;
    p->country    = info.country;
    p->name       = info.name;
    p->level      = info.level;
    p->cls        = info.cls;
    p->guild_name = guild_name;
    // GetRanking (rank / month_rank) is display-only in the player
    // verticals — filled by a later slice.

    // AddTNMTPlayer(entry, p, TNMTSTEP_1st, chief = self)
    // (TWorldSvr.cpp:5913).
    p->entry_id = entry->seed.entry_id;
    p->chief_id = p->char_id;
    entry->first.emplace(p->char_id, p);
    players_.emplace(p->char_id, p);
    return true;
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
