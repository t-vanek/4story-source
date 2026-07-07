#pragma once

// BowRegistry — the Bow battleground state (legacy CTBowSystem,
// Server/TWorldSvr/BowSystem.{h,cpp}). W6-24 shipped the queue +
// points stubs; **W6-54** ports the full scheduler:
//
//   * the daily start-time table (m_vTIMES, seconds of local day)
//     with the upper_bound wraparound election (Init / SetNextTime);
//   * the ALARM → PEACE(buy) → BATTLE → BODPEACE window state
//     machine (SetStatus) driven by the RunBowTick sweeper, incl.
//     the 12-second BODPEACE countdown that fires BOW_END once and
//     then EndBattle;
//   * the solo (m_mapBOWREG) + per-guild (m_mapGuildMember) queues,
//     gated on BS_ALARM exactly like the legacy;
//   * CreateMatch: guild blocks split by their front player's
//     country, the smallest-difference whole-guild rebalance, solo
//     players filled onto the smaller team, the residual half-move,
//     and the forced D/C team country stamp;
//   * UpdatePoints with the legacy wipe-to-zero rule (opponent at 1
//     drops to 0 and the peace countdown starts — the W6-24 stub's
//     clamp deviation is repaired here);
//   * the BS_PEACE late-join path (running match only) and the
//     single-player release.
//
// Side effects (map broadcasts, the BOW-server commands, teleports,
// the TAddBOWPlayer/TClearBOWPlayers DB legs) are surfaced through
// the returned action structs — the wire layer drives them, same
// factoring as TournamentRegistry.
//
// Clocks are injected: `clt` = seconds of the local day (legacy
// dwCurrentCLT), `now_ms` = a monotonic millisecond tick (legacy
// GetTickCount) so tests stay deterministic.

#include "bow_constants.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <vector>

namespace tworldsvr {

struct TBowEntry
{
    std::uint32_t char_id  = 0;
    std::uint32_t key      = 0;
    std::uint8_t  country  = 0;
    std::uint32_t guild_id = 0;  // tactics first, else guild, else 0
};

// Active-match roster row (legacy BOWPLAYER — country here is the
// assigned TEAM, force-stamped D/C by CreateMatch).
struct TBowPlayer
{
    std::uint32_t char_id = 0;
    std::uint32_t key     = 0;
    std::uint8_t  country = 0;
};

class BowRegistry
{
public:
    BowRegistry() = default;
    BowRegistry(const BowRegistry&) = delete;
    BowRegistry& operator=(const BowRegistry&) = delete;

    // ---- configuration (boot) -----------------------------------

    // TBOWSETTINGSCHART row (first row wins — legacy breaks after
    // one). Marks the registry configured; without it the tick and
    // the queues stay inert (legacy: no module instance at all).
    void Configure(std::uint16_t map_id, std::uint8_t min_players,
                   std::uint8_t max_nation_difference,
                   std::uint32_t alarm_dur, std::uint32_t buy_dur,
                   std::uint32_t battle_dur);
    bool Configured() const;

    // TCUSTOMTIMECHART BT_BOW rows (seconds of local day).
    void AddStartTime(std::uint32_t clt);

    // CTBowSystem::Init (BowSystem.cpp:50): sort the table and pick
    // the first start past `clt` (wrapping to the earliest).
    void Init(std::uint32_t clt);

    // ---- queue ops (W6-24 wire handlers) --------------------------

    // AddPlayerToQueue (BowSystem.cpp:82). `admin` mirrors the
    // legacy default-FALSE parameter (no caller passes TRUE).
    // Result: the BOWREG_* byte — including the undocumented
    // BOWREG_FAIL+1 (=4) "joined the running match late" marker.
    // On that late-join path `late_join` carries the roster row the
    // caller must teleport in (ADDBOWPLAYERS to the BOW server +
    // PREPAREFORBOW to the char's main + the TAddBOWPlayer leg).
    struct AddOutcome
    {
        std::uint8_t result = bow::kFail;
        bool         late_join = false;
        TBowPlayer   player{};
    };
    AddOutcome AddPlayer(std::uint32_t char_id, std::uint32_t key,
                         std::uint8_t country, std::uint32_t guild_id,
                         bool admin = false);

    // DeletePlayerFromQueue (BowSystem.cpp:183): BS_ALARM-gated,
    // guild vectors scanned first, then the solo map.
    std::uint8_t RemovePlayer(std::uint32_t char_id, std::uint32_t key);

    // ---- scheduler ------------------------------------------------

