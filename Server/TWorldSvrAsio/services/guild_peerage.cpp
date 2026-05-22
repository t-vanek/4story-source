#include "services/guild_peerage.h"
#include "services/guild_constants.h"

#include <cstddef>

namespace tworldsvr::guild {

namespace {

// Per legacy GUILD_PEER_* enum (NetCode.h:1988-1993):
//   GUILD_PEER_NONE     = 0
//   GUILD_PEER_BARON    = 1
//   GUILD_PEER_VISCOUNT = 2
//   GUILD_PEER_COUNT    = 3
//   GUILD_PEER_MARQUIS  = 4
//   GUILD_PEER_DUKE     = 5
constexpr std::uint8_t kPeerBaron    = 1;
constexpr std::uint8_t kPeerViscount = 2;
constexpr std::uint8_t kPeerCount    = 3;
constexpr std::uint8_t kPeerMarquis  = 4;
constexpr std::uint8_t kPeerDuke     = 5;

} // namespace

bool CheckPeerage(const TGuildLevelRow* level_row,
                  std::uint8_t          requester_duty,
                  std::uint8_t          new_peer,
                  const TGuild&         guild)
{
    if (new_peer == 0) return true;
    if (new_peer > kMaxGuildPeerCount) return false;

    // Slot-cap check — count current holders of this peer rank
    // and reject when the destination is full. Caller already
    // holds TGuild.lock so the iteration is safe.
    std::size_t at_rank = 0;
    for (const auto& m : guild.members)
        if (m.peer == new_peer) ++at_rank;

    if (level_row != nullptr)
    {
        const std::size_t cap =
            level_row->peer_slots[new_peer - 1];
        if (at_rank >= cap) return false;

        // Chief-only band per legacy switch (TGuild.cpp:224-258).
        const bool chief_only =
            (level_row->level == 3 || level_row->level == 4)
                ? (new_peer == kPeerBaron)
            : (level_row->level == 5 || level_row->level == 6)
                ? (new_peer == kPeerViscount)
            : (level_row->level == 7 || level_row->level == 8)
                ? (new_peer == kPeerCount)
            : (level_row->level == 9)
                ? (new_peer == kPeerMarquis)
            : (level_row->level == 10)
                ? (new_peer == kPeerDuke)
            : false;

        if (chief_only && requester_duty != kDutyChief) return false;
    }
    // level_row == nullptr → relaxed gate: only the global
    // MAX_GUILD_PEER_COUNT cap applies. Matches W3a-4d's "missing
    // TGUILDCHART = empty cache" dev-friendly behavior.

    return true;
}

} // namespace tworldsvr::guild
