# Source monster attack commands and skill clocks — Phase A, unverified

Baseline `53a8805`, local `main`, 2026-10-17. Original BEGINATK and ATTACK now
have concrete source eligibility, execution, silence, reuse/timing, target capture,
selection and completion paths in the existing native AI executor. Conditional
commands/calculations are **IMPLEMENTED_UNVERIFIED**. Boot metadata and the owned
MONATTACK outbox/Asio connection are **INTEGRATED_UNVERIFIED**, within the ordinary
normal-monster scope and existing actor isolation. This is not a completed client
monster cast or full combat path. All populated admission, Initialize and
nonstale ACK barriers remain; full hosted gameplay and Phase A are **IMPLEMENTING**.

No build, test, SQL, migration, import, publisher, service, client, container,
runtime verification, push or deployment ran. Only source/interface/CMake/SQL-text/
diff/inventory inspection and actual backup hashing were performed. No new schema
migration, grant or publisher was needed:044–083 and existing grants/publishers
remain written/unexecuted, next084. Private extracts and pre-existing local
`_rewrite/docs/audits/` stay outside Git.

## Authority and profile

Inspected original server/shared/client revision:
`c77f79017d7940c1500ba36a5dbf8bda6fefb67c`. Source trees, attributions/license
headers, backups and old migrations are unchanged. The implemented branch is
ordinary non-BOW/non-BR normal OT_MON. TVERSION0x2918 remains only a source marker;
matching executable/assets/region and actual client compatibility are unverified.

Actual local backup SHA256, re-read without restore or SQL interpretation:

| Archive | SHA256 |
|---|---|
| TGAME_RAGEZONE.bak | `2cff1635d523322d3de8ddafaeacc3335581dc338dca901239ac80288f51d259` |
| TGLOBAL_RAGEZONE.bak | `fca545bcb90bcc72aa4f3514a6177c2b88d2b0bf71bf847b0128f090a20877fb` |

Private complete historical manifests and actual original079–083 query-result
order evidence are still unavailable at their publication inputs. No values,
template IDs, order receipts or historical content were invented. Existing
publication/startup fences remain prerequisites, not proof of executed acceptance.

## Source to implementation and actual connections

| Original file / symbol | Modern file / connected consumer |
|---|---|
| `TAICmdBeginAtk.cpp::GetDelay/CanRun/ExecAI` | `native_monster_ai.cpp::CanRun/Delay/Execute`, `native_monster_attack.cpp::Attack`; existing Initialize/Event/DoCommand and batch ACK paths |
| `TAICmdAttack.cpp::CanRun/ExecAI`; inherited `TAICommand::GetDelay` | same executor branch7, original binding delay, event target comparison and existing completion/repeat |
| `TMonster.cpp2347::CanSilenceAttack`; `TMapType.h89` | `native_monster_skill_use.cpp::NativeMonsterCanSilenceAttack`; actual source skill kind and ordered maintained vectors |
| `TObjBase.cpp4568::SkillUse`; `TSkill.cpp77/87/120/224::Use/GetReuseDelay/GetReuseRemainTick/CanUse` | `UseNativeMonsterSkill`, `NativeMonsterSkillRemaining`, existing `skill_timing::ReuseDelay`; actual owned `slot.skills` clocks feed subsequent source selection |
| `TObjBase.cpp1409/1558/1868/1893/4247::CalcAbilityValue/Calc2ndAbility/GetAtkSpeed/GetAtkSpeedRate/ApplyEffectionBuff`; `TSkill.cpp154::CalcAbilityValue/CalcCure` | `NativeMonsterAttackTiming`, local Rate/Ability, exact083 vectors and existing pure base-delay arithmetic |
| `TSkillTemp.cpp49/62/77::CalcValue/GetValue/Calculate` | `native_skill_value.h` extracts the existing native signed arithmetic; native cure wrappers retain their later variance bound |
| `TObjBase::CTObjBase`; `TMap.cpp664::InitMonster` | owned `remaining` vector and optional `instance_skill` establish actual constructor empty/null; learned insertion remains independent of remain membership |
| `TMapSvr.cpp::LoadData`; existing082 full-source view | `LoadNativeMonsterSkills` now also reads nReuseDelayInc, dwKindDelay, bKind, bSpeedApply and bTargetRange; same verified release, signed DWORD bits and strict BYTE/NULL handling |
| `CSSender.cpp1208::SendCS_MONATTACK_ACK`; `CSProtocol.h822`; `Client/TClient/CSHandler.cpp2267::OnCS_MONATTACK_ACK` | `NativeMonsterAttack` capture, `EncodeNativeMonAttackAck`, bounded `StageNativeCellNotices` and existing fenced `PublishNativeCellNotices` |

