#pragma once

// TmsRegistry — in-memory store for the TMS ("temporary messaging
// system", a.k.a. the in-game multi-party conference channels)
// currently open across the cluster. The legacy TWorldSvr keeps
// this as a single `MAPTMS m_mapTMS` keyed by a monotonic
// `m_dwTMSIndex`, guarded by the same critical section as
// m_mapTCHAR (SSHandler.cpp OnMW_TMS*). The Asio rewrite uses the
// same 16-shard partitioning as the other registries so conference
// churn doesn't serialise on one global lock.
//
// Concurrency model is identical to PartyRegistry: shard-level
// shared_mutex for the registry index, per-group std::mutex for
// field mutation. The lock-ordering contract from
// Server/TWorldSvrAsio/README.md §5 extends here — a handler that
// holds a TChar lock must not then take a TTms lock and vice
// versa; snapshot the ids you need under one lock, release, then
// take the other (the W4-11 handlers follow this).
//
// TTms members are stored by char_id (legacy keeps raw
// LPTCHARACTER pointers in m_mapMember). The char's TMS back-link
// is the `tms` id-set on TChar, looked up through this registry on
// demand — the same cycle-free pattern party_id / guild_id use, so
// neither registry holds a strong ref into the other.

#include <array>
#include <cstdint>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace tworldsvr {

// One open TMS conference. Mirrors tagTMS (TWorldType.h): the id,
// the name of the most-recently-departed member (used to re-pair a
// collapsed 1:1 channel), and the current member char_ids.
struct TTms
{
    mutable std::mutex lock;

    std::uint32_t id = 0;           // m_dwID
    std::string   last_member;      // m_strLastMember (a char name)

    // Member char_ids (legacy m_mapMember). Typical size 2..N;
    // lookups are linear.
    std::vector<std::uint32_t> members;

    bool IsMember(std::uint32_t char_id) const
    {
        for (auto id : members)
            if (id == char_id) return true;
        return false;
    }

    std::size_t Size() const { return members.size(); }

    // Append a member. Returns false if char_id is already present.
    bool AddMember(std::uint32_t char_id)
    {
        if (IsMember(char_id)) return false;
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
};

// TmsRegistry owns the cluster-wide conference index. Lifetime:
// created once at boot, destroyed at shutdown. Thread-safe. Same
// API shape as PartyRegistry but keyed on a 32-bit id.
class TmsRegistry
{
public:
    static constexpr std::size_t kShardCount = 16;

    TmsRegistry() = default;
    TmsRegistry(const TmsRegistry&) = delete;
    TmsRegistry& operator=(const TmsRegistry&) = delete;

    // Insert a freshly-opened conference. Returns false if the id
    // was already registered (caller must not overwrite). The id is
    // allocated via GenId() before calling this.
    bool Insert(std::shared_ptr<TTms> t);

    std::shared_ptr<TTms> Remove(std::uint32_t tms_id);
    std::shared_ptr<TTms> Find(std::uint32_t tms_id) const;

    // Allocate a currently-unused TMS id in [1, 2^32-1]. Legacy
    // hands out `++m_dwTMSIndex` (monotonic, never recycled); we
    // scan a rolling cursor for a free slot instead so the 32-bit
    // space is reusable. id 0 is reserved for "no conference".
    std::uint32_t GenId();

    std::size_t Size() const;
    std::vector<std::uint32_t> SnapshotIds() const;

private:
    struct Shard
    {
        mutable std::shared_mutex                               mtx;
        std::unordered_map<std::uint32_t, std::shared_ptr<TTms>> groups;
    };

    static std::size_t ShardOf(std::uint32_t key)
    {
        static_assert((kShardCount & (kShardCount - 1)) == 0,
                      "kShardCount must be a power of two");
        return key & (kShardCount - 1);
    }

    std::array<Shard, kShardCount> m_shards;

    mutable std::mutex m_gen_mtx;
    std::uint32_t      m_gen_cursor = 0;
};

} // namespace tworldsvr
