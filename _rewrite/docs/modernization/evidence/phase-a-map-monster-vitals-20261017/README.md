# Monster maxima, REFILL and transformation admission — Phase A, unverified

Baseline `011c238`, local `main`, 2026-10-17. Source monster ability/maxima
calculations and ordinary REFILL command logic are **IMPLEMENTED_UNVERIFIED**.
Real boot, ADDMON and captured HPMP/transformation output connections are
**INTEGRATED_UNVERIFIED** within actor isolation. MONMOVE now invokes the source
DEAD/Transformation prefix of OnDie; supported transformation branches create a
real owned maintained effect, resize and refill, rather than only a packet.
Full transformation/OnDie/MONMOVE remain **IMPLEMENTING**. Positive buff/posture
replacement, effect expiry/release and the base death/reward/loot/dead-AI chain
remain explicit missing dependencies. No populated admission/Initialize/ACK or
non-fresh Regen guard was removed. Full Phase A remains **IMPLEMENTING**.

No build, test, SQL, migration, import, publisher, service, client, container,
runtime verification, push or deployment ran. Work consisted of static source,
interface, CMake, SQL-text, diff and inventory inspection plus backup hashing.
No schema migration, grant or publisher change was necessary: existing044–083
and grants/publishers remain written/unexecuted; next084. Private extracts and
pre-existing `_rewrite/docs/audits/` remain outside Git.

## Source authority and unresolved profile

Inspected original server/shared/client revision:
`c77f79017d7940c1500ba36a5dbf8bda6fefb67c`. Static Git comparison confirms
inspected Monster/ObjBase/Skill/SkillTemp, Refill/Regen, LoadData/DBAccess,
CSHandler/CSSender/SSSender, NetCode and client CSHandler paths still match it.
Original trees, attributions/license headers, backups and old migrations are
unchanged. Scope is ordinary non-BOW/non-BR OT_MON. TVERSION0x2918 is a source
marker; executable/assets/region correspondence and actual client compatibility
remain unverified. Complete private historical manifests and actual079–083
query-order receipts are still unavailable at publication inputs. No missing
historical value, skill ID, query order or receipt was synthesized.

Actual local backups re-hashed without restoration or SQL interpretation:

| Archive | SHA256 |
|---|---|
| TGAME_RAGEZONE.bak | `2cff1635d523322d3de8ddafaeacc3335581dc338dca901239ac80288f51d259` |
| TGLOBAL_RAGEZONE.bak | `fca545bcb90bcc72aa4f3514a6177c2b88d2b0bf71bf847b0128f090a20877fb` |

## Source to implementation and real call paths

| Original file / symbol | Modern implementation / connection |
|---|---|
| `TObjBase.cpp1409::CalcAbilityValue`, `4247::ApplyEffectionBuff`; `TSkill.cpp154/167::CalcAbilityValue/CalcCure` | `native_monster_skill_use.cpp::CalculateNativeMonsterAbility/Ability/Add`; existing attack timing delegates base100, maxima delegate real raw attribute base |
| `TMonster.cpp1700/1708::GetMaxHP/GetMaxMP` | `NativeMonsterMaximum`; actual Regen, REFILL, Transformation and `ProjectNativeMonsterAppearance` |
| `TAICmdRefill.cpp::GetDelay/CanRun/ExecAI` | existing executor `CanRun/Delay/Execute` case10 -> `native_monster_vitals.cpp::Refill` -> owned HPMP FIFO -> original completion |
| `TMapSvr.cpp2830`; `DBAccess.h` original chart fields | `LoadNativeMonsterCatalog::transform_skill`; full079 view supplies wTransSkillID and model sizes |
| `TMapSvr.cpp2697/2698/2717/2734`; `TSkill.cpp133/147` | full082 reader supplies actual duration/increment/posture/positive/static; `NativeMonsterMaintainTemplate`;083 premium removal also retires that metadata |
| `CSHandler.cpp1036`; `TMonster.cpp528/2221/2232::OnDie/IsTransformation/Transformation` | actual host movement >64 -> `DieFromRoam` -> `Transformation`; remaining base death still refuses |
| `TObjBase.cpp2953/3964/4614`; `TSkill.cpp3/35/133` | `AdmitTransformation`, real owned `NativeMonsterMaintain`, duration/start/charge and silence state, Push guard; full replacement/release remains separate |
| `TMonster.cpp1585::ResetSize` | every raw TRANS model row changes only size; zero restores own model, missing nonzero model retains size |
| `CSSender.cpp1226/1315`; `Client/TClient/CSHandler.cpp2806/3492` | owned `NativeMonsterVitals/Transformation`, exact LE encoders, existing bounded outbox and current-recipient Asio publisher |

