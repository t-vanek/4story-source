#pragma once

// Original Control inventory types, directly from linked CTProtocol.h:26–35.
// TSVRTYPE / TSERVER rows retain these raw IDs; never remap archival metadata
// to an inferred installer order. NetCode's TSVR_TYPE peer roles are a separate
// namespace (notably Relay5 there versus Control inventory Relay8 here).
// BOW/BR are Map modes, not additional invented Control inventory types.

#include <cstdint>

namespace tcontrolsvr {

namespace svr_type {

constexpr std::uint8_t kNull     = 0;
constexpr std::uint8_t kCtlSvr   = 1;
constexpr std::uint8_t kLoginSvr = 2;
constexpr std::uint8_t kWorldSvr = 3;
constexpr std::uint8_t kMapSvr   = 4;
constexpr std::uint8_t kPatchSvr = 5;
constexpr std::uint8_t kFtpSvr   = 6;   // excluded from source inventory load
constexpr std::uint8_t kLogSvr   = 7;
constexpr std::uint8_t kRlySvr   = 8;
constexpr std::uint8_t kPreFtpSvr= 9;

} // namespace svr_type

} // namespace tcontrolsvr
