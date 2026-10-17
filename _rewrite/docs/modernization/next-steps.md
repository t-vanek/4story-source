# Continuation plan and acceptance gates

The master mission remains active. This file records the next coherent slices;
it does not defer ordinary implementation for permission. Source backups remain
authoritative for historical values, original code/client for supported behavior.

Owner instruction updated (2026-10-14): continue implementation with local Git
only, without GitHub pushes or deployment. The preceding publication ended at
`2e406dc`. Private extracts/local audit output stay outside Git. Services, clients,
containers, builds, tests, SQL and runtime verification remain deferred; this
increment uses static source/diff inspection only.
See [active rewrite status](full-rewrite-status.md).

Local Phase A continuation (2026-10-17), baseline `5e835e3`: written083,
SELECT grants and the offline skill-data publisher now define complete original
per-skill query order, explicit empty results, full/active source equality and
ordinary TPREMIUMSKILLCHART removals. Actual native boot loads all eight data
fields, applies source template removals and pins the release on subsequent
owner/battle/AI reads. CheckAttack consumes real status/AI rows; ordered disguise
and GetCountry feed CHGHOST and native NPC access. Initial replica constructor
effects and retained demotion effects are distinct from unloaded state. These
paths are **IMPLEMENTED_UNVERIFIED / INTEGRATED_UNVERIFIED** in their documented
scope; all populated admission/actual-player AI guards remain. General effect/
replica mutation and expiry, combat SetAggro, host attack/skill/movement, PC events/
loot locks and full hosting remain incomplete. Existing player learn/load/use/item
readers also need the same source premium-availability contract. Full Phase A
remains **IMPLEMENTING**.083/grants/publisher are written only;044–083 unapplied,
next084. No build/test/SQL/import/runtime/push/deployment ran.
[Source contracts and handoff](evidence/phase-a-native-skill-state-20261017/README.md).

Previous Phase A continuation (2026-10-17), baseline `fbc91a7`: source CHGHOST
now produces owned aggro/target state, follows source notification/skill ordering
and invokes real LeaveAggro/reset/re-entry on a missing new host. LeaveAggro keeps
signed-key ties, raw-country filtering and recursive return semantics; leader/
party follower events use current real spawn vectors and source query order.
CHGMODE now consumes original eligibility/host arguments, mode transitions,
recovery/output helper and base completion. These conditional command/recipient
paths are **IMPLEMENTED_UNVERIFIED** and wired to the existing executor/factory/
outbox, **INTEGRATED_UNVERIFIED** within actor isolation. Actual current player
country reads use the accepted lifetime and native snapshot, refusing unsupported
or unloaded disguise state. CheckAttack supports the real empty-effect branch;
nonempty monster status metadata is still missing. Combat SetAggro, PC monster
events/loot locks, remaining awake/dead commands and full hosting remain
**IMPLEMENTING**. All admission/actual-player AI guards remain. No new migration;
044–082 are written/unapplied, next083. No build/test/SQL/import/runtime/push or
deployment ran. [Source contracts and handoff](evidence/phase-a-map-monster-aggro-20261017/README.md).

Previous Phase A continuation (2026-10-17), baseline `f006388`: native ResetHost
now resolves the actual global accepted actor, captures per-recipient mode/color
before clearing host/target/aggro/damage, and queues source far-from-start resets
through the existing shared AI FIFO and batch ACK. Ordinary monster cell movement
preserves source leave/channel/enter ordering, one AT_LEAVE and final captured
MONMOVE. Native global actor retirement precedes teardown DB waits while dirty
reservations remain retained. These paths are **INTEGRATED_UNVERIFIED** within
actor isolation; populated recipient/relocation branches and monster battle-mode
clock/output calculations are **IMPLEMENTED_UNVERIFIED**. All player-admission
and actual-player AI guards remain. Live aggro/PC monster events, remaining host
requests/awake/dead commands, effects and other lifetimes are still incomplete;
full hosting and Phase A remain **IMPLEMENTING**. No new migration;044–082 remain
written/unapplied, next083. No build, test, SQL, import, runtime, push or deployment
ran. [Source contracts and exact handoff](evidence/phase-a-map-monster-reset-20261017/README.md).

Previous Phase A continuation (2026-10-17), baseline `b5c42da`: native monster
allocation/Regen now produces owned learned skill objects with real constructor
rank and DWORD reuse clocks. Written082 derives exact skill/formula/class/race
readers from the pinned079 publication; actual native boot and release checks
consume them, **INTEGRATED_UNVERIFIED**. Source SelectSkill, CanUseSkillNOW,
pure resource maxima, FindHost, selected SetHost and distinct CHKHOST branches
are **IMPLEMENTED_UNVERIFIED** within the stated ordinary PC scope. NotifyHost
and MONSETSKILL capture owned source-ordered notices through the existing bounded
outbox and Asio publisher. These recipient branches remain behind unchanged
player-admission/Initialize/ACK barriers; this is not populated hosted gameplay.
Populated ResetHost/mode/reset queues, PC-to-monster events, other target/effect
producers and awake/dead commands remain incomplete. Full Phase A is still
**IMPLEMENTING**.082/grants/publisher are written only;044–082 unapplied, next083.
No build, test, SQL, import, runtime, push or deployment ran.
[Source contracts and exact handoff](evidence/phase-a-map-monster-skills-20261017/README.md).

Previous Phase A continuation (2026-10-17), baseline `73a1eea`: native PC-only
MOVE now preserves source ghost/body separation, HP0 direction keys, raw CanHost
transitions and the observed DWORD move clock, including ghost early return.
Strict speed `>3.40f` uses the existing exact PostgreSQL operator publication.
Owned preparation/application fences route/World waits by connection, role,
epoch and cell revision; actual load and confirmed handoff retain the source
local movement/host lifecycle. These paths are **INTEGRATED_UNVERIFIED** within
ordinary source-empty processes. CanHost geometry and source SetHost candidate
selection are **IMPLEMENTED_UNVERIFIED**; selected-host assignment, NotifyHost/
SelectSkill and PC-to-monster events remain incomplete and guarded. No populated
actor barrier was removed. Full MOVE/gameplay and Phase A remain **IMPLEMENTING**.
No new migration;044–081 remain written/unapplied, next082. No build, test, SQL,
import, runtime, push or deployment ran.
[Source contracts and exact handoff](evidence/phase-a-map-player-host-20261017/README.md).

Previous Phase A continuation (2026-10-17), baseline `7e1339b`: native Map now has
the complete source ADDMON wire encoder, lazy source GetColor and real-slot
projection for fresh monsters. Normal monster entry/Remove collect source-ordered
ADD/DELMON notices; a bounded owned outbox connects actual Regen/Remove callbacks
to the existing native AI IO turn and Asio packet drain. Template level, real
HP/MP, orientation and region replace prototype values; full effect records retain
source DWORD clocks/BYTE prefix rules. Wire/color are **IMPLEMENTED_UNVERIFIED**;
fresh state/cell/outbox paths are **INTEGRATED_UNVERIFIED** within actor isolation.
Current live projection refuses nonempty effects without their stat producer.
Populated processes still cannot admit players: PC monster events, CanHost/
NotifyHost/SelectSkill, host requests and concrete awake/dead AI remain missing.
Full player/monster coexistence and full Phase A remain **IMPLEMENTING**. No new
migration;044–081 remain written/unapplied, next082. No build, test, SQL, import,
runtime, push or deployment ran.
[Source contracts and exact handoff](evidence/phase-a-map-monster-visibility-20261017/README.md).

Previous Phase A continuation (2026-10-17), baseline `85a9aa2`: native Map now
uses the actual source PC cell lifecycle for Ready, movement, confirmed ownership
changes and disconnect. Owned primary/replica identities, enabled/main/extended
cell predicates, ordered ENTER/LEAVE/channel exits and reverse-neighbor MOVE
replace the native whole-channel/prototype flood. Source ordinary CanFight and
actual transient pitch/directions/action feed ENTER. Owned bounded publication
uses the existing Asio strand/channel/drain without suspending cell mutation.
**INTEGRATED_UNVERIFIED** for PC-only ordinary source-empty processes. The latched
populated-process admission barrier remains: full monster ADDMON/client hosts,
concrete hosted AI and player/monster coexistence are incomplete. Ghost movement
is explicitly refused; full MOVE/gameplay and full Phase A remain **IMPLEMENTING**.
No new migration;044–081 remain written/unapplied, next082. No build, test, SQL,
import, runtime, push or deployment ran.
[Source contracts and exact handoff](evidence/phase-a-map-player-cells-20261017/README.md).

Previous Phase A continuation (2026-10-17), baseline `8bc9008`: original Map AI
requests now reach owned timer/batch queues and actual delayed ACK execution in
the existing Asio lifecycle. Source timer-before-request order, DWORD elapsed
arithmetic, delivery-time CanRun, AICOMPLETE/repeat sequencing and current host-key
capture are preserved, **INTEGRATED_UNVERIFIED** within ordinary actor-free maps.
Object generations and removal/shutdown/authority retirement fence queued work.
The former Schedule/entered-monster constructor barriers are replaced by a real
queue and a latched player-admission barrier when source slots/jobs survive boot;
this is protective isolation, not hosted gameplay. Full visibility/ADDMON/client
hosts, concrete awake/dead commands, party instances and full Phase A remain
**IMPLEMENTING**. No new migration;044–081 remain written/unapplied, next082.
No build, test, SQL, import, runtime, push or deployment ran.
[Source contracts and exact handoff](evidence/phase-a-map-ai-queue-20261017/README.md).

Previous Phase A continuation (2026-10-17), baseline `39b94e1`: native Map now
loads the actual global CTBLChannelList and source TGetServerID join, builds
primary/extended cells in source order and registers/removes pre-actor monster
membership. CHECKMAIN and movement/entry/replica connection sets consume this
owned graph, **INTEGRATED_UNVERIFIED** within those scopes. The two source
position-conversion orders remain distinct; MAX_CHANNEL51 and ambiguous TOP1
queries have explicit safety boundaries. Written081/publisher/grants are
unexecuted;044–081 remain unapplied, next082. Delayed AI publication and surviving
entered monsters still stop construction at their explicit timer/admitted-actor
boundaries before listening. Visibility/ADDMON/client hosts, full monster
lifecycle and full Phase A remain **IMPLEMENTING**. No build, test, SQL, import,
runtime, push or deployment ran.
[Source contracts and exact handoff](evidence/phase-a-map-cells-20261017/README.md).

Previous Phase A continuation (2026-10-17), baseline `d61a3ae`: native Map now
loads exact source AI commands, per-command conditions and ordered trigger
bindings. Actual source Initialize consumes them between slot insertions;
ordered conditions/base completion and fresh Regen preparation preserve group
redirection, weighted selection, FLOAT placement and signed Win32 HP/MP clamps.
**INTEGRATED_UNVERIFIED** within this pre-actor bootstrap scope. Required
EnterMAP/cell/visibility and delayed AI publication still stop construction
before listening; awake/dead/runtime lifecycle remains incomplete. There is no
fake live-registry projection or successful dropped timer. Written080/publisher/
grants are unexecuted;044–080 remain unapplied, next081. Full monster lifecycle,
battles/bootstrap and full Phase A remain **IMPLEMENTING**. No build, test,
SQL, import, runtime, push or deployment ran.
[Source contracts and exact handoff](evidence/phase-a-map-monster-ai-20261017/README.md).

Previous Phase A continuation (2026-10-16), baseline `0f37aef`: native Map now
loads the source server-scoped monster catalog with original per-query order,
group/party membership and raw attribute-family identity. Source default-spawn
allocation preserves physical maps/channels, anonymous and essential slots,
instance IDs and synchronous Initialize boundaries, **INTEGRATED_UNVERIFIED**
within allocation only. Battle spawn bindings consume the same catalog;
round-robin prototype population is excluded from native ownership.
**Native startup now refuses the first unported CTMonster::Initialize call,
before listening.** This is a protective implementation barrier, not completed
monster gameplay or a usable populated world. Written079/publisher/grants are
unexecuted;044–079 remain unapplied, next080. AI graph/regen/cells/client hosts,
full monster lifecycle/battles/bootstrap and full Phase A remain **IMPLEMENTING**.
No build, test, SQL, import, runtime, push or deployment ran.
[Source contracts and exact handoff](evidence/phase-a-map-monster-allocation-20261016/README.md).

