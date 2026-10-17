#pragma once
#include "native_monster_spawn.h"

namespace tmapsvr {
bool NativeMonsterCheckPrevAct(const NativeMonsterSlot&,std::uint16_t active);
std::uint32_t NativeMonsterPureMaximum(const NativeMonsterCatalog&,const NativeMonsterSlot&,FormulaType);
bool NativeMonsterCanUseSkillNow(const NativeMonsterCatalog&,const NativeMonsterSlot&,std::uint16_t,std::uint32_t tick);
std::uint32_t NativeMonsterSkillRemaining(const NativeMonsterSkill&,std::uint32_t tick);
bool NativeMonsterCanSilenceAttack(const NativeMonsterCatalog&,const NativeMonsterSlot&,std::uint16_t);
struct NativeMonsterAbility {std::uint32_t value{};std::int32_t increase{};};
NativeMonsterAbility CalculateNativeMonsterAbility(const NativeMonsterCatalog&,const NativeMonsterSlot&,
    std::uint32_t base,std::uint8_t exec,const NativeMonsterMaintain* instance_override=nullptr);
std::uint32_t NativeMonsterMaximum(const NativeMonsterCatalog&,const NativeMonsterSlot&,bool hp);
SkillAttackTiming NativeMonsterAttackTiming(const NativeMonsterCatalog&,const NativeMonsterSlot&,std::uint8_t speed);
void UseNativeMonsterSkill(const NativeMonsterCatalog&,NativeMonsterSlot&,std::uint16_t,std::uint32_t tick);
float NativeMonsterTargetDistance(const NativeMonsterMap&,const NativeMonsterSlot&);
// Null host is the source no-op, distinct from a real host with no chosen skill.
// Absence of a chosen skill emits no substitute MONSETSKILL packet.
std::optional<NativeCellNotice> SelectNativeMonsterSkill(const NativeMonsterCatalog&,const NativeMonsterMap&,
    NativeMonsterSlot&,const std::optional<MapSessionClaim>& host,bool new_host,std::uint32_t tick);
NativeCellNotices NotifyNativeMonsterHost(const NativeMonsterMap&,const NativeMonsterSlot&,
                                        const std::optional<MapSessionClaim>& previous,bool previous_accepted=false);
} // namespace tmapsvr