The new C++ file is registered once in the existing Map core target. No parallel
monster runtime, state registry or persistence was added. Source transient
monster state is not written to an invented character table.

## Actual maxima and numerical order

The former speed-only base100 calculation now accepts the actual DWORD base and
returns both the modified value and the original signed increase. Instance
CalcValue uses all matching ABILITY rows. An explicit instance override changes
that term, while cure still targets the object's actual member instance, exactly
as source. Maintained vectors preserve SA_BUFF and grouped per-object cure sums,
duplicate matching attributes, instance/cure sign crossing, active signed clamp,
conditional STATUS_MAGIC and original DWORD products/accumulation. Remaining
objects contribute CONTINUE then PASSIVE per object against the original base.
Final signed clamp and returned-delta addition keep source order. Constructor
empty remain/null instance is still distinct from learned skills.

GetMaxHP/MP starts with the actual attribute object's raw bar and MHP50/MMP51;
zero after calculation becomes1. A missing attribute is an error, not an object
with zero bars. Percentage resource eligibility still uses the separate source
pure/formula maximum; it was not changed to these attribute maxima. Attack speed
still uses base100 and the same defined arithmetic. Signed/floating undefined
overflow refuses; defined DWORD wrap and Win32 signed bits remain explicit.
The signed returned increase is now also checked when combined with remain.
Calculation/transformation scans refuse more than one million visited rows.

Fresh Regen now calls these same getters. Its non-fresh guard remains because
effect release and other source cleanup are not complete. ADDMON now derives
current maxima from the real graph instead of old cached maxima. Only its
obsolete nonempty-stat-graph refusal was removed; actual attribute/generation/
maxima readiness and all actor isolation checks remain. Visibility does not
clamp or mutate current HP/MP. Existing full maintained records retain their
source BYTE count prefix and51-byte wire projection; new charge/silenced fields
are local state and do not change that wire layout.

## REFILL and supported transformation sequence

REFILL checks WAKEUP and event-host equality before base conditions, delay0.
It has no battle-mode or CheckAttack gate. Its owned map must be the real object
map. It fills HP from GetMaxHP, then MP from GetMaxMP, captures HPMP for reverse
neighbors, then runs source AICOMPLETE. There is no host-key increment. The source
sender's rare numeric-ID party echo is still a known boundary below.

Out-of-roam OnDie first ignores DEAD status. Transformation reads the monster's
actual wTransSkillID, resolves the surviving global skill, then checks whether
that ID already exists in maintained state without filtering rank, action or
expiry. Missing/zero skill or existing same skill returns FALSE and reaches the
remaining base-death boundary. That boundary does not zero bars or fabricate a
successful corpse/loot result.

MaintainSkill requires at least one SA_BUFF row. As in source, a NULL Maintain
result still makes Transformation return TRUE, without a fabricated effect,
refill or packet. The IsTrans-on-both-sides rejection preserves this same source
outcome. The concrete UpdateBuffSkill admission supports the real empty vector,
negative parity (same ID was already excluded), and positive/posture branches
which require no replacement. Any actual shared BUFF data collision or posture
replacement throws before mutation, rather than pretending the omitted priority,
value, self/radius and EraseMaintainSkill chain ran. It is not full buff stacking.

An admitted transformation allocates an owned rank1 CTSkill equivalent with
source host PC ID, attacker monster ID/type, hit/attack-level/attacker-level0,
zero powers, selectable TRUE, actual war country and current XYZ. Duration uses
DWORD duration + increment*(rank-1); SetEndTick uses start=max(tick,1) for nonzero
duration and0 otherwise, with actual charge tick including0. Silence records the
actual selected next skill. Static/dead Push eligibility is preserved. The
effect is inserted into the actual maintained vector used by status, silence,
timing, previous-action checks, maxima and ADDMON.

