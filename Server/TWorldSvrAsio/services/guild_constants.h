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

} // namespace tworldsvr::guild
