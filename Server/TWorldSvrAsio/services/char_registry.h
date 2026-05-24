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

// W4-1 — one entry in a character's friend list (legacy TFRIEND in
// m_mapTFRIEND, keyed by the friend's char id). Stored in
// TChar::friends; mutated under the owning TChar's lock.
struct TFriend
{
    std::uint32_t id        = 0;     // friend's char id (m_dwID)
    std::string   name;              // m_strName
    std::uint8_t  type      = 0;     // m_bType (friend::FT_*)
    bool          connected = false; // m_bConnected (online)
    std::uint32_t region    = 0;     // m_dwRegion (last-seen zone)
    std::uint8_t  group     = 0;     // m_bGroup (friend-group bucket)
};

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
// OnMW_ADDCHAR_ACK / OnMW_CLOSECHAR_ACK actually touch. **W3a-3**
// extends with the identity fields (name + country + aid_country
// + class + level) that OnMW_CHANGECHARBASE_ACK populates and that
// OnRW_ENTERCHAR_REQ reads. Each W3+ phase appends here as more
// handlers land (guild/party/corps back-pointers, BR/BoW session
// pointers, soulmate / friend lists, …).
//
// `lock` guards every mutable field. Handlers that mutate more
// than one TChar must acquire locks in increasing char_id order.
struct TChar
{
    mutable std::mutex lock;

    // Identity — set at insertion time, never changes.
    std::uint32_t char_id = 0;
    std::uint32_t key     = 0;
    std::uint32_t user_id = 0;

    // The map server that "owns" this character's main session.
    // Additional map connections live in `cons`. Mirrors legacy
    // m_bMainID + m_mapTCHARCON.
    std::uint8_t  main_server_id = 0;

    // Lifecycle flags.
    bool          logout            = false;
    bool          saving            = false;
    bool          db_loading        = false;
    bool          main_id_changing  = false;

    // Per-map-server connection table. Insert ordering is by
    // server_id arrival; lookups are linear (typical size 1–3).
    std::vector<TCharCon> cons;

    // W3a-3 identity fields. Populated by OnMW_CHANGECHARBASE_ACK
    // (and its initial CHARINFO push that arrives shortly after
    // OnMW_ADDCHAR_ACK in the legacy flow). Default-zero until
    // then; OnRW_ENTERCHAR_REQ reflects whatever's been set.
    std::string   name;            // m_strNAME, original case
    std::uint8_t  country     = 0; // m_bCountry (TCONTRY_A/B/N)
    std::uint8_t  aid_country = 0; // m_bAidCountry
    std::uint8_t  klass       = 0; // m_bClass (TCLASS_*)
    std::uint8_t  level       = 0; // m_bLevel
    std::uint8_t  race        = 0; // m_bRace (TRACE_*)
    std::uint8_t  sex         = 0; // m_bSex
    std::uint8_t  face        = 0; // m_bFace
    std::uint8_t  hair        = 0; // m_bHair
    std::uint16_t map_id      = 0; // m_wMapID — last seen map
    float         pos_x       = 0.0f;
    float         pos_y       = 0.0f;
    float         pos_z       = 0.0f;

    // W3a-4 guild back-pointer. Legacy holds `CTGuild* m_pGuild`;
    // we keep a `guild_id` (0 = no guild) and look the TGuild up
    // through GuildRegistry on demand. This avoids cycles in
    // shared_ptr ownership (registry owns TGuild + TChar, neither
    // holds strong refs to the other) and keeps the per-char
    // lock free of TGuild's lock. The matching W3a-4 tactics
    // back-pointer lands together with the guild-tactics handlers.
    std::uint32_t guild_id    = 0;

    // W3a-33 — the guild this char is a hired tactics member of
    // (legacy m_pTactics). 0 = not a tactics member. Distinct
    // from guild_id (full membership): a char can be a full
    // member of one guild AND a tactics mercenary of another.
    std::uint32_t tactics_guild_id = 0;

    // W3b-1 party back-pointer. Legacy holds `CTParty* m_pParty`;
    // we keep a `party_id` (0 = no party) and resolve the TParty
    // through PartyRegistry on demand — same cycle-free pattern as
    // guild_id. `party_waiter` mirrors m_bPartyWaiter: set true
    // while an invite dialog is pending on this char's client so a
    // second inviter is rejected with PARTY_WAITERS.
    std::uint16_t party_id     = 0;
    bool          party_waiter = false;

    // W3b-1 combat stats. Legacy m_dwMaxHP / m_dwHP / m_dwMaxMP /
    // m_dwMP, refreshed by SetCharStatus on every party-flow packet
    // (the map server ships the current values so world can fan
    // them out in the MW_PARTYJOIN_REQ / MW_PARTYMANSTAT_REQ
    // broadcasts). Zero until the first party packet sets them.
    std::uint32_t max_hp = 0;
    std::uint32_t hp     = 0;
    std::uint32_t max_mp = 0;
    std::uint32_t mp     = 0;

    // W4-1 social state. `region` mirrors legacy m_dwRegion (the
    // char's last-known zone, shipped in friend/soulmate presence
    // updates; 0 until a region handler ports). `friends` is the
    // legacy m_mapTFRIEND friend list (typical size ≤ MAX_FRIEND).
    std::uint32_t        region = 0;
    std::vector<TFriend> friends;

    // W4-3 named friend groups (legacy m_mapFRIENDGROUP, BYTE id →
    // name), capped at MAX_FRIENDGROUP. Each TFriend.group references
    // one of these (0 = ungrouped).
    std::vector<std::pair<std::uint8_t, std::string>> friend_groups;
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

    // W3a-3: case-insensitive name lookup. Legacy m_mapTCHARNAME
    // uses MakeUpper as the index key; we mirror that so handlers
    // can call FindByName(strName) and reach the right entry
    // regardless of the inbound packet's casing. Returns nullptr
    // for an unset / unknown name.
    std::shared_ptr<TChar> FindByName(const std::string& name) const;

    // W3a-3: atomically update a char's name + the name-index.
    // Returns false if char_id is not registered or if new_name
    // collides with another char (name uniqueness is enforced
    // case-insensitively). Pass an empty new_name to clear the
    // index entry without renaming (used at CloseChar time when
    // we want the char gone from the name lookup but keep the
    // entry for the upcoming CloseChar handler).
    bool Rename(std::uint32_t char_id, const std::string& new_name);

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
    // W3a-3: per-shard name index. Keyed by ToUpper(name) so
    // FindByName is case-insensitive. Shard selected by
    // std::hash<std::string> of the uppercased name — independent
    // of the char_id shard so a rename only touches one name shard
    // (and one char_id shard via the TChar back-pointer).
    struct NameShard
    {
        mutable std::shared_mutex                                 mtx;
        std::unordered_map<std::string, std::shared_ptr<TChar>>   names;
    };

    static std::size_t ShardOf(std::uint32_t key)
    {
        static_assert((kShardCount & (kShardCount - 1)) == 0,
                      "kShardCount must be a power of two");
        return key & (kShardCount - 1);
    }
    static std::size_t NameShardOf(const std::string& upper_name)
    {
        return std::hash<std::string>{}(upper_name) & (kShardCount - 1);
    }
    static std::string ToUpper(const std::string& s);

    std::array<Shard,     kShardCount> m_shards;
    std::array<UserShard, kShardCount> m_user_shards;
    std::array<NameShard, kShardCount> m_name_shards;
};

} // namespace tworldsvr
