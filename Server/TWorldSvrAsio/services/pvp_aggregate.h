#pragma once

// W3a-28 — CalcWeekRecord helper. Re-derives a member's rolling
// 7-day PvP outcome aggregate from their per-day vRecord history
// AND trims rows older than the kPvPRecordWindowDays window in
// the same pass (legacy parity: TGuild.cpp:615 walks the vector
// once, erases stale rows in-place, and accumulates the kept
// rows into weekrecord).
//
// Idempotent: re-calling with the same `today_day_index` is a
// no-op on already-trimmed input. Caller must hold the parent
// guild's lock — operates directly on the member by reference.

#include "services/guild_constants.h"
#include "services/guild_registry.h"

#include <cstdint>

namespace tworldsvr {

void CalcWeekRecord(TGuildMember& member, std::int64_t today_day_index);

// Convenience overload — samples std::time(nullptr) / kDaySec
// for today_day_index. Used by the war-result fan-in path.
void CalcWeekRecord(TGuildMember& member);

} // namespace tworldsvr
