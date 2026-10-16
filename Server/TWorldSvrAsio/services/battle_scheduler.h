#pragma once
#include "battle_constants.h"
#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace tworldsvr {
struct BattleClock {
    std::int64_t second{};
    std::uint32_t day_second{};
    std::uint8_t weekday{}; // original CTime: Sunday1 .. Saturday7
};
struct BattleTime {
    std::uint8_t type{},status{},day{},week{};
    std::uint32_t duration{},start{},alarm_start{},alarm_end{},peace{},left{};
    bool running{},loaded{};
};
struct BattleSignal {std::uint8_t type{},status{};std::uint32_t start{},second{};};
class BattleScheduler {
public:
    // Original global m_battletime array exists even without a chart row.
    // Static storage zeros plus tagBATTLETIME's constructor are explicit here;
    // loaded distinguishes those objects from actual database definitions.
    void Load(const std::vector<BattleTime>&,std::vector<std::uint32_t>,const BattleClock&);
    std::optional<BattleSignal> Tick(const BattleClock&);
    const BattleTime& Castle() const {return m_times[battle::kTypeCastle];}
private:
    bool m_loaded{};
    std::array<BattleTime,battle::kTypeCount> m_times{};
    std::vector<std::uint32_t> m_missions;
};
}
