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

#include "services/guild_constants.h"

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
// W3a-23 — per-member PvP outcome bucket. Mirrors legacy
// TENTRYRECORD (TWorldType.h:~370). Used both for the rolling
// weekly aggregate (TGuildMember.weekrecord) and the per-day
// history rows (TGuildMember.vRecord — added in W3a-28).
// Points array uses the full kPvPEventCount=8 storage; the
// wire only emits the first 6 (PVPE_KILL_H..PVPE_WIN-1).
struct TPvPRecord
{
    std::uint16_t kill_count = 0;
    std::uint16_t die_count  = 0;
    std::array<std::uint32_t, guild::kPvPEventCount> points{};
};

// W3a-28 — one per-day row of a member's PvP record history.
// Same payload as TPvPRecord plus a `day_index` tag (legacy
// dwDate = m_timeCurrent / DAY_ONE, i.e. days-since-epoch).
// CalcWeekRecord sums the last 7 day_indexes into weekrecord.
struct TPvPDayRecord
{
    std::int64_t  day_index  = 0;
    std::uint16_t kill_count = 0;
    std::uint16_t die_count  = 0;
    std::array<std::uint32_t, guild::kPvPEventCount> points{};
};

// W3a-27 — one row of the guild's PvP point reward audit log.
// Mirrors legacy `m_vPointReward` entries (TGUILDPVPOINTREWARDTABLE).
// Populated on the DB-write path (`OnGuildPointRewardReq` —
// W3a-14) and read back via `OnGuildPointLogAck` (W3a-27).
struct TPointRewardEntry
{
    std::int64_t  date_unix = 0;
    std::string   recipient_name;
    std::uint32_t point     = 0;
};

// W3a-33 — a hired tactics member of a guild (legacy
// TTACTICSMEMBER, stored in TGuild.m_mapTTactics keyed by
// char_id). Distinct from a full TGuildMember: a tactics member
// is a mercenary recruited via the tactics-wanted board for a
// fixed term (day → end_time), paid an up-front reward of
// PvP-points + money. `id` is the member's char_id.
struct TTacticsMember
{
    std::uint32_t id           = 0;   // char_id
    std::string   name;
    std::uint8_t  level        = 0;
    std::uint8_t  klass        = 0;
    std::uint32_t reward_point = 0;   // PvP-points paid up front
    std::int64_t  reward_money = 0;   // combined-cooper money paid
    std::uint32_t gain_point   = 0;   // PvP-points earned for the guild
    std::uint8_t  day          = 0;   // contract length in days
    std::int64_t  end_time     = 0;   // Unix epoch sec the term ends
};

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

    // W3a-7 — TGUILDMEMBERLIST_REQ exposes these to the client UI.
    // Castle / camp track the W5+ castle-war system; the connected
    // date is Unix-epoch seconds from TGUILDMEMBERTABLE. All three
    // stay default-zero until their owning subsystem ports.
    std::uint16_t castle             = 0;   // m_wCastle
    std::uint8_t  camp               = 0;   // m_bCamp
    std::int64_t  connected_date_unix = 0;  // m_dlConnectedDate

    // W3a-23 — rolling 7-day PvP outcome aggregate. Returned by
    // OnGuildPvPRecordAck. W3a-28 wired the per-day vRecord
    // history below + a CalcWeekRecord helper that re-derives
    // this on every war-result fan-in.
    TPvPRecord    weekrecord{};

    // W3a-28 — per-day PvP record history. Appended (or merged
    // with the existing today's row) on every OnLocalRecordAck
    // tick. Stays bounded at ~7 entries by CalcWeekRecord which
    // both sums the last 7 days AND trims older rows inline
    // (legacy parity TGuild.cpp:615 CalcWeekRecord). No
    // separate sweep needed.
    std::vector<TPvPDayRecord> vRecord;
};

// One entry of the guild articles board (legacy MAPTGUILDARTICLE
// / TGUILDARTICLETABLE row). The article ID is monotonically-
// increasing per-guild (TGuild.article_index counter), not a DB
// primary key — clients reference articles by id within their
// own guild, so collisions across guilds are fine.
struct TGuildArticle
{
    std::uint32_t id    = 0;          // m_dwID
    std::uint8_t  duty  = 0;          // m_bDuty — writer's duty at post time
    std::string   writer;             // m_strWritter — writer's name
    std::string   title;              // m_strTitle
    std::string   body;               // m_strArticle
    std::uint32_t time_unix = 0;      // m_strDate ← legacy stores formatted
                                      //   "YYYY-MM-DD" string; we keep the
                                      //   raw epoch and let the client
                                      //   format on read.
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

    // W3a-9 fields exposed by SendMW_GUILDINFO_REQ. Most stay
    // zero until their owning subsystem ports:
    //   pvp_month_point — resets monthly (W5+ scheduler)
    //   rank_total / rank_month — guild ranking system (W5+)
    //   stat_level / stat_point / stat_exp — guild stats subsystem
    std::uint32_t pvp_month_point = 0;
    std::uint32_t rank_total      = 0;
    std::uint32_t rank_month      = 0;
    std::uint8_t  stat_level      = 0;
    std::uint8_t  stat_point      = 0;
    std::uint32_t stat_exp        = 0;

    // W3a-25 — inter-guild alliance / enemy relationships.
    // Legacy stores both as comma-separated DWORD strings in
    // TGUILDTABLE.szAllience / .szEnemy (yes, the legacy column
    // is misspelled). We hold them as proper vectors in memory;
    // the SOCI persistence path is deferred (would require a
    // schema migration to a relational join table, or a CSV
    // round-trip that matches the legacy column shape exactly).
    // Until then the in-memory state survives until process
    // restart — useful for W5+ war-system reads that look up
    // relationships during a castle siege.
    std::vector<std::uint32_t> alliance_ids;
    std::vector<std::uint32_t> enemy_ids;

    // W3a-27 — rolling PvP point reward audit log. Legacy
    // m_vPointReward; appended whenever a chief uses
    // PvP-useable-points to reward a member. Populated by the
    // W3a-14 OnGuildPointRewardReq fan-in (which both persists
    // to TGUILDPVPOINTREWARDTABLE and mirrors here for the
    // reader) and read back by W3a-27 OnGuildPointLogAck. The
    // legacy SELECT TOP 50 (CTBLGuildPvPointReward) suggests
    // legacy clients trim the view to the latest 50 entries —
    // we don't currently trim in-memory because the log is per-
    // process and load-from-DB wiring lives in a later batch.
    std::vector<TPointRewardEntry> point_log;

    // W3a-33 — hired tactics members (legacy m_mapTTactics),
    // keyed by char_id. A guild may hold up to its level chart's
    // tactics_count mercenaries. Separate from `members` (full
    // guild membership).
    std::vector<TTacticsMember> tactics_members;

    // W3a-33 helper — find a hired tactics member by char_id.
    TTacticsMember* FindTactics(std::uint32_t char_id)
    {
        for (auto& t : tactics_members)
            if (t.id == char_id) return &t;
        return nullptr;
    }

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

    // --- W3a-8 article board state -----------------------------
    //
    // article_index is a strictly-monotonic counter that
    // AddArticle bumps before returning the new ID. Mirrors the
    // legacy `m_dwArticleIndex` field. Persisted across guild
    // unloads because deleted IDs aren't reused.
    std::uint32_t              article_index = 0;
    std::vector<TGuildArticle> articles;
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
