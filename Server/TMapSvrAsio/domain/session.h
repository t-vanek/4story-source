#pragma once

// Session-row domain types. These describe what the F4 handshake
// reads back from TCURRENTUSER — no service-interface dependency
// here, so tests and audit emitters can include just the POD.

#include <cstdint>
#include <string>

namespace tmapsvr {

enum class MapSessionRole { Primary, Replica };

// Subset of TCURRENTUSER columns the F4 handshake reads back.
// Populated by IMapSessionValidator::LookupSession.
struct MapSessionClaim {
    std::uint32_t user_id{}, key{}, char_id{};
    std::uint8_t group{}, channel{};
    std::uint64_t connection_id{};
    MapSessionRole role{MapSessionRole::Primary};
    std::uint32_t endpoint_ip{};
    std::uint16_t endpoint_port{};
    std::uint64_t authority_epoch{}; // primary tenure, independent of socket generation
};

struct MapSessionInfo
{
    std::uint32_t  dwUserID  = 0;
    std::uint32_t  dwKEY     = 0;
    std::uint8_t   bGroupID  = 0;
    std::uint8_t   bChannel  = 0;
    std::string    szLoginIP;
    std::uint32_t  dwCharID = 0;
    bool           bLocked   = false;
    MapSessionRole role{MapSessionRole::Primary};
};

} // namespace tmapsvr
