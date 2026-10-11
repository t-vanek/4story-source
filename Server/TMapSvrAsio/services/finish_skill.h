#pragma once
#include "services/client_senders.h"
#include "wire_codec.h"
#include <cmath>
#include <span>
#include <stdexcept>

namespace tmapsvr {
// CSSender.cpp SendCS_FINISHSKILL_ACK: Windows BOOL occupies four bytes.
struct FinishSkillRequest {
    std::uint32_t host{},attacker{};
    std::uint8_t type{};
    float x{},y{},z{};
    std::uint16_t skill{},party{};
    std::uint32_t linked{},fake{};
    std::vector<SkillTarget> targets;
};
inline FinishSkillRequest DecodeFinishSkill(std::span<const std::byte> bytes) {
    FinishSkillRequest out;wire::Reader r(bytes.data(),bytes.size());std::uint8_t count{};
    if(!r.Read(out.host)||!r.Read(out.attacker)||!r.Read(out.type)||!r.Read(out.x)||!r.Read(out.y)||!r.Read(out.z)||
       !r.Read(out.skill)||!r.Read(out.linked)||!r.Read(out.fake)||!r.Read(out.party)||!r.Read(count)||
       !std::isfinite(out.x)||!std::isfinite(out.y)||!std::isfinite(out.z))
        throw std::runtime_error("Invalid FINISHSKILL header");
    for(unsigned i=0;i<count;++i) {
        SkillTarget t;if(!r.Read(t.id)||!r.Read(t.type))throw std::runtime_error("Truncated FINISHSKILL target");
        out.targets.push_back(t);
    }
    if(!r.Eof())throw std::runtime_error("Trailing FINISHSKILL bytes");
    return out;
}
// The client removes missing/ineligible targets without reordering survivors.
// Duplicated accepted occurrences retain their multiplicity.
inline bool FinishTargetsMatch(const std::vector<SkillTarget>& accepted,const std::vector<SkillTarget>& actual) {
    std::size_t next=0;
    for(const auto& t:accepted)if(next<actual.size()&&t.id==actual[next].id&&t.type==actual[next].type)++next;
    return next==actual.size();
}
enum class FinishAudience : std::uint8_t { Owner=0, Neighbors=1, Nearby=2 };
struct FinishSkillCommit {
    std::shared_ptr<const CharSnapshot> snapshot;
    // Original DEFEND then conditional HPMP for each occurrence; no new opcode.
    std::vector<std::pair<std::uint16_t,std::vector<std::byte>>> packets;
    // Server-only recipient metadata, parallel to the ordered packets.
    std::vector<FinishAudience> audiences;
};
inline std::vector<std::byte> EncodeCureDefend(const CharSnapshot& s,const FinishSkillRequest& r,
    const SkillAttackProfile& p) {
    std::vector<std::byte> b;
    const auto add=[&](auto v){wire::WritePOD(b,v);};
    add(s.dwCharID);add(s.dwCharID);add(std::uint8_t{1});add(std::uint8_t{1});
    add(s.dwCharID);add(std::uint8_t{1}); // original FINISHSKILL supplies own PC host
    add(std::uint32_t{0});add(std::uint32_t{0}); // no client action/animation fields
    add(std::uint8_t{0});add(std::uint32_t{0}); // not a maintained effect
    add(p.critical);add(std::uint8_t{1}); // guaranteed HT_NORMAL source cure
    add(p.attack_type==1?p.physical_level:p.magic_level);add(s.bLevel);
    add(p.physical_min);add(p.physical_max);add(p.magic_min);add(p.magic_max);
    add(std::uint8_t{1});add(std::uint8_t{0});add(s.bCountry);add(s.payload->aid_country);
    add(r.skill);add(p.rank);add(std::uint16_t{0});add(std::uint8_t{1});
    add(r.x);add(r.y);add(r.z);add(s.fPosX);add(s.fPosY);add(s.fPosZ);
    add(std::uint8_t{0});return b; // no damage entries for cure
}
}
