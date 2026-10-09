#pragma once
#include "domain/main_transfer.h"
#include <optional>
#include <span>

namespace tmapsvr::transfer {
// Throws invalid_argument for unsupported flags/invalid floats or a body that
// exceeds the original 16-bit SS frame. Neither function changes wire fields.
std::vector<std::byte> Encode(const State&);
std::optional<State> Decode(std::span<const std::byte>);
}
