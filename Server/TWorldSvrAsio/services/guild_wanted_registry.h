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

// One row of TGUILDWANTEDTABLE. Mirrors tagTGUILDWANTED. The
// `app_count` field tracks pending applicants — the application
// records themselves live in a separate W3a-12+ subsystem.
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

private:
    mutable std::shared_mutex                              m_mtx;
    std::unordered_map<std::uint32_t, TGuildWanted>        m_entries;
};

} // namespace tworldsvr
