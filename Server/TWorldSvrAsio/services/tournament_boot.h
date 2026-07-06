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

namespace tworldsvr {

void LoadTournamentState(TournamentRegistry&    registry,
                         ITournamentRepository& repo,
                         std::int64_t           now);

} // namespace tworldsvr