    // One pass of the legacy per-second window walk
    // (TWorldSvr.cpp:4094) + CTBowSystem::SetStatus. Returns every
    // side effect the walk produced this tick, in legacy order.
    struct TickActions
    {
        // BOWNotify broadcast (status, second, D-points, C-points);
        // fired at most twice a tick (the match-failed path emits a
        // trailing BS_NORMAL frame).
        struct Notify
        {
            std::uint8_t  status = 0;
            std::uint32_t second = 0;
            std::uint8_t  points_d = 0;
            std::uint8_t  points_c = 0;
        };
        std::vector<Notify> notifies;

        // MW_BOWCOMMANDEXEC_REQ to the BOW server (BOW_START /
        // BOW_END), in order.
        std::vector<std::uint8_t> commands;

        bool notify_nonqueued = false;  // BS_BATTLE edge

        // CreateMatch fired: teleport the roster in (ADDBOWPLAYERS
        // + per-char PREPAREFORBOW + TAddBOWPlayer legs).
        bool teleport_in = false;
        std::vector<TBowPlayer> roster;

        // EndBattle fired: ENDBOWWAR(winner) + TClearBOWPlayers.
        bool ended = false;
        std::uint8_t winner = 0;
        std::vector<TBowPlayer> end_roster;
    };
    TickActions Tick(std::uint32_t clt, std::uint64_t now_ms,
                     bool bow_server_connected);

    // UpdatePoints (BowSystem.cpp:521): the scorer gains a point;
    // an opponent sitting at exactly 1 wipes to 0 and the BODPEACE
    // countdown arms, otherwise the opponent decays by 1 (floored
    // above 0). Returns the BOWNotify echo payload.
    struct PointsOutcome
    {
        bool          configured = false;
        std::uint8_t  status = 0;
        std::uint32_t second = 0;
        std::uint8_t  points_d = 0;
        std::uint8_t  points_c = 0;
    };
    PointsOutcome UpdatePoints(std::uint8_t country,
                               std::uint64_t now_ms);

    // ReleaseSinglePlayer (BowSystem.cpp:596): only when the char is
    // on the active roster, the key matches and a winner is already
    // decided. The caller sends RELEASESINGLEBOWPLAYER + the
    // TDeleteSingleBOWPlayer leg and the registry drops the row
    // (legacy TeleportBOWPlayer single-char overload,
    // TWorldSvr.cpp:7918).
    struct ReleaseOutcome
    {
        bool         release = false;
        std::uint8_t winner  = 0;
    };
    ReleaseOutcome ReleaseSinglePlayer(std::uint32_t char_id,
                                       std::uint32_t key);

    // ---- accessors -------------------------------------------------

    std::uint8_t  Points(std::uint8_t country) const;
    std::uint8_t  Winner() const;            // m_bWinner
    std::uint32_t Tick() const;              // m_dwTick (ACK echo)
    std::uint8_t  Status() const;
    std::uint32_t NextStart() const;         // m_dwStart
    bool          Running() const;
    std::size_t   QueueSize() const;         // solo + guild entries
    std::size_t   MatchSize() const;
    bool          Contains(std::uint32_t char_id) const;   // roster
    bool          InQueue(std::uint32_t char_id) const;

private:
    struct NotifySnapshot
    {
        std::uint8_t  status = 0;
        std::uint32_t second = 0;
    };

    void ReleaseLocked();
    void SetNextTimeLocked();
    std::uint8_t CreateMatchLocked();        // BOWMATCH_*
    void SetStatusLocked(std::uint8_t status, std::uint32_t second,
                         std::uint64_t now_ms, TickActions& out);

    mutable std::shared_mutex m_lock;

    // Config.
    bool          m_configured = false;
    std::uint16_t m_map_id = 0;
    std::uint8_t  m_min_players = 0;          // loaded, legacy-unused
    std::uint8_t  m_max_nation_difference = 0;
    std::uint32_t m_alarm_dur = 0;
    std::uint32_t m_buy_dur = 0;
    std::uint32_t m_battle_dur = 0;
    std::vector<std::uint32_t> m_times;

    // State machine.
    bool          m_running = false;
    std::uint8_t  m_status = bow::kBsNormal;
    std::uint8_t  m_winner = bow::kCountryNeutral;
    std::uint32_t m_start = 0;                // m_dwStart (CLT)
    std::uint32_t m_tick = 0;                 // m_dwTick
    std::uint64_t m_peace_tick_ms = 0;        // m_dwPeaceTick
    bool          m_sent_end = false;         // the static SentEnd

    std::array<std::uint8_t, 2> m_points{};   // [D, C]

    // Queues + roster (legacy m_mapBOWREG / m_mapGuildMember /
    // m_mapBOWPLAYERS). std::map for deterministic iteration —
    // legacy uses ordered maps too.
    std::map<std::uint32_t, TBowEntry> m_reg;
    std::map<std::uint32_t, std::vector<TBowEntry>> m_guilds;
    std::map<std::uint32_t, TBowPlayer> m_players;
};

} // namespace tworldsvr