Previous Phase A continuation (2026-10-16), baseline `81191cc`: native Map now
loads original battle-zone and local/castle/mission/SkyGarden boot state from an
explicit PostgreSQL projection. Source boss-template validity, logical default
channel/instance-map binding, weekly ownership and actual ANSI hero bytes feed
HEROLIST and NPC talk/trainer/ordinary-shop access, **INTEGRATED_UNVERIFIED**
within that boot/read scope. Written078/grants/publisher are unexecuted;
044–078 remain unapplied, next079. Real monster realization/AI, switches,
GodBall, occupation/rewards, hero mutations and full source bootstrap remain
**IMPLEMENTING**. Unsupported World mutations retire the boot authority and
link; this is protective refusal, not battle support. No build, test, SQL,
import, runtime, push or deployment ran.
[Source contracts and exact handoff](evidence/phase-a-map-battle-regions-20261016/README.md).

Immediate implementation dependency: apply the ordinary source premium-skill
availability contract consistently to native player learn/load/use/item callers;
keep missing-template handling distinct from a proven empty skill-data query.
Then implement BeginAtk/CanSilenceAttack/SkillUse and host attack/movement, using
the ordered083 metadata and current source clocks. Monster maintained-effect
creation/removal/stat recomputation, replica buff/expiry producers and combat
SetAggro with actual damage/heal/skill inputs remain required. PC Enter/Leave/
channel/MoveGhost must retain event order and real MONITEMLIST/TAKE lock lifetimes.
Non-PC/transform/death/loot, non-fresh Regen, dynamic/battle/quest/party-instance
lifetimes, MOVE stand-hide and position quests stay open. Keep all latched actor
admission and actual-player Initialize/ACK guards until coexistence is connected.
No CS_MONHOST_REQ exists in this source profile. Full Phase A remains IMPLEMENTING;
next migration084; no Phase B/C execution.

Previous Phase A continuation (2026-10-16), baseline `8c0b11e`: original World
battle/time charts, explicit source calendar, defined normal-battle timer branches
and a bounded timer-to-batch FIFO are wired into native startup and one-second
processing, **INTEGRATED_UNVERIFIED** within World. Enable output includes
SkyGarden; non-mission peace trims real cached guild records and clears castle
scores. The guild reader now emits actual conditional last records. Written077,
grants and publisher are unexecuted;044–077 remain unapplied, next078. Actual
Map regions/spawns/switches/occupation/rewards and full source bootstrap remain
**IMPLEMENTING**. Map rejects those unported enable paths explicitly; no flag-only
success. No build, test, SQL, import, runtime, push or deployment ran.
[Source contracts and exact handoff](evidence/phase-a-world-battles-20261016/README.md).

Previous Phase A continuation (2026-10-16), baseline `99d029f`: the original
server-only physical/logical channel projection now fences native Map ownership
and World peer authorization, **INTEGRATED_UNVERIFIED** within the documented
single-source-group/at-most255-key scope. Routing activation now has offline
Map/World barriers and stale-token retirement. Source battle selectors are
corrected to TOURNAMENT2/MISSION3/SKYGARDEN4. Written076/grants are unexecuted;
recorded044–076 remain unapplied, next077. The network still uses the explicit
RW-as-Map extension; source MW_CONNECT_ACK/full bootstrap and full Phase A remain
**IMPLEMENTING**. No build, test, SQL, import, runtime, push or deployment ran.
[Source contracts and exact next dependencies](evidence/phase-a-map-topology-20261016/README.md).

Previous Phase A continuation (2026-10-16), baseline `c379231`: native Map RPS
start/item debit, source round calculation, real World limit request/result,
giveup/terminal reward mail and transient Control cache updates are wired into
the current PostgreSQL gameplay path, **INTEGRATED_UNVERIFIED** for ordinary
ready primaries. Local counters follow actual object lifetime and are not
invented transfer/save fields. Written075 and Map grants are unexecuted;
recorded044–075 remain unapplied, next076. Full RPS parity (BR/late replica),
source Map admission/native World bootstrap and full Phase A remain OPEN.
No build, test, SQL, import, service, client, container, push or deployment ran.
[Source contracts, deviations and deferred acceptance](evidence/phase-a-map-rps-20261016/README.md).

Previous Phase A continuation (2026-10-15), baseline `0533c73`: native World RPS
catalog/history hydration, atomic expiry/win batches, source calendar conversion
and serialized Control updates are wired into the existing World paths,
**INTEGRATED_UNVERIFIED** in that World-only scope. Complete RPS gameplay
remains **IMPLEMENTING** because native Map start/round/reward and reply
consumers are missing. Written074/import/grants are unexecuted; written044–074
remain unapplied, next075. Full Phase A, source Map admission, PostgreSQL-only
World bootstrap and original-client acceptance remain OPEN. No build, test,
SQL, import, service, client, container, push or deployment occurred.
[Source contracts and deferred acceptance](evidence/phase-a-world-rps-20261015/README.md).

Previous local Phase A work (2026-10-15), based on `7a8dfee`, implements the
source3604 whole-handler return when current learned3603 is absent. ITEMUSE/ACT
retain initiation, activity, reuse and debit; eligible FINISH records an explicit
terminal no-effect receipt, without Defend, MISS802 or CheckEquipSkill. A present
rank0 remains distinct. Already-dead own-PC pure HP/MP FINISH now skips targets
and runs its actual equipment epilogue for accepted ordinary/loop/ACT origins.
**INTEGRATED_UNVERIFIED**; written073 adds nullable terminal disposition with
no historical backfill. Written044–073 are unapplied; next074. No build, test,
SQL, import, service, client, container, push or deployment ran. Full Phase A,
secondary vitals, source admission/native World bootstrap and general combat
remain OPEN. [Source contract and deferred acceptance](evidence/phase-a-finish-terminal-20261015/README.md).

Previous code `7a8dfee` (2026-10-15), based on `5f81b9f`, rechecks the full
Ready session identity/role/epoch after checkpoint waits and gameplay-lease
acquisition, so a request cannot continue under a different ownership tenure.
The initial replica opcode filter remains. Native CURE exceptions now admit
128/229 only with actual maintained129/230 absence; 3604 uses the current actual
learned3603 rank, including0, for calculation and DEFEND only. Trigger critical
and outgoing powers/attack level remain distinct. Init/ACT preflight and each
eligible FINISH target share this resolution; MISS still resolves exception
context before skipping numeric work, while empty/dead targets do not resolve it.
Missing3603 on a nonempty target and richer partner erasure remain unavailable.
**INTEGRATED_UNVERIFIED**: static source/API/SQL-text/packet/diff review only;
no build/test/SQL/import/service/client/container execution, push or deployment.
No new wire/schema/publication/grant: written044–072 remain unapplied; next073.
Secondary vitals convergence, general foreign/negative combat, richer exception
effects and actual published-data/original-client acceptance remain OPEN.
[Native cast authority and source exceptions](evidence/phase-a-native-cast-authority-20261015/README.md).

Previous code `5f81b9f` (2026-10-15), based on `1a9fa82`, integrates final-HP0
completion for the bounded native own-PC positive-template CURE. After complete
ordered rows and the target cap, actual zero-MP cleanup precedes LASTHIT4 DEFEND
and conditional HPMP with wire HP0/MP0; silent ordinary death then emits DIE to
neighboring cells. Later dead-target occurrences skip hit/RNG/clock work while
terminal equipment cleanup remains. Existing atomic FINISH/core/effect/fullgraph
receipts and current party/recipient output authority remain in force.
Death requires the actual ordinary actor/graph and existing verified070 map
context. Native constructor/admission/dispatch proof bounds active transient
state; historical duel records are not active-duel authority. Shared death-state
internals preserve DROP's separate ACK, old-MP and recovery+5000 behavior.
**INTEGRATED_UNVERIFIED**: static source/API/SQL-text/packet/diff review only;
no build/test/SQL/import/service/client/container execution, push or deployment.
No new schema/publication/grant: written044–072 remain unapplied; next073.
Active duel/ANGEL, rich entities, special-mode death, negative-template/foreign
combat, hardcoded effects and actual original-client acceptance remain OPEN.
[Bounded CURE death and deferred acceptance](evidence/phase-a-cure-death-20261015/README.md).

Previous code `1a9fa82` (2026-10-15), based on `e2e4784`, admits signed
numeric HP/MP deltas for the existing odd-positive-template own-PC pure CURE.
The shared Delta helper accepts ±143165576 after the actual combine; FINISH
computes signed variance with division toward zero, converts the completed
sample to DWORD and preserves resource addition modulo2^32 before the final
per-target cap. Negative numeric values do not change template classification
or introduce damage/aggro/battle-mode behavior. Final HP must remain>0.
Init/ACT preflight and normal ordinary/loop/ACT FINISH share current-rank/maxima
calculation. MISS/empty skip numeric work; real zero/default rows retain RNG.
Existing item accounting, atomic receipts/checkpoints and current party/recipient
authority remain in force. No new wire field, schema, publication or grant.
**INTEGRATED_UNVERIFIED**: static source/API/SQL-text/packet/diff review only;
no build/test/SQL/import/service/client/container execution, push or deployment.
Written044–072 remain unapplied; next073. Final-HP0 Defend death, negative-template
combat, unsafe signed operations, hardcoded effects, foreign targets and actual
original-client/CRT acceptance remain OPEN.
[Signed numeric CURE and deferred acceptance](evidence/phase-a-cure-signed-20261015/README.md).

Previous code `e2e4784` (2026-10-15), based on `cde2ded`, implements actual
Win32 DWORD modulo product/subtraction and living HP/MP resource addition.
Combine bits are projected to signed INT before the final positive variance
bound; percent divides the reduced DWORD product. Resource additions wrap per
row, then clamp once per Defend target. Final HP0 is refused before cleanup,
output or writes; intermediate row HP0 may be restored by a later row.
Actual unrecognized Calc uses source GetValue0 before its selected combine;
unrecognized combine retains Calculate0 only after GetValue. Real rows keep
RNG, Defend, cleanup and one-use receipts. Init/ACT and normal native FINISH
share current-rank/maxima calculation; MISS/empty skip numeric evaluation.
**INTEGRATED_UNVERIFIED**: static source/API/SQL-text/packet/diff review only;
no build/test/SQL/import/service/client/container execution, push or deployment.
No schema/publisher/grant change: written044–072 remain unapplied; next073.
Negative final deltas, final-HP0 death, unsafe INT operations, hardcoded effects,
general combat, other targets, CRT/libm and original-client acceptance remain OPEN.
[Source DWORD arithmetic and actual default branches](evidence/phase-a-cure-dword-20261015/README.md).

Previous code `cde2ded` (2026-10-15), based on `09317c7`, adds the known
CURE combine modes to the bounded own-PC action-item path. Checked signed
Calc0..3 values feed actual source INCREASE, DECREASE, MULTIPLY, DIVIDE and
PRECENT rules against current maximum HP/MP. DWORD factor conversion and
integer-product-before-percentage order are preserved; product/subtraction
wrap and negative final deltas remain unavailable. Final nonnegative deltas
retain the source all-roll variance bound. Init/ACT preflight and every normal
FINISH origin use the same checked calculation; the unsafe ordinary fallback
is removed. MISS and empty FINISH skip cure arithmetic and randomness.
**INTEGRATED_UNVERIFIED**: static source/API/SQL-text/packet/diff review only;
no build/test/SQL/import/service/client/container execution, push or deployment.
No schema/publisher/grant change: written044–072 remain unapplied; next073.
Wrapped/negative effects, hardcoded128/229/3604, general combat, other targets,
CRT/libm boundary parity and original-client acceptance remain OPEN.
[Actual cure combine semantics and checked completion](evidence/phase-a-cure-combine-20261015/README.md).

Previous code `09317c7` (2026-10-15), based on `8ed424c`, adds bounded
exponential Calc2 values to actual own-PC action-item cures. It reads the
outgoing skill's start/next levels and actual formula34 coefficient, preserving
the source FLOAT conversion and rank0 exponent0. The preceding stat refresh's
formula reader also checks FLOAT range before narrowing X/Y rates. A checked cure path
rejects nonfinite/unrepresentable conversion, negative integer magnitude and
values above the source all-roll INT variance bound before hooks/debit or
normal FINISH arithmetic. MISS and empty FINISH skip that evaluation. Existing
ordinary INCREASE cures use the same bound; other combine modes are unchanged.
**INTEGRATED_UNVERIFIED**: static source/API/SQL-text/packet/diff review only;
no build/test/SQL/import/service/client/container execution, push or deployment.
No new schema/publisher/grant: written044–072 remain unapplied; next073.
Other combines, negative/wrapped effects, hardcoded128/229/3604, general combat,
other targets, CRT/libm boundary parity and original-client acceptance remain OPEN.
[Actual exponential metadata and checked cure arithmetic](evidence/phase-a-actitem-exponential-20261015/README.md).

