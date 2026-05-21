#pragma once

// Helpers shared by the SOCI service implementations:
//
//   1. Narrow casts from int32 (SOCI's preferred bind type for
//      tinyint/smallint columns on most ODBC drivers) to the typed
//      domain fields (uint8 / uint16 / uint32). Wrapping the cast
//      in a named function makes the row-mapping site easier to
//      grep when a column changes type.
//
//   2. Null-safe string extraction. ODBC + SOCI hand back a
//      soci::indicator alongside the value; treating i_null as an
//      empty string matches the legacy CTBLChar / CTBLNpc fetch
//      semantics ("missing name = blank") so the row mapper code
//      doesn't need to branch.

#include <soci/soci.h>

#include <cstdint>
#include <string>

namespace tmapsvr::db {

inline std::uint8_t  Narrow8 (std::int32_t v) noexcept { return static_cast<std::uint8_t> (v); }
inline std::uint16_t Narrow16(std::int32_t v) noexcept { return static_cast<std::uint16_t>(v); }
inline std::uint32_t Narrow32(std::int32_t v) noexcept { return static_cast<std::uint32_t>(v); }
inline std::int64_t  Sign64  (std::int64_t v) noexcept { return v; }
inline float         NarrowF (double v)       noexcept { return static_cast<float>(v); }

// Null-safe string. Returns empty string when the indicator reports
// the column was NULL; otherwise returns the value untouched.
inline std::string SafeString(const std::string& value,
                              soci::indicator   ind)
{
    return (ind == soci::i_ok) ? value : std::string{};
}

} // namespace tmapsvr::db
