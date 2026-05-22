#pragma once

// IGuildRepository — read/write interface to the guild persistence
// layer. Concrete impls: SociGuildRepository (TGUILDTABLE +
// TGUILDMEMBERTABLE via SOCI) and FakeGuildRepository (in-memory
// for tests).
//
// W3a-1 covers the **read path** only — LoadAll for the boot
// warmup (legacy CSPLoadGuilds) and Find by id for the demand
// path. The write path (Establish / Update / Disorg / member
// add/leave/duty/kickout) lands in W3a-2 with the matching
// handlers and the sender layer that ACKs back to the requesting
// map server.

#include "services/guild_registry.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace tworldsvr {

class IGuildRepository
{
public:
    virtual ~IGuildRepository() = default;

    // Boot warmup — read every non-disbanded guild from the
    // backing store and return a fresh, fully-populated TGuild for
    // each. Caller hands them to GuildRegistry::Insert. Heavy
    // (single-shot at boot); not on a hot path.
    //
    // Legacy parity: the bulk of CSPGetGuilds + a per-guild
    // CSPGetGuildMembers join. Modern impl fans both out in one
    // round trip.
    virtual std::vector<std::shared_ptr<TGuild>> LoadAll() = 0;

    // Demand-path lookup — called by handlers when a packet
    // references a guild_id that's not in the cache yet. Returns
    // nullopt if the row doesn't exist.
    virtual std::optional<std::shared_ptr<TGuild>> FindById(
        std::uint32_t guild_id) = 0;
};

} // namespace tworldsvr
