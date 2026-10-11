#pragma once
#include <cstdint>
#include <string>

namespace tmapsvr {
// Source DWORD balances from TGetPvPRecord / TGetMonthPvPoint. This does
// not substitute for the separate rank, class-record and reward contracts.
struct PvpBalance {
    std::uint32_t total{},available{},month{};
    bool operator==(const PvpBalance&) const = default;
    std::string Json() const {
        return "["+std::to_string(total)+","+std::to_string(available)+","+std::to_string(month)+"]";
    }
};
}
