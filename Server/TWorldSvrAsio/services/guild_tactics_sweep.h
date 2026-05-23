#pragma once

// SweepExpiredTactics — periodic prune of tactics-member
// contracts whose fixed term (TTacticsMember.end_time) has
// elapsed. Replaces the legacy EXPIRED_GT path
// (CTGuild::AddTactics registers an OnEventExpired entry; the
// timer service fires SM_EVENTEXPIRED_ACK at end_time, which
// ends the contract). Our SOCI-direct port has no separate
// timer service, so the sweep runs in-process on a
// RegistryRefresher tick (mirrors the W3a-19 wanted-board sweep).
//
// Expiry is an end-of-term, not a quit or kick: no money/point
// refund (legacy DelTactics is only called with a refund on the
// self-leave path). The sweep just removes the member from the
// guild's roster and clears the char's tactics back-pointer.

#include "services/char_registry.h"
#include "services/guild_registry.h"

#include <boost/asio/awaitable.hpp>

namespace tworldsvr {

// One sweep pass:
//   1. Sample now.
//   2. For each guild (via GuildRegistry::SnapshotIds) drop any
//      tactics member whose end_time <= now, collecting the
//      freed char_ids.
//   3. Clear each freed char's tactics_guild_id back-pointer.
//   4. Log a one-line summary when anything expired.
//
// `chars` may be null (the back-pointer clear is then skipped).
boost::asio::awaitable<void>
SweepExpiredTactics(GuildRegistry& guilds, CharRegistry* chars);

} // namespace tworldsvr
