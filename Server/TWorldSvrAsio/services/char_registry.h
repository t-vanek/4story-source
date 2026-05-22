#pragma once

// CharRegistry — in-memory store for characters currently online
// across the cluster. The legacy TWorldSvr keeps this as a single
// `MAPTCHARACTER m_mapTCHAR` guarded by one critical section
// (PATCH_README §6 W-2). The Asio rewrite partitions the store
// across N shards so guild churn and per-char field updates don't
// serialise on one global lock.
//
// Concurrency model — "per-char actor", per W-2:
//
//   * Registry-level lookup (Insert/Remove/Find) takes the SHARD
//     lock only — never the per-char lock. This means Find()
//     returns a shared_ptr<TChar> while another shard's writer is
//     active, with zero contention.
//   * Field-level mutation on a TChar takes that TChar's own
//     `std::mutex m_lock`. Handlers that touch multiple chars must
//     lock them in monotonically-increasing dwCharID order to avoid
//     deadlock — see Server/TWorldSvrAsio/README.md §5.
//   * Active-user tracking (m_mapACTIVEUSER in legacy) is a
//     separate sharded set so the user→has-any-char-online predicate
//     doesn't iterate all shards.
//
// TChar field layout matches the subset of `tagTCHARACTER` that
// W2 handlers touch. Guild/party/corps/BR/BoW back-pointers land
// in W3a/W3b/W6 — each phase appends fields here. Holding the
// struct in `shared_ptr` means the registry can drop its entry
// while a handler still has a reference (the destructor runs once
// all references die).

#include <array>
#include <cstdint>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tworldsvr {

// One inbound connection from a map server pinned to a character.
// Mirrors the legacy TCHARCON struct (m_mapTCHARCON entry per
// map server bServerID). Held in TChar::cons; mutated under the
// owning TChar's lock.
struct TCharCon
{
    std::uint8_t  server_id     = 0;     // bServerID — which map svr
    std::uint32_t ip_addr       = 0;     // dwIPAddr — client public IP
    std::uint16_t port          = 0;     // wPort   — client port
    bool          ready         = false; // m_bReady — passed the entry handshake
    bool          valid         = true;  // m_bValid — connection accepted
};

// Per-character state. W2 ships only the fields that
// OnMW_ADDCHAR_ACK / OnMW_CLOSECHAR_ACK actually touch. Each W3+
// phase appends here (guild/party/corps back-pointers, BR/BoW
// session pointers, soulmate / friend lists, …).
//
// `lock` guards every other field. Handlers that mutate more than
// one TChar must acquire locks in increasing char_id order.
struct TChar
{
    mutable std::mutex lock;

    // Identity — set at insertion time, never changes.
    std::uint32_t char_id = 0;
    std::uint32_t key     = 0;
    std::uint32_t user_id = 0;

    // The map server that "owns" this character's main session.
    // Additional map connections (e.g. for arena / sub-zones) live
    // in `cons`. Mirrors legacy m_bMainID + m_mapTCHARCON.
    std::uint8_t  main_server_id = 0;

    // Subset of the legacy lifecycle flags handlers consult at
    // W2 entry/exit. Guild/party/BR/BoW state arrives in W3+.
    bool          logout            = false; // m_bLogout
    bool          saving            = false; // m_bSave
    bool          db_loading        = false; // m_bDBLoading
    bool          main_id_changing  = false; // m_bCHGMainID

    // Per-map-server connection table. Insert ordering is by
    // server_id arrival, NOT sorted; lookups are linear (legacy
    // average size: 1–3 entries per char, so an unordered_map
    // would cost more in cache misses than the linear walk saves).
    std::vector<TCharCon> cons;
};

// CharRegistry owns the cluster-wide char index. Lifetime: created
// once at boot, destroyed at shutdown. Thread-safe.
class CharRegistry
{
public:
    static constexpr std::size_t kShardCount = 16;

    CharRegistry() = default;
    CharRegistry(const CharRegistry&) = delete;
    CharRegistry& operator=(const CharRegistry&) = delete;

    // Insert a fresh char. Returns false if char_id is already
    // registered — caller must treat that as "additional connection
    // from another map server" and route to the existing entry.
    bool Insert(std::shared_ptr<TChar> ch);

    // Remove and return the char (or nullptr if absent). Caller
    // sees a moved-out shared_ptr; the registry's reference count
    // is decremented but the TChar lives until the last external
    // reference dies (matches the legacy CloseChar semantics).
    std::shared_ptr<TChar> Remove(std::uint32_t char_id);

    // Read-only lookup. Returns nullptr if absent. The returned
    // shared_ptr is safe to outlive the next Remove() call.
    std::shared_ptr<TChar> Find(std::uint32_t char_id) const;

    // Total count across all shards. Locks each shard in turn —
    // O(N_shards) for an exact count.
    std::size_t Size() const;

    // Snapshot of every char_id currently registered. Used by
    // admin tooling + tests; locks every shard in order. Don't
    // call from a hot path.
    std::vector<std::uint32_t> SnapshotIds() const;

    // ---- Active user tracking (legacy m_mapACTIVEUSER) ----
    //
    // The user index is independent of the char index because the
    // legacy module asks "is any character of user X online?"
    // without caring which char. Sharded the same way.

    void MarkUserActive(std::uint32_t user_id);
    void MarkUserInactive(std::uint32_t user_id);
    bool IsUserActive(std::uint32_t user_id) const;
    std::size_t ActiveUserCount() const;

private:
    struct Shard
    {
        mutable std::shared_mutex                                 mtx;
        std::unordered_map<std::uint32_t, std::shared_ptr<TChar>> chars;
    };
    struct UserShard
    {
        mutable std::shared_mutex          mtx;
        std::unordered_set<std::uint32_t>  users;
    };

    static std::size_t ShardOf(std::uint32_t key)
    {
        // Direct modulo. Char IDs are densely allocated in legacy
        // (sequential per shard at the DB layer), so the low bits
        // already distribute evenly across kShardCount=16. An
        // earlier Knuth multiplicative hash had a bug — the
        // multiply overflowed uint64_t and the shift produced
        // indices > kShardCount-1, segfaulting on shard access.
        static_assert((kShardCount & (kShardCount - 1)) == 0,
                      "kShardCount must be a power of two");
        return key & (kShardCount - 1);
    }

    std::array<Shard,     kShardCount> m_shards;
    std::array<UserShard, kShardCount> m_user_shards;
};

} // namespace tworldsvr