Previous code `8ed424c` (2026-10-15), based on `5da1bf0`, admits action-item
cures with an actual learned trigger, including a present rank0 object. FINISH
reads its current rank and baseline critical selected by the trigger's type;
it keeps the outgoing cure's own powers/type/attack level. ACT acknowledgement
rank1, zero powers and critical0 remain distinct, with no ordinary trigger
cost, reuse or charge. Missing trigger remains rank0/critical0; missing metadata
for a present trigger is unavailable.
Actual Calc0/1/3 INCREASE rows retain flat, linear or falling source values at
that rank. Initiation/ACT preflight normal negative magnitudes before hooks or
debit; MISS and empty completion do not calculate a cure. Both actual mode2 and
trigger critical255 retain the existing actual802 capability checks.
**INTEGRATED_UNVERIFIED**, static source/API/SQL-text/packet/diff review only; no
build/test/SQL/import/service/client/container execution, push or deployment.
No new migration: written044–072 remain unapplied; next073. Exponential Calc2,
other combine modes, wrapped/negative effects, hardcoded128/229/3604, general
combat, other targets and actual original-client acceptance remain OPEN.
[Actual trigger rank, critical and cure values](evidence/phase-a-actitem-trigger-20261015/README.md).

Previous code `5da1bf0` (2026-10-15), based on `391f499`, implements the
source MISS branch of the admitted own-PC positive cure. Actual GOHOME2 or
critical255 now consumes its accepted finish, emits the original MISS DEFEND
without cure arithmetic/randomness, and invokes actual ForceMaintain802 after
zero-MP cleanup. Missing802 follows the source no-effect branch; an existing
richer802 definition remains unavailable instead of becoming an inert marker.
The supported actual positive STATUS_NONE802 retains duration/static/erase flags,
independent duplicate clocks, expiry, cancellation, death and native persistence.
ACTION/ordinary cast/ACT erase actual flagged800/802 rows; LOOP keeps its source
exception. Empty target lists perform no hit, cure or802 force for every origin.
**INTEGRATED_UNVERIFIED**, static source/API/SQL-text/packet/diff review only; no
build/test/SQL/import/service/client/container execution, push or deployment.
No new migration: written044–072 remain unapplied; next073. Richer802/status
semantics, general damage/BLOCK/aggro/SoulHP, other targets, bootstrap and actual
original-client acceptance remain OPEN.
[Source MISS, actual802 lifecycle and deferred acceptance](evidence/phase-a-native-self-miss-20261015/README.md).

Previous code `391f499` (2026-10-15), based on `42451e1`, permits the
already admitted positive own-PC HP/MP cure in a basic native party, including
SOLO. FINISHSKILL retains the supplied party WORD as source-unused metadata for
this pure CURE profile; it cannot change membership or choose recipients.
ACT initiation/consumption no longer requires party0. Actual local party context
and current actor/recipient authority are rechecked across publication awaits;
World still derives MANSTAT recipients from its own current membership and SQL
peer authority. Ordinary ACTION replies use the same current grid/nearby guard.
Shared finish refuses source GOHOME2 hits while retaining empty ordinary/loop
completion and cleanup.
**INTEGRATED_UNVERIFIED**, static source/API/SQL-text/packet/diff review only; no
build/test/SQL/import/service/client/container execution, push or deployment.
No new migration: written044–072 remain unapplied; next073. Other-target/group
healing, full corps/guild/special-mode effects, combat and original-client
acceptance remain OPEN.
[Party self-cure contract and deferred acceptance](evidence/phase-a-party-self-cure-20261015/README.md).

Previous code `42451e1` (2026-10-15), based on `a3fa3ef`, connects
original ITEMUSE action initiation, ACTITEMSUSE item consumption and FINISHSKILL
completion for an actual partyless own-PC positive instant HP/MP cure. The
initializer does not debit or arm reuse; ACT preserves source activity/erase,
strict cooldown and late NEEDITEM ordering. Source wire rank1 remains distinct
from the actual unlearned-trigger effect rank0. Successful consumption and a
one-use accepted cast commit with the existing native authority/checkpoint path;
nearby output rechecks current actor/recipient authority after awaits.
Written071 records cast origin and actual ACT route; written072 and a separate
offline publisher require complete verified guild-skill nonmembership, including
for ordinary FINISHSKILL. Both migrations are **not applied**; next073.
Gameplay is **INTEGRATED_UNVERIFIED**, publication tools/SQL are
**IMPLEMENTED_UNVERIFIED**. Static source/API/SQL-text/diff review only; no
build/test/SQL/import/service/client/container execution, push or deployment.
Full action-item combat, party/learned-trigger/richer effects, source DWORD wrap,
bootstrap and actual original-client acceptance remain OPEN.
[Action-item source contract, publication and deferred acceptance](evidence/phase-a-native-actitems-20261015/README.md).

Previous code `a3fa3ef` (2026-10-15), based on `d5377dd`, integrates
source positive maintained-status collisions and permits ordinary native revival
with retained static800. Actual STATUS_NONE0 does not collide: another800 is
appended with its own clock, without invented END/STAT or resetting older rows.
Nonzero statuses compare actual type/attribute, priority and rank-one value in
source vector order; action/cast removal retains one event per erased row.
Cancellation and per-row erasure output use current native actor/recipient
authority before publication.
**INTEGRATED_UNVERIFIED**, static review only; no build/test/SQL/import/service/
client/container run, new migration (next071), push or deployment. Exact native
checkpoint/transfer multiplicity remains a documented recovery policy; source DB
save-only filters/first-eligible duplicate collapse, full combat/SoulHP, richer
effects/modes and original-client acceptance remain OPEN.
[Source collision, persistence distinction and deferred acceptance](evidence/phase-a-revival-replacement-20261015/README.md).

Previous code `d5377dd` (2026-10-15), based on `d86d407`, connects the
original own-PC environmental DROPDAMAGE request to native PostgreSQL HP/MP,
effects and checkpoints. The bounded ordinary death branch preserves source
DROP → HPMP (old MP) → DIE order, silent nonstatic-effect removal and dead ENTER
action. Historical prototype monster AI is isolated from native actors. Written070
adds explicit routing-bound battle-zone/GodBall publication; it is **not applied**.
Gameplay is **INTEGRATED_UNVERIFIED**; SQL/publisher/grants are
**IMPLEMENTED_UNVERIFIED**. Static review only: no build/test/SQL/import/service/
client/container run, push or deployment. At that revision, full combat/SoulHP,
special death, retained800 revival and original-client acceptance remained OPEN.
[Source contract, publication and deferred acceptance](evidence/phase-a-native-death-20261015/README.md).

Previous code `d86d407` (2026-10-15), based on `1fe7404`, integrates ordinary
native PvE resource recovery and the original one-BYTE CHGMODE request through
the existing PostgreSQL authority/operation/checkpoint path. Actual formula,
powered equipment and passive HPR/MPR rates feed independent three-second clocks;
entry, local replica handoff, battle delay/timeout and supported cast/self-Defend
hooks retain their source distinctions. **INTEGRATED_UNVERIFIED**: static review
only; no build/test/SQL/service/client/container run, migration (next070), push or
deployment. Full combat/death/SoulHP, unsupported effect/guild/companion graphs,
other mode lifecycles and original-client acceptance remain OPEN.
[Source contract, local clocks and deferred acceptance](evidence/phase-a-native-recovery-20261015/README.md).

Previous code `1fe7404` (2026-10-15), based on `1c83b5f`, moves native
REVIVAL out of the cache-only full-heal prototype and into the current PostgreSQL
mutation/checkpoint path. The bounded same-cell primary path uses actual source
30/40% bars, aftermath/stat/mode replies and validated protection800. Stored
penalty steps survive hydration; the local decay clock is reconstructed without
new wire fields. **INTEGRATED_UNVERIFIED**: static review only, no build/test/SQL/
service/client/container run, migration (next070), push or deployment. Cross-cell
AOI/replica/mode variants, unsupported800 data and BYTE decay underflow remain
OPEN, together with full death/combat/SoulHP/rewards/bootstrap and client acceptance.
[Source contracts, supported scope and deferred acceptance](evidence/phase-a-native-revival-20261015/README.md).

Previous code `1c83b5f` (2026-10-15), based on `199fa5e`, repairs native
Login/Map account and session identity binding. Source INTEGER columns preserve
DWORD bits as signed32 values; operational BIGINT receipt/unlock keys retain
positive wire values. Actual authenticated character lobby and START, Map claim,
replica, load, checkpoint, transfer, mutation authority and release paths retain
their existing ownership/atomicity fences. **INTEGRATED_UNVERIFIED**: static
source/API/SQL-text/diff review only; no build/test/SQL/service/client/container
run, new migration (next070), push or deployment. The positive-only character
allocator is unchanged; full high-bit character allocation, original bootstrap,
party/gameplay dependencies and client acceptance remain OPEN.
[Exact storage domains and deferred acceptance](evidence/phase-a-native-identity-20261015/README.md).

Previous code `199fa5e` (2026-10-15), based on `b251cdf`, permits supported
native statistic/equipment transactions in a basic party using actual effect,
guild/local, companion and recall capability checks. World retains only real
party DEL events which never reached a send attempt, drains them after current
SQL Ready proof, and publishes final attributes to the primary even when a
replica triggers recovery. Pending deltas gate new membership; route readiness is
rechecked after the awaited repair. No uncertain-output retry or roster reset.
**INTEGRATED_UNVERIFIED**: static review only; no build/test/SQL/service/client/
container run or new migration (next070). Full roster/bootstrap, high-bit identity,
party rewards/combat and other unported gameplay remain OPEN. Local Git only;
no push. [Source contracts, changes and deferred acceptance](evidence/phase-a-party-stats-20261015/README.md).

Action-item and admitted ordinary/loop cures preserve signed variance, source
DWORD product/subtraction/resource modulo, actual BYTE defaults and ordered rows.
Their final-HP0 branch now completes ordinary native death with LASTHIT DEFEND,
conditional wire HPMP0/0, silent nonstatic release and neighbor DIE, after actual
zero-MP cleanup. Subsequent dead-target occurrences skip numerical/clock work;
terminal CheckEquipSkill remains. On lethal completion, existing070 publication
and shared ordinary actor/companion/recall/local bounds join the current primary,
statistic and effect authority already required for all FINISH requests.
Unsupported completion still refuses
atomically, without refunding or replaying an earlier ACT debit.

Continue actual active-duel, ANGEL/companion, GodBall/local/castle/arena/tournament,
recall/self/transform and source-native interoperability as separate gameplay.
Current native constructor/admission/dispatch absence is a bounded capability
proof, not implementation of those systems; every new activation producer must
extend the death state/authority contract before admission. Historical duel
records and inactive pet inventory must never substitute for actual live state.
Keep origin-specific DROP versus Defend packets/recovery clocks and the current
per-packet audiences; do not merge them into a generic damage adapter.
See [source and implementation evidence](evidence/phase-a-cure-death-20261015/README.md).

The current pure-CURE path handles128/229 with proven actual129/230 absence
and3604 with current learned3603 effect rank. Continue actual129/230 notifying
erasure and richer target effects only with their complete stat/effect contract.
Missing3603 on a living nonempty target now follows the source whole-handler
return with an explicit terminal no-effect073 receipt. Do not add Defend,
equipment cleanup, refund or replay of an earlier ACT to that branch. Empty
and already-dead targets skip the override and retain terminal equipment cleanup;
already-dead ordinary/loop/ACT FINISH is now admitted with the existing accepted
receipt and current owner/graph fences. Dead ACT initiation remains unported.
Keep trigger critical/outgoing powers and3603 calculation/DEFEND rank distinct,
including actual rank0. See [terminal contract](evidence/phase-a-finish-terminal-20261015/README.md).

