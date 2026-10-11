#pragma once
#include "LegacyEventInfo.h"
#include <array>
#include <cstdint>
#include <string>

namespace tmapsvr {
// Source m_wEventValue is a Map-process projection of World messages, not a
// historical database table. Snapshot on the Map reactor before DB offload:
// later event updates cannot change an already accepted economy operation.
struct WorldEventSnapshot {
    std::uint64_t connection{},revision{};
    std::array<std::uint16_t,fourstory::protocol::kEventCount> values{};
    std::string Json() const {
        std::string out="["+std::to_string(connection)+","+std::to_string(revision)+",[";
        for(std::size_t i=0;i<values.size();++i){if(i)out+=',';out+=std::to_string(values[i]);}
        return out+"]]";
    }
};
}
