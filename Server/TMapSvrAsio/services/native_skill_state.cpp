#include "native_skill_state.h"
#include "native_monster_spawn.h"
#include <stdexcept>

namespace tmapsvr {
namespace {
const std::vector<SkillDataRow>& Data(const NativeMonsterCatalog& catalog,std::uint16_t skill) {
    if(!catalog.skills.contains(skill))throw std::runtime_error("Native maintained skill template absent or source-removed");
    const auto data=catalog.skill_data.find(skill);
    if(data==catalog.skill_data.end())throw std::runtime_error("Native maintained skill has no proven data query");
    return data->second; // present-empty query is distinct from missing template/query authority
}
}
bool NativeMonsterAttackAllowed(const NativeMonsterCatalog& catalog,const NativeMonsterSlot& slot) {
    if(slot.getaway||!slot.can_attack)return false;
    for(const auto& effect:slot.maintained)for(const auto& data:Data(catalog,effect.skill)) {
        // TObjBase::CheckAttack for OT_MON. It reads EVERY data row, without
        // action, rank, remaining-duration, or effect-start filtering.
        if(data.bType==SDT_STATUS&&(data.bExec==SDT_STATUS_BLOCK||data.bExec==SDT_STATUS_HOLD||
            data.bExec==SDT_STATUS_HYPNOSIS||data.bExec==SDT_STATUS_FLASH||data.bExec==SDT_STATUS_DARK))return false;
        if(data.bType==SDT_AI&&data.bExec==SDT_RUNAWAY)return false;
    }
    return true;
}
std::uint8_t NativeHaveDisguiseBuff(const NativeMonsterCatalog& catalog,std::span<const MaintainedEffect> effects) {
    for(const auto& effect:effects)for(const auto& data:Data(catalog,effect.skill))
        if(data.bType==SDT_TRANS&&(data.bExec==SDT_TRANS_DISGUISE_D||data.bExec==SDT_TRANS_DISGUISE_C||data.bExec==SDT_TRANS_DISGUISE_B))
            return data.bExec; // first actual maintained/data-vector match, not numeric executor order
    return 0;
}
std::uint8_t NativeDisguisedCountry(const NativeMonsterCatalog& catalog,std::uint8_t raw_country,
                                  std::span<const MaintainedEffect> effects) {
    // HaveDisguiseBuff then GetCountry traverses the same immutable vectors
    // twice with no intervening mutation. Its first matching transform decides.
    const auto disguise=NativeHaveDisguiseBuff(catalog,effects);
    switch(disguise) {
    case SDT_TRANS_DISGUISE_D:return 0;
    case SDT_TRANS_DISGUISE_C:return 1;
    case SDT_TRANS_DISGUISE_B:return 2;
    default:return raw_country;
    }
}
} // namespace tmapsvr
