#pragma once

// CheckPeerage — pure-logic gate the OnMW_GUILDPEER_ACK handler
// consults before promoting/demoting a member's peerage rank.
// Lifted out of CTGuild::CheckPeerage (TGuild.cpp:205) so it can
// be unit-tested against synthetic guild + level rows without
// spinning up the full registry.
//
// Rules (legacy parity):
//   1. bPeer = 0 → always allowed (clearing peerage is free).
//   2. bPeer > MAX_GUILD_PEER_COUNT (5) → refused.
//   3. The destination slot must not be full. Each TGuildLevelRow
//      caps how many members can hold each rank via peer_slots[].
//   4. Mid-tier peerage requires GUILD_DUTY_CHIEF when the guild's
//      level matches the rank's "exclusive" band:
//        level 3-4  : BARON (1)   chief-only
//        level 5-6  : VISCOUNT(2) chief-only
//        level 7-8  : COUNT  (3)  chief-only
//        level 9    : MARQUIS(4)  chief-only
//        level 10   : DUKE   (5)  chief-only
//      Outside the band, any duty can hold the rank (subject to
//      the slot cap).
//
// Defensive null handling: if level_row is nullptr (guild_levels
// cache not loaded, e.g. dev DB without TGUILDCHART), the helper
// allows any non-zero peer up to MAX_GUILD_PEER_COUNT. Operators
// running without TGUILDCHART see a relaxed gate rather than
// blanket-refusal — matches the W3a-4d "missing chart = empty
// cache" philosophy. The duplicate count comes from the caller-
// supplied member list (the registry data the handler already
// has under TGuild.lock).

#include "services/guild_level_cache.h"
#include "services/guild_registry.h"

#include <cstdint>

namespace tworldsvr::guild {

bool CheckPeerage(const TGuildLevelRow* level_row,
                  std::uint8_t          requester_duty,
                  std::uint8_t          new_peer,
                  const TGuild&         guild);

} // namespace tworldsvr::guild
