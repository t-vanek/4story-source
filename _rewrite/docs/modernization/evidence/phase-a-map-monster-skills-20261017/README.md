# Native monster skills and host notifications — Phase A, unverified

Baseline `b5c42da`, local `main`, 2026-10-17. Actual catalog/learned-object boot
producers and owned outbox integration are **INTEGRATED_UNVERIFIED** within their
actor-isolated scope. Source pure resource inputs, SelectSkill, selected SetHost/
CHKHOST and notification recipient branches are **IMPLEMENTED_UNVERIFIED** for
ordinary PC targets. Initialize/nonstale ACK still refuse actual players and
all populated-process admission guards remain. Full hosting/gameplay and Phase A
remain **IMPLEMENTING**; no usable populated world is claimed.

No build, test, SQL, import, migration, service, client, container, runtime check,
push or deployment ran. Source/interface/SQL-text/CMake/diff/inventory inspection
only.082 and its SELECT grant are written, not executed;044–082 remain unapplied,
next083. The existing079 publisher was extended in place, not run. Private
extracts and pre-existing `_rewrite/docs/audits/` remain outside Git.

## Authority and data boundaries

Original inspected server/client source revision remains
`c77f79017d7940c1500ba36a5dbf8bda6fefb67c`; reference trees and attributions were
not edited. TVERSION0x2918 is source evidence only; matching executable/assets/
region and actual client compatibility remain **UNVERIFIED**. Ordinary non-BOW/
non-BR is the implemented branch. No mechanics from another version were added.

Actual local backup files were rehashed without executing/restoring their data:

| Archive | SHA256 |
|---|---|
| TGAME_RAGEZONE.bak | `2cff1635d523322d3de8ddafaeacc3335581dc338dca901239ac80288f51d259` |
| TGLOBAL_RAGEZONE.bak | `fca545bcb90bcc72aa4f3514a6177c2b88d2b0bf71bf847b0128f090a20877fb` |

Private complete historical manifests and actual original query-order receipts
needed for079–081 remain unavailable in checked publication paths.082 derives
readers from existing079 full publication and checks value, row-count and byte
receipt equality against active character skill/formula and content class/race
releases. It creates no rows or saved hosts/skills and alters no old migration,
archive or historical snapshot. Missing publication refuses before native boot;
a proven missing object inside a complete source catalog follows its actual
source branch. No SQL Server runtime dependency was introduced.

## Concrete source to modern consumers

| Original file / symbol | Modern implementation / actual connection |
|---|---|
| `TMap.cpp554/664::InitMonster`; `TAICmdRegen.cpp` learned CTSkill insert; `TSkill.cpp3::CTSkill` | `NativeMonsterSlot::skills` now owns WORD-keyed rank/use/delay state; `InitMonster/ResolveTemplate` insert actual existing templates before Initialize/Regen, preserving prior objects on duplicate insertion |
| `TMapSvr.cpp::LoadData/GetFormulaInit/GetFormulaRateX`; original class/race/skill queries | `LoadNativeMonsterSkills/CheckNativeMonsterSkillRelease`; scoped082 views feed the existing native catalog before slot allocation; subsequent owner/battle/monster release checks pin the same publication |
| `TObjBase.cpp1449/1816/1855/1991/2001` pure status/maxima; `TSkill.cpp::GetRequiredHP/MP/CanUse`; `TMonster.cpp1453::CanUseSkillNOW` | `NativeMonsterPureMaximum/CanUseSkillNow`; actual class/race/formula/learned clocks/HP/MP/maintained state; reuse of existing `skill_engine::RequiredResource` for defined type1/2 arithmetic |
| `TObjBase.cpp2939::CheckPrevAct`; `TMonster.cpp1461::SelectSkill` | `NativeMonsterCheckPrevAct/SelectNativeMonsterSkill`; actual native SetHost and CHKHOST executor branches invoke selection after host notifications |
| `TMonster.cpp485/1540/1554::FindHost/GetTargetDistance` | `FindNativeMonsterHost/NativeMonsterTargetDistance`; source neighboring cells and canonical PC lifetime/position/constructor size; non-PC recognized target producers explicitly remain missing |
| `TAICmdSetHost.cpp::ExecAI`; `TAICmdChkHost.cpp::CanRun/ExecAI` | Existing executor kind2 and new kind3 flow: actual selectors, fallback activation/revision, old-host comparison, host-key/status mutation, NotifyHost/SelectSkill, base completion/repeat |
| `TMonster.cpp2369::NotifyHost`; `CSSender.cpp688/3812`; `Client/TClient/CSHandler.cpp1055/6883` | Owned MonsterHost/MonsterSkill notices -> required executor Publish callback -> existing Stage/TakeNativeCellNotices -> native IO publisher -> existing Asio PostPacket/frame/drain |

