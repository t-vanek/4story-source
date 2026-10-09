#pragma once
#include "domain/inventory.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace tmapsvr {
struct SkillCastRequest {
    std::uint16_t skill{};
    bool loop{};
    std::uint64_t elapsed_ms{}; // actual interval between staged timer samples
    std::vector<std::byte> request,acknowledgement;
    std::vector<SkillItemDebit> debits;
};
struct SkillCastCommit {
    std::shared_ptr<const CharSnapshot> snapshot;
    std::vector<EquipmentEffectEvent> ended; // original erase order before cast ACK
};
}