The existing dispatcher already filters secondary opcodes; the new post-wait
identity/role/epoch fence prevents a waiting request from crossing ownership
tenures. It does not implement secondary HP/MP replication. Source ENTER has no
own-PC bars, HPMP/DIE client handlers do not write another Map cache, and current
checkpoint revision is not a sequence for every gameplay mutation. Any broader
native vitals projection needs a separate authority/order contract and must not
activate payload-null prototype gameplay or invent a client/peer packet.
See [source contracts and deferred acceptance](evidence/phase-a-native-cast-authority-20261015/README.md).

Continue actual negative-template or foreign-target effects without borrowing
ACT wire rank1. CRT/libm boundaries, real published exception eligibility,
ordinary death/revival and signed resource/client behavior require Phase B
acceptance. These changes do not close the full combat/AI/reward gate.

The admitted own positive-cure MISS path now uses actual source802 metadata.
Continue richer802 data/status/ability and BLOCK801 rather than guessing a marker
or emitting success; keep actual other-target/negative-damage authority, aggro,
SoulHP and mode-specific lifecycles separate. Missing802 is a real source branch,
not evidence that a present unsupported definition can be omitted. Actual data
and original-client acceptance of the admitted profile remain unexecuted.

Next dependencies include full source movement/AOI and replica/mode revival,
unsupported800 templates and aftermath underflow/normalization. Implement the
actual negative Defend/positive-PC combat-damage producer before cross-actor
SoulHP; it is not a revival bonus. Own-PC environmental damage now has its real
producer and deliberately never invokes Defend/SoulHP. Retained static800 now
follows source STATUS_NONE append semantics during ordinary revival; full
original-client duplicate presentation and fresh DB save-filter/first-eligible
collapse parity remain required. Do not replace source live/transfer duplicates
with a guessed single timer or apply DB-only deduplication to every checkpoint.
Party/monster XP, complete client roster and source Map admission/bootstrap
remain required. Full high-bit character allocation still needs a
source/schema/sequence policy for the positive-only012 allocator. Preserve both
INTEGER identity and unsigned BIGINT receipt key domains; do not replay unknown
writes or fake roster clearing.

Ordinary native recovery and manual/battle mode now have a concrete production
path; full STOPRECOVER and other effect graphs, companion/guild/local rate
dependencies, source wrapped numeric branches and broader ACTITEMSUSE/nonself
damage activity producers remain separate implementation work. The positive
own-PC action-item producer now admits basic-party self cure; continue actual
negative, other-target/group and richer source effects without
granting accepted finishes for failed consumption. Source failed-ACT used-record behavior
and permissive repeated finishes are distinct from the native one-use policy.
Retain the original target-local mode/recovery clocks at existing-replica handoff;
do not append unbacked clocks to the transfer wire or add a guessed sit-down healing bonus.

Previous code `b251cdf` (2026-10-14), based on `622fb2e`, connects manual
party chief/type changes and repairs post-ready attributes for both primary and
replica. The existing ENTERCHAR identity is repeated only after SQL/local Ready;
World verifies native primary/replica ownership and republishes actual ATTR,
without importing unknown replica vitals or repeating connection verdicts.
Handoff start waits for party decisions/fan-out; Map admission releases its lease
at Ready before the independent recovery/AOI path. Source chief result10 and
TYPE main-only result0/no-ATTR ordering are preserved. **INTEGRATED_UNVERIFIED**:
static review only, no build/test/SQL/service/client/container run or new migration
(next070). Full roster reconciliation, source admission/bootstrap and full
party/gameplay dependencies remain OPEN. Local Git only; no push.
[Source contracts, differences and deferred acceptance](evidence/phase-a-party-recovery-20261014/README.md).

Previous code `622fb2e` (2026-10-14), based on `2e406dc`, connects source
basic party ADD/JOIN/DEL, succession/dissolution/logout cleanup, primary/replica
ATTR and native HPMP/level MANSTAT. Normal party chat uses the existing full-text
pipeline and current World membership. Typed peer ownership, native SQL pair
proofs, one-use invitations and serialized membership guard suspended operations.
Post-CONREADY MANSTAT repairs real World ATTR after handoff; source wire fields
remain unchanged. World logout now retains source historical account counts.
**INTEGRATED_UNVERIFIED**: static review only, no build/test/SQL/service/client/
container run and no new migration (next070). Source Map admission/full bootstrap,
party rewards/equipment/corps/guild/arena/instance/mode branches and complete
Phase A remain OPEN. Local Git only; no push.
[Source contracts, differences and deferred acceptance](evidence/phase-a-native-party-20261014/README.md).

Previous code `80b7698` (2026-10-14), based on `acbcaaf`, ports actual
Map–World and Control–World links to source Plain16, preserving source receive
semantics and current-socket/no-replay guards. The modern RW catalog ACK now reads
DWORD message keys. Control service types follow linked CTProtocol exactly
(World3/Patch5/Log7/Relay8), and World monitoring counts actual raw sessions.
**INTEGRATED_UNVERIFIED**, static review only; no build/test/SQL/service/client/
container run or new migration (next070). Original Map MW_CONNECT_ACK admission
and full bootstrap remain OPEN: the current RW-as-Map path is a modern extension.
Party/group producers, other service/operator framing, TLS and complete gameplay
remain required. [Source contracts, differences and deferred acceptance](evidence/phase-a-world-framing-20261014/README.md).

Previous code `acbcaaf`, based on `0b97c18`, connects direct
Control–Map administration through the original unencrypted16-byte CPacket.
Configured/current Control transport reaches announcements, ready primary/replica
character kicks, real four-DWORD service monitoring and historical active-user
reset. PostgreSQL checks use live Map ownership/exact claims; ordinary save/release
retains uncertain reservations. Periodic probes replace the missing producer and
response echo loop; kick audit records requested intent. **INTEGRATED_UNVERIFIED**,
source review only, no build/test/SQL/service/client/container run. No new migration;
next070. Other modern8 service/operator framing, TLS, direct Map event replay,
other admin operations and full gameplay/bootstrap remain OPEN. [Source proof,
framing correction and subsequent acceptance](evidence/phase-a-control-map-20261014/README.md).

Previous code `0b97c18`, based on `c389976`, connects original
Map fallback/local chat, TMS, native bans and operator authority, targeted admin
notices and event text into the existing Map/World dispatchers and PostgreSQL
claim fences. Written069 adds exact operator publication and durable raw-name ban
and audit state; the pinned source operator table has zero rows, with no invented GM
IDs. Current actors, protected CHAT1/INVITE2 and admission projections are guarded.
These paths are **INTEGRATED_UNVERIFIED**; SQL/import/grants are
**IMPLEMENTED_UNVERIFIED**. No build, test, SQL/tool, service, client or container
ran. Connected Relay, group topology, BR/BOW branches, mutable operator tools and
full native World/bootstrap/gameplay remain OPEN. [Source mapping, deliberate
repairs and deferred acceptance](evidence/phase-a-chat-tms-20261014/README.md).

Previous code `c389976`, based on `75125a2`, integrates atomic soulmate SEARCH/REGREADY/REG,
ready-primary initial level-gap END and verified native guild-level publication/
loading. Written067/068 are not applied. No build or execution occurred.
World also uses existing059's complete51-row message publication for relay,
event operator names and source TMS envelopes; Map chat/TMS/operator consumers
remain open. Exact maintained sampling is connected at periodic checkpoint,
logout and transfer, with general expiry producers/save filters still open.
[Exact source contracts, differences and subsequent verification](evidence/phase-a-source-review-20261013/README.md).
Continue native guild lifecycle/Map hydration and the remaining World bootstrap;
complete party reward/combat producers before soulmate XP/emergency HP. Missing
full private manifests, Patch pre-version SQL and unchanged client assets remain
specific external inputs, not reasons to drop those functions from scope.

Earlier code `1f1eb59` / `e709281` adds native World ownership and directed friend/group/
soulmate persistence (written migration066), original Map friend/group relays,
conditional soulmate FRIENDLIST fields and Map soulmate state/end handling.
Native social/mail transactions fence the current World; social writes also
validate primary Map/session claims. Commits precede cache/replies; uncertain
writes stop World without replay.
The full historical social importer/grants and offline World barriers are written
only. Production World now requires an explicit database, while native guild/war/
event bootstrap and complete soulmate Map economy/XP/load checks remain unfinished.
Map/World compile/link in Debug and Release; no tests, DB or runtime ran.
These scoped paths are **INTEGRATED_UNVERIFIED**, and Phase A/PG-only runtime are
still incomplete. [Source contracts, repairs and compile evidence](evidence/phase-a-native-social-20261013/README.md).

Historical baseline at `2fa2396` (2026-10-11): account secure-code lifecycle is integrated
through all seven original requests, migration042, protected native learning and
inventory/equipment, CHARINFO, fresh/full-graph persistence and current-owner
fencing. Migration043 preserves account security and its audit across low-level
character deletion. See `security-contract.json` for executed verification and explicit
modern changes (initialized enabled flag, durable expiry, disabled-state transfer,
strict byte input and commit-before-response). Backups and migrations001–043 are
immutable. Phase A has added unapplied migrations044–073; the next free number is074.

Earlier code `11e0df1` / `dd07164` repairs both mutual-friend acceptance
paths: two source directed edges commit together before cache/success responses,
with registered ready-primary checks before persistence and after await. Any
mutual persistence failure stops World without a write replay. World compiles
in Debug/Release; no tests or runtime ran. This preserves the existing schema
adapter and **does not complete native friend schemas/imports, World ownership
or the other best-effort social paths**. [Scope and remaining native work](evidence/phase-a-mutual-friends-20261013/README.md).

Code `e84ece4` / `65be6d1` integrates quest SENDPOST and native offline bill
returns. QUESTEXEC/child hooks select original rewards into the fenced reserved
queue; World returns due offline bills with authoritative item ownership and
notification/audit in one transaction. Written064/065 and `--quest-mail` are
unexecuted. Map/World compile/link in Debug/Release; no tests, DB or runtime ran.
The backup appearance omission is explicitly repaired from CTItem; source gem/
grade losses and distinct online/offline sender-ID rules are preserved. Full
Phase A, PostgreSQL-only World bootstrap and original-client acceptance remain
incomplete. [Contracts, differences and compilation evidence](evidence/phase-a-quest-mail-offline-bills-20261013/README.md).

Active next source paths after this increment:

The ordinary native RPS chain now has its real Map producer/consumer and reward
mail path. Preserve075 audit-only local state, source packet bodies, no generic
ITEMUSE/quest credit, original thresholds/next-key lookup and actual constructor
lifetime. Continue late World decisions during replica/main transfer using real
role/result authority; then BR mode. Do not mark these variants implemented or
invent a successful decision/refund. Full source-bootstrap prerequisite below
remains the next cross-system dependency. Written075–077/grants are unexecuted; next078.

The076 topology and077 World battle/time publication, timer and batch paths now
exist. Next port actual Map TLOCAL construction from TMapSvr.cpp3150–3440 and
its real zone ownership, monster/spawn/AI, switch, occupation persistence and
reward dependencies, then LOCAL/CASTLE/MISSION/SKYGARDEN enable consumers.
The Map dispatcher currently refuses these paths explicitly; a scheduled battle
can retire the partial World link and drain clients. This is a missing feature,
not completed support. Do not replace real regions with empty vectors or accept
only status flags. Source mission undefined branches/order profiles remain
explicit limitations. Written077/grants/publisher are unexecuted; next078.
Then complete guild/tactics history/expiry, native rank/tournament and the other
ordered initial states before switching RW-as-Map to source MW_CONNECT_ACK.
Never synthesize a registration ACK or claim full OnTimer/bootstrap parity.

Cross-cutting bootstrap prerequisite: original Map MW_CONNECT_ACK admission and native
World bootstrap. Continue from Server/TMapSvr/{TMapSvr.cpp::LoadData/InitNetwork;
SSSender.cpp::SendMW_CONNECT_ACK;DBAccess.h channel queries} and
Server/TWorldSvr/SSHandler.cpp::OnMW_CONNECT_ACK, then existing AsioWorldClient,
World OnRelaysvrReq/peer registry/main/schema_validator and Map bootstrap consumers.
Use076's actual pinned TCHANNELCHART/TSVRCHART/TMAPCHART projection and the exact
active build flags. Preserve MAKELONG map/channel keys versus BYTE serialized
keys/counts; unresolved >255-key and cross-group profiles still require data proof.
No invented channel list or registration acknowledgement. The
source and preserved publication tools exist; a complete fingerprinted data
release and topology are required to close any data-dependent ambiguity. Keep
unavailable procedure bodies as specific blockers and implement independent
bootstrap parts. Full source World/Map
bootstrap remains the first dependency: source sends no registration ACK, whereas the
current native RW-as-Map exchange remains a documented extension. Resolve source
channel key/count casts using exact data/topology proof; do not infer a channel
list or synthesize acknowledgement. Plain16 transport is now integrated on these
links. Continue source16 framing for remaining Control/operator/service peers.
Preserve actual CS16 client crypto; retained modern8 fixtures do not establish
original framing parity. Direct Map event/cash producer/consumer
conflict, other Control commands/TLS, admin-shell queued ownership and source
combat/logout/transfer kick behavior remain required. The bounded five-operation
Control–Map path is integrated, unverified; no permission wait or Phase B startup
replaces those remaining implementation tasks.

