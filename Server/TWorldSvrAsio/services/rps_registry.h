#pragma once

// RpsRegistry — in-memory store for the RPS (rock-paper-scissors)
// event game configs. Mirrors legacy `m_mapRPSGame` keyed by
// `MAKEWORD(bType, bWinCount)`. Each entry holds the per-key
// (win / draw / lose) probability triple plus the optional
// "win-keep" cap (max wins per `win_period` days) that gates how
// often a player can re-win the same (type, win_count) pair.
//
// Holds the win-date log inline on the entry (legacy m_vWinDate) so
// the cap can be evaluated cheaply on each MW_RPSGAME_ACK without a
// DB round-trip. Persistence to TRPSGAMERECORDTABLE is deferred;
// see handlers_rps.cpp's OnRpsGameRecordReq stub note.

#include <cstdint>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace tworldsvr {

struct TRpsGame
{
    std::uint8_t  type        = 0;
    std::uint8_t  win_count   = 0;
    std::uint8_t  win_prob    = 0;
    std::uint8_t  draw_prob   = 0;
    std::uint8_t  lose_prob   = 0;
    std::uint16_t win_keep    = 0;   // 0 = no cap
    std::uint16_t win_period  = 0;   // days

    // Recent win timestamps (Unix seconds) for the win-keep cap.
    // Maintained by RecordWin: entries older than 30 days are
    // pruned; entries within `win_period` days count against the
    // cap. Legacy m_vWinDate.
    std::vector<std::int64_t> win_dates;
};

class RpsRegistry
{
public:
    RpsRegistry() = default;
    RpsRegistry(const RpsRegistry&) = delete;
    RpsRegistry& operator=(const RpsRegistry&) = delete;

    // Result codes for RecordWin.
    enum class Outcome
    {
        kAllowed,      // Win recorded, player may receive the prize.
        kCapReached,   // win_keep cap hit within win_period — deny.
        kNotFound,     // No config for the (type, win_count) pair.
    };

    // One persistence side-effect from the RecordWin gate. The
    // legacy SendDM_RPSGAMERECORD_REQ takes a BYTE record flag
    // (TRUE=insert this win / FALSE=expire this old win) + the
    // affected (char_id, type, win_count, date). We surface them
    // to the caller (instead of firing the DB write inline) so the
    // handler can route them through the W3a-* CoOffloadIf
    // pattern when an IRpsRepository finally lands.
    struct PersistOp
    {
        bool          insert;     // true = add, false = remove
        std::uint32_t char_id;    // 0 when expiring an old entry
        std::uint8_t  type;
        std::uint8_t  win_count;
        std::int64_t  date_unix;
    };

    // Run the win-cap gate for the (type, win_count) game key:
    //   - prunes win_dates entries older than 30 days (emits a
    //     remove PersistOp per expired entry);
    //   - counts win_dates entries within win_period days;
    //   - if count >= win_keep (and win_keep > 0), returns
    //     kCapReached;
    //   - otherwise appends `now` to win_dates and emits an
    //     insert PersistOp.
    // When win_keep == 0 the cap doesn't apply — RecordWin
    // returns kAllowed without touching win_dates (legacy parity:
    // the entire `if(rps.m_wWinKeep)` block is skipped).
    Outcome RecordWin(std::uint8_t type, std::uint8_t win_count,
                      std::uint32_t char_id, std::int64_t now,
                      std::vector<PersistOp>& out_ops);

    // Admin update — overwrites the probability + cap fields on an
    // existing key. Legacy parity: unknown keys are skipped (the
    // legacy `if(it == end) continue;` silent-drop). Returns true
    // if the key existed (and was updated).
    bool Set(std::uint8_t type, std::uint8_t win_count,
             std::uint8_t win_prob, std::uint8_t draw_prob,
             std::uint8_t lose_prob, std::uint16_t win_keep,
             std::uint16_t win_period);

    // Seed a brand-new key (admin / boot). Legacy parity: returns
    // false if the key already exists. Tests + future boot-time
    // loader use this; the wire `OnCT_RPSGAMECHANGE_REQ` only
    // *updates* (Set), it doesn't insert.
    bool Insert(const TRpsGame& game);

    // Snapshot every config row for the GAMEDATA_ACK reply. Order
    // is unordered_map iteration order (legacy uses the same).
    std::vector<TRpsGame> Snapshot() const;
    std::size_t           Size() const;

private:
    static constexpr std::uint16_t Key(std::uint8_t type, std::uint8_t wc)
    { return static_cast<std::uint16_t>(type)
           | static_cast<std::uint16_t>(wc) << 8; }

    mutable std::shared_mutex m_lock;
    std::unordered_map<std::uint16_t, TRpsGame> m_games;
};

} // namespace tworldsvr
