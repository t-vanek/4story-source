#include "services/fake_guild_repository.h"

namespace tworldsvr {

namespace {
// Deep copy a TGuild so the seeded entry stays immutable from the
// caller's point of view. shared_ptr to a fresh instance with the
// same field values (mutex is default-constructed, fresh).
std::shared_ptr<TGuild> Clone(const std::shared_ptr<TGuild>& src)
{
    auto dst = std::make_shared<TGuild>();
    dst->id                = src->id;
    dst->name              = src->name;
    dst->chief_char_id     = src->chief_char_id;
    dst->chief_name        = src->chief_name;
    dst->level             = src->level;
    dst->fame              = src->fame;
    dst->fame_color        = src->fame_color;
    dst->gi                = src->gi;
    dst->exp               = src->exp;
    dst->guild_points      = src->guild_points;
    dst->status            = src->status;
    dst->gold              = src->gold;
    dst->silver            = src->silver;
    dst->cooper            = src->cooper;
    dst->max_cabinet       = src->max_cabinet;
    dst->disorg            = src->disorg;
    dst->disorg_time       = src->disorg_time;
    dst->establish_time    = src->establish_time;
    dst->pvp_total_point   = src->pvp_total_point;
    dst->pvp_useable_point = src->pvp_useable_point;
    dst->country           = src->country;
    dst->members           = src->members;  // value copy
    return dst;
}
} // namespace

void FakeGuildRepository::AddGuild(std::shared_ptr<TGuild> g)
{
    if (!g) return;
    std::lock_guard lock(m_mtx);
    m_guilds[g->id] = std::move(g);
}

std::vector<std::shared_ptr<TGuild>> FakeGuildRepository::LoadAll()
{
    std::vector<std::shared_ptr<TGuild>> out;
    std::lock_guard lock(m_mtx);
    out.reserve(m_guilds.size());
    for (const auto& [_, g] : m_guilds)
        out.push_back(Clone(g));
    return out;
}

std::optional<std::shared_ptr<TGuild>>
FakeGuildRepository::FindById(std::uint32_t guild_id)
{
    std::lock_guard lock(m_mtx);
    auto it = m_guilds.find(guild_id);
    if (it == m_guilds.end()) return std::nullopt;
    return Clone(it->second);
}

bool FakeGuildRepository::SetDisorg(std::uint32_t guild_id,
                                     std::uint8_t  disorg,
                                     std::uint32_t time_unix)
{
    std::lock_guard lock(m_mtx);
    m_calls.push_back({Call::Kind::kSetDisorg, guild_id, 0, disorg,
                       time_unix, 0, 0, 0});
    auto it = m_guilds.find(guild_id);
    if (it == m_guilds.end()) return false;
    std::lock_guard g(it->second->lock);
    it->second->disorg      = disorg;
    it->second->disorg_time = time_unix;
    return true;
}

bool FakeGuildRepository::UpdateMemberDuty(std::uint32_t char_id,
                                            std::uint32_t guild_id,
                                            std::uint8_t  new_duty)
{
    std::lock_guard lock(m_mtx);
    m_calls.push_back({Call::Kind::kUpdateMemberDuty, guild_id, char_id,
                       new_duty, 0, 0, 0, 0});
    auto it = m_guilds.find(guild_id);
    if (it == m_guilds.end()) return false;
    std::lock_guard g(it->second->lock);
    if (auto* m = it->second->FindMember(char_id))
    {
        m->duty = new_duty;
        return true;
    }
    return false;
}

bool FakeGuildRepository::UpdateFame(std::uint32_t guild_id,
                                      std::uint32_t fame,
                                      std::uint32_t fame_color)
{
    std::lock_guard lock(m_mtx);
    m_calls.push_back({Call::Kind::kUpdateFame, guild_id, 0, fame,
                       fame_color, 0, 0, 0});
    auto it = m_guilds.find(guild_id);
    if (it == m_guilds.end()) return false;
    std::lock_guard g(it->second->lock);
    it->second->fame       = fame;
    it->second->fame_color = fame_color;
    return true;
}

bool FakeGuildRepository::RemoveMember(std::uint32_t char_id,
                                        std::uint32_t guild_id)
{
    std::lock_guard lock(m_mtx);
    m_calls.push_back({Call::Kind::kRemoveMember, guild_id, char_id,
                       0, 0, 0, 0, 0});
    auto it = m_guilds.find(guild_id);
    if (it == m_guilds.end()) return false;
    std::lock_guard g(it->second->lock);
    return it->second->RemoveMember(char_id);
}

bool FakeGuildRepository::AddMember(std::uint32_t char_id,
                                     std::uint32_t guild_id,
                                     std::uint8_t  level,
                                     std::uint8_t  duty)
{
    std::lock_guard lock(m_mtx);
    m_calls.push_back({Call::Kind::kAddMember, guild_id, char_id,
                       level, duty, 0, 0, 0});
    auto it = m_guilds.find(guild_id);
    if (it == m_guilds.end()) return false;
    std::lock_guard g(it->second->lock);
    // No-op if already a member (legacy CSPGuildMemberAdd has UPSERT
    // semantics via primary-key conflict; the fake mirrors that).
    if (it->second->FindMember(char_id) != nullptr) return true;
    TGuildMember m;
    m.char_id  = char_id;
    m.guild_id = guild_id;
    m.level    = level;
    m.duty     = duty;
    it->second->members.push_back(std::move(m));
    return true;
}

bool FakeGuildRepository::IncrementContribution(std::uint32_t char_id,
                                                 std::uint32_t guild_id,
                                                 std::uint32_t exp,
                                                 std::uint32_t gold,
                                                 std::uint32_t silver,
                                                 std::uint32_t cooper,
                                                 std::uint32_t pvp_point)
{
    std::lock_guard lock(m_mtx);
    m_calls.push_back({Call::Kind::kIncrementContribution, guild_id,
                       char_id, exp, gold, silver, cooper, pvp_point});
    auto it = m_guilds.find(guild_id);
    if (it == m_guilds.end()) return false;
    std::lock_guard g(it->second->lock);
    it->second->exp   += exp;
    it->second->gold  += gold;
    it->second->silver += silver;
    it->second->cooper += cooper;
    it->second->pvp_total_point   += pvp_point;
    it->second->pvp_useable_point += pvp_point;
    if (auto* m = it->second->FindMember(char_id))
        m->service += exp;   // legacy: dwService accumulates EXP
    return true;
}

std::vector<FakeGuildRepository::Call> FakeGuildRepository::Calls() const
{
    std::lock_guard lock(m_mtx);
    return m_calls;
}

} // namespace tworldsvr
