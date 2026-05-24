#pragma once

// PartyRegistry — in-memory store for the parties currently formed
// across the cluster. The legacy TWorldSvr keeps this as a single
// `MAPTPARTY m_mapTParty` guarded by the same critical section as
// m_mapTCHAR / m_mapTGuild (PATCH_README §6 W-2). The Asio rewrite
// uses the same 16-shard partitioning as CharRegistry / GuildRegistry
// so party formation churn doesn't serialise on one global lock.
//
// Concurrency model is identical to GuildRegistry: shard-level
// shared_mutex for the registry index, per-party std::mutex for
// field mutation. The lock-ordering contract from
// Server/TWorldSvrAsio/README.md §5 extends here — a handler that
// holds a TChar lock must not then take a TParty lock and vice
// versa; snapshot the ids you need under one lock, release, then
// take the other (the W3b handlers follow this).
//
// TParty members are stored by char_id (legacy keeps raw
// LPTCHARACTER pointers in m_vMember). The char's TParty back-link
// lives on TChar::party_id, looked up through this registry on
// demand — the same cycle-free pattern guild_id uses, so neither
// registry holds a strong ref into the other.
//
// W3b-1 ships the lifecycle index + the member-set helpers the
// PARTYADD validation gate needs (IsChief / Size). Party creation
// (PartyRegistry::Insert with a freshly-allocated id) and chief
// succession land with the W3b-2 PARTYJOIN / W3b-3 PARTYDEL flows.

#include <array>
#include <cstdint>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

namespace tworldsvr {

// One party. Mirrors the subset of CTParty (TParty.h) that the
// world-side handlers touch: id, loot mode, the optional corps
// link, chief + turn-order back-pointers, the arena flag, and the
// ordered member list.
struct TParty
{
    mutable std::mutex lock;

    std::uint16_t id            = 0; // m_wID
    std::uint8_t  obtain_type   = 0; // m_bObtainType (PT_*)
    std::uint16_t corps_id      = 0; // m_wCorpsID (0 = no corps)
    std::uint32_t chief_char_id = 0; // m_dwChiefID
    std::uint32_t order_char_id = 0; // m_dwOrder (loot-turn cursor)
    bool          arena         = false; // m_bArena

    // Member char_ids in join order (legacy m_vMember). Typical
    // size 2..7; lookups are linear.
    std::vector<std::uint32_t> members;

    bool IsChief(std::uint32_t char_id) const
    {
        return chief_char_id != 0 && char_id == chief_char_id;
    }

    bool IsMember(std::uint32_t char_id) const
    {
        for (auto id : members)
            if (id == char_id) return true;
        return false;
    }

    std::uint8_t Size() const
    {
        return static_cast<std::uint8_t>(members.size());
    }

    // Append a member. The first member to join seeds the loot
    // turn-order cursor (legacy AddMember sets m_dwOrder when the
    // list was empty). Returns false if char_id is already present.
    bool AddMember(std::uint32_t char_id)
    {
        if (IsMember(char_id)) return false;
        if (members.empty()) order_char_id = char_id;
        members.push_back(char_id);
        return true;
    }

    // Remove a member by char_id. Returns true if it was present.
    bool RemoveMember(std::uint32_t char_id)
    {
        for (auto it = members.begin(); it != members.end(); ++it)
            if (*it == char_id) { members.erase(it); return true; }
        return false;
    }

    // Loot turn-order helpers (legacy CTParty, used by the PT_ORDER
    // round-robin item distribution). Defined in party_registry.cpp.
    // All assume the caller holds the party lock.

    // Member position of char_id (0 if absent — legacy GetOrderIndex).
    std::uint8_t GetOrderIndex(std::uint32_t char_id) const;

    // Advance the loot cursor (m_dwOrder) to the member after
    // char_id, wrapping to the front (legacy SetNextOrder).
    void SetNextOrder(std::uint32_t char_id);

    // Pick the next looter among `eligible` (the members in range of
    // the drop), honouring + advancing the turn cursor. Returns 0 if
    // none of `eligible` are current members (legacy GetNextOrder).
    std::uint32_t GetNextOrder(const std::vector<std::uint32_t>& eligible);
};

// PartyRegistry owns the cluster-wide party index. Lifetime: created
// once at boot, destroyed at shutdown. Thread-safe. Same API shape
// as GuildRegistry.
class PartyRegistry
{
public:
    static constexpr std::size_t kShardCount = 16;

    PartyRegistry() = default;
    PartyRegistry(const PartyRegistry&) = delete;
    PartyRegistry& operator=(const PartyRegistry&) = delete;

    // Insert a freshly-formed party. Returns false if the id was
    // already registered (caller must not overwrite). W3b-2's
    // PARTYJOIN allocates the id via GenId() before calling this.
    bool Insert(std::shared_ptr<TParty> p);

    std::shared_ptr<TParty> Remove(std::uint16_t party_id);
    std::shared_ptr<TParty> Find(std::uint16_t party_id) const;

    // Allocate a currently-unused party id in [1, 65535]. Legacy
    // pre-seeds a recycled-id queue (m_qGenPartyID); we scan a
    // rolling cursor for a free slot instead — id 0 is reserved
    // for "no party" (TChar::party_id sentinel). Returns 0 only if
    // the entire 16-bit id space is occupied (65 534 live parties,
    // never reached in practice). The result is advisory: a caller
    // must still check Insert()'s return and re-generate on the
    // (single-threaded-io: impossible) race.
    std::uint16_t GenId();

    std::size_t Size() const;
    std::vector<std::uint16_t> SnapshotIds() const;

private:
    struct Shard
    {
        mutable std::shared_mutex                                  mtx;
        std::unordered_map<std::uint16_t, std::shared_ptr<TParty>> parties;
    };

    static std::size_t ShardOf(std::uint16_t key)
    {
        static_assert((kShardCount & (kShardCount - 1)) == 0,
                      "kShardCount must be a power of two");
        return key & (kShardCount - 1);
    }

    std::array<Shard, kShardCount> m_shards;

    // Rolling cursor for GenId(), guarded by its own mutex so id
    // allocation doesn't contend with the per-shard index locks.
    mutable std::mutex m_gen_mtx;
    std::uint16_t      m_gen_cursor = 0;
};

} // namespace tworldsvr
