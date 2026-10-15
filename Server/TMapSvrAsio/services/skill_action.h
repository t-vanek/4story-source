#pragma once
#include "domain/inventory.h"
#include "services/client_senders.h"
#include "wire_codec.h"
#include <stdexcept>
#include <span>

namespace tmapsvr {
struct SkillActionRequest {
    std::uint32_t object{},act{},animation{};
    std::uint8_t type{},action{},channel{};
    std::uint16_t map{},skill{};
};
inline SkillActionRequest DecodeSkillAction(std::span<const std::byte> bytes) {
    SkillActionRequest out;wire::Reader r(bytes.data(),bytes.size());
    if(!r.Read(out.object)||!r.Read(out.type)||!r.Read(out.action)||!r.Read(out.act)||
       !r.Read(out.animation)||!r.Read(out.channel)||!r.Read(out.map)||!r.Read(out.skill)||!r.Eof())
        throw std::runtime_error("Invalid ACTION request length");
    return out;
}
struct SkillActionCommit {
    std::shared_ptr<const CharSnapshot> snapshot;
    std::vector<std::byte> acknowledgement;
    std::vector<EquipmentEffectEvent> ended;
};
}
