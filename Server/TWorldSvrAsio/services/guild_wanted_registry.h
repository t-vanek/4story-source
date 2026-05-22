#pragma once

// GuildWantedRegistry — cluster-wide list of "we are recruiting"
// postings. At most one entry per guild (legacy semantics — a
// chief's second post overwrites the first). Entries auto-expire
// after kGuildWantedPeriodSec (14 days from legacy
// TWorldType.h::GUILDWANTED_PERIOD); a future timer-thread sweep
// prunes stale entries — for now handlers just return all
// entries and the client filters on `end_time` in the UI.
//
// Cardinality is low (peak ~1k entries in legacy production) so
// we use a single shared_mutex + unordered_map keyed by guild_id.
// No sharding overhead.

#include <cstdint>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace tworldsvr {

// One applicant entry — a player who clicked "apply" on a guild
// wanted posting. Stored both inside the parent TGuildWanted's
// `applicants` vector AND in GuildWantedRegistry's reverse index
// (`m_app_by_char`) so OnGuildVolunteeringDelAck can look up
// "where did this char apply?" in O(1).
struct TGuildWantedApp
{
    std::uint32_t char_id   = 0;
    std::uint32_t wanted_id = 0;   // guild_id of the parent wanted entry
    std::uint32_t region    = 0;   // m_dwRegion — refreshed at LIST time
    std::uint8_t  level     = 0;
    std::uint8_t  klass     = 0;
    std::string   name;
};

// One row of TGUILDWANTEDTABLE. Mirrors tagTGUILDWANTED.
struct TGuildWanted
{
    std::uint32_t guild_id  = 0;
    std::uint8_t  country   = 0;
    std::uint8_t  min_level = 0;
    std::uint8_t  max_level = 0;
    std::int64_t  end_time  = 0;   // Unix epoch sec (legacy m_dlEndTime)
    std::string   name;            // guild name (copied from TGuild at add)
    std::string   title;
    std::string   text;

    // W3a-12 applicant list — populated by OnGuildVolunteeringAck;
    // cleared on entry removal or member acceptance.
    std::vector<TGuildWantedApp> applicants;
};

class GuildWantedRegistry
{
public:
    GuildWantedRegistry() = default;
    GuildWantedRegistry(const GuildWantedRegistry&) = delete;
    GuildWantedRegistry& operator=(const GuildWantedRegistry&) = delete;

    // Upsert by guild_id. The legacy AddGuildWanted treats a
    // second add from the same guild as an in-place update of
    // the existing entry; we mirror that here. Returns true on
    // insert/update.
    bool AddOrUpdate(const TGuildWanted& w);

    // Remove the entry for a guild. Returns true if an entry
    // was removed; false when no entry existed.
    bool Remove(std::uint32_t guild_id);

    // Read-only lookup.
    std::optional<TGuildWanted> Find(std::uint32_t guild_id) const;

    // Snapshot every entry whose country matches `country`. Used
    // by OnGuildWantedListAck; legacy filters cross-country on
    // the world side so each shard only sees its own postings.
    std::vector<TGuildWanted>
        SnapshotByCountry(std::uint8_t country) const;

    std::size_t Size() const;

    // --- W3a-12 applicant flow -----------------------------------
    //
    // Application lifecycle:
    //   - AddApp checks all 5 legacy gates (already-applied-same /
    //     already-applied-elsewhere / no-such-wanted / expired /
    //     level-out-of-range) + the country gate (caller supplies
    //     applicant.country since TGuildWantedApp doesn't carry
    //     it). Returns one of guild::kSame / kAlreadyApply /
    //     kFail / kWantedEnd / kMismatchLevel / kSuccess.
    //   - DelApp removes by char_id from both indices.
    //   - SnapshotAppsFor returns the applicant list for a guild
    //     (chief's VOLUNTEERLIST handler).
    //   - FindAppByChar returns the wanted_id this char applied
    //     to (or 0 if none); used by future "already_applied"
    //     hints in the wanted board.

    std::uint8_t AddApp(const TGuildWantedApp& app, std::uint8_t country);
    bool         DelApp(std::uint32_t char_id);
    std::vector<TGuildWantedApp> SnapshotAppsFor(
        std::uint32_t guild_id) const;
    std::uint32_t FindAppByChar(std::uint32_t char_id) const;

private:
    mutable std::shared_mutex                              m_mtx;
    std::unordered_map<std::uint32_t, TGuildWanted>        m_entries;
    // Reverse index: applicant char_id → wanted entry's guild_id.
    // Drives DelApp + FindAppByChar without scanning every entry.
    std::unordered_map<std::uint32_t, std::uint32_t>       m_app_by_char;
};

} // namespace tworldsvr