Every TRANS row, regardless of action, calls source ResetSize with its raw model
WORD in original order. It does not replace the monster's template, attributes,
mode, attack capability or host key. Then HP and MP refill from the new graph.
For each reverse-neighbor recipient, captured HPMP precedes transformation DEFEND.
On successful Transformation the original MONMOVE continues, captures the final
movement state after the whole batch and leaves status/action as source did.
General maintained expiry is still missing: no expired record is silently
deleted by a predicate or presented as a completed timer.

## Packet contracts and remaining sender edge

HPMP is21 bytes: DWORD id, BYTE OT_MON2, max/current HP DWORD and max/current MP
DWORD. The sender's bLevel parameter is absent from this client packet.
Transformation DEFEND is84 bytes, matching source and client field order. Its
wire host0/type0, maintain TRUE, tick0, hit0/HT_NORMAL, attack level1, actual monster
level, powers0, select TRUE/cancel FALSE, actual raw country/aid, transform skill
rank1, back skill0, perform TRUE, twice actual XYZ and empty damage-map count are
source literal arguments. The owned maintained host/duration differ from these
wire fields by source design; the encoder does not invent them as defaults.

Both outputs capture the recipient claim, source generation and values before
later events or slot removal. Existing Asio posting/draining and lifetime fences
apply. The unchanged65536 notice/64MiB FIFO budget now charges actual21/84-byte
bodies per recipient, with an early gathering count bound. Publication errors
retire authority via existing AI or host-request error paths.

`SendCS_HPMP_ACK` also tests only numeric `dwID == receiver.m_dwID`, plus nonzero
GetPartyID, before sending `MW_PARTYMANSTAT_ACK`; it does not test OT_PC. A monster
ID can share that namespace. This branch is not assumed impossible and no bogus
PC update is invented: native publication checks actual hydrated receiver party
state and explicitly refuses this unported echo/order dependency on collision.
That refusal currently occurs before HPMP publication, while source enqueues
HPMP before the World echo. This is an incomplete-path safety boundary, not a
completed compatibility fix. Normal distinct-ID monster packets are connected.

## Exact next implementation and deferred acceptance

Implement `MAP.MONSTER.MAINTAIN.REPLACE` next: original EraseMaintainSkill,
SetTransAttack(0) and ReleaseMaintain, with old/new maxima, captured SKILLEND
recipient order, pre/post CheckAttack and real AT_DEFEND/highest-aggro SetAggro,
hide/self and transform cleanup, IsDie bars/HPMP/recursive OnDie and static
retention. Trace actual GetAbility/value/priority/self/radius collision semantics
before replacing `AdmitTransformation`'s explicit boundary. Port the required
normal-monster SetAggro and full stat getters instead of returning arbitrary
values. Then finish base OnDie, actual keeper/damage/party rewards/quest/MW,
event/battle drops, AddItem/pets and AT_DEAD/removal/respawn. Zero-damage source
death can still generate items; it is not an empty-loot exception.

General effects/expiry, non-PC actors/targets, PC monster events/loot locks,
remaining awake/dead AI, stock-client monster SKILLUSE/LOOP/FINISH/DEFEND and the
numeric-ID party echo remain open. Required future PostgreSQL transformations
must use the next unused migration084 and be written only. Populated actor
barriers cannot be removed based on these conditional paths.

Phase B scenarios prepared, **not run**: arbitrary base versus speed100; raw
sign-bit/zero attributes, real missing pointers, override versus actual cure
instance; duplicate cure attributes and signed subtotals; conditional magic and
DWORD products; returned-delta/signed/floating/work-limit refusal; fresh Regen
equivalence and ADDMON with current nonempty effect graphs without bar mutation;
all REFILL gates/order/keys/completion; missing/removed/present-empty transform
skill, existing same ID regardless expiry, NULL Maintain TRUE, IsTrans veto,
negative parity/posture/disjoint versus real collision; exact maintained fields,
zero/nonzero duration/tick0/DWORD wrap/silenced skill/static-dead Push; ordered
TRANS rows, zero/missing model and size-only effects; exact21/84-byte packet
fields, per-recipient order, later object mutation/reuse; transformation returns
to MONMOVE final batch; party ID collision with SOLO/no-party/real-party and
unhydrated state; bounded publication failure and retained actor isolation.
No historical test result is promoted to evidence for this code.

Unified inventory349 unique IDs/13 columns; DB map78 rows/6 columns. These are
navigation counts, not completion percentages. IMPLEMENTATION_COMPLETE_UNVERIFIED
has not been reached; Phase B and C remain deferred.
