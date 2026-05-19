#pragma once

// Standalone Nation enum, separated from login_server.h so utility
// modules (e.g. services/charname_validator) can depend on it without
// pulling in Boost.Asio / TNetLib transitively.

#include <cstdint>

namespace tloginsvr {

// Region/locale switch — controls per-deployment quirks:
//   * JP appends a `BYTE bChanneling` to the CS_LOGIN_REQ tail
//     (legacy CSHandler.cpp:173). It also dispatches to a different
//     stored proc on the legacy side (CSPLoginJP vs CSPLogin); the
//     modern server passes the channeling byte through AuthRequest
//     so the SOCI impl can branch.
//   * CheckCharName in CS_CREATECHAR_REQ honors the nation-specific
//     character set the legacy server picked from m_bNation
//     (CSHandler.cpp:1010-1066). See ICharService::Create.
//
// US == "ASCII only" is the safe default if the operator forgets to
// set it; it matches the legacy non-localized build. Values match the
// legacy NATION_* constants so SOCI impls that need to write the
// nation column don't need a translation table.
enum class Nation : std::uint8_t
{
    US      = 3,  // legacy NATION_US
    Germany = 1,  // NATION_GERMAN
    Taiwan  = 2,  // NATION_TAIWAN
    Japan   = 4,  // NATION_JAPAN
    Korea   = 5,  // NATION_KOREA
    Russia  = 6,  // NATION_RUSSIA
};

} // namespace tloginsvr
