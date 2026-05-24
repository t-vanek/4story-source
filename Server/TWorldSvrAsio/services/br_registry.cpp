#include "br_registry.h"

namespace tworldsvr {

namespace {

// Internal helper — caller holds m_lock (any mode). Mirrors
// FindPlayerInPremade's scan.
bool PlayerInAnyTeam(
    const std::unordered_map<std::uint32_t, TBrTeam>& teams,
    std::uint32_t char_id)
{
    for (const auto& [chief, team] : teams)
        if (team.members.find(char_id) != team.members.end())
            return true;
    return false;
}

} // namespace

std::uint8_t
BrRegistry::AddPlayerToQueue(std::uint32_t char_id, std::uint32_t key,
                             std::uint8_t klass, const std::string& name)
{
    std::unique_lock g(m_lock);
    if (m_solo_queue.find(char_id) != m_solo_queue.end() ||
        PlayerInAnyTeam(m_premade_teams, char_id))
        return br::kAlreadyInQueue;
    m_solo_queue.emplace(char_id,
        TBrPlayer{char_id, key, klass, name, /*ready=*/false});
    return br::kSuccess;
}

std::uint8_t
BrRegistry::ErasePlayerFromQueue(std::uint32_t char_id, std::uint32_t key)
{
    std::unique_lock g(m_lock);
    auto it = m_solo_queue.find(char_id);
    if (it == m_solo_queue.end() || it->second.key != key)
        return br::kFail;
    m_solo_queue.erase(it);
    return br::kSuccess;
}

void
BrRegistry::JoinPremadeTeam(std::uint32_t chief_id, std::uint32_t chief_key,
                            std::uint8_t chief_class, const std::string& chief_name,
                            std::uint32_t mate_id, std::uint32_t mate_key,
                            std::uint8_t mate_class, const std::string& mate_name)
{
    std::unique_lock g(m_lock);
    auto& team = m_premade_teams[chief_id];   // value-init on first touch
    if (team.chief_id == 0)
    {
        team.chief_id = chief_id;
        team.members.emplace(chief_id,
            TBrPlayer{chief_id, chief_key, chief_class, chief_name, false});
    }
    team.members.emplace(mate_id,
        TBrPlayer{mate_id, mate_key, mate_class, mate_name, false});
}

void
BrRegistry::ErasePlayerFromPremade(std::uint32_t char_id)
{
    std::unique_lock g(m_lock);
    // Chief-leave dissolves the team (legacy parity).
    auto chief_it = m_premade_teams.find(char_id);
    if (chief_it != m_premade_teams.end())
    {
        m_premade_teams.erase(chief_it);
        return;
    }
    // Otherwise look the mate up in each team's roster.
    for (auto& [chief, team] : m_premade_teams)
    {
        auto m_it = team.members.find(char_id);
        if (m_it != team.members.end())
        {
            team.members.erase(m_it);
            return;
        }
    }
}

bool
BrRegistry::FindPlayerInPremade(std::uint32_t char_id) const
{
    std::shared_lock g(m_lock);
    return PlayerInAnyTeam(m_premade_teams, char_id);
}

std::uint8_t
BrRegistry::GetPremadePlayerCountByChief(std::uint32_t chief_id) const
{
    std::shared_lock g(m_lock);
    auto it = m_premade_teams.find(chief_id);
    if (it == m_premade_teams.end()) return 0;
    return static_cast<std::uint8_t>(it->second.members.size());
}

std::uint32_t
BrRegistry::GetChiefIdByMateId(std::uint32_t char_id) const
{
    std::shared_lock g(m_lock);
    if (m_premade_teams.find(char_id) != m_premade_teams.end())
        return char_id;
    for (const auto& [chief, team] : m_premade_teams)
        if (team.members.find(char_id) != team.members.end())
            return chief;
    return 0;
}

bool
BrRegistry::FlagPlayerReady(std::uint32_t char_id, std::uint32_t key)
{
    std::unique_lock g(m_lock);
    for (auto& [chief, team] : m_premade_teams)
    {
        auto it = team.members.find(char_id);
        if (it != team.members.end() && it->second.key == key)
        {
            if (it->second.ready) return false;   // already flagged
            it->second.ready = true;
            return true;
        }
    }
    return false;
}

bool
BrRegistry::FlagTeamReady(std::uint32_t chief_id)
{
    std::unique_lock g(m_lock);
    auto it = m_premade_teams.find(chief_id);
    if (it == m_premade_teams.end()) return false;
    auto& team = it->second;
    for (const auto& [member_id, player] : team.members)
    {
        if (member_id == chief_id) continue;
        if (!player.ready) return false;
    }
    if (team.ready) return false;       // already team-ready
    team.ready = true;
    return true;
}

std::vector<TBrPlayer>
BrRegistry::SnapshotTeam(std::uint32_t chief_id, std::string& chief_name_out,
                         bool& ready_out) const
{
    std::shared_lock g(m_lock);
    chief_name_out.clear();
    ready_out = false;
    std::vector<TBrPlayer> rows;
    auto it = m_premade_teams.find(chief_id);
    if (it == m_premade_teams.end()) return rows;
    ready_out = it->second.ready;
    rows.reserve(it->second.members.size());
    for (const auto& [_, p] : it->second.members)
    {
        if (p.char_id == chief_id) chief_name_out = p.name;
        rows.push_back(p);
    }
    return rows;
}

void
BrRegistry::VoteForMap(std::uint32_t user_id, const std::string& map_name)
{
    std::unique_lock g(m_lock);
    m_map_votes.emplace(user_id, map_name);   // first vote wins
}

void
BrRegistry::VoteForMode(std::uint32_t user_id, std::uint8_t mode)
{
    std::unique_lock g(m_lock);
    m_mode_votes.emplace(user_id, mode);
}

std::size_t BrRegistry::QueueSize() const
{
    std::shared_lock g(m_lock);
    return m_solo_queue.size();
}

std::size_t BrRegistry::TeamCount() const
{
    std::shared_lock g(m_lock);
    return m_premade_teams.size();
}

std::size_t BrRegistry::MapVoteCount() const
{
    std::shared_lock g(m_lock);
    return m_map_votes.size();
}

std::size_t BrRegistry::ModeVoteCount() const
{
    std::shared_lock g(m_lock);
    return m_mode_votes.size();
}

void
BrRegistry::ReleaseSinglePlayer(std::uint32_t char_id, std::uint32_t key)
{
    std::unique_lock g(m_lock);
    // Solo queue — key-checked.
    auto solo = m_solo_queue.find(char_id);
    if (solo != m_solo_queue.end() && solo->second.key == key)
        m_solo_queue.erase(solo);
    // Premade — chief-leave dissolves the team, mate-leave drops the
    // member (legacy ErasePlayerFromPremade parity). No key check —
    // legacy doesn't enforce one for the premade scan.
    auto chief_it = m_premade_teams.find(char_id);
    if (chief_it != m_premade_teams.end())
    {
        m_premade_teams.erase(chief_it);
        return;
    }
    for (auto& [chief, team] : m_premade_teams)
    {
        auto m_it = team.members.find(char_id);
        if (m_it != team.members.end())
        {
            team.members.erase(m_it);
            return;
        }
    }
}

} // namespace tworldsvr
