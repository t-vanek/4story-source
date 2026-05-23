#pragma once

// GuildTacticsWantedRegistry — cluster-wide list of "we need
// tactics members" recruitment postings. Unlike the regular
// guild wanted board (one entry per guild, GuildWantedRegistry),
// a guild may post MULTIPLE tactics-wanted entries, each with a
// globally-unique id assigned from a monotonic counter (legacy
// m_dwTacticsIndex). Capped at kMaxTacticsWantedPerGuild per
// guild. Entries auto-expire after end_time; the W3a-19-style
// sweep extension lands later.
//
// Storage: unordered_map<guild_id, vector<TGuildTacticsWanted>>
// keyed by guild_id (legacy MAPVTGUILDTACTICSWANTED) + a reverse
// index id -> guild_id so Find(id) is O(1). Cardinality stays
// low (a few entries per guild, ~hundreds of guilds) so a single
// shared_mutex guards both maps.

#include "services/guild_constants.h"

#include <cstdint>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace tworldsvr {

// One applicant to a tactics-wanted posting (legacy
// TGUILDTACTICSWANTEDAPP). Reward fields are copied from the
// parent posting at apply time so the chief's volunteer-list UI
// can show what each applicant was promised.
struct TGuildTacticsWantedApp
{
    std::uint32_t char_id        = 0;
    std::uint32_t wanted_id      = 0;   // the posting id applied to
    std::uint32_t wanted_guild_id = 0;  // posting's guild
    std::uint32_t region         = 0;   // refreshed at LIST time
    std::uint8_t  level          = 0;
    std::uint8_t  klass          = 0;
    std::string   name;
    std::uint8_t  day            = 0;
    std::uint32_t point          = 0;
    std::uint32_t gold           = 0;
    std::uint32_t silver         = 0;
    std::uint32_t cooper         = 0;
};

// One tactics-wanted posting. Mirrors legacy TGUILDTACTICSWANTED.
struct TGuildTacticsWanted
{
    std::uint32_t id        = 0;   // globally-unique posting id
    std::uint32_t guild_id  = 0;
    std::uint8_t  country   = 0;
    std::string   name;            // guild name (copied at add)
    std::string   title;
    std::string   text;
    std::uint8_t  day       = 0;   // recruitment duration in days
    std::uint8_t  min_level = 0;
    std::uint8_t  max_level = 0;
    std::uint32_t point     = 0;   // PvP-point reward offered
    std::uint32_t gold      = 0;
    std::uint32_t silver    = 0;
    std::uint32_t cooper    = 0;
    std::int64_t  end_time  = 0;   // Unix epoch sec
};

class GuildTacticsWantedRegistry
{
public:
    GuildTacticsWantedRegistry() = default;
    GuildTacticsWantedRegistry(const GuildTacticsWantedRegistry&) = delete;
    GuildTacticsWantedRegistry&
        operator=(const GuildTacticsWantedRegistry&) = delete;

    // Allocate the next globally-unique posting id. Legacy
    // ++m_dwTacticsIndex; the caller passes the result back into
    // AddOrUpdate when creating a fresh posting (a non-zero
    // requested_id updates an existing posting instead).
    std::uint32_t NextId();

    // Upsert. When `entry.id` matches an existing posting for
    // the same guild, updates it in place. Otherwise inserts a
    // new posting (subject to the per-guild cap). Returns one of
    // guild::kSuccess / kMaxWanted / kFail.
    std::uint8_t AddOrUpdate(const TGuildTacticsWanted& entry);

    // Remove a posting by (guild_id, id). Returns kSuccess if
    // removed, kFail if no such posting.
    std::uint8_t Remove(std::uint32_t guild_id, std::uint32_t id);

    // Read-only lookup by global id.
    std::optional<TGuildTacticsWanted> Find(std::uint32_t id) const;

    // Snapshot every posting whose country matches `country`.
    std::vector<TGuildTacticsWanted>
        SnapshotByCountry(std::uint8_t country) const;

    std::size_t Size() const;

    // --- W3a-32 applicant flow -----------------------------------
    //
    // Parallels GuildWantedRegistry's W3a-12 applicant API.
    // AddApp runs the legacy gates:
    //   - already applied to this same guild → kSame
    //   - already applied elsewhere          → kAlreadyApply
    //   - no such posting                    → kFail
    //   - country mismatch                   → kFail
    //   - posting expired                    → kWantedEnd
    //   - level out of [min,max]             → kMismatchLevel
    //   - applicant is in the posting's own guild → kSameGuildTactics
    //   - otherwise                          → kSuccess
    // The caller supplies the applicant's war-country + current
    // guild_id (the registry doesn't reach into CharRegistry).
    std::uint8_t AddApp(const TGuildTacticsWantedApp& app,
                        std::uint8_t  country,
                        std::uint32_t applicant_guild_id);
    bool         DelApp(std::uint32_t char_id);
    // All applicants across every posting owned by `guild_id`
    // (the chief's volunteer-list view).
    std::vector<TGuildTacticsWantedApp> SnapshotAppsFor(
        std::uint32_t guild_id) const;
    // The wanted-guild_id this char applied to (0 if none) —
    // drives the wanted board's already_applied flag.
    std::uint32_t FindAppGuildByChar(std::uint32_t char_id) const;

private:
    mutable std::shared_mutex m_mtx;
    std::unordered_map<std::uint32_t,
                       std::vector<TGuildTacticsWanted>> m_by_guild;
    std::unordered_map<std::uint32_t, std::uint32_t>     m_guild_by_id;
    std::uint32_t                                        m_next_id = 0;

    // Applicants keyed by char_id (one pending application per
    // char). The value carries the posting id + guild so DelApp
    // and the volunteer-list scan don't need a posting walk.
    std::unordered_map<std::uint32_t, TGuildTacticsWantedApp> m_apps;
};

} // namespace tworldsvr
