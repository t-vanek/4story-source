#pragma once

// LoadTournamentState — the boot-time hydrate (TWorldSvr.cpp:
// 1574-1889 minus the TVIEW_TOURNAMENTPLAYER reload, which lands
// with the player vertical): limited level, the persisted current
// step, the main tournament (schedule + entries + rewards +
// first-group count), every operator lucky event (time + schedule +
// entries + rewards), the resume base recompute, and the initial
// schedule election (the legacy timer-boot TournamentUpdate()).
//
// Synchronous by design — runs before the listener accepts traffic,
// exactly like the other boot warms in main.cpp. Factored out of
// main so the wire test can drive it against the fake repository.

#include "tournament_registry.h"
#include "tournament_repository.h"

#include <cstdint>
#include <functional>

namespace tworldsvr {

// IsFirstGroup source (legacy m_arFirstGradeGroup scan,
// TWorldSvr.cpp:5645) — the fame first-grade array lives in
// MonthRankRegistry; main passes a closure over it so this stays
// service-decoupled. Empty function = nobody is first-grade (the
// port's fame table starts empty at boot until the first rollover).
using TnmtFirstGroupFn =
    std::function<bool(std::uint8_t country, std::uint32_t char_id)>;

void LoadTournamentState(TournamentRegistry&    registry,
                         ITournamentRepository& repo,
                         std::int64_t           now,
                         const TnmtFirstGroupFn& is_first_group = {});

} // namespace tworldsvr
