#pragma once
#include <cstdint>
#include <string>

namespace tmapsvr {
// Account-owned state. Unlock belongs to the non-recycled Login session key;
// revision and wall-clock deadline never extend the original client protocol.
struct SecureCodeState {
    bool hydrated{},created{},disabled{},unlocked{};
    std::uint8_t tries{};
    std::uint32_t source_tick{};
    std::uint64_t revision{};
    std::int64_t deadline_ms{};
    std::string code;
    bool operator==(const SecureCodeState&) const = default;
    bool Blocks() const {return created&&!disabled&&!unlocked;}
};
}
