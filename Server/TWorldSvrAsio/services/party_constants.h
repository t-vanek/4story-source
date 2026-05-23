#pragma once

// party_constants — single source of truth for the party-subsystem
// wire enums, mirroring the legacy values in
// Lib/Own/TProtocol/include/NetCode.h. The guild family keeps its
// own guild_constants.h for the same reason: handlers + senders +
// tests must agree on the exact byte values the client expects, and
// inlining magic numbers per-handler is how the pre-port code grew
// silent mismatches.

#include <cstdint>

namespace tworldsvr::party {

// TPARTY_RESULT (NetCode.h:390). The `bResult` byte of
// MW_PARTYADD_REQ. PARTY_AGREE doubles as "the target's client
// should pop the join dialog"; the rest are terminal failures the
// requester's client surfaces as a toast.
inline constexpr std::uint8_t kAgree     = 0;  // PARTY_AGREE
inline constexpr std::uint8_t kDeny      = 1;  // PARTY_DENY
inline constexpr std::uint8_t kBusy      = 2;  // PARTY_BUSY (arena)
inline constexpr std::uint8_t kNoUser    = 3;  // PARTY_NOUSER
inline constexpr std::uint8_t kNoReqUser = 4;  // PARTY_NOREQUSER
inline constexpr std::uint8_t kWaiters   = 5;  // PARTY_WAITERS
inline constexpr std::uint8_t kAlready   = 6;  // PARTY_ALREADY
inline constexpr std::uint8_t kFull      = 7;  // PARTY_FULL
inline constexpr std::uint8_t kNotChief  = 8;  // PARTY_NOTCHIEF
inline constexpr std::uint8_t kNoParty   = 9;  // PARTY_NOPARTY
inline constexpr std::uint8_t kChgChief  = 10; // PARTY_CHGCHIEF
inline constexpr std::uint8_t kCountry   = 11; // PARTY_COUNTRY

// PARTY_TYPE (NetCode.h:1960) — the loot-distribution mode carried
// on a party (m_bObtainType). Echoed through the ADD/JOIN flow.
inline constexpr std::uint8_t kObtainFree    = 0; // PT_FREE
inline constexpr std::uint8_t kObtainSolo    = 1; // PT_SOLO
inline constexpr std::uint8_t kObtainHunter  = 2; // PT_HUNTER
inline constexpr std::uint8_t kObtainLottery = 3; // PT_LOTTERY
inline constexpr std::uint8_t kObtainChief   = 4; // PT_CHIEF
inline constexpr std::uint8_t kObtainOrder   = 5; // PT_ORDER

// MAX_PARTY_MEMBER (TParty.h:3).
inline constexpr std::uint8_t kMaxPartyMember = 7;

// ASK_YES (NetCode.h:222) — the accept value of the join dialog
// answer byte (used by the W3b-2 PARTYJOIN flow).
inline constexpr std::uint8_t kAskYes = 0;

// TCONTRY_N (NetCode.h:1096) — the "neutral" country. War-country
// resolution (legacy GetWarCountry): a char's effective faction is
// its aid_country unless that is neutral, in which case it falls
// back to its home country.
inline constexpr std::uint8_t kCountryNeutral = 3;

// Effective war-country for matchmaking gates (legacy
// CTWorldSvrModule::GetWarCountry, TWorldSvr.cpp:7714).
inline constexpr std::uint8_t WarCountry(std::uint8_t country,
                                         std::uint8_t aid_country)
{
    return aid_country != kCountryNeutral ? aid_country : country;
}

} // namespace tworldsvr::party
