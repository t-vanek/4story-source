#pragma once
#include <array>
#include <cstdint>
#include <string>

namespace tmapsvr {
// CTPlayer constructor-local state. Original LOADCHAR/RELEASEMAIN never sends
// either counter; a target object starts at0 and an existing object keeps its own.
struct SourceRpsState {
    std::uint8_t type{},wins{};
    // Modern authority repair: one outstanding World decision per local actor
    // tenure. No nonce is added to the source protocol.
    bool pending{};
    std::uint8_t card{};
    std::uint64_t epoch{},connection{};
    bool operator==(const SourceRpsState&) const = default;
    void ClearPending(){pending=false;card=0;epoch=connection=0;}
    std::string Json() const {
        return '['+std::to_string(type)+','+std::to_string(wins)+','+
            std::to_string(pending?1:0)+','+std::to_string(card)+','+
            std::to_string(epoch)+','+std::to_string(connection)+']';
    }
};
struct NativeRpsGame {
    std::uint8_t type{},wins{};
    std::uint16_t entry_item{},win_keep{},win_period{};
    std::uint32_t money{};
    std::array<std::uint16_t,2> reward_item{};
    std::array<std::uint8_t,2> reward_count{};
    std::array<std::uint8_t,3> probability{};
    std::string Json() const {
        return '['+std::to_string(type)+','+std::to_string(wins)+','+std::to_string(entry_item)+','+
            std::to_string(money)+','+std::to_string(reward_item[0])+','+std::to_string(reward_item[1])+','+
            std::to_string(reward_count[0])+','+std::to_string(reward_count[1])+','+
            std::to_string(probability[0])+','+std::to_string(probability[1])+','+std::to_string(probability[2])+','+
            std::to_string(win_keep)+','+std::to_string(win_period)+']';
    }
};
}