Both new C++ files are registered once in the existing Map CMake target. No new
service, registry, runtime, language or parallel gameplay implementation exists.

## Command, packet and clock order

BEGINATK first requires WAKEUP, BATTLE, nonzero target, actual FindHost and
CheckAttack, before source base conditions. Its delay is0. Exec finds the host
again; only host + next skill + CanSilenceAttack + current-tick CanUse sends.
Silence precedes CanUse here. Nonzero bTargetRange addresses the monster itself
as OT_MON2; otherwise current target ID/type are captured. The host-only packet
precedes SkillUse, then SelectSkill(FALSE). A failed eligibility of that selected
skill does not cause reselection in BEGINATK. The host key increments regardless
of whether a packet was sent, then AICOMPLETE executes with original arguments.

ATTACK has WAKEUP/BATTLE/target/FindHost eligibility and base conditions, without
CheckAttack. It inherits the binding delay. Exec acts only when current target
ID/type match the event's real-host ID/type. CanUse precedes CanSilenceAttack.
Matching events still SelectSkill(FALSE) when no next skill is ready; a null
host remains the original selection no-op. There is no ATTACK host-key increment.
The existing AICOMPLETE and delayed repeat flow keep current keys/generations.
These differences are preserved rather than replaced by one common gate.

MONATTACK is exactly12 body bytes: DWORD attacker, DWORD target, BYTE attacker
type2, BYTE target type, WORD skill, explicitly little-endian. The notice owns
recipient claim, monster generation and target/skill fields before subsequent
clock/selection/event mutation. Existing recipient connection/role/epoch checks
apply; there is no actor lookup to reconstruct historical packet fields later.
Existing count/byte budgets now account for this body. Reentrant deletion or
generation replacement after publication staging prevents later dereference.

SkillUse computes speed rate before base delay, then own rank-based reuse. The
source DWORD additions/multiplication wrap before /100; the signed rank increment
must be representable. A clock rebases only when its remaining duration is
strictly shorter than the requested delay. Tick0 is the source unused-clock
sentinel even if a delay was assigned, and elapsed DWORD subtraction wraps.
Nonzero kind delay then visits same-kind skills in WORD-map order, including the
used skill itself. Kind delay bypasses speed/rate scaling and never shortens a
longer remaining delay. Arithmetic/peer metadata validate against a local copy
before any owned clock replacement. AI staging failures retire authority and
discard undelivered notices through the existing error path.

No HP/MP subtraction is added to these commands: source SkillUse only arms
reuse. Resource gates in SelectSkill use the real current bars/pure maxima as
before. Current-tick CanUse in attack execution is distinct from selection's
tick + actual attack-wait lookahead. Cost/debit and target effects belong to the
later client-request flow.

## Timing, source state and intentional safety differences

TAD_PHYSICAL/LONG use FTYPE_NAS4 with raw class/race DEX; MAGIC uses FTYPE_MNAS16
with raw WIS. Class/race object absence returns base0; missing formula getters
return actual source0 fields. TAD_NONE/unknown bypasses these branches with
delay0/rate100. Formula FLOAT narrowing and final Win32 signed delay clamp remain.
Ordinary native monster allocation actually contains only its empty default
inventory; no equipped inventory or equipment values are synthesized.

