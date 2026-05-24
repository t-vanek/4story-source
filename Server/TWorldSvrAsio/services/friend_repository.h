#pragma once

// IFriendRepository — read interface to the friend / friend-group /
// soulmate persistence layer. Concrete impls: SociFriendRepository
// (TFRIENDTABLE + TFRIENDGROUPTABLE + TSOULMATETABLE via SOCI) and
// FakeFriendRepository (in-memory for tests).
//
// W4-15 covers the **load path** only — the per-char fetch that
// populates TChar.friends / friend_groups / soulmate when a char
// comes online (legacy DM_FRIENDLIST round-trip, collapsed to a
// direct query like the guild repo). The write path (persisting
// FRIENDASK / REPLY / ERASE / GROUP* mutations) stays deferred — the
// in-memory registry remains authoritative within a session.
//
// The friend tables store only ids + group; names / level / class
// come from a JOIN against TCHARTABLE (legacy CTBLFriend /
// CTBLFriendTarget). The repo returns the raw forward + reverse
// edges; the friend *type* (FT_FRIEND / FT_FRIENDFRIEND / FT_TARGET)
// is derived by the caller from the intersection of the two sets,
// matching legacy OnDM_FRIENDLIST_ACK (SSHandler.cpp:1683).

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
};

// Everything LoadForChar returns for one character.
struct FriendLoad
{
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
};

class IFriendRepository
{
public:
    virtual ~IFriendRepository() = default;

    // Fetch a char's full social graph for the in-memory hydrate at
    // login. Heavy-ish (3-4 queries); called once per char-online via
    // CoOffloadIf so it never blocks the io_context. Returns an empty
    // FriendLoad for a char with no rows.
    virtual FriendLoad LoadForChar(std::uint32_t char_id) = 0;

    // --- W4-16 friend-group write-back ---------------------------
    //
    // Persist the four friend-group mutations the W4-3 handlers do
    // in memory. Each maps onto one legacy CSP wrapper (CSPFriendGroup
    // Make / Delete / Name / Change, SSHandler.cpp:6490+). Best-effort
    // like the guild writes — handlers don't reverse the in-memory
    // mutation on a false return; the in-memory registry stays
    // authoritative within a session. Called via CoOffloadVoidIf so
    // SOCI never blocks the io_context.

    // INSERT a new named group row (TFRIENDGROUPTABLE).
    virtual bool MakeGroup(std::uint32_t char_id, std::uint8_t group,
                           const std::string& name) = 0;
    // DELETE a group row.
    virtual bool DeleteGroup(std::uint32_t char_id, std::uint8_t group) = 0;
    // UPDATE a group's name.
    virtual bool RenameGroup(std::uint32_t char_id, std::uint8_t group,
                             const std::string& name) = 0;
    // UPDATE one friend's group bucket (TFRIENDTABLE.bGroup).
    virtual bool ChangeFriendGroup(std::uint32_t char_id,
                                   std::uint32_t friend_id,
                                   std::uint8_t  group) = 0;
};

} // namespace tworldsvr
