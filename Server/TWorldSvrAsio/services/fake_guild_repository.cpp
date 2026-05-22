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

bool FakeGuildRepository::UpdateMemberPeer(std::uint32_t char_id,
                                            std::uint32_t guild_id,
                                            std::uint8_t  new_peer)
{
    std::lock_guard lock(m_mtx);
    m_calls.push_back({Call::Kind::kUpdateMemberPeer, guild_id, char_id,
                       new_peer, 0, 0, 0, 0});
    auto it = m_guilds.find(guild_id);
    if (it == m_guilds.end()) return false;
    std::lock_guard g(it->second->lock);
    if (auto* m = it->second->FindMember(char_id))
    {
        m->peer = new_peer;
        return true;
    }
    return false;
}

bool FakeGuildRepository::UpdateMaxCabinet(std::uint32_t guild_id,
                                            std::uint8_t  max_cabinet)
{
    std::lock_guard lock(m_mtx);
    m_calls.push_back({Call::Kind::kUpdateMaxCabinet, guild_id, 0,
                       max_cabinet, 0, 0, 0, 0});
    auto it = m_guilds.find(guild_id);
    if (it == m_guilds.end()) return false;
    std::lock_guard g(it->second->lock);
    it->second->max_cabinet = max_cabinet;
    return true;
}

bool FakeGuildRepository::AddArticle(std::uint32_t      guild_id,
                                      std::uint32_t      article_id,
                                      std::uint8_t       duty,
                                      const std::string& writer,
                                      const std::string& title,
                                      const std::string& body,
                                      std::uint32_t      time_unix)
{
    std::lock_guard lock(m_mtx);
    m_calls.push_back({Call::Kind::kAddArticle, guild_id, article_id,
                       duty, time_unix, 0, 0, 0});
    auto it = m_guilds.find(guild_id);
    if (it == m_guilds.end()) return false;
    (void)writer; (void)title; (void)body;
    // The fake repo doesn't keep article body content — only the
    // call record. The in-memory mirror lives on TGuild.articles
    // (which the handler already updated before calling us).
    return true;
}

bool FakeGuildRepository::DelArticle(std::uint32_t guild_id,
                                      std::uint32_t article_id)
{
    std::lock_guard lock(m_mtx);
    m_calls.push_back({Call::Kind::kDelArticle, guild_id, article_id,
                       0, 0, 0, 0, 0});
    auto it = m_guilds.find(guild_id);
    return it != m_guilds.end();
}

bool FakeGuildRepository::UpdateArticle(std::uint32_t      guild_id,
                                         std::uint32_t      article_id,
                                         const std::string& title,
                                         const std::string& body)
{
    std::lock_guard lock(m_mtx);
    m_calls.push_back({Call::Kind::kUpdateArticle, guild_id, article_id,
                       0, 0, 0, 0, 0});
    auto it = m_guilds.find(guild_id);
    if (it == m_guilds.end()) return false;
    (void)title; (void)body;
    return true;
}

bool FakeGuildRepository::DeleteGuild(std::uint32_t guild_id)
{
    std::lock_guard lock(m_mtx);
    m_calls.push_back({Call::Kind::kDeleteGuild, guild_id, 0, 0, 0,
                       0, 0, 0});
    m_guilds.erase(guild_id);
    return true;
}

bool FakeGuildRepository::AddWanted(std::uint32_t      guild_id,
                                     std::uint8_t       min_level,
                                     std::uint8_t       max_level,
                                     const std::string& title,
                                     const std::string& text,
                                     std::int64_t       end_time_unix)
{
    std::lock_guard lock(m_mtx);
    m_calls.push_back({Call::Kind::kAddWanted, guild_id, 0,
                       min_level, max_level,
                       static_cast<std::uint32_t>(end_time_unix), 0, 0});
    (void)title; (void)text;
    return true;
}

bool FakeGuildRepository::DeleteWanted(std::uint32_t guild_id)
{
    std::lock_guard lock(m_mtx);
    m_calls.push_back({Call::Kind::kDeleteWanted, guild_id, 0, 0, 0,
                       0, 0, 0});
    return true;
}

bool FakeGuildRepository::AddVolunteerApp(std::uint32_t char_id,
                                           std::uint32_t wanted_id)
{
    std::lock_guard lock(m_mtx);
    m_calls.push_back({Call::Kind::kAddVolunteerApp, wanted_id, char_id,
                       0, 0, 0, 0, 0});
    return true;
}

bool FakeGuildRepository::DelVolunteerApp(std::uint32_t char_id)
{
    std::lock_guard lock(m_mtx);
    m_calls.push_back({Call::Kind::kDelVolunteerApp, 0, char_id, 0, 0,
                       0, 0, 0});
    return true;
}

bool FakeGuildRepository::UpdatePvPoints(std::uint32_t guild_id,
                                          std::uint32_t total_point,
                                          std::uint32_t useable_point,
                                          std::uint32_t month_point)
{
    std::lock_guard lock(m_mtx);
    m_calls.push_back({Call::Kind::kUpdatePvPoints, guild_id, 0,
                       total_point, useable_point, month_point, 0, 0});
    auto it = m_guilds.find(guild_id);
    if (it == m_guilds.end()) return false;
    std::lock_guard g(it->second->lock);
    it->second->pvp_total_point   = total_point;
    it->second->pvp_useable_point = useable_point;
    it->second->pvp_month_point   = month_point;
    return true;
}

bool FakeGuildRepository::UpdateLevel(std::uint32_t guild_id,
                                      std::uint8_t  level)
{
    std::lock_guard lock(m_mtx);
    m_calls.push_back({Call::Kind::kUpdateLevel, guild_id, 0, level, 0, 0,
                       0, 0});
    auto it = m_guilds.find(guild_id);
    if (it == m_guilds.end()) return false;
    std::lock_guard g(it->second->lock);
    it->second->level = level;
    return true;
}

bool FakeGuildRepository::LogPointReward(std::uint32_t      guild_id,
                                         std::uint32_t      point,
                                         const std::string& /*recipient_name*/,
                                         std::uint32_t      total_point,
                                         std::uint32_t      useable_point)
{
    std::lock_guard lock(m_mtx);
    // Recipient name dropped from Call record — same pattern as
    // AddArticle. Tests can verify numeric fields + arrival order.
    m_calls.push_back({Call::Kind::kLogPointReward, guild_id, 0, point,
                       total_point, useable_point, 0, 0});
    auto it = m_guilds.find(guild_id);
    if (it == m_guilds.end()) return false;
    std::lock_guard g(it->second->lock);
    it->second->pvp_total_point   = total_point;
    it->second->pvp_useable_point = useable_point;
    return true;
}

std::vector<FakeGuildRepository::Call> FakeGuildRepository::Calls() const
{
    std::lock_guard lock(m_mtx);
    return m_calls;
}

} // namespace tworldsvr