1. Complete native Map guild/party/corps/tactics membership producers and source
   chat targets;BR team whisper/party and BOW peace authority;source Relay path
   and RW ban propagation if its missing source contracts are found. Implement
   native mutable operator tools/reset without inventing historical GM IDs and
   finish Control result-loss/recovery correlation. Bounded Map/World chat/TMS,
   targeted/event text and SQL protection are now integrated, unverified; event
   text alone does not complete event reward/state machinery.
2. `TWorldSvrAsio/db/schema_validator.cpp`, World main and native ownership:
   replace the remaining legacy guild/war/event/rank/item/service bootstrap and
   repositories. Native friend/group/erase and World process ownership are now
   integrated; Map soulmate economy and ready-primary initial-load level checks
   are now connected, unverified. Complete soulmate party XP/emergency HP,
   native guild lifecycle/Map hydration and remaining World bootstrap.
3. Quest DROPITEM/SPAWNMON/TELEPORT/SWITCH/DIEMON/DEFENDSKILL/REGEN plus non-request
   kill/position/map/monster producers; preserve completed SENDPOST and rewards.
4. Other admin/event/BR/rank reserved-mail producers; notification retention and
   historical staged-save/obsolete attachment reconciliation. Offline bill
   returns are integrated, not executed or accepted.
5. Complete all other Phase A gameplay/operational responsibilities. Then perform
   the whole static implementation gate, Phase B and Phase C in that order.

The older increment notes below retain their original scope. The gaps for
SENDPOST, offline bills and native friend/group ownership are superseded by
the current code above; complete native World and soulmate gameplay remain open.

Code `45d130d` integrates the World minute timer and native reserved-mail delivery:
source batch limit30, new item IDs at delivery, raw attributes, post/status and
Map notification receipts commit together. Written063, grants and historical
queue importer are unexecuted. Map/World compile/link in Debug/Release; no tests
or runtime were run. Quest/admin/event/BR/rank producers and complete native
World bootstrap remain open. [Scope, source conflict and compilation evidence](evidence/phase-a-reserved-mail-20261013/README.md).

Code `85bf923` / `feea4a1` / `b4ff581` now integrates ordinary player POSTSEND and native
protected-list CRUD/load/transfer; enqueue receipts and old-recipient superseding
are transactional. POSTRECV uses the recipient Map’s current live second. Current compiled scope is Map
and World Debug/Release; no Phase B/C execution. Continue with:

1. Native World ownership/social repositories, including atomic friend erasure
   behind PROTECTEDCHECK; the existing World path still uses legacy SOCI tables.
   Chat/invite consumers are now connected in the069 increment; continue
   deal/duel consumers and trade reservations.
2. Reserved delivery is integrated by45d130d; continue its quest/event/admin/BR/
   rank producers. Follow `QuestSendPost.cpp` and `TQuestSendPost.sql`, publish
   original TQTITLECHART with source SQL VARCHAR conversion and prefix/truncation.
   Repair the proved omitted/no-default wMoggItemID contract explicitly; the
   omitted bGradeEffect selects source default0. Preserve SENDPOST's RANDOM
   behavior (no accumulated-sum break) and the distinct ephemeral/delivery IDs.
   Native World startup/schema and all other repositories still need completion.
3. Offline bill expiry and `TJobsPostReturn` reconciliation; its obsolete
   TPOSTITEMTABLE branch must not overwrite current native STORAGE_POST authority.
   Complete notification retention and failure/concurrency acceptance later.
4. Remaining quest actions, combat/general effects/AI, economy/social modes,
   World repositories and operational integrations remain required by Phase A.

[Exact scoped contracts and continuation](evidence/phase-a-mail-send-protected-20261013/README.md).

Code `d3904c5` now integrates POSTRETURN/payment, sender-owned online bill expiry
and cross-Map notification receipts. The historical mailbox importer is written,
not executed; it explicitly blocks nonempty obsolete TPOSTITEMTABLE and requires
original byte/clock/codec profiles. The later `85bf923` implements ordinary sending/protected state; continue native
World reserved/quest/event/admin mail, trade reservations and offline bill-job
reconciliation on this same path. Do not start Phase B until the full
implementation gate is met. [Contracts and exact acceptance backlog](evidence/phase-a-mail-returns-20261013/README.md).

Code `18ee496` supersedes the carried-item/expired-bag-mail gap below: alarms,
expiry, native attachment ownership, read/view/claim/empty-delete and entry mail
counters are integrated, unverified. The later d3904c5 adds returns/payment and an import tool. Continue POSTSEND,
World reserved/quest/event mail and offline expiry/reconciliation on this same
native path, plus broader World/social persistence and remaining Map systems.
Preserve source pagination quirks and documented containment/delete/bill repairs.
[Exact source scope and deferred acceptance](evidence/phase-a-item-mail-20261013/README.md).

Code `3bc3bd1` connects NPC item/trainer lists and ordinary, quest, magic and PvP
purchases to the native transaction. Migration058 adds fenced PvP balances and
an explicit offline archival importer; CHARINFO and transfer retain the original
three point fields. Unknown-quest free purchases are explicitly rejected. BOW,
portal/monster shops, dynamic territory/discount rules, expiration and full PvP
remain open. The current078 boot continuation above supplies bounded actual
territory-country access; it does not implement ownership changes.
[Source scope and compile-only evidence](evidence/phase-a-npc-shops-20261011/README.md).
Continue the remaining mail producers above, remaining NPC variants,
full PvP records/rewards/rollover and broader World/social persistence. The balance
importer and all new migration/purchase/concurrency acceptance remain Phase B work.

The active continuation is now [full-rewrite-status.md](full-rewrite-status.md):
remaining premium/PCBang, complete monster EXP benefits, remaining item kinds and
general maintained-effect simulation/save filters after entitlement `fa4c282` and resource/item-cooldown
commit `ff50e58` (shared cooldown persistence is now implemented, unverified).
Code `c4385af` additionally connects Soul Lottery/820, MONEY/AP/special boxes,
ITEMSELL and DURATIONREP for the documented ordinary-player profile. All remain
unverified; special-box publication requires migration054 and a complete source
archive. `0457d3d` now connects ordinary REFINE/WASTEREFINE,912/913 consumables,
World numeric events and source CalcProb effect consumption to native persistence.
`750c797` orders Control frame writes and CRUD/tick/replay, restricts direct pushes
and prevents old peer cleanup from removing a successor. These are unverified;
see [source contracts and acceptance backlog](evidence/phase-a-refine-events-20261011/README.md).
Continue conditional economy discounts/probability bonuses and dynamic territory
mutations beyond the current078 boot registry, shop/deal/tournament producers
and general item skills. Native ordinary
gambling/upgrade branches and905/911/919/921/922/939 are now connected at0fe24d2;
see [contracts and exact remaining dependencies](evidence/phase-a-item-upgrades-20261011/README.md).
Complete their conditional territory/PCBang/BOW integration and broader economy
producers without re-running Phase B tests during Phase A. Complete monster event12/13 effects and Control crash/deactivation/
one-shot delivery; ordered socket writes do not provide application receipts.
Also continue
remaining quest action types, switch/territory state,
World persistence, Control event delivery/recovery and the unresolved Patch pre-version contracts.

Earlier backlog (still required where not superseded above):
Continue conditional trainer discounts/territory ownership, NPC talk quest
selection beyond the new bounded NPC talk path, general timed effects and
multi-target combat. Original default skill-log persistence is now implemented
in migration044 and the native purchase transaction, still UNVERIFIED. Preserve the completed title list/selection and original gold-title quirks
in `titles-contract.json`. Other title awards, resets and achievement conditions
remain required. Security must also be integrated into the remaining protected
economy handlers as they acquire native persistence. BOW entry/reset and secure-code
administrative recovery remain in scope. Any future character change that retains
a Login key must explicitly define the original player-object unlock boundary;
current native reconnect obtains a new non-recycled key. Clock-step behavior and
all original executable acceptance remain unresolved.

Resolved source dependency, implemented but unverified: default `DM_LOGSKILL_REQ` → `CSPLogSkill` →
`TLogSkill` → `TSKILLLOG` database path. `TMapSvr/stdafx.h:16` leaves
`DEF_UDPLOG` disabled; `UdpSocket.cpp:1042-1094` has the alternate skill UDP
implementation entirely commented. The backup `TLogSkill` inserts character,
three remaining currency fields, skill/rank and insertion time. The
native durable path is now in app_world.TSKILLLOG; the source has no gameplay
reader, only destructive admin cleanup. Generic Map UDP telemetry is not skill-log parity. Do not infer an active UDP skill sender from a call
inside the disabled preprocessor branch.

## Earlier scoped continuation notes

These notes preserve source investigations and dependent gameplay work. Their
old migration numbers and completed security gates are historical; use the
current schema and priorities at the top of this document.

Current source correction (2026-10-11): ordinary PC skills use
`CS_FINISHSKILL_ACK`, not the DEFEND path assumed by the preceding continuation.
`TClientGame.cpp:14050,28759`, `CSSender.cpp:2980` and
`TMapSvr/CSHandler.cpp:20197` establish this primary-connection request, target
filtering, two 32-bit BOOLs and server-side target iteration. Migration 036 now
integrates the first instant own-PC Cure transaction and a separate finish head.
See `skill-finish-contract.json`. Extend this existing implementation to remaining
FINISHSKILL effects/targets, charging/repeats/links, then exception DEFEND paths.
Migration 037 now integrates native ACTION gates, durable one-cast charge binding
and First Aid 240 self-finish after its 300ms source minimum. All three configurations
pass 3,101 native checks; 54 additive upgrade checks pass. This does not certify
full combat, original-client acceptance or general charge interruptions. Preserve the current
source corrections and explicit modern repairs in `skill-actions-contract.json`.
Migration 038 now integrates ACTION and self-cure finishes with existing 131/132
postures, Defend zero-MP removal, original audience/order, and atomic effect/finish
receipts. All three configurations pass 3,276 native checks; 65 upgrade checks
pass. Preserve `finish-effects-contract.json` and its bounded verification.
The permanent postures have zero attack/hide erase flags; ACTION preserves them.
Continue with original charge-speed/animation data, movement and damage
interruptions, general maintained-effect action side effects and multi-target ownership.
Aid 629 has main-recall targeting; its cure data alone does not authorize self-heal.
Preserve restored pure HP/MP percentage costs across ACTION/SKILLUSE/LOOPSKILL.
For effect removal, trace `EraseBuffByAttack` (TObjBase.cpp:4663): posture/recall
exceptions, BEA_ATTACK and hide-skill/erase-hide flags, ordered END/stat/HPMP
notifications and atomic recovery. Do not simply drop all maintained effects.
The shared database clock permits cross-owner timing but clock-jump behavior still
requires a documented guarantee and fault injection.
For other targets, establish mutable target ownership and atomic attacker/defender
updates before publishing effects. Preserve each original ordered effect row and
the DEFEND → conditional HPMP order. Cure's nearby-monster aggro and party relay
remain required even though this first self-resource transaction is integrated.
The generic client-authoritative native DEFEND prototype is explicitly blocked.
Do not treat the old attack diagnostic in the untracked audit directory as an
accepted gameplay contract or include unrelated audit files in these commits.

Retain the unresolved no-cast-ID ambiguity: an identical packet can represent a
later legitimate outstanding finish. Current same-login FIFO/subsequence budgets
bound effects to accepted casts, but do not prove universal replay discrimination.
No guessed expiry or new client field is a substitute for documenting this limit.
All backups and applied migrations **001–041** are immutable; next migration **042**.

