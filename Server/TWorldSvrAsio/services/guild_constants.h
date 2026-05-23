#pragma once

// Guild-related enum constants mirroring NetCode.h. Single source
// of truth for the W3a-1+ guild handlers — handler files include
// this instead of redeclaring ad-hoc constexpr values (an earlier
// W3a-2/W3a-4 bug used positional guesses that didn't match the
// legacy enum order, leading to wire-incompatible reason codes
// that the round-trip tests didn't catch).
//
// All values trace back to Lib/Own/TProtocol/include/NetCode.h
// (enum TGUILD_RESULT, enum TGUILD_DUTY). Keep the // line refs
// in sync if NetCode.h ever reorders.

#include <cstdint>

namespace tworldsvr::guild {

// TGUILD_RESULT (NetCode.h:435). The relay/map peer treats these
// as the bResult byte in MW_GUILD*_REQ replies.
enum : std::uint8_t {
    kSuccess             = 0,   // GUILD_SUCCESS
    kJoinDeny            = 1,   // GUILD_JOIN_DENY
    kJoinBusy            = 2,   // GUILD_JOIN_BUSY
    kFail                = 3,   // GUILD_FAIL
    kAlreadyGuildName    = 4,
    kNotChief            = 5,
    kAlreadyMember       = 6,
    kNotMember           = 7,
    kHaveGuild           = 8,
    kNotFound            = 9,
    kEstablishErr        = 10,
    kDisorganizationErr  = 11,
    kLeaveSelf           = 12,  // GUILD_LEAVE_SELF
    kLeaveKick           = 13,  // GUILD_LEAVE_KICK
    kLeaveDisorganization = 14, // GUILD_LEAVE_DISORGANIZATION
    kJoinSuccess         = 15,
    kNoDuty              = 16,
    kMemberFull          = 17,
    kMismatchLevel       = 18,
    kSameGuildTactics    = 19,
    kNoMoney             = 20,
    kNoPoint             = 21,  // GUILD_NOPOINT
    kMaxWanted           = 22,
    kWantedEnd           = 23,
    kAlreadyApply        = 24,
    kSame                = 25,
};

// TGUILD_DUTY (NetCode.h:1981). Member rank within a guild.
// CHIEF is 2, not 1 — earlier W3a-1 code wrote `kGuildDutyChief =
// 1` which silently matched VICECHIEF on the wire.
enum : std::uint8_t {
    kDutyNone     = 0,  // GUILD_DUTY_NONE
    kDutyViceChief = 1, // GUILD_DUTY_VICECHIEF
    kDutyChief    = 2,  // GUILD_DUTY_CHIEF
};

// Constants the legacy handlers read off m_pGuild. Used for the
// fame-change cost check and other point-budget gates.
constexpr std::uint32_t kPvPointCostFameChange = 30000;

// ASK_YES / ASK_NO (NetCode.h:222) — generic accept/decline byte
// used by the guild invite answer flow + future party invite.
constexpr std::uint8_t kAskYes = 0;
constexpr std::uint8_t kAskNo  = 1;

// Article board caps (NetCode.h:27, 29, 75). Enforced by
// OnGuildArticleAdd / OnGuildArticleUpdate handlers; oversized
// payloads get silent-dropped to match legacy parity.
constexpr std::size_t  kMaxBoardTitle        = 256;
constexpr std::size_t  kMaxBoardText         = 2048;
constexpr std::uint8_t kMaxGuildArticleCount = 100;

// GUILDWANTED_PERIOD from TWorldType.h: how long a "we are
// recruiting" entry stays in the wanted-board before expiring.
// 14 days = 1 209 600 seconds. The legacy timer-thread sweeps
// expired entries via SM_EVENTEXPIRED_ACK fan-out; the modern
// equivalent (a periodic prune coroutine) lands with the W7+
// scheduler work.
constexpr std::int64_t kGuildWantedPeriodSec = 60LL * 60 * 24 * 14;

// Volunteer-application kind discriminator. The legacy
// TGUILDVOLUNTEERTABLE row carries a `bType` column with two
// values: GUILDAPP_MEMBER for ordinary guild applicants (the
// W3a-12 flow), GUILDAPP_TACTICS for tactics-alliance
// applicants (the deferred W3a-* tactics subsystem). The
// constants aren't exported from a legacy header — the source
// just uses raw 0/1 throughout — so we name them here.
constexpr std::uint8_t kVolunteerKindMember  = 0;
constexpr std::uint8_t kVolunteerKindTactics = 1;

// W3a-21 PvP event-point bucket count. The legacy
// CSPSaveGuildPvPRecord persists `dwPoint_1..dwPoint_8` (one
// DWORD per PVPE_* bucket — see TGUILDPVPRECORDTABLE schema +
// PvPEvent enum). Wire packet (DM_PVPRECORD_REQ) carries the
// same 8 DWORDs per row.
constexpr std::size_t kPvPEventCount = 8;

// W3a-28 PvP record per-day history constants. Legacy
// `dwDate = m_timeCurrent / DAY_ONE` where DAY_ONE = 86 400
// (seconds in one day). CalcWeekRecord keeps any vRecord row
// whose `day_index + kPvPRecordWindowDays > today_day_index`,
// so the window is the last 7 distinct day-buckets inclusive
// of today.
constexpr std::int64_t kDaySec               = 86'400;
constexpr std::int64_t kPvPRecordWindowDays  = 7;

// W3a-29 PvP-point gain/use fan-in constants.
// `bOwnerType` discriminator (NetCode.h:2250) — TOWNER_CHAR
// relays to the char's map peer, TOWNER_GUILD applies the
// delta to the guild's PvP-point banks.
constexpr std::uint8_t kPvPOwnerChar  = 0;   // TOWNER_CHAR
constexpr std::uint8_t kPvPOwnerGuild = 1;   // TOWNER_GUILD
// `bType` bitmask (NetCode.h:122) — which PvP-point bank(s)
// the delta touches. GainPvPoint also bumps month_point
// whenever TOTAL is set; UsePvPoint never decrements month.
constexpr std::uint8_t kPvPMaskTotal   = 1;  // PVP_TOTAL
constexpr std::uint8_t kPvPMaskUseable = 2;  // PVP_USEABLE

// W3a-29 — max entries kept in TGuild.point_log. Legacy
// CTGuild::PointLog (TGuild.cpp:603) inserts newest-first and
// pop_back()s once size exceeds 50 (matching the read-side
// CTBLGuildPvPointReward SELECT TOP 50).
constexpr std::size_t kPointLogMaxEntries = 50;

// Max guild-name length in bytes — matches the legacy MAX_NAME
// ceiling (50 bytes for the ANSI build the original server runs).
// The legacy validator at SSHandler.cpp:3056 just compares
// `GetLength() > MAX_NAME` and drops the request on overflow;
// we mirror that as a kGuildNameError reply.
constexpr std::size_t kGuildMaxNameLen = 50;

} // namespace tworldsvr::guild
