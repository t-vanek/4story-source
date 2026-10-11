#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace tmapsvr {
// Original LOADCHAR/RELEASEMAIN fields. Remaining is seconds; skill timers
// are milliseconds. Expiry is a calendar instant, not a process-local tick.
struct DuringItem {
    std::uint16_t item{};
    std::uint8_t type{};
    std::uint32_t remaining{};
    std::int64_t expires{};
};
struct DuringItemState {
    std::vector<DuringItem> items;
    // Exact normalized storage observed on load/last mutation. Hydration may
    // reinterpret DAY/TIME from the pinned template at EnterMAP, never in a save.
    std::string durable_json;
    std::int64_t entered_at{};
    bool pending_entry=true;
};
}
