#pragma once

// chat_constants — CHAT_GROUP / CHAT_TYPE wire enums (NetCode.h:2009)
// + the peace-country sentinel, for the cross-shard chat relay.

#include <cstdint>

namespace tworldsvr::chat {

// CHAT_GROUP — the channel a message is addressed to.
inline constexpr std::uint8_t kWhisper  = 0; // direct (or operator)
inline constexpr std::uint8_t kNear     = 1; // map-local (handled on map)
inline constexpr std::uint8_t kMap      = 2;
inline constexpr std::uint8_t kWorld    = 3;
inline constexpr std::uint8_t kParty    = 4;
inline constexpr std::uint8_t kGuild    = 5;
inline constexpr std::uint8_t kForce    = 6; // corps
inline constexpr std::uint8_t kOperator = 7;
inline constexpr std::uint8_t kTactics  = 8; // guild + tactics members
inline constexpr std::uint8_t kShow     = 9; // system/announce

// CHAT_TYPE.
inline constexpr std::uint8_t kTypeNormal = 0; // CHAT_NOMAL
inline constexpr std::uint8_t kTypeOp     = 1; // CHAT_OP

// TCONTRY_PEACE (NetCode.h:1097) — the whisper war-country gate is
// waived when either party is in the peace country.
inline constexpr std::uint8_t kCountryPeace = 4;

} // namespace tworldsvr::chat