Previous foundation (combat): migration **035** and the actual cast transaction
are integrated and pass 2,761 native checks per Debug, ASan/UBSan and installed
Release configuration, plus schema preservation and failure/recovery tests. Preserve
`accepted-casts-contract.json` and its bounded verification evidence. The per-character
accepted-cast chain now records request/ACK, costs/timers/items/effects and fences
stale snapshots. Preserve outgoing powers before ordinary zero-MP effect removal;
LOOPSKILL must retain the source distinction. Do not treat this ledger as hit
replay protection until Defend actually consumes ordered target occurrences.

After extending the primary FINISHSKILL path, make `CS_DEFEND_REQ` consume authoritative accepted-cast state and recompute
source hit/effect rules instead of trusting client powers. Original packets have
no new cast ID: reconstruct legitimate matching/order from the original client
and server before choosing an expiry/matching contract. Include primary transfer,
late packets, repeated targets, concurrent defenders and retries. No invented
matching window or client protocol extension is authorized. Preserve source-derived
power ordering and the mixed-attribute correction in `cast-powers-contract.json`.

Source audit for the next hit transaction: `CSHandler.cpp:1520–1610` resolves
`wTriggerID`, rejects server-only/map-restricted effects, and recomputes rank,
critical, countries and powers when the attacker is present; it has distinct
missing-attacker rules. Stored cast ACK powers therefore cannot universally
replace this calculation. `Client/TClient/TClientGame.cpp:14457` sends the
**defender's** action/animation IDs, not the cast's action IDs. Match authorization
without assuming these IDs are equal. Resolve `CheckDEFMSG`, target ownership,
trigger skills, pursuit overrides, area/delayed hits and cross-Map delivery before
claiming replay-safe complete hit compatibility. `CheckDEFMSG` at
`Client/TClient/TClientGame.cpp:22799` sends player-to-monster hits from the attacker
and hits against a player from that defender, after resolving recall/self owners;
`CSSenderAll.cpp:245–317` uses SayToAll. A universal sender==attacker check would
break original PvP. Record this distinction in the hit authorization tests.

Previous continuation (active effects): preserve migrations **032–034** and
`equipment-contract.json`, `postures-contract.json`, `effect-end-contract.json`.
Own-PC `CS_SKILLEND_REQ` now cancels permanent warrior postures atomically, preserves
source no-match ACK semantics and answers replica connections without writes.
Actual two-Map TCP carries a live posture, cancels it on the successor and returns
without resurrecting it. General effect cancellation, non-PC targets and complete
remote visibility are still required.

Continue the **authoritative combat/effect transaction**: `CheckEternalBuff` is
called inside original `CTObjBase::Defend` (TObjBase.cpp:1102), ordinary
`OnCS_SKILLUSE_REQ` (CSHandler.cpp:2972, after resource deduction, instance-power
calculation and `EraseBuffByAttack`) and `OnCS_ACTITEMSUSE_REQ` (CSHandler.cpp:20070).
The previous Defend-only conclusion was incomplete. LOOPSKILL and `OnTimer` do not
call it. Native bounded own-PC cures now use accepted-cast authority and durable
effects, including the action-item path; general negative Defend/combat remains
OPEN. Preserve ordinary cast powers before any effect removal,
and retain the atomic costs/zero-MP removal now integrated by 035. Continue with timer expiry,
death/static rules, dependent `wPosture` skills and complete collisions/attack effects. Preserve permanent remaining=0 and the
source whole-two-hand/empty-slot pointer-lifetime quirk.
Then complete special-equipment combat state, party/companion/guild/local-battle
updates, race costumes and remote visibility. Keep drops, timed bags, secured
inventory/trade/store integration and all economy systems in scope.

Preserve the explicitly certified old-two-table → current-four-table actor upgrade
and its immutable graph receipts. It does not authorize arbitrary content changes,
character/routing releases or transitive upgrades. All backups and migrations
**001–041** are immutable; the next schema migration is **041**.

1. **Preserve verified Login boundaries; finish remaining contracts.** Migrations
   008–010 and existing services implement atomic auth/session writes, key-specific
   cleanup, process ownership/fencing, sequential dispatch and graceful drain.
   Source non-direct 0x2918 email verification now follows client-driven LOGIN retry:
   per-connection grant, code/issuance limits, expiry, current rule revalidation,
   no persistent IP trust, and SMTP worker/deadline behavior. Preserve the actual
   PG/TCP/SMTP evidence in `login-security*.json`. Historical issuance was disabled;
   direct-login/JP/TW and non-ASCII retry behavior still need independent evidence.
   Remote Map duplicate handling and historical TAgreement gifts remain partial.
   Historical credential import, full collation/code-page equivalence and the
   actual client executable remain evidence gates; they do not block the next
   character/world implementation slice.
2. **Extend the verified native character/world boundary.** Preserve migrations
   001–034 and the checkpoint, retirement and replica contract evidence. Actual
   Login/World/Map TCP now covers single-owner fresh admission, complete inventory
   and learned-skill wire lists, source-based max stats, core save, logout and
   reconnect. The Map owner and connection generation fence all native operations;
   delayed Login termination cannot delete consumed claims. Initial and periodic core
   checkpoints now support fenced recovery of ready v1 claims; old/orphaned or
   drifted state stays blocked. Transient changes since the last checkpoint can be
   lost on a crash. World-link loss now closes clients, waits for durable teardown,
   and gates admission on a new registration ACK; see `world-link-contract.json`
   and its run evidence. World→peer writes now serialize complete frames with
   bounded per-peer backlog, and World accepts only expected secondary endpoints;
   see `world-writes-contract.json`. The subsequent retirement fix distinguishes
   unannounced/rejected connections from World-announced characters. INVALIDCHAR,
   DELCHAR and CLOSECHAR now retire the exact local identity without close-all echo;
   durable ready primaries still save. World close-all requires an accepted registered
   Map connection. See `map-retirement-contract.json` and its two-Map rejection
   regression; previously a failed second Map claim deleted the valid World char.
   Migration 019 now supplies separate one-use replica grants bound to both Map
   tokens, the primary generation, exact target endpoint and catalog releases.
   Actual ADDCONNECT→secondary CONNECT→CHARDATA/ENTERCHAR→CONREADY works for fresh
   admission without guilds/recalls; the replica never loads/saves the primary graph.
   The pinned source deployment separates channels rather than neighboring cells;
   tests explicitly override one compatibility view in a disposable DB and restore
   its exact definition, without changing imported historical rows. See
   `map-replica-contract.json`. Replicas observe local primary actors and track
   movement; other secondary gameplay dispatch remains unported.
   The subsequent World handoff coordinator binds CHECKMAIN/RELEASEMAIN/ENTERSVR
   to expected peers and phases and bounds incomplete transfers; see
   `world-handoff-contract.json`. It does not itself transfer native DB ownership.
   The complete source transfer DTO/codec is now byte-verified against an
   independent 861-byte fixture covering every variable section. Native item
   hydration retains raw magic and all six DWORD extensions alongside narrowed
   client projections. Fresh native load now restores remaining skill cooldowns
   into the live gate; snapshot/restore carries durations across process clocks.
   See `main-transfer-state-contract.json` for precise scope and evidence.
   Migration 020 now integrates actual movement-triggered primary transfer,
   full source-format graph capture/hydration, process/connection/authority fencing,
   exact prepare/consume receipts, replica/checkpoint rebinding and original client
   CONNECT/CONREADY reactivation. Version-2 checkpoints retain the graph after
   transfer. Source prepared recovery and target loaded/ready recovery preserve
   the committed state, while mismatches remain orphaned. Native TCP tests include
   same-socket round trips, skill rejection after graph-backed relogin and a client
   close during delayed target COMMIT. Gameplay drain is bounded and interrupted
   by client close. Preserve `native-primary-transfer-contract.json` and its
   Debug/sanitizer/installed evidence; no new client fields were introduced.
   Migration 021 now persists fresh-primary skill cooldowns using contract-3
   core/skill receipts and narrowly granted updates to TSKILLTABLE.dwRemainTick.
   Exact retries, rollback, relogin and crash recovery retain learned ranks and
   remaining durations; a skill drift blocks recovery. Full transfer graphs keep
   contract 2. Preserve `skill-checkpoints-contract.json` and its evidence.
   Native casts now use learned ownership/rank and pinned resource definitions,
   calculate source type-1/2 HP/MP costs, and check/deduct under one state lock.
   Unknown skills, forged caster/route and malformed bodies cannot mutate resources;
   success ACKs retain the learned rank. See `skill-costs-contract.json`.
   Normal native new-use cooldown generation now follows `TObjBase::SkillUse`
   and `CTSkill::Use`: rank increments, source physical/ranged/magic formulae,
   powered weapon slots, passive/item speed rates and same-kind extension are
   integrated. See `skill-timing-contract.json`. Next add active buff/disarm/disguise
   timing and authoritative effect expiry; speed-dependent casts currently close
   without mutation when a transferred graph has buffs. Native owned-PC loop
   timing/resource/wire gates and non-consuming weapon checks are now integrated;
   see `skill-loop-contract.json`. Ordinary map/previous-active-effect and shared
   non-consuming weapon gates now preserve source rejection precedence, including
   timer arming before normal weapon rejection; see `skill-gates-contract.json`.
   Fresh and transferred-primary reagent consumption commits with core/timers via
   022–023; preserve `skill-reagents-contract.json` and `graph-reagents-contract.json`.
   Single-hit arrow/bolt transactions now use 024; preserve `ammunition-contract.json`.
   Multi-target, per-bag multi-stack transactions now use 025; preserve
   `ammo-batch-contract.json`. Source multi-attack expansion now uses 026; preserve
   `multi-attack-contract.json`. Next finish equipment-triggered effects, cash branches, full effects and cancellation;
   recompute timing/item eligibility when equipment or learned skills mutate.
   CHARINFO and initial readiness now sample live skill durations without
   restarting timers; preserve `admission-timers-contract.json`. Implement active
   effect/quest/recall/companion timer semantics and simulation
   using the existing typed graph. Merely retaining these sections is not native
   gameplay. Trace original bStartAct=2/tutorial/Bow/dbload=1 branches and classify
   the source quest sender's shadowed dwTick before adopting its arithmetic.
   Add multi-neighbor churn and full cell/AOI actor enter/exit ownership, including
   delayed updates, dropped connections and simultaneous nearby movers. Preserve
   World peer/phase checks and SQL authority epochs; the source protocol itself has
   no epoch in CHECKMAIN, so do not claim every same-socket replay is distinguishable.
   Cell-boundary routing and primary role movement are verified; complete AOI
   and entity ownership across processes are still incomplete. Native replica admission does not prove
   those gameplay paths. Never treat an orphan as a successful
   save or clear it merely to make a login test pass. Native admission currently
   refuses persisted guild memberships and active recalls; implement those graph
   dependencies, mail/client ancillary synchronization and full World social
   persistence. The World test registry is in memory, not a completed native World
   repository. Runtime DB/time semantics remain separate from historical rows.
   Movement save is verified, but movement validation/AOI/pathfinding parity is not.
   Do not wire the legacy item/skill repositories to app_world via search_path.
3. **Items/inventory transactions.** Starter allocation is now atomic and preserves
   original world 0 high-water 735812. `item_world` is separate from client group ID.
   Extend the current native schema/repositories for ownership transfers, other
   storage kinds, expiry, currency and durable rewards after reading their source
   contracts. Current Login item rows deliberately support character inventory only;
   use a new migration to extend them, not edits to applied 012/013.
4. **Spawn and core gameplay parity.** Current templates use real recovered stats.
   Port original ordinary/essential/weighted/leader selection, packed entity IDs,
   regen states and level-dependent attributes with explicit missing-content
   behavior. Continue damage/skills/death/quests into real durable rewards. See
   `protocol-compatibility.md` for known differences, including zero-count handling.
5. **Existing advanced modules.** Preserve implemented World handlers. Work through
   guild/social/mail/events/war/tournament/Bow/BR and original tool duties using
   the complete file/opcode index. Replace remaining T-SQL dependencies individually,
   with verified return values and transaction semantics.
6. **Original-client and reliability acceptance.** Identify/hash the actual supported
   executable and assets; confirm `0x2918` packet behavior with authorized captures
   or client execution. Test startup→login→character→world→gameplay→save→disconnect→
   reconnect. Profile real tick/DB behavior, run sanitizers/concurrency/fault tests,
   pin build dependencies, and remove the temporary SQL Server runtime package only
   after all persistent paths use PostgreSQL. Windows native PG build is unverified.