Rate starts with the source100. Instance CalcValue considers every matching
ABILITY row. Maintained SA_BUFF ability contributions and CalcCure attribute
matches retain original vectors and duplicate attribute multiplicity. Instance
plus cure sign crossing and active subtotal clamp precede conditional
ApplyEffectionBuff. STATUS_MAGIC13 has no action filter; each product/division is
DWORD arithmetic and its total preserves Win32 bits. Remaining objects then
contribute SA_CONTINUE followed by SA_PASSIVE per object, before final signed
clamp. Unknown calculation/combine BYTE branches keep the original zero result;
missing template/query authority refuses, distinct from present-empty data.

CanSilenceAttack first checks global template presence, then every real
maintained/data row without duration/action/rank filtering. SILENCE2 rejects
requested kinds other than source MONSTER_BASE_SKILL_KIND101. It does not reuse
CheckAttack's broader blocker predicate or clear effects as part of the query.

The actual normal constructor establishes an empty remain vector and null
instance. InitMonster learned rows do not establish passives. Full Defend/
PerformSkill creation, mutation and expiry of maintained/remain/instance state
remain unported. Their nonempty timing calculations are conditional written
code, not proof those producers exist. Fresh Regen and ADDMON refuse nonempty
stat graphs until effect-aware maxima are implemented, avoiding stale bars.

Native signed/floating overflow is rejected rather than copied as C++ undefined
behavior. Defined DWORD arithmetic preserves bits explicitly. Timing/status
scans additionally refuse over one million visited rows, including nested cure
attribute matches; this resource limit is a documented refusal, not a new game
rule or fabricated successful result. Existing source queue/event work limits
remain. The checked GetValue/Calculate was extracted from native cure without
applying cure's variance bound to non-cure timing; native cure still applies its
same final signed variance-product bound. Generic prototype skill math remains
separate from authoritative native consumers and gains no verification claim.

## Remaining chain, deferred acceptance and handoff

Client `OnCS_MONATTACK_ACK` decodes these fields and calls SendCS_SKILLUSE_REQ.
Original `CSHandler.cpp::OnCS_SKILLUSE_REQ` resolves OT_MON separately. The modern
native handler still permits only own-PC attack identity, and native DEFEND
resolution refuses unsupported actors. This increment does not implement
monster SKILLUSE/LOOP/FINISH/DEFEND, target-owner persistence, damage/aggro/death,
or the complete client cycle. No MONATTACK_REQ handler is invented: this source
profile defines the macro, but has no corresponding original server dispatch.

Phase B scenarios (prepared, not executed): full082 new scalar presence/NULL/
BYTE and signed reuse fields; absent/premium-removed templates versus empty083
queries; silence of any action/rank/duration and kind101 exemption; BeginAtk
versus Attack blocker differences, real-host ID/type mismatch, null host, missing
next skill, blocked reuse and their different reselection/key behavior; self
versus external target flag; exact12-byte wire order and real host lifetime;
AICOMPLETE removal/key change and repeat; cooldown equal/shorter/longer, tick0,
DWORD wrap, rank0/negative increment, same-kind inclusion/order and rate bypass;
NAS/MNAS missing objects/formula and float truncation; actual empty versus
nonempty instance/maintained/remain, cure attribute duplicates/sign crossing,
conditional MAGIC and defined unsigned products; overflow and work-limit refusal;
unchanged native cure variance/MISS/empty behavior after arithmetic extraction.
No historical passing test is promoted to coverage of this change.

Next port ordinary source FOLLOW and host MONMOVE with actual accepted host
pose/lifetime and original event ordering, then connect the stock client's
OT_MON SKILLUSE/LOOP/FINISH/DEFEND chain with real target/resource/effect/aggro
producers. Complete effect-aware maxima, maintained/remain/instance mutation and
replica expiry, combat SetAggro and PC/loot-lock consumers before coexistence.
Other source subsystems remain open in the unified inventory. Inventory339 unique
IDs/13 columns; DB map78 rows/6 columns are navigation metadata, not completion
percentages. Next migration084. Full Phase A remains incomplete; Phase B/C deferred.
