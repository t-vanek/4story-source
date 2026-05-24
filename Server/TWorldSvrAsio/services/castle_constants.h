#pragma once

// castle_constants — result codes for the W5 castle-war apply flow.
//
// CBS_RESULT (the legacy castle-build/siege result enum) is
// referenced by the client (CSHandler.cpp) and the map server but is
// **not defined anywhere in this source tree**, so the numeric values
// here are reconstructed:
//   - kSuccess = 0 is certain: the client tests `bResult != CBS_SUCCESS`
//     as the not-an-error sentinel, and every other *_SUCCESS in the
//     codebase is 0.
//   - the error codes are ordered to match the client's switch
//     (CSHandler.cpp:13563): FULL / NOTFOUND / NOTREADY / CANTAPPLY.
//     Their exact values are inferred (first-after-success ordering);
//     only kFull is currently emitted by the world (the 49-applicant
//     cap), and only on the rare cap-hit error toast. Verify against
//     the real enum if it surfaces.

#include <cstdint>

namespace tworldsvr::castle {

inline constexpr std::uint8_t kSuccess  = 0; // CBS_SUCCESS (certain)
inline constexpr std::uint8_t kFull     = 1; // CBS_FULL (inferred)
inline constexpr std::uint8_t kNotFound = 2; // CBS_NOTFOUND (inferred)
inline constexpr std::uint8_t kNotReady = 3; // CBS_NOTREADY (inferred)
inline constexpr std::uint8_t kCantApply = 4; // CBS_CANTAPPLY (inferred)

// Per-castle applicant cap (legacy CTGuild::CanApplyWar literal).
inline constexpr std::uint16_t kMaxApplicant = 49;

} // namespace tworldsvr::castle
