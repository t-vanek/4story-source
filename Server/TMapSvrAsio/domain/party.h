#pragma once
#include <string>

namespace tmapsvr {
// Read-only native actor authority. The World owns transient membership;
// neither a local zero party nor a completed transport write creates a party.
struct PartyAuthority {
    std::string name;
    bool action_blocked{},protected_inviter{};
};
}
