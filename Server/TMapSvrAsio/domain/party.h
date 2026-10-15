#pragma once
#include <string>
#include <cstdint>

namespace tmapsvr {
// Read-only native actor authority. The World owns transient membership;
// neither a local zero party nor a completed transport write creates a party.
struct PartyAuthority {
    std::string name;
    bool action_blocked{},protected_inviter{};
};
// Process-local projection proof only. This records the World-supplied Map
// cache; it is not a World registry membership or PostgreSQL authority row.
struct PartyProjection {
    std::uint16_t id{},commander{};
    std::uint32_t chief{};
    std::uint8_t type{};
    bool hydrated{};
    bool operator==(const PartyProjection&) const = default;
};
}
