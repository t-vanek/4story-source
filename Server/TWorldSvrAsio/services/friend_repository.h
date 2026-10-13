#pragma once

// Directed friend/group/soulmate persistence. Native PostgreSQL operations
// require the current World owner and live primary Map authority. Handlers
// commit before publishing cache changes or successful responses. The legacy
// SOCI adapter and FakeFriendRepository remain separate compatibility/test
// adapters; they do not establish native production coverage.
// Names/level/class resolve against TCHARTABLE; forward/reverse intersections
// determine FT_FRIEND / FT_FRIENDFRIEND / FT_TARGET as in DM_FRIENDLIST_ACK.

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace tworldsvr {

// One friend edge resolved against TCHARTABLE. `level` / `klass` are
// 0 for reverse-only edges (the CTBLFriendTarget query selects only
// id + name — legacy parity).
struct FriendRow
{
    std::uint32_t id    = 0;
    std::string   name;
    std::uint8_t  level = 0;
    std::uint8_t  klass = 0;
    std::uint8_t  group = 0;
    bool protected_by_owner = false;
};

// Everything LoadForChar returns for one character.
struct IncomingSoulmate {
    std::uint32_t owner=0,target=0,time=0;
    std::string target_name;
    std::uint8_t target_level=0,target_class=0;
};

struct FriendLoad
{
    // Explicit stale-authority rejection, never an empty successful graph.
    bool authority_valid=true;
    // (bGroup, szName) from TFRIENDGROUPTABLE.
    std::vector<std::pair<std::uint8_t, std::string>> groups;
    // Friends this char added (CTBLFriend: F.dwCharID = char).
    std::vector<FriendRow> forward;
    // Chars who added this char (CTBLFriendTarget: F.dwFriendID = char).
    std::vector<FriendRow> reverse;
    // TSOULMATETABLE row (target = 0 → no pairing).
    bool          has_soulmate   = false;
    std::uint32_t soulmate_target = 0;
    std::uint32_t soulmate_time   = 0;
    std::string soulmate_name;
    std::uint8_t soulmate_level=0, soulmate_class=0;
    std::vector<IncomingSoulmate> incoming_soulmates;
};

// Server-side evidence, never added to client packets. A zero key is only
// accepted by the legacy/test adapters; native persistence requires a live
// primary Map claim. Counterpart identity is required for mutual acceptance.
struct FriendAuthority {
    std::uint32_t key=0;
    std::uint8_t server=0;
    std::uint32_t other_key=0;
    std::uint8_t other_server=0;
    bool require_unlocked=false; // Source secured erase/group-delete, not protected-check cleanup.
};

class IFriendRepository
{
public:
    virtual ~IFriendRepository() = default;

    // Fetch a char's full social graph for the in-memory hydrate at
    // login. Heavy-ish (3-4 queries); called once per char-online via
    // CoOffloadIf so it never blocks the io_context. Returns an empty
    // FriendLoad for a char with no rows.
    virtual FriendLoad LoadForChar(std::uint32_t char_id, FriendAuthority authority = {}) = 0;

    // Group mutations preserve source orphan buckets: deleting a group does
    // not reset the bGroup on existing directed edges.

    // INSERT a new named group row (TFRIENDGROUPTABLE).
    virtual bool MakeGroup(std::uint32_t char_id, std::uint8_t group,
                           const std::string& name, FriendAuthority authority = {}) = 0;
    // DELETE a group row.
    virtual bool DeleteGroup(std::uint32_t char_id, std::uint8_t group, FriendAuthority authority = {}) = 0;
    // UPDATE a group's name.
    virtual bool RenameGroup(std::uint32_t char_id, std::uint8_t group,
                             const std::string& name, FriendAuthority authority = {}) = 0;
    // UPDATE one friend's group bucket (TFRIENDTABLE.bGroup).
    virtual bool ChangeFriendGroup(std::uint32_t char_id,
                                   std::uint32_t friend_id,
                                   std::uint8_t  group, FriendAuthority authority = {}) = 0;

    // One directed row is one added friend. Acceptance persists both rows
    // together, retaining existing buckets; it never falls back to two commits.

    // INSERT a (char_id, friend_id, bGroup=0) row.
    virtual bool InsertFriend(std::uint32_t char_id,
                              std::uint32_t friend_id, FriendAuthority authority = {}) = 0;
    // Both directed edges in one persistence transaction. Existing edges keep
    // their group, matching TFriendInsert's IF NOT EXISTS. No per-edge fallback.
    virtual bool InsertMutualFriends(std::uint32_t char_id,
                                     std::uint32_t friend_id, FriendAuthority authority = {}) = 0;
    // DELETE the (char_id, friend_id) row (the char's forward edge).
    virtual bool EraseFriend(std::uint32_t char_id,
                             std::uint32_t friend_id, FriendAuthority authority = {}) = 0;

    // TSoulmateReg changes only the initiator's row. Inbound projections do
    // not create reverse rows. TSoulmateEnd stamps dwTime; TSoulmateDel deletes
    // the exact directed relationship after its silence duration.

    // Upsert this char's soulmate row to point at `target`.
    virtual bool RegSoulmate(std::uint32_t char_id,
                             std::uint32_t target, FriendAuthority authority = {}) = 0;
    // Stamp this char's existing row for the source silence period.
    virtual bool EndSoulmate(std::uint32_t char_id, std::uint32_t time, FriendAuthority authority = {}) = 0;
    virtual bool EndSoulmateExact(std::uint32_t char_id,std::uint32_t target,std::uint32_t time,FriendAuthority authority = {})
    {return EndSoulmate(char_id,time,authority);}
    // Delete only this char's row when it points at `target`.
    virtual bool DelSoulmate(std::uint32_t char_id,
                             std::uint32_t target, FriendAuthority authority = {}) = 0;
};

} // namespace tworldsvr
