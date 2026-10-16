#include "services/guild_registry.h"
#include "services/pvp_aggregate.h"
#include <algorithm>
#include <limits>
#include <stdexcept>

#include "services/guild_constants.h"

namespace tworldsvr {
void GuildRegistry::InitializeRecentRecordDate() {
    std::uint32_t recent=0;auto ids=SnapshotIds();std::sort(ids.begin(),ids.end());
    for(auto id:ids)if(auto guild=Find(id)) {
        std::lock_guard guard(guild->lock);
        for(const auto& member:guild->members)for(const auto& row:member.vRecord) {
            if(row.day_index<0||row.day_index>std::numeric_limits<std::uint32_t>::max())
                throw std::runtime_error("Source guild record DWORD date unavailable");
            recent=std::max(recent,static_cast<std::uint32_t>(row.day_index));
        }
    }
    SetRecentRecordDate(recent);
    for(auto id:ids)if(auto guild=Find(id)) {
        std::lock_guard guard(guild->lock);
        for(auto& member:guild->members)CalcWeekRecord(member,recent);
    }
}

bool TGuild::RemoveTactics(std::uint32_t char_id, bool refund)
{
    for (auto it = tactics_members.begin();
         it != tactics_members.end(); ++it)
    {
        if (it->id != char_id) continue;
        if (refund)
        {
            // Return the up-front PvP-point + money payment to
            // the guild's useable banks (legacy DelTactics
            // bKick==0 GainPvPoint + GainMoney).
            pvp_useable_point += it->reward_point;
            const std::int64_t money =
                guild::CalcMoney(gold, silver, cooper) +
                it->reward_money;
            guild::SplitMoney(money, gold, silver, cooper);
        }
        tactics_members.erase(it);
        return true;
    }
    return false;
}

bool GuildRegistry::Insert(std::shared_ptr<TGuild> g)
{
    if (!g) return false;
    auto& shard = m_shards[ShardOf(g->id)];
    std::unique_lock lock(shard.mtx);
    auto [it, inserted] = shard.guilds.emplace(g->id, std::move(g));
    return inserted;
}

std::shared_ptr<TGuild>
GuildRegistry::Remove(std::uint32_t guild_id)
{
    auto& shard = m_shards[ShardOf(guild_id)];
    std::unique_lock lock(shard.mtx);
    auto it = shard.guilds.find(guild_id);
    if (it == shard.guilds.end()) return nullptr;
    auto out = std::move(it->second);
    shard.guilds.erase(it);
    return out;
}

std::shared_ptr<TGuild>
GuildRegistry::Find(std::uint32_t guild_id) const
{
    const auto& shard = m_shards[ShardOf(guild_id)];
    std::shared_lock lock(shard.mtx);
    auto it = shard.guilds.find(guild_id);
    return it == shard.guilds.end() ? nullptr : it->second;
}

std::size_t GuildRegistry::Size() const
{
    std::size_t total = 0;
    for (const auto& shard : m_shards)
    {
        std::shared_lock lock(shard.mtx);
        total += shard.guilds.size();
    }
    return total;
}

std::vector<std::uint32_t> GuildRegistry::SnapshotIds() const
{
    std::vector<std::uint32_t> out;
    for (const auto& shard : m_shards)
    {
        std::shared_lock lock(shard.mtx);
        out.reserve(out.size() + shard.guilds.size());
        for (const auto& [id, _] : shard.guilds)
            out.push_back(id);
    }
    return out;
}

} // namespace tworldsvr