## Known technical follow-ups

- Preserve the TCP session strand and atomic `IsOpen` checks. All channel and
  socket operations, including composed-read/write continuations and cancellation,
  must share that strand. Shutdown must cancel backpressured producers before
  closing the channel. The separate optional `TlsAsioSession` still needs a
  concurrency/shutdown audit; native PostgreSQL TLS is provided by libpq and is
  unrelated to that optional game transport. Shared application registries are
  not made globally thread-safe by a per-session transport strand.

- Review the explicit TCheckIP wrapper/procedure ordering difference in
  `evidence/auth-source-contract.json` before claiming complete login error parity.
  Native behavior currently follows the backup procedure.

- Preserve native hydration order: source `TMapSvr/SSHandler.cpp:2019`
  sends CHGCHANNEL and CHARINFO before final CONNECT; the original client loads
  the map in CHARINFO and activates its frame in CONNECT. CONREADY must not
  duplicate CHARINFO. `native-map-charinfo-order-before.json` reproduces the
  previous error; final `native-map-replica*.json` verifies the correction.
- Primary transfer must carry transient state. Original
  `TMapSvr/SSSender.cpp:1999 SendMW_RELEASEMAIN_ACK` serializes current core,
  inventory/cabinet items, skills and remaining cooldowns, buffs, quest progress,
  hotkeys, item cooldowns and further companion state. World
  `SSHandler.cpp:2284` forwards it to the new main. A stale database reload or
  promotion of the existing summary-only replica would discard state. Migration
  020 now provides the fenced transfer transaction, authority epochs and graph
  recovery. Preserve its source widths, order, supported-state checks and tests
  while implementing the remaining active gameplay represented by that graph.

- Preserve the implemented replica split: World sends CONRESULT to the main;
  the client sends CONREADY to every returned server ID. Secondary ENTERCHAR
  hydrates only the source summary; full recall graphs remain refused. Grants
  bind the exact primary owner/generation and target token/endpoint with one-use
  expiry, keeping mutable primary ownership unique. Avoid acquiring another runtime
  owner's row lock after the account lock: owner replacement uses owner→account
  ordering and can deadlock with the reverse order. Replica teardown must never
  release the primary's current-user reservation.

- Native login now uses account-row locking, a unique current-user constraint and
  one transaction. Preserve those boundaries; the ODBC path retains older behavior.
  Key-specific logout and abandoned-login cleanup are verified. Do not regress
  these while implementing durable character/world handoff.
- Existing eight legacy CTest executables silently return success when their DB
  fixture is missing. Track them as internal skips; new native tests return 77 so
  CTest reports a skip explicitly. Do not combine mock/transport success into a
  claim that auth/inventory repositories work on PG.
- Existing ORM compiler warnings in `peer_auth_repository.h` use `st, soci::use(...)`
  rather than a demonstrated statement bind. Review and exercise these production
  writes against PG before enabling peer-auth persistence; do not silence warnings.
- Pool disconnects surface failures; automatic reconnection and transaction hygiene
  after arbitrary caller misuse remain unimplemented. Never replay writes with an
  unknown commit outcome. The test reconnects via a fresh pool explicitly.
- `[content]` supports a secret environment variable. Mutable configurations and
  repository error paths need a complete secret/logging audit during their port.
- Immutable caches are loaded atomically from one PG snapshot and remain fixed
  until restart. Coordinated cluster-wide live catalog activation is not implemented.
- CI is wired for Debug and ASan/UBSan synthetic native pool/Login tests without backup data; remote execution is not claimed. Real-catalog evidence
  must be refreshed locally when queries, mappings or content migrations change.

## Preserved evidence and local workflow

Keep all private extracts/credentials outside Git. Current private reference
manifest is under `/tmp/fourstory-db-forensics/reference-snapshot-pinned/manifest.json`;
its fingerprint is `c4a43e0657002913be45a096caa4b329f198abcc5553737a98a8d586ae572d95`.
The helper creates labelled disposable containers; reuse no unrelated databases.
Build in the Ubuntu image with `/src` bound to the checkout because the current
`build/linux-debug` CMake cache contains container paths.

Do not overwrite migrations `001`–`030` after application. Add migrations `031` onward.
Do not revert unrelated pre-existing client/binary/library working-tree changes.


Prior security-increment Debug verification on 2026-10-09 (Europe/Prague): 187 CTest entries, 176 passed, eight
legacy DB tests skipped internally, three native tests explicitly skipped without
fixtures. Native Login/pool integration and six selected ASan/UBSan crypto/transport/Login
tests passed, including actual PG failover and shutdown with leak detection; 22 Python DB tests passed on a fresh DB.
The installed Map binary also passed the pinned original catalog verification
again in `evidence/security-catalog-release.json`. See the
modernization README and evidence files for image/cleanup state of the latest run.

Prior security Release image `localhost/fourstory:postgresql-security` (ID
`f8335e8dac8f03f15f85a152eb20479fb28eb7393af610f4ceb1bcd0d5176b25`)
passed six-daemon smoke, installed native Login TCP/SMTP/database verification,
and installed Map pinned-catalog verification as UID 10001. Native Login runs
record 97 C++ checks, the established Login protocol checks and 54 security-wire
checks in each Debug, ASan and Release verification. No external mail was sent.

The packet read close guard now retains its session through reactor teardown.
The previously failing echo test passed thirty instrumented repetitions and is
part of the six-suite sanitizer preset/CI. Full Debug CTest and 22 reconstruction
DB tests passed. See `evidence/sanitizers.json` and
`evidence/transport-teardown-regression.json` for scope and exclusions.
The deployment grants are exercised directly by the real native fixture.

Earlier character increment: 188 CTest entries (176 passed, eight legacy internal
skips, four explicit native skips), 25 Python reconstruction/publication tests.
Dedicated real-PostgreSQL character tests run separately in Debug and ASan/UBSan;
see `characters*.json` for exact checks. No original-client executable was run.

Private character manifest:
`/tmp/fourstory-character-20261009/reference/manifest.json`, normalized fingerprint
`0a2a42d8f62eb8d80bffa2f44f54e5c4920793d9cea25c927e565fa6e063ebb8`.
It contains 107,977 source rows in 22 tables. The original Map manifest remains
unchanged. Extract using `extract_reference.py --profile characters`; import with
`migrate.py`, then explicitly publish with `activate_character_catalog.py`.

Do not treat the earlier lobby evidence as complete client/world acceptance. The
native Map follow-up below extends it while preserving the verified Login and
character boundaries. See `character-implementation-contract.json` for
unresolved source/runtime differences and `character-cleanup.json` for lab state.

Earlier verified image: `localhost/fourstory:postgresql-characters`, ID `684c7dbddae8411dfe1e5469b9ff86eead661cf21ee8ad00b6bb281b98336b9d`, UID/GID 10001:10001. Installed Login encrypted character workflow and six-daemon smoke passed; details in `character-container-verification.json`.

Map admission follow-up verified on 2026-10-09: 190 Debug CTest entries (178
passed, eight internal legacy DB skips, four explicit native DB skips). Each plain
and encrypted Map TCP fixture passed 85 checks. The six current Debug binaries
were mounted read-only in the existing character Release runtime image for health,
Map–World DNS and SIGTERM verification; this is not a new installed Release image.
Current backup procedure/routing review is `evidence/map-entry-backup-contracts.json`.
Private routing extracts remain in `/tmp/fourstory-map-entry-20261009/routing-review`;
the owned lab and secret files were removed. Subsequent migrations 014–017 now
implement Login routing/handoff and native Map ownership; preserve those applied files.

Native Map runtime follow-up (2026-10-09): migrations 016–017, independent actor
publication and actual native Map services are integrated. Resume from
`evidence/map-runtime-contract.json`, not the earlier unimplemented-claim notes.
This increment was initially verified with mounted Debug/sanitizer binaries.
The later checkpoint Release image below now includes it. The older
`localhost/fourstory:postgresql-characters` image does not contain these Map changes.
Verification: 192 Debug CTest entries (178 passes, eight legacy internal skips,
six explicit native skips); separate Debug and ASan runs each pass 36 native Map,
57 encrypted three-daemon TCP and 29 pool/TLS checks; all 19 sanitizer preset
entries and all 31 Python tests pass. The character regression passes as well.
The SIGTERM-during-ready-commit test verifies no stranded account reservation.
Actor manifest: `/tmp/fourstory-map-runtime-20261009/actor-reference/manifest.json`,
SHA256 `e47806df593b8646d6e52c90e7638131f1baac7f90eac3fb1c3c957940739381`.
It contains 746 source rows in two tables; publish independently using
`activate_actor_catalog.py`. Check `map-runtime-cleanup.json` before reusing a lab.


Native checkpoint follow-up (2026-10-09): `018-native-map-checkpoints.sql` is applied
and immutable. Read `map-checkpoint-contract.json` and `native-map-checkpoint*.json`
for current behavior. The source interval is 30 minutes; tests explicitly shorten
it to 500 ms. A real Map SIGKILL during a delayed checkpoint retains the prior
committed core, and restart/relogin through the same World succeeds. A concurrent
final save cannot be overwritten by an older checkpoint. Old no-receipt claims and
core drift remain orphaned; do not backfill receipts to bypass this gate.
The Map peer high byte now correctly identifies server type 4, independent of the
application world ID. World TCP-loss cleanup keeps native DB recovery ownership.
Checkpoint increment installed image: `localhost/fourstory:postgresql-checkpoint`, ID
`e061e3a704196ea884b28c20a557b9c5f40c9b777eab0f8df37cd313eea3d58b`, UID/GID
10001:10001. Debug, sanitizer and installed Release runs each pass 59 native Map,
79 encrypted three-daemon TCP and 29 pool/TLS checks. Release verification uses
the installed Login/World/Map daemons with mounted Debug integration executables.
All six installed daemons pass health/DNS/SIGTERM checks; all 20 sanitizer preset
entries and all 31 Python tests pass. Full Debug CTest: 192 entries, 178 actual
passes, eight legacy internal skips, six explicit native DB skips, zero failures.
See `evidence/map-checkpoint-container-verification.json` and
`evidence/map-checkpoint-build-fingerprints.json`. No original-client execution
is claimed. The subsequent World-link increment implements client teardown and
registration gating; consult its contract for current continuation work.
The private lab is `/tmp/fourstory-checkpoint-20261009`; consult
`evidence/map-checkpoint-cleanup.json` for its terminal state. Original reference
snapshots remain at the paths recorded above.


World-link follow-up (2026-10-09): `services/world_client`, Map listener/CONNECT
and main now gate native admission on current-peer registration, close clients
on World loss and await durable teardown before reconnecting. No migration added;
001–018 and both backups are unchanged. Resume from `world-link-contract.json`.
Debug, ASan/UBSan and installed Release each pass 59 native Map, 131 encrypted
three-daemon TCP and 29 pool/TLS checks. Full Debug CTest: 193 entries (179 actual
passes, eight legacy internal skips, six explicit native skips), zero failures.
All 22 sanitizer preset entries pass after rebuilding the preset.
Earlier verified image: `localhost/fourstory:postgresql-world-link`, ID
`529effcfb3c59321a8b3ccd1d7151b00161af6a9da374a0dbb0860dd0eee8f86`, UID/GID
10001:10001. Release uses installed Login/World/Map daemons and mounted Debug
integration executables; six installed-daemon health/DNS/SIGTERM checks pass.
The owned PG-only lab is `/tmp/fourstory-world-link-20261009`; consult
`evidence/world-link-cleanup.json`. Private reference snapshots above remain valid.
The subsequent world-writes and map-retirement increments resolve the writer and
retirement prerequisites. Follow the current README and top-level continuation
steps for dynamic routing and primary transfer. World heartbeat/blackhole detection, complete social
persistence, transactional gameplay and real-client execution remain outstanding.


