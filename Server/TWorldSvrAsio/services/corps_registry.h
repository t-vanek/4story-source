#pragma once

// CorpsRegistry — in-memory store for the corps (party alliances /
// "squads under a general") currently formed across the cluster.
// Legacy TWorldSvr keeps this as `MAPTCORPS m_mapTCorps` under the
// same critical section as the char/guild/party maps (PATCH_README
// §6 W-2). The Asio rewrite uses the same 16-shard partitioning +
// per-entry-mutex actor model as PartyRegistry / GuildRegistry.
//
// A corps is a set of parties (squads). One squad is the commander
// (`commander_party_id`); its chief is the general
// (`general_char_id`). A party's membership back-link lives on
// `TParty.corps_id` (0 = no corps), resolved through this registry
// on demand — the cycle-free pattern guild_id / party_id use.
//
// Lock ordering (README §5) extends to: char → party → corps. A
// handler snapshots what it needs under one lock and releases it
// before taking the next; no two of these locks are ever held
// simultaneously.
//
// W3c-1 ships the lifecycle index + the squad-set helpers the
// CORPSASK gate needs (Size for the MAX_CORPS_PARTY check). Corps
// creation (Insert with a freshly-allocated id) + commander
// succession land with the W3c-2 CORPSREPLY / W3c-3 CORPSLEAVE
// flows.

#include <array>
#include <cstdint>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

namespace tworldsvr {

struct TCorps
{
    mutable std::mutex lock;

    std::uint16_t id                 = 0; // m_wID
    std::uint16_t commander_party_id = 0; // m_wCommander
    std::uint32_t general_char_id    = 0; // m_dwGeneralID

    // Member squad (party) ids, in join order. Typical size 2..7.
    std::vector<std::uint16_t> squads;

    bool IsParty(std::uint16_t party_id) const
    {
        for (auto id : squads)
            if (id == party_id) return true;
        return false;
    }

    std::uint8_t Size() const
    {
        return static_cast<std::uint8_t>(squads.size());
    }

    // Append a squad. Returns false if already present.
    bool AddParty(std::uint16_t party_id)
    {
        if (IsParty(party_id)) return false;
        squads.push_back(party_id);
        return true;
    }

    // Remove a squad by party id. Returns true if it was present.
    bool RemoveParty(std::uint16_t party_id)
    {
        for (auto it = squads.begin(); it != squads.end(); ++it)
            if (*it == party_id) { squads.erase(it); return true; }
        return false;
    }
};

class CorpsRegistry
{
public:
    static constexpr std::size_t kShardCount = 16;

    CorpsRegistry() = default;
    CorpsRegistry(const CorpsRegistry&) = delete;
    CorpsRegistry& operator=(const CorpsRegistry&) = delete;

    bool Insert(std::shared_ptr<TCorps> c);
    std::shared_ptr<TCorps> Remove(std::uint16_t corps_id);
    std::shared_ptr<TCorps> Find(std::uint16_t corps_id) const;

    std::size_t Size() const;
    std::vector<std::uint16_t> SnapshotIds() const;

private:
    struct Shard
    {
        mutable std::shared_mutex                                  mtx;
        std::unordered_map<std::uint16_t, std::shared_ptr<TCorps>> corps;
    };

    static std::size_t ShardOf(std::uint16_t key)
    {
        static_assert((kShardCount & (kShardCount - 1)) == 0,
                      "kShardCount must be a power of two");
        return key & (kShardCount - 1);
    }

    std::array<Shard, kShardCount> m_shards;
};

} // namespace tworldsvr
