#pragma once

// CastleWarRegistry — the castle-war occupation scoreboard + the
// attacker/defender selection engine (legacy m_mapCastleWarInfo +
// NotifyCastleWarInfo + SelectCastleWarGuild + CompareOccupation +
// CompareGuildRank; TWorldSvr.cpp:7292-7620, SSHandler.cpp:9579).
//
// Maps report per-castle occupation results (6 slots per local, 11
// points for a DEFEND hold, 10 for an ACCEPT capture). The world
// aggregates per-guild bonus totals, keeps the per-slot ACCEPT
// history (feeds the occupation tiebreaker), computes the per-country
// top-3, and elects one defender + one attacker per castle — with
// the recursive cross-castle stealing rule: a guild that would be
// champion of two castles keeps the one where it scored more and is
// re-elected away from the other.
//
// Ordering note: the legacy containers are ordered std::maps and the
// selection ties fall back to iteration order — we keep std::map for
// byte-identical outcomes.

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace tworldsvr {

inline constexpr std::uint8_t kCastleCountryCount = 4; // TCONTRY_COUNT
inline constexpr std::uint8_t kCastleOccupySlots  = 6;
inline constexpr std::uint16_t kOccupyDefendBonus = 11;
inline constexpr std::uint16_t kOccupyAcceptBonus = 10;

struct CastleTop3Entry
{
    std::uint32_t id    = 0;
    std::string   name;
    std::uint16_t point = 0;
};

struct CastleWarInfo
{
    std::uint16_t id = 0;
    // guild → accumulated bonus (legacy m_mapGuild, DWORD values).
    std::map<std::uint32_t, std::uint32_t> guild_points;
    // guild → per-slot ACCEPT bonus history (legacy m_mapOccupy).
    std::map<std::uint32_t,
             std::array<std::uint16_t, kCastleOccupySlots>> occupy;
    // Scratch for the selection passes (legacy m_mapEnableGuild).
    std::map<std::uint32_t, std::uint32_t> enable;
    // rank (1..3) → entry, for war countries D(0) / C(1).
    std::array<std::map<std::uint8_t, CastleTop3Entry>, 2> top3;

    std::uint32_t atk_guild   = 0;
    std::uint32_t def_guild   = 0;
    std::uint8_t  def_country = 3;                 // TCONTRY_N
    std::array<std::uint16_t, kCastleCountryCount> country_point{};
};

// What the selection needs to know about a guild (resolved by the
// handler from GuildRegistry so the registry stays lock-free of it).
struct CastleGuildFacts
{
    bool          found       = false;
    std::uint8_t  country     = 3;
    std::string   name;
    std::uint32_t pvp_total   = 0;
    std::size_t   member_count = 0;
    std::int64_t  establish   = 0;
};

using CastleGuildFactsFn =
    std::function<CastleGuildFacts(std::uint32_t)>;

// One broadcast/replay row (matches SendMW_CASTLEWARINFO_REQ).
struct CastleWarBroadcastRow
{
    CastleWarInfo info;
    std::uint32_t def_id = 0;
    std::string   def_name;
    std::uint32_t atk_id = 0;
    std::string   atk_name;
};

class CastleWarRegistry
{
public:
    CastleWarRegistry() = default;
    CastleWarRegistry(const CastleWarRegistry&) = delete;
    CastleWarRegistry& operator=(const CastleWarRegistry&) = delete;

    // Replace the stored report for `report.id` (legacy erase+insert,
    // SSHandler.cpp:9656). Only id / guild_points / occupy matter;
    // computed fields reset on the next Recompute.
    void Store(CastleWarInfo report);

    // The full legacy NotifyCastleWarInfo pipeline: reset scratch,
    // country points + top-3, per-castle defender pass (with the
    // recursive cross-castle stealing), cross-castle defender
    // exclusion, attacker pass, then emit one row per castle.
    // `castle_war_day` = m_battletime[BT_CASTLE].m_bDay (weekday the
    // castle war runs, 1..7) — drives the occupation tiebreaker.
    std::vector<CastleWarBroadcastRow>
        Recompute(const CastleGuildFactsFn& facts,
                  std::uint8_t castle_war_day);

    // Replay path (legacy wCastle==0): serialize the CURRENT state,
    // names resolved now, no recompute.
    std::vector<CastleWarBroadcastRow>
        Snapshot(const CastleGuildFactsFn& facts) const;

    std::size_t Size() const;

private:
    CastleWarInfo* SelectWarGuild(bool is_def, CastleWarInfo& castle,
                                  const CastleGuildFactsFn& facts,
                                  std::uint8_t castle_war_day,
                                  std::uint32_t* out_top);

    static std::uint32_t CompareOccupation(const CastleWarInfo& c1,
                                           std::uint32_t g1,
                                           const CastleWarInfo& c2,
                                           std::uint32_t g2,
                                           std::uint8_t war_day);
    static bool CompareGuildRank(const CastleGuildFacts& a,
                                 const CastleGuildFacts& b);

    mutable std::mutex m_mtx;
    std::map<std::uint16_t, CastleWarInfo> m_castles;
};

} // namespace tworldsvr
