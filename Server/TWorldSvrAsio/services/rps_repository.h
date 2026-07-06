#pragma once

// IRpsRepository — DB surface for the RPS event game (W6-49):
// TRPSGAMECHART config load + TRPSGAMERECORDTABLE win-date hydrate
// (legacy CTBLRPSGame / CTBLRPSGameRecord boot queries,
// TWorldSvr.cpp:844) and the TRPSGameRecord SP the win-cap ledger
// persists through (legacy OnDM_RPSGAMERECORD_REQ,
// SSHandler.cpp:13232).

#include "rps_registry.h"

#include <cstdint>
#include <vector>

namespace tworldsvr {

struct RpsWinDateRow
{
    std::uint8_t  type      = 0;
    std::uint8_t  win_count = 0;
    std::int64_t  date_unix = 0;
};

class IRpsRepository
{
public:
    virtual ~IRpsRepository() = default;

    // TRPSGAMECHART rows (win_dates left empty).
    virtual std::vector<TRpsGame> LoadGames() = 0;

    // TRPSGAMERECORDTABLE rows (char id is not needed by the
    // in-memory ledger — legacy discards it too).
    virtual std::vector<RpsWinDateRow> LoadWinDates() = 0;

    // TRPSGameRecord(record, char_id, type, win_count, date) —
    // record=true inserts a win row, false deletes the expired one.
    virtual bool Record(bool insert, std::uint32_t char_id,
                        std::uint8_t type, std::uint8_t win_count,
                        std::int64_t date_unix) = 0;
};

} // namespace tworldsvr
