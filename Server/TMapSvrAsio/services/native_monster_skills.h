#pragma once
#include "native_monster_spawn.h"

namespace tmapsvr {
bool NativeMonsterCheckPrevAct(const NativeMonsterSlot&,std::uint16_t active);
std::uint32_t NativeMonsterPureMaximum(const NativeMonsterCatalog&,const NativeMonsterSlot&,FormulaType);
bool NativeMonsterCanUseSkillNow(const NativeMonsterCatalog&,const NativeMonsterSlot&,std::uint16_t,std::uint32_t tick);
float NativeMonsterTargetDistance(const NativeMonsterMap&,const NativeMonsterSlot&);
// Null host is the source no-op, distinct from a real host with no chosen skill.
// Absence of a chosen skill emits no substitute MONSETSKILL packet.
std::optional<NativeCellNotice> SelectNativeMonsterSkill(const NativeMonsterCatalog&,const NativeMonsterMap&,
    NativeMonsterSlot&,const std::optional<MapSessionClaim>& host,bool new_host,std::uint32_t tick);
NativeCellNotices NotifyNativeMonsterHost(const NativeMonsterMap&,const NativeMonsterSlot&,
                                        const std::optional<MapSessionClaim>& previous);
} // namespace tmapsvr
