#pragma once

// friend_constants — wire enums for the friend subsystem, mirroring
// Lib/Own/TProtocol/include/NetCode.h. Same single-source-of-truth
// rationale as guild_/party_/corps_constants.h.

#include <cstdint>

namespace tworldsvr::frnd {

// FRIEND_RESULT (NetCode.h:534) — the result byte of
// MW_FRIENDADD_REQ / MW_FRIENDERASE_REQ.
inline constexpr std::uint8_t kSuccess  = 0; // FRIEND_SUCCESS
inline constexpr std::uint8_t kRefuse   = 1; // FRIEND_REFUSE
inline constexpr std::uint8_t kBusy     = 2; // FRINED_BUSY
inline constexpr std::uint8_t kNotFound = 3; // FRIEND_NOTFOUND
inline constexpr std::uint8_t kAlready  = 4; // FRIEND_ALREADY
inline constexpr std::uint8_t kMax      = 5; // FRIEND_MAX

// FRIEND_TYPE (NetCode.h:544) — m_bType of a friend-list entry.
inline constexpr std::uint8_t kTypeFriend       = 0; // FT_FRIEND
inline constexpr std::uint8_t kTypeTarget       = 1; // FT_TARGET (pending)
inline constexpr std::uint8_t kTypeFriendFriend = 2; // FT_FRIENDFRIEND (mutual)

// FRIEND_CONNECTION (NetCode.h:551).
inline constexpr std::uint8_t kConnection    = 0; // FRIEND_CONNECTION
inline constexpr std::uint8_t kDisconnection = 1; // FRIEND_DISCONNECTION

// MAX_FRIEND / MAX_FRIENDGROUP (TWorldType.h:146).
inline constexpr std::uint8_t kMaxFriend      = 64;
inline constexpr std::uint8_t kMaxFriendGroup = 5;

} // namespace tworldsvr::frnd
