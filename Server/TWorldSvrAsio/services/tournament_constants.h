#pragma once

// Tournament enums from NetCode.h — TOURNAMENT_STEP (2529),
// TOURNAMENT_ENTRY_TYPE (2554), TOURNAMENT_WIN (2567), TMATCH_TYPE
// (2574), TOURNAMENT_EVENT_TYPE (2591) — plus the two #define'd
// scalars (NetCode.h:135-136). Only the values the W6-50 slice
// routes on are named; the step ladder is surfaced whole because
// the schedule math keys on it.

#include <cstdint>

namespace tworldsvr::tournament {

// TOURNAMENT_STEP (NetCode.h:2529). The MAKEWORD(step, group) pair
// is the schedule-map key; step order is the ladder order.
constexpr std::uint8_t kStepReady   = 0;   // TNMTSTEP_READY
constexpr std::uint8_t kStep1st     = 1;   // TNMTSTEP_1st
constexpr std::uint8_t kStepNormal  = 2;   // TNMTSTEP_NORMAL
constexpr std::uint8_t kStepParty   = 3;   // TNMTSTEP_PARTY
constexpr std::uint8_t kStepMatch   = 4;   // TNMTSTEP_MATCH
constexpr std::uint8_t kStepEnter   = 5;   // TNMTSTEP_ENTER
constexpr std::uint8_t kStepQFinal  = 6;   // TNMTSTEP_QFINAL
constexpr std::uint8_t kStepQFEnd   = 7;   // TNMTSTEP_QFEND
constexpr std::uint8_t kStepSFEnter = 8;   // TNMTSTEP_SFENTER
constexpr std::uint8_t kStepSFinal  = 9;   // TNMTSTEP_SFINAL
constexpr std::uint8_t kStepSFEnd   = 10;  // TNMTSTEP_SFEND
constexpr std::uint8_t kStepFEnter  = 11;  // TNMTSTEP_FENTER
constexpr std::uint8_t kStepFinal   = 12;  // TNMTSTEP_FINAL
constexpr std::uint8_t kStepEnd     = 13;  // TNMTSTEP_END

// TOURNAMENT_EVENT_TYPE (NetCode.h:2591) — CT_TOURNAMENTEVENT op.
constexpr std::uint8_t kTetNone        = 0;
constexpr std::uint8_t kTetList        = 1;
constexpr std::uint8_t kTetScheduleAdd = 2;
constexpr std::uint8_t kTetScheduleDel = 3;
constexpr std::uint8_t kTetEntryAdd    = 4;
constexpr std::uint8_t kTetEntryDel    = 5;
constexpr std::uint8_t kTetPlayerAdd   = 6;
constexpr std::uint8_t kTetPlayerDel   = 7;
constexpr std::uint8_t kTetPlayerEnd   = 8;

// TOURNAMENT_ENTRY_TYPE (NetCode.h:2554).
constexpr std::uint8_t kEntryParty   = 1;
constexpr std::uint8_t kEntryPrivate = 2;

// TMATCH_TYPE (NetCode.h:2574) — index into TnmtPlayer::result.
constexpr std::uint8_t kMatchQFinal = 0;
constexpr std::uint8_t kMatchSFinal = 1;
constexpr std::uint8_t kMatchFinal  = 2;
constexpr std::uint8_t kMatchCount  = 3;

// TOURNAMENT_WIN (NetCode.h:2567).
constexpr std::uint8_t kWinNone = 0;
constexpr std::uint8_t kWinWin  = 1;
constexpr std::uint8_t kWinLose = 2;

// NetCode.h:135-136.
constexpr std::uint8_t kTournamentSlot      = 8;    // TOURNAMENT_SLOT
constexpr std::uint8_t kTournamentBasePrize = 100;  // TOURNAMENT_BASEPRIZE

// TET_PLAYERADD error-echo fillers (SSHandler.cpp:12204-12205 /
// 12607-12610): TCLASS_COUNT / TCONTRY_N from NetCode.h:1091+.
constexpr std::uint8_t kClassCount = 6;   // TCLASS_COUNT
constexpr std::uint8_t kCountryB   = 2;   // TCONTRY_B (last playable)
constexpr std::uint8_t kCountryN   = 3;   // TCONTRY_N

// TOURNAMENT_RESULT (NetCode.h:2530 region, values confirmed at
// NetCode.h:830-843).
constexpr std::uint8_t kResultSuccess    = 0;
constexpr std::uint8_t kResultDisqualify = 1;
constexpr std::uint8_t kResultTimeout    = 2;
constexpr std::uint8_t kResultAlreadyReg = 3;
constexpr std::uint8_t kResultNotFound   = 4;
constexpr std::uint8_t kResultFull       = 5;
constexpr std::uint8_t kResultClass      = 6;
constexpr std::uint8_t kResultMoney      = 7;
constexpr std::uint8_t kResultItem       = 8;
constexpr std::uint8_t kResultLevel      = 9;
constexpr std::uint8_t kResultFail       = 10;

// Legacy m_bFirstGroupCount cap (NetCode.h:130).
constexpr std::uint8_t kFirstGroupCap = 17;  // FIRSTGRADEGROUPCOUNT

// The main (non-event) tournament's fixed schedule id — legacy boot
// passes wTourID=1 into SetTournamentTime for the TTOURNAMENTCHART
// tournament; ids above it are operator-created lucky events.
constexpr std::uint16_t kMainTournamentId = 1;

} // namespace tworldsvr::tournament
