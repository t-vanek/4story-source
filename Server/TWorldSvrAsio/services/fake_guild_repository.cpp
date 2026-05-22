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

} // namespace tworldsvr
