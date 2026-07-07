#include "services/bow_registry.h"

#include <algorithm>
#include <cstdlib>
#include <utility>

namespace tworldsvr {

// ---- configuration -------------------------------------------------

void BowRegistry::Configure(std::uint16_t map_id,
                            std::uint8_t min_players,
                            std::uint8_t max_nation_difference,
                            std::uint32_t alarm_dur,
                            std::uint32_t buy_dur,
                            std::uint32_t battle_dur)
{
    std::unique_lock g(m_lock);
    m_configured            = true;
    m_map_id                = map_id;
    m_min_players           = min_players;
    m_max_nation_difference = max_nation_difference;
    m_alarm_dur             = alarm_dur;
    m_buy_dur               = buy_dur;
    m_battle_dur            = battle_dur;
    // The legacy ctor runs Release() straight away — BS_NORMAL is
    // the effective boot state (the BS_PEACE member-init is dead).
    ReleaseLocked();
}

bool BowRegistry::Configured() const
{
    std::shared_lock g(m_lock);
    return m_configured;
}

void BowRegistry::AddStartTime(std::uint32_t clt)
{
    std::unique_lock g(m_lock);
    m_times.push_back(clt);
}

void BowRegistry::Init(std::uint32_t clt)
{
    std::unique_lock g(m_lock);
    if (m_times.empty())
        return;
    // CTBowSystem::Init (BowSystem.cpp:50). The legacy follow-up
    // rescan (`if (m_dwStart < dwCurrentCLT && it != end)`) can
    // never fire — upper_bound already guarantees *it > clt — so it
    // is not reproduced.
    std::sort(m_times.begin(), m_times.end());
    auto it = std::upper_bound(m_times.begin(), m_times.end(), clt);
    m_start = it != m_times.end() ? *it : m_times.front();
}

void BowRegistry::SetNextTimeLocked()
{
    if (m_times.empty())
        return;
    // CTBowSystem::SetNextTime (BowSystem.cpp:584).
    auto it = std::upper_bound(m_times.begin(), m_times.end(),
        m_start);
    m_start = it != m_times.end() ? *it : m_times.front();
}

// ---- queue ops -----------------------------------------------------

BowRegistry::AddOutcome
BowRegistry::AddPlayer(std::uint32_t char_id, std::uint32_t key,
                       std::uint8_t country, std::uint32_t guild_id,
                       bool admin)
{
    std::unique_lock g(m_lock);
    AddOutcome out{};
    if (!m_configured)
        return out;                     // legacy: no module instance

    // AddPlayerToQueue (BowSystem.cpp:82).
    if (country > bow::kCountryC)
    {
        out.result = bow::kCountry;
        return out;
    }

    if (m_status == bow::kBsPeace || admin)
    {
        // Late-join into a RUNNING match only.
        if (m_players.empty() ||
            m_players.find(char_id) != m_players.end())
        {
            out.result = bow::kFail;
            return out;
        }
        // (The legacy dequeue call here is a no-op — the BS_ALARM
        // gate inside DeletePlayerFromQueue rejects it at BS_PEACE.)

        TBowPlayer p{};
        p.char_id = char_id;
        p.key     = key;

        std::uint8_t count[2] = {0, 0};
        for (const auto& [cid, pl] : m_players)
            if (pl.country <= bow::kCountryC)
                ++count[pl.country];
        p.country = count[bow::kCountryC] < count[bow::kCountryD]
                        ? bow::kCountryC
                        : bow::kCountryD;
        m_players.emplace(char_id, p);

        out.result    = bow::kLateJoin;   // BOWREG_FAIL + 1
        out.late_join = true;
        out.player    = p;
        return out;
    }

    if (m_status != bow::kBsAlarm)
    {
        out.result = bow::kFail;
        return out;
    }

    if (guild_id)
    {
        if (m_reg.find(char_id) != m_reg.end())
        {
            out.result = bow::kFail;
            return out;
        }
        auto it = m_guilds.find(guild_id);
        if (it == m_guilds.end())
        {
            m_guilds[guild_id].push_back(
                TBowEntry{char_id, key, country, guild_id});
            out.result = bow::kSuccess;
            return out;
        }
        for (const auto& e : it->second)
            if (e.char_id == char_id)
            {
                out.result = bow::kFail;
                return out;
            }
        it->second.push_back(
            TBowEntry{char_id, key, country, guild_id});
        out.result = bow::kSuccess;
        return out;
    }

    if (m_reg.find(char_id) == m_reg.end())
    {
        m_reg.emplace(char_id,
            TBowEntry{char_id, key, country, guild_id});
        out.result = bow::kSuccess;
        return out;
    }
    out.result = bow::kAlreadyInQueue;
    return out;
}

std::uint8_t BowRegistry::RemovePlayer(std::uint32_t char_id,
                                       std::uint32_t key)
{
    std::unique_lock g(m_lock);
    // DeletePlayerFromQueue (BowSystem.cpp:183).
    if (m_status != bow::kBsAlarm)
        return bow::kFail;

    for (auto& [gid, members] : m_guilds)
        for (std::size_t i = 0; i < members.size(); ++i)
            if (members[i].char_id == char_id &&
                members[i].key == key)
            {
                members.erase(members.begin() +
                    static_cast<std::ptrdiff_t>(i));
                return bow::kSuccess;
            }

    auto it = m_reg.find(char_id);
    if (it != m_reg.end() && it->second.key == key)
    {
        m_reg.erase(it);
        return bow::kSuccess;
    }
    return bow::kFail;
}

// ---- match creation ------------------------------------------------

std::uint8_t BowRegistry::CreateMatchLocked()
{
    // CTBowSystem::CreateMatch (BowSystem.cpp:213). The routine only
    // ever returns BOWMATCH_SUCCESS (the min-players config is
    // loaded but never consulted — legacy parity).
    m_winner = bow::kCountryNeutral;

    using Team = std::map<std::uint32_t, TBowEntry>;
    Team first, second;

    // Guild blocks land whole, routed by the FRONT member's country.
    for (const auto& [gid, members] : m_guilds)
    {
        if (members.empty())
            continue;
        const std::uint8_t front_country = members.front().country;
        for (const auto& e : members)
        {
            if (front_country == bow::kCountryD)
                first.emplace(e.char_id, e);
            else
                second.emplace(e.char_id, e);
        }
    }

    // Whole-guild rebalance when the solo pool can't bridge the gap
    // (BowSystem.cpp:245). The legacy compares an uninitialized
    // iterator when no candidate guild exists — the sane rendering
    // of that UB is "no move". Its member-move loop also increments
    // an erase-invalidated map iterator; each member exists once, so
    // find-and-move is the equivalent well-defined walk.
    const int gap = std::abs(static_cast<int>(first.size()) -
                             static_cast<int>(second.size()));
    if (gap > static_cast<int>(m_reg.size()) +
                  static_cast<int>(m_max_nation_difference))
    {
        Team* smaller = first.size() < second.size() ? &first
                                                     : &second;
        Team* bigger  = smaller == &first ? &second : &first;

        int  best_diff  = 255;
        bool have_guild = false;
        const std::vector<TBowEntry>* move_guild = nullptr;

        for (const auto& [gid, members] : m_guilds)
        {
            if (smaller->empty())
                break;
            if (members.empty())
                continue;
            if (members.front().country ==
                smaller->begin()->second.country)
                continue;

            const int diff = std::abs(
                (static_cast<int>(smaller->size()) +
                 static_cast<int>(members.size())) -
                (static_cast<int>(bigger->size()) -
                 static_cast<int>(members.size())));
            if (diff < best_diff)
            {
                best_diff  = diff;
                move_guild = &members;
                have_guild = true;
            }
        }

        if (have_guild)
            for (const auto& e : *move_guild)
            {
                auto it = bigger->find(e.char_id);
                if (it != bigger->end())
                {
                    smaller->emplace(it->first, it->second);
                    bigger->erase(it);
                }
            }
    }

    // Solo pool fills the smaller team, one by one.
    for (const auto& [cid, e] : m_reg)
    {
        if (first.size() < second.size())
            first.emplace(cid, e);
        else
            second.emplace(cid, e);
    }

    // Residual half-move (BowSystem.cpp:345). The legacy loop walks
    // an erase-invalidated iterator past the second move; the sane
    // rendering moves exactly diff/2 entries (none when diff == 1).
    const int diff = std::abs(static_cast<int>(first.size()) -
                              static_cast<int>(second.size()));
    if (diff > static_cast<int>(m_max_nation_difference))
    {
        Team* from = first.size() > second.size() ? &first : &second;
        Team* to   = from == &first ? &second : &first;
        int moved = 0;
        for (auto it = from->begin();
             it != from->end() && moved < diff / 2; )
        {
            to->emplace(it->first, it->second);
            it = from->erase(it);
            ++moved;
        }
    }

    // Team country stamp + roster adoption (BowSystem.cpp:365).
    for (const auto& [cid, e] : first)
        m_players.emplace(cid,
            TBowPlayer{e.char_id, e.key, bow::kCountryD});
    for (const auto& [cid, e] : second)
        m_players.emplace(cid,
            TBowPlayer{e.char_id, e.key, bow::kCountryC});

    for (std::size_t i = 0; i < bow::kTeamCount; ++i)
        m_points[i] = bow::kMaxPoints / bow::kTeamCount;

    return bow::kMatchSuccess;
}

// ---- the status machine --------------------------------------------

void BowRegistry::SetStatusLocked(std::uint8_t status,
                                  std::uint32_t second,
                                  std::uint64_t now_ms,
                                  TickActions& out)
{
    // CTBowSystem::SetStatus (BowSystem.cpp:413). The caller already
    // guarantees the BOW server connection; the !m_running gate (the
    // first-tick silent arm) lives here.
    if (!m_running)
        return;

    if (status == bow::kBsBodPeace && m_status != bow::kBsBodPeace)
        m_peace_tick_ms = now_ms;

    if (m_peace_tick_ms)
    {
        if (!m_sent_end)
        {
            m_winner = bow::kWinnerTie;
            if (m_points[bow::kCountryD] > m_points[bow::kCountryC])
                m_winner = bow::kWinnerDefugel;
            else if (m_points[bow::kCountryC] >
                     m_points[bow::kCountryD])
                m_winner = bow::kWinnerCraxion;

            out.commands.push_back(bow::kCmdEnd);
            m_sent_end = true;
        }

        status = m_status = bow::kBsBodPeace;

        const std::uint64_t left = now_ms - m_peace_tick_ms;
        // The legacy 12 - left/1000 wraps past the deadline — the
        // DWORD wrap ships for at most one frame (quirk kept).
        second = 12u - static_cast<std::uint32_t>(left / 1000);
        if (left >= bow::kBodPeaceMs)
        {
            // EndBattle (BowSystem.cpp:574): teleport-out + reset.
            out.ended  = true;
            out.winner = m_winner;
            for (const auto& [cid, p] : m_players)
                out.end_roster.push_back(p);
            SetNextTimeLocked();
            ReleaseLocked();
            m_sent_end = false;
        }

        out.notifies.push_back(TickActions::Notify{status, second,
            m_points[bow::kCountryD], m_points[bow::kCountryC]});
        return;
    }

    out.notifies.push_back(TickActions::Notify{status, second,
        m_points[bow::kCountryD], m_points[bow::kCountryC]});

    switch (status)
    {
    case bow::kBsBattle:
        if (m_status != status)
        {
            out.commands.push_back(bow::kCmdStart);
            out.notify_nonqueued = true;
        }
        break;
    case bow::kBsPeace:
    {
        if (!m_players.empty() || status == m_status)
            return;                      // legacy early return

        const std::uint8_t result = CreateMatchLocked();
        m_reg.clear();
        m_guilds.clear();

        if (result != bow::kMatchSuccess)
        {
            // Unreachable in practice (CreateMatch always succeeds);
            // the legacy branch shape is kept.
            SetNextTimeLocked();
            ReleaseLocked();
            out.notifies.push_back(TickActions::Notify{
                bow::kBsNormal, 0, m_points[bow::kCountryD],
                m_points[bow::kCountryC]});
            return;
        }
        out.teleport_in = true;
        for (const auto& [cid, p] : m_players)
            out.roster.push_back(p);
        break;
    }
    default:
        break;
    }

    m_tick   = second;
    m_status = status;
}

BowRegistry::TickActions
BowRegistry::Tick(std::uint32_t clt, std::uint64_t now_ms,
                  bool bow_server_connected)
{
    std::unique_lock g(m_lock);
    TickActions out{};
    // TWorldSvr.cpp:4094: no module / no BOW server → idle.
    if (!m_configured || !bow_server_connected)
        return out;

    const std::uint32_t total =
        m_alarm_dur + m_buy_dur + m_battle_dur;
    if (clt > m_start && clt < m_start + total + 15)
    {
        const std::uint32_t elapsed = clt - m_start;
        if (elapsed <= m_alarm_dur)
            SetStatusLocked(bow::kBsAlarm, m_alarm_dur - elapsed,
                now_ms, out);
        else if (elapsed >= m_alarm_dur &&
                 elapsed <= m_alarm_dur + m_buy_dur)
            SetStatusLocked(bow::kBsPeace,
                m_alarm_dur + m_buy_dur - elapsed, now_ms, out);
        else if (elapsed >= m_alarm_dur + m_buy_dur &&
                 elapsed <= total)
            SetStatusLocked(bow::kBsBattle, total - elapsed, now_ms,
                out);
        else if (clt >= m_start + total)
            SetStatusLocked(bow::kBsBodPeace, total + 15 - elapsed,
                now_ms, out);

        // Legacy flips m_bRunning AFTER the SetStatus call — the
        // first in-window tick is a silent arm (quirk kept).
        m_running = true;
    }
    return out;
}

// ---- points / release ----------------------------------------------

BowRegistry::PointsOutcome
BowRegistry::UpdatePoints(std::uint8_t country, std::uint64_t now_ms)
{
    std::unique_lock g(m_lock);
    PointsOutcome out{};
    if (!m_configured)
        return out;
    out.configured = true;

    // CTBowSystem::UpdatePoints (BowSystem.cpp:521): the scorer
    // gains a point; an opponent sitting at exactly 1 wipes to 0 and
    // the BODPEACE countdown arms (the match-end trigger); otherwise
    // the opponent decays by 1, floored above zero.
    if (country == bow::kCountryD || country == bow::kCountryC)
    {
        const std::uint8_t other = country == bow::kCountryD
                                       ? bow::kCountryC
                                       : bow::kCountryD;
        ++m_points[country];
        if (m_points[other] == 1)
        {
            m_points[other] = 0;
            m_peace_tick_ms = now_ms;
        }
        else if (static_cast<int>(m_points[other]) - 1 > 0)
            --m_points[other];
    }

    out.status   = m_status;
    out.second   = m_tick;
    out.points_d = m_points[bow::kCountryD];
    out.points_c = m_points[bow::kCountryC];
    return out;
}

BowRegistry::ReleaseOutcome
BowRegistry::ReleaseSinglePlayer(std::uint32_t char_id,
                                 std::uint32_t key)
{
    std::unique_lock g(m_lock);
    ReleaseOutcome out{};
    // CTBowSystem::ReleaseSinglePlayer (BowSystem.cpp:596) folded
    // with the single-char TeleportBOWPlayer (TWorldSvr.cpp:7918):
    // roster hit + key match + a decided winner → release + drop.
    auto it = m_players.find(char_id);
    if (it == m_players.end() || it->second.key != key ||
        m_winner == bow::kCountryNeutral)
        return out;
    out.release = true;
    out.winner  = m_winner;
    m_players.erase(it);
    return out;
}

void BowRegistry::ReleaseLocked()
{
    // CTBowSystem::Release (BowSystem.cpp:603). The winner field is
    // deliberately NOT reset — post-battle single releases read it.
    for (std::size_t i = 0; i < bow::kTeamCount; ++i)
        m_points[i] = bow::kMaxPoints / bow::kTeamCount;
    m_running       = false;
    m_status        = bow::kBsNormal;
    m_tick          = 0;
    m_peace_tick_ms = 0;
    m_reg.clear();
    m_players.clear();
    m_guilds.clear();
}

// ---- accessors -----------------------------------------------------

std::uint8_t BowRegistry::Points(std::uint8_t country) const
{
    std::shared_lock g(m_lock);
    return country <= bow::kCountryC ? m_points[country] : 0;
}

std::uint8_t BowRegistry::Winner() const
{
    std::shared_lock g(m_lock);
    return m_winner;
}

std::uint32_t BowRegistry::Tick() const
{
    std::shared_lock g(m_lock);
    return m_tick;
}

std::uint8_t BowRegistry::Status() const
{
    std::shared_lock g(m_lock);
    return m_status;
}

std::uint32_t BowRegistry::NextStart() const
{
    std::shared_lock g(m_lock);
    return m_start;
}

bool BowRegistry::Running() const
{
    std::shared_lock g(m_lock);
    return m_running;
}

std::size_t BowRegistry::QueueSize() const
{
    std::shared_lock g(m_lock);
    std::size_t n = m_reg.size();
    for (const auto& [gid, members] : m_guilds)
        n += members.size();
    return n;
}

std::size_t BowRegistry::MatchSize() const
{
    std::shared_lock g(m_lock);
    return m_players.size();
}

bool BowRegistry::Contains(std::uint32_t char_id) const
{
    std::shared_lock g(m_lock);
    return m_players.find(char_id) != m_players.end();
}

bool BowRegistry::InQueue(std::uint32_t char_id) const
{
    std::shared_lock g(m_lock);
    if (m_reg.find(char_id) != m_reg.end())
        return true;
    for (const auto& [gid, members] : m_guilds)
        for (const auto& e : members)
            if (e.char_id == char_id)
                return true;
    return false;
}

} // namespace tworldsvr