## Skill state and calculations

A source learned CTSkill is an object, not merely an ID. Owned objects start at
rank1, use_tick0 and delay_tick0 from the actual constructor. Missing templates
are not installed. Essential allocation and actual nonessential template
resolution both produce these objects. Duplicate source map insertion preserves
the original object/rank/clocks; the rewrite avoids leaking the rejected new
allocation. Slots erased/replaced with a new generation do not inherit them.
Actual monster attack/use/cooldown mutation remains unported; the constructor
state is not presented as a completed live skill-use producer.

082 loads actual ranges, previous-active ID, base reuse delay, resource use/type,
start/next rank levels and first-stat formula rate. WORD/DWORD source signed SQL
storage is preserved by explicit bit conversions; duplicate keys/NULL/invalid
numeric input refuse. Class/race presence is still distinct from an all-zero
row. All source metadata comes from the same pinned full run. The publisher's
new final082 visibility check is inside its existing transaction, before commit.
It does not run a migration or fabricate a skill/host object in PostgreSQL.

Pure resource maxima are NOT the monster attribute-family max HP/MP used by
GetMaxHP/MP. GetPureMaxHP/MP use init + DWORD(pure CON/MEN * FLOAT rate). The WORD
base sum wraps before double pow and assignment to FLOAT; the next FLOAT product
is truncated before DWORD init addition. Level0 really uses exponent-1, with no
level1 clamp, rounding-to-nearest integer or minimum1 invented. The existing
prototype stat_engine is not substituted. Source explicitly returns pure stat0
when either class/race pointer is absent, then the formula init still contributes.
Missing formula methods explicitly return0 in the original; the loader retains
presence and only these documented accessors supply that source fallback.

CanUseSkillNOW preserves short-circuit order: CanUse at wrapping DWORD(tick +
attack_wait_tick), pure-HP cost <= current HP, pure-MP cost <= current MP, then
CheckPrevAct. Use tick0 means ready; otherwise DWORD elapsed >= delay is ready.
Unobserved attack-wait tick is distinct from real Regen-produced0 and refuses
when evaluated; no undefined constructor value is guessed. Flat rank cost keeps
source FLOAT rounding before DWORD conversion; percentage multiplication wraps
as DWORD before /100. Unknown resource-type switch branches return0 in source,
so that specific behavior is retained. Invalid floating conversions refuse.
CheckPrevAct matches all maintained vector skill IDs, including expired records
not yet removed by the real lifetime producer, and active0 passes.

## Selection, targets and output order

Selection uses actual WORD learned-map order. Previous selection is captured,
source target distance calculated and next selection cleared in that order. The
in-range and secondary candidates preserve every source min/max-range comparison,
CanUse short circuit, base reuse-delay priority, secondary max-range tie and final
secondary promotion predicate. It does not simplify selection to closest range,
ready-only skills or highest damage. The first candidate can be retained without
calling CanUse, exactly as source does. No random draw or cooldown debit is added.

A null host is a no-op, preserving next skill. A real host with no candidate
clears next skill and emits no packet. An unchanged selected ID emits no packet
unless bNew is true. A present skill ID0 remains distinct from no object. Target
distance uses source FindHost in neighboring cells; no host or unrecognized target type
returns the source65535 sentinel. OT_PC measures the host itself, ignoring
m_dwTargetID, subtracting combined source radii from FLOAT sqrt with floor0.
PC size currently has the real CTObjBase constructor0.5 producer. Transform size
mutation is still missing. Recognized recall7/self11/companion18 targets refuse
because their actual object lookup/size producers are absent; this is not a claim
that those source objects are absent or at zero distance.

SetHost retains first preselection, DWORD distance/recency and raw fallback from
previous work. Fallback now actually activates canonical CanHost and refreshes
its cell index/revision without manufacturing a MoveGhost event. CHKHOST first
returns FALSE if the existing neighboring host CanHost; otherwise it chooses the
strict smallest Manhattan DWORD distance without first preselection or a move-
clock filter, then the original raw fallback. Only SetHost sets OS_WAKEUP. Same
old/new host returns FALSE without completion. Selected changes increment the
DWORD host key, set host, NotifyHost, SelectSkill(TRUE), then base Complete.

