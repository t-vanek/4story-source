#pragma once
#include <cstdint>
#include <vector>

namespace tmapsvr {
struct CommunicationAuthority {
    bool is_operator{};
    bool protected_sender{};
    std::int64_t chat_ban_time{};
};
struct ChatObservation {std::uint32_t tick{},size{};};
// Source CheckChat is process-local and is not part of RELEASEMAIN's graph.
struct ChatFloodState {
    std::uint32_t recent{},size{};
    std::vector<ChatObservation> observations;
};
}
