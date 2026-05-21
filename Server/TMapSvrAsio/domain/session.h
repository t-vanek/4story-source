#pragma once

// Session-row domain types. These describe what the F4 handshake
// reads back from TCURRENTUSER — no service-interface dependency
// here, so tests and audit emitters can include just the POD.

#include <cstdint>
#include <string>

namespace tmapsvr {

// Subset of TCURRENTUSER columns the F4 handshake reads back.
// Populated by IMapSessionValidator::LookupSession.
struct MapSessionInfo
{
    std::uint32_t  dwUserID  = 0;
    std::uint32_t  dwKEY     = 0;
    std::uint8_t   bGroupID  = 0;
    std::uint8_t   bChannel  = 0;
    std::string    szLoginIP;
    bool           bLocked   = false;
};

} // namespace tmapsvr
