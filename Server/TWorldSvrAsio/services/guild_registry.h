#pragma once

// GuildRegistry — in-memory store for guilds currently loaded by
// the cluster. The legacy TWorldSvr keeps this as a single
// `MAPTGUILD m_mapTGuild` guarded by the same critical section as
// `m_mapTCHAR` (PATCH_README §6 W-2). The Asio rewrite uses the
// same 16-shard partitioning as CharRegistry — guild lookups on
// member-add / member-kick / chat-broadcast happen at the speed of
// W-2 contention being eliminated.
//
// Concurrency model is identical to CharRegistry: shard-level
// shared_mutex for the registry index, per-guild std::mutex for
// field mutation. Handlers that mutate multiple guilds (e.g.
// alliance/enemy linking) must lock them in monotonically-
// increasing dwGuildID order to avoid deadlock.
//
// Lifetime of TGuild is managed via shared_ptr. The registry keeps
// a strong reference; handlers cache their own shared_ptr so the
// guild lives even past a Remove() (legacy "disorg" timer needs
// this — the guild lingers in m_mapTGuildEx for a grace period).

#include <array>
#include <cstdint>
#include <ctime>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace tworldsvr {

// One row of TGUILDMEMBERTABLE for a guild member, expanded with
// the runtime fields the legacy module pulls from the linked
// TChar (m_pChar back-pointer). Held inside TGuild::members.
struct TGuildMember
{
    std::uint32_t char_id   = 0;   // dwCharID — PK
    std::uint32_t guild_id  = 0;   // dwGuildID — FK
    std::uint8_t  duty      = 0;   // bDuty — chief / vice / officer / member
    std::uint8_t  peer      = 0;   // bPeer — peerage rank
    std::uint32_t service   = 0;   // dwService — contribution score

    // Runtime cache from TChar (set when SetMemberConnection runs).
    // Cleared when the member goes offline.
    std::string   name;            // m_strName
    std::uint8_t  level     = 0;   // m_bLevel — class level
    std::uint8_t  klass     = 0;   // m_bClass
    std::uint32_t tactics   = 0;   // m_dwTactics — tactics-guild id
    std::uint8_t  war_country = 0; // m_bWarCountry
};

// One row of TGUILDTABLE plus the in-memory caches the runtime
// touches. Mirrors `class CTGuild` (Server/TWorldSvr/TGuild.h);
// W3a-1 ships only the fields OnGuildLoadAck and a future
// OnGuildInfoAck actually read. Cabinet items, articles, alliance/
// enemy lists, point-reward log, tactics members, stats — all of
// those land in W3a-2/W3a-3 when the matching handlers get ported.
struct TGuild
{
    mutable std::mutex lock;

    // Identity — never mutates after the first insert.
    std::uint32_t id    = 0;
    std::string   name;
    std::uint32_t chief_char_id = 0;
    std::string   chief_name;

    // Mutable fields.
    std::uint8_t  level             = 0;
    std::uint32_t fame              = 0;
    std::uint32_t fame_color        = 0;
    std::uint32_t gi                = 0;    // guild-investment legacy field
    std::uint32_t exp               = 0;
    std::uint8_t  guild_points      = 0;    // bGPoint
    std::uint8_t  status            = 0;    // bStatus
    std::uint32_t gold              = 0;
    std::uint32_t silver            = 0;
    std::uint32_t cooper            = 0;
    std::uint8_t  max_cabinet       = 0;
    std::uint8_t  disorg            = 0;    // 1 = disbanding (in m_mapTGuildEx)
    std::uint32_t disorg_time       = 0;    // dwTime — disorg countdown
    std::int64_t  establish_time    = 0;    // __time64_t in legacy
    std::uint32_t pvp_total_point   = 0;
    std::uint32_t pvp_useable_point = 0;
    std::uint8_t  country           = 0;    // copied from chief on load

    // Members — keyed by char_id. Linear lookups are fine; a typical
    // guild has < 200 members and FindMember is the hot path on
    // chat / member-join. The legacy module uses a map_string-
    // keyed-by-name auxiliary index too; W3a-1 skips that and lets
    // FindMember(name) do a linear scan. W3a-3 will add a name
    // index once benchmarks demand it.
    std::vector<TGuildMember> members;

    // --- W3a-4 in-place member helpers --------------------------
    //
    // Each one expects the caller to hold `lock` already. The
    // returned pointer is into `members` and is invalidated by
    // any subsequent erase/push_back, so callers must finish
    // reading before mutating the vector.

    TGuildMember* FindMember(std::uint32_t char_id_to_find)
    {
        for (auto& m : members)
            if (m.char_id == char_id_to_find) return &m;
        return nullptr;
    }
    const TGuildMember* FindMember(std::uint32_t char_id_to_find) const
    {
        for (const auto& m : members)
            if (m.char_id == char_id_to_find) return &m;
        return nullptr;
    }

    // Remove the member with the matching char_id. Returns true if
    // a member was removed, false if char_id wasn't a member. Does
    // NOT touch any TChar back-pointers — caller must clear
    // TChar.guild_id separately so the actor-model invariant
    // "TChar.guild_id is consistent with TGuild membership" holds.
    bool RemoveMember(std::uint32_t char_id_to_remove)
    {
        for (auto it = members.begin(); it != members.end(); ++it)
        {
            if (it->char_id == char_id_to_remove)
            {
                members.erase(it);
                return true;
            }
        }
        return false;
    }
};

class GuildRegistry
{
public:
    static constexpr std::size_t kShardCount = 16;

    GuildRegistry() = default;
    GuildRegistry(const GuildRegistry&) = delete;
    GuildRegistry& operator=(const GuildRegistry&) = delete;

    // Insert a freshly-loaded guild. Returns false if guild_id was
    // already registered — caller treats that as "DB load race" and
    // discards the duplicate without overwriting (matches legacy
    // OnDM_GUILDLOAD_ACK at SSHandler.cpp:8958-8960).
    bool Insert(std::shared_ptr<TGuild> g);

    std::shared_ptr<TGuild> Remove(std::uint32_t guild_id);
    std::shared_ptr<TGuild> Find(std::uint32_t guild_id) const;

    std::size_t Size() const;
    std::vector<std::uint32_t> SnapshotIds() const;

private:
    struct Shard
    {
        mutable std::shared_mutex                                  mtx;
        std::unordered_map<std::uint32_t, std::shared_ptr<TGuild>> guilds;
    };

    static std::size_t ShardOf(std::uint32_t key)
    {
        static_assert((kShardCount & (kShardCount - 1)) == 0,
                      "kShardCount must be a power of two");
        return key & (kShardCount - 1);
    }

    std::array<Shard, kShardCount> m_shards;
};

} // namespace tworldsvr
