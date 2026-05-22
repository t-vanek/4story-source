#pragma once

// GuildLevelCache — small read-only mirror of TGUILDCHART that
// the guild handlers consult for per-level caps:
//
//   * max member count (CheckPeerage gate, AddMember gate)
//   * cabinet slot count (GuildCabinetMax handler in W3a-5+)
//   * tactics members + battle-set + guard / turret counts
//   * per-peerage-rank slot limits (bPeer[5])
//
// Loaded once at boot from TGUILDCHART via IGuildLevelRepository,
// then read by handlers without locking — the table is immutable
// at runtime (operators tune it offline + reload via a
// not-yet-ported admin handler). std::array is sized for one row
// per legacy MAX_GUILD_LEVEL + a sentinel zero row at index 0,
// so `Find(bLevel)` is a direct array index.
//
// Legacy parity: maps to TWorldType.h::tagTGUILDLEVEL +
// CTWorldSvrModule::FindGuildLevel (TWorldSvr.cpp:3976).

#include <array>
#include <cstdint>
#include <vector>

namespace tworldsvr {

constexpr std::uint8_t kMaxGuildLevel = 10;          // NetCode.h MAX_GUILD_LEVEL
constexpr std::uint8_t kMaxGuildPeerCount = 5;       // NetCode.h MAX_GUILD_PEER_COUNT

// One row of TGUILDCHART. POD; rows are immutable post-load.
struct TGuildLevelRow
{
    std::uint8_t  level            = 0;   // bLevel — primary key
    std::uint32_t exp              = 0;   // dwEXP — exp needed for this level
    std::uint8_t  max_count        = 0;   // bMaxCnt — member cap
    std::uint8_t  min_count        = 0;   // bMinCnt — disband threshold
    std::uint8_t  cabinet_count    = 0;   // bCabinetCnt
    std::uint8_t  tactics_count    = 0;   // bTacticsCnt
    std::uint8_t  battle_set_count = 0;   // bBattleSetCnt
    std::uint8_t  guard_count      = 0;   // bGuardCnt
    std::uint8_t  royal_guard_count = 0;  // bRoyalGuardCnt
    std::uint8_t  turret_count     = 0;   // bTurretCnt
    // Per-peerage slot caps. Index 0 = GUILD_PEER_BARON, …, 4 =
    // GUILD_PEER_DUKE. The legacy "bPeer1..bPeer5" columns are
    // 1-indexed in the SQL, but the in-memory layout (and our
    // mirror) keeps them 0-indexed.
    std::array<std::uint8_t, kMaxGuildPeerCount> peer_slots{};
};

class GuildLevelCache
{
public:
    GuildLevelCache() = default;
    GuildLevelCache(const GuildLevelCache&) = delete;
    GuildLevelCache& operator=(const GuildLevelCache&) = delete;

    // Populate from a sequence of rows. Rows whose bLevel is out
    // of range (1..kMaxGuildLevel) are dropped with a debug log.
    // Calling LoadFrom a second time replaces the existing rows
    // (used by future TGUILDCHART hot-reload).
    void LoadFrom(const std::vector<TGuildLevelRow>& rows);

    // Lookup by level. Returns nullptr for level 0 or any level
    // that wasn't preloaded. Reads are lock-free since LoadFrom
    // is called once at boot before the listener accepts traffic.
    const TGuildLevelRow* Find(std::uint8_t level) const;

    // Total number of preloaded rows. Used by main.cpp's startup
    // log + by tests asserting on the warmup count.
    std::size_t Size() const { return m_count; }

private:
    // Index 0 reserved for "no guild" (level == 0). Slots 1..N
    // hold real rows; rows > kMaxGuildLevel are ignored.
    std::array<TGuildLevelRow, kMaxGuildLevel + 1> m_rows{};
    std::array<bool, kMaxGuildLevel + 1>           m_present{};
    std::size_t                                    m_count = 0;
};

} // namespace tworldsvr
