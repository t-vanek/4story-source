#pragma once
#include "native_monster_presentation.h"
#include <optional>
#include <span>

namespace tmapsvr {
struct NativeMonsterMoveInput {
    std::uint32_t id{};
    std::uint8_t type{},channel{};
    std::uint16_t map{}; // source decodes these route fields but never uses them for lookup
    NativeMonsterMotion motion;
};
// Complete count/size/finite validation precedes any owned actor mutation.
// A valid zero-count request is distinct from malformed input.
std::optional<std::vector<NativeMonsterMoveInput>> DecodeNativeMonMoveRequest(std::span<const std::byte>);
} // namespace tmapsvr