NotifyHost captures previous FALSE first; all other neighbors follow in reverse
source vector order, skipping previous character ID and setting TRUE only for
the current host. It applies no invented main/role/channel filter. MONHOST body
is5 bytes (DWORD + BYTE); MONSETSKILL is8 (DWORD + WORD + WORD), opcodes0x528F and
0x535C. The original client's host flag and default-skill/range readers match
those definitions at source level; executable acceptance is still unverified.
Owned notices retain generation/recipient lifetime and scalar values across
subsequent mutation/removal. Existing FIFO/capacity/current-recipient checks and
Asio framing/buffer ownership are reused. The byte cap now charges8 for the skill
body; no successful prefix is returned on overflow. No new packet field or host
request opcode was invented.

## Remaining implementation boundaries

These selected branches are connected to the existing executor but not enabled
with actual players: unchanged public Initialize/ACK actor guards and latched
process admission protect the still incomplete population. ResetHost's nonempty
host/target/aggro path remains an explicit refusal, not a completed host release.
Source global old-host lookup, ChgMode notifications, damage/aggro cleanup and
SM_RESETHOST request/batch relocation must be implemented next. Other awake/dead
AI can still refuse AICOMPLETE branches. Initial empty-host source behavior stays.

PC cell/MoveGhost monster events, host-generated movement/attack requests and
skill Use clocks, non-PC targets/transform size, maintained-effect mutation/stat
recalculation, non-fresh death/Regen/loot, dynamic/battle/quest/party-instance
lifetimes remain open. MOVE stand-hide/position quests and the broader server
inventory are also unfinished. Full Phase A is not complete.

## Deferred Phase B scenarios — none executed

1. Apply082/grants only in later verification; publish actual full/archive/order
   inputs. Check all four receipt mismatches, missing/checkpoint/byte variants,
   role denial and changed active release. Confirm no archival or live rows change
   and native construction stops before serving without complete source proof.
2. Compare source learned rank/tick constructor state and duplicate/template-
   change insertion, missing templates, slot removal/reuse and WAIT lifetimes.
   Exercise pure maxima against attribute maxima, missing class versus actual
   zero class, missing formula source0, WORD wrap, FLOAT narrowing, level0, DWORD
   init/percentage wrap, rank0 and invalid casts. Never invent missing metadata.
3. Compare short-circuit cooldown0, attack-wait observed0/absent, DWORD wrap,
   exact expiry, HP/MP equality, type0/1/2/unknown and previous-active maintained
   presence/expired records. Confirm no CanUse call occurs on source branches
   retaining a lone first candidate without comparison.
4. Cover every in-range/secondary/tie/final promotion branch, min/max boundaries,
   source learned WORD order, no host/no skill/skill0, unchanged selection versus
   bNew, missing host sentinel and PC target radius. Keep non-PC targets blocked
   until their actual object producers exist.
5. Compare SetHost versus CHKHOST: first preselection/recency/3000 boundary, valid
   old host early return, raw fallback activation, no-candidate ResetHost guard,
   host-key wrap, wakeup only in SetHost, output order before AICOMPLETE/repeat.
6. Compare exact5/8-byte client bodies and old/new/reverse-neighbor packet order,
   logical channels/replicas, close/handoff, erased/reused generations and outbox
   count/byte/per-peer overflow. No actual populated gameplay is accepted merely
   because standalone packet or selector scenarios pass.

## Exact next handoff

Port `TMonster.cpp2392::ResetHost`, `TObjBase.cpp295::ChgMode` and
`SSHandler.cpp439/464::OnSM_RESETHOST_REQ/ACK`: actual old-host lookup, mode output,
aggro/damage clearing and owned queued relocation through real monster cells.
Then port PC-to-monster Enter/Leave/channel/MoveGhost events, host-produced
CS_MONMOVE_REQ and attack/skill lifecycles, remaining awake/dead commands and
non-PC/effect/transform producers. Keep all actor barriers until connected
coexistence is implemented; do not replace ResetHost with a fake successful clear.
Next migration083; Phase B/C remain deferred. Inventory has316 unique IDs/13
columns; DB map75 rows/6 columns. Record counts are not completion metrics.