Latest replica increment (2026-10-09): migration 019 and the actual native Map
handlers now implement fresh secondary admission, summary hydration and separate
replica retirement. Primary CHARINFO now precedes CONNECT, matching the source
client. Debug, ASan/UBSan and installed Release each pass 96 native Map, 29 pool/TLS,
153 four-daemon TCP, 36 replica-wire, 66 World-peer and seven second-Map rejection
checks. CTest: 195 entries, 181 actual passes, eight internal skips, six explicit
native skips, no failures; sanitizer preset: 24 passes. Plain/encrypted admission:
173 checks each. See `evidence/map-replica-contract.json` for boundaries and the
synthetic partition used in replica tests. Current image:
`localhost/fourstory:postgresql-map-replica`, ID
`bebc88410bb885dd7eb6ee0a2b0faa49454681ed5cd3a56b54ce4d6fd238664f`, UID/GID
10001:10001. The owned PG-only lab `/tmp/fourstory-map-replica-20261009` is stopped;
`evidence/map-replica-cleanup.json` records container/network and temporary secret
cleanup. Private snapshots remain available. Continue with the transfer/state
requirements above; the full server and real-client acceptance remain unfinished.


Earlier verified skill resource increment (2026-10-09): read
`evidence/skill-costs-contract.json` and the Debug/ASan/installed Release reports.
All three configurations pass 880 native database/network checks; default Debug
has 200 entries (186 actual passes, 14 skips), and all 33 sanitizer entries pass.
That increment used local image `localhost/fourstory:postgresql-skill-costs`,
`42ba521c5b853387551f80892d7977fcc3391813e42d2c45c779a555a2f0f6dc`, UID/GID
10001:10001; six-daemon smoke passed. Test labs were disposable PostgreSQL only;
consult `skill-costs-cleanup.json` before reusing any private work path. Source
snapshots remain at the documented paths. Preserve migrations 001–021. Continue
with the later timing increment and active gameplay as described above;
this increment does not establish a complete combat loop or original-client run.


Earlier verified timing increment (2026-10-09): `skill-timing-contract.json` and its
Debug/ASan/installed Release reports each record 906 native database/network
checks. All 33 sanitizer entries pass; Debug has 200 entries (186 actual passes,
14 skips). That increment's local image `localhost/fourstory:postgresql-skill-timing`
is `81b6c61dca05fae4150e2079de58ba7e6adc685aaaa79e087430974bee3f32ec`,
UID/GID 10001:10001; six-daemon smoke passed. See `skill-timing-cleanup.json` for
owned lab cleanup; original reference manifests are retained. Continue active
buff/disarm/disguise simulation and remaining cancellation branches;
normal rank/weapon/passive/item/same-kind cooldown generation is now integrated.
Equipment/learned-skill changes must rebuild the cache. Migrations 001–021 remain
immutable; no historical source values were changed by this increment.

Earlier verified admission-timer increment (2026-10-09): CHARINFO and initial
CONREADY checkpoints now sample live remaining skill durations independently.
Loaded payloads and timer origins remain unchanged; expired durations become zero.
Read `admission-timers-contract.json` and its Debug/ASan/installed Release reports:
910 native checks per configuration, 186 actual Debug passes (200 entries, 14
skips), all 33 sanitizer entries and six-daemon health/DNS/SIGTERM smoke passed.
The previous installed image fails the delayed-readiness regression. That increment
used `localhost/fourstory:postgresql-admission-timers`, image
`7007a8af428eef454ef1033e286d6b7a2a8ec32b1c6b2f70aaa56833a8f30c52`, UID/GID
10001:10001. Consult `admission-timers-cleanup.json` before reusing lab paths.
Continue active buff/disarm/disguise simulation, effect expiry and source-traced
active loop effects, consumables and cancellation. The original client executable remains an acceptance
gate. Preserve backups and migrations 001–021; the next schema migration is 022.

Earlier verified loop increment (2026-10-09): native owned-PC `CS_LOOPSKILL_REQ`
and its original ACK now implement distinct loop timing, cooldown-first resource
gates, HP equality, missing maintained-effect rejection and non-consuming weapon
checks. Shared timer state prevents switching normal/loop opcodes to bypass reuse.
Read `skill-loop-contract.json`: 929 native checks per Debug/ASan/installed Release,
186 actual Debug passes (200 entries, 14 skips), all 33 sanitizer suites and
six-daemon smoke pass. Previous installed admission-timers image lacks LOOPSKILL_ACK.
That increment used `localhost/fourstory:postgresql-skill-loop`, image
`d4a8bc5990d871e051853c37097842288f6886b6d5c4e1e8d209bd9cca7b873e`, UID/GID
10001:10001. See `skill-loop-cleanup.json` for owned lab cleanup. Next port
consumable inventory transactions, authoritative active effects and cast lifecycle
before exposing cancellation. Preserve the source's cancellation context: the
client sends CANCELSKILL when an acknowledged cast cannot activate; arbitrary
client timer resets are not an acceptable substitute for a cast lifecycle.
Full combat/AOI/target rules and original-client execution remain unfinished.


Preceding verified cast-requirement increment (2026-10-09): native ordinary casts
now check source map, previous active effect and non-consuming weapon eligibility.
The source arms own/shared-kind timers before a supported ordinary weapon failure;
HP/MP stay unchanged. Loop ordering remains distinct. Unsupported consumables or
buff-dependent work close before mutation. See `skill-gates-contract.json` and
its Debug/ASan/installed Release evidence: 933 native checks per configuration,
186 actual Debug passes (200 entries, 14 skips), all 33 sanitizer suites and
six-daemon health/DNS/SIGTERM smoke pass. The previous installed loop image wrongly
accepts skill1329 outside its source map550 and fails the new encrypted regression.

Image used for that increment: `localhost/fourstory:postgresql-skill-gates`,
`745e22cc4acafd7dd304a5fbc60bd1292c8869d690e13ae06412a45b386ad85d`, UID/GID
10001:10001. See `skill-gates-cleanup.json` for owned lab cleanup. Preserve backups
and migrations 001–021; next migration is 022. Continue the durable consumable/
ammunition/durability transaction, authoritative effects and cast lifecycle ports.
Normal premium-medal/toggle/tournament/peace-zone rules, combat powers/damage,
full target/AOI behavior and original-client execution remain unfinished.


Earlier verified reagent increment (2026-10-09): migration 022 implements fresh
ready-primary single-reagent transactions for ordinary and loop casts. Exact item
row hashes and claim/catalog fences protect count/deletion. Core, timers and receipt
commit before inventory/success ACKs. Checkpoint leases prevent stale overwrites;
unknown outcomes retain reservations and block stale logout writes. Preserve
`skill-reagents-contract.json`, 973 native checks in each Debug/sanitizer/installed
run, 186 Debug passes (14 fixture skips), all 33 sanitizer suites and six-service
container smoke. That increment used `localhost/fourstory:postgresql-skill-reagents`;
see container/build/cleanup evidence for exact identities.

The graph extension in migration 023 now consumes reagents after primary transfer
and graph-backed relogin. Preserve exact item/full-graph receipt validation, unsigned
IDs and unrelated sections. This storage path keeps normalized item/skill rows
stale by design; future inventory/skill work must keep the complete graph authoritative
or introduce a separately verified normalization migration. Never fall back to the
old rows when a graph is present. See `graph-reagents-contract.json`.

Graph increment verification: 1,046 native checks in each Debug, ASan/UBSan and installed
Release run, 186 actual Debug passes (14 fixture skips), all 33 sanitizer suites
and six-service container smoke. That increment used
`localhost/fourstory:postgresql-graph-reagents`, ID
`7a25026d3c20798049579845d0ea653b313b2f45aa772222da49f54a5758adad`.
See graph-reagents container/build/cleanup evidence for identities and scope.

Migrations 024–025 now consume ammunition for 1–16 non-expanded flagged targets
on fresh or transferred primary characters. Preserve the first compatible powered
weapon, loaded weapon fingerprint, same-kind selection and ordered per-bag stack
algorithm including BYTE overflow. All stack writes and grouped `cast_id` receipts
must remain atomic with core, timers and the recovery checkpoint. Keep complete
transfer graphs authoritative; never fall back to stale normalized item rows.

The batch increment covers rollback on a stale second stack or failed second
receipt, return transfer, graph-backed relogin and process recovery. Preserve
`ammo-batch-contract.json` and its Debug/sanitizer/Release, migration-upgrade,
container and cleanup evidence. Version this work only locally on `main` until
complete gameplay is finished; do not push or deploy to GitHub.

Source multi-attack expansion now uses 026: learned-rank SA_ONCE ability sum,
BYTE narrowing, random per-target duplication and first-target padding. Preserve
`multi-attack-contract.json` and direct batch evidence. Expanded ammo validates
the exact canonical rank/budget in the same transaction; graph ranks take priority
over stale normalized skill rows. Only budgets 0–16 are modeled for target expansion,
with zero-target ammo still refused; valid recovered ranks need at most seven hits.

Preserve backups and migrations 001–040; next schema change is 041. Native
whole-stack carried moves and different-template swaps now have source ACKs,
exact per-bag capacities, atomic slot changes, graph authority and grouped
receipts. Preserve `inventory-moves-contract.json` and the updated Map grants.
Continue with equipment-triggered active effects and durability mutation; preserve
the ordinary equipment stat/timing and eligibility recomputation now integrated by 032. Then add authoritative effect expiry, cast lifecycle and
combat/target/AOI rules. Reagent-plus-weapon combinations and zero-target ammo
remain unsupported. Premium templates 25020–25022 are absent from the backup;
do not fabricate historical definitions. Original-client executable acceptance
is still required. Continue local commits on `main` only, without GitHub pushes
or deployment until complete gameplay is finished.


Inventory continuation: the normal five-byte MOVEITEM path is integrated, using
`PlanInventoryMove`, an unpublished frozen snapshot, `MoveInventoryItems` and
post-commit original item descriptors. Preserve initially-immediate slot uniqueness,
per-operation receipt grouping, canonical graph state and uncertain-outcome
reservation retention. Migration 028 now integrates source CTItem equality, split/merge and the shared
allocator, including actual two-Map split/move/return/merge packets. Migration 032 now implements ordinary equipment
slot/class/level/two-hand rules and derived-stat/timing recomputation. Continue
generic effect and advanced equipment dependencies. Preserve actual two-Map
inventory packet coverage and native process-recovery tests.
Do not fabricate premium content or mutate historical rows. Keep local main commits
only until complete gameplay; Log uses 029, statistics 030, actor transition 031
equipment 032, postures 033, explicit cancellation 034, accepted casts 035, skill
finishes 036, actions 037 and finish effects 038. Next new migration: 041.


The owner confirmed that only uncompiled client sources are available, without
client data. Track binary/data/environment acceptance in `client-acceptance.md`;
continue independent server work and do not declare a supported executable build.
The Log unknown-COMMIT retry defect is now fixed and verified with actual PostgreSQL
reply loss for single/bulk transactions; see `evidence/log-outcomes-contract.json`.
Preserve the separate confirmed/unknown counters and never requeue an unknown
outcome. Migration 029 and the real runtime grant script now supply native LP_LOG
storage and native Count/LatestN/WhereUserId, with exact raw CHAR byte preservation
and startup contract checks. Keep the native-audit grants/query/wire/restart evidence.
Next implement retention/reconciliation and original log-tool integrations;
retain the source LP_CHAT no-op classification described below.
Retries remain RAM-only and discarded connections need pool replacement; verified
committed-row SIGKILL recovery is not recovery of queued or unknown records. The Asio
worker's posted-arrival backlog is also separate from the bounded retry queue and
still needs explicit admission/backpressure limits.

Equipment continuation source notes: `TObjBase::CanEquip` checks wrap, learned
equipment skill, slot mask, class mask and effective level in that order.
`CSHandler::OnCS_MOVEITEM_REQ` normalizes secondary slots, displaces conflicting
weapons through CanPush/PushTItem, handles warrior maintains and local-zone item
attributes. `CTPlayer::ChangeEquipItem` sends EQUIP to neighbors, MOVEITEM and
CHARSTATINFO to self, clamps HP/MP, sends HPMP and checks equipment skills; the
caller also sends its final MOVEITEM. Migration 032 preserves this ordering and
recomputes native stats/timing without accumulating skill-point groups. Continue
warrior maintains, CheckEquipSkill cancellation and local-zone/advanced effects;
their unsupported branches are still refused before mutation. See the equipment
contract for exact tested cases and the source pointer-lifetime posture condition.

Log source clarification: `CUdpSocket.cpp:442–450` initializes all `LP_COUNT`
handlers to `Packet_Nothing` and assigns only LP_LOG to Packet_LogDB. LP_CHAT is
therefore a source no-op, not evidence of a missing historical chat-audit table.
Preserve/document its classification when auditing original senders and tools;
do not invent persistent chat behavior from the opcode name alone.
