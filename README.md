# 4Story Emulator Server

**Development policy (owner instruction updated 2026-10-14):** continue
implementation and commit on local `main` only. Do not push to GitHub or deploy.
The preceding one-time publication ended at `2e406dc`; it does not authorize
publication of subsequent implementation. Builds, tests, SQL execution, services,
clients, containers and runtime verification remain deferred. Private database
extracts and pre-existing local audit output stay outside Git.

**Current workflow:** complete implementation (Phase A), then verification and
migration (Phase B), then local container startup (Phase C). New changes are
unverified until their later acceptance runs. See the
[active rewrite status](_rewrite/docs/modernization/full-rewrite-status.md).
Historical test results below retain their original bounded scope.

An incremental **C++20** reimplementation of the 4Story server cluster, targeting
**Linux containers and native PostgreSQL**. The compatibility goal is the original
client with unchanged packet layouts. The project is **in development**: selected
native database and protocol flows are verified, while complete gameplay and an
actual original-client session remain unfinished.

Original Windows sources live in `Server/T*Svr/`; the running modern implementations
live in `Server/T*SvrAsio/`. See the [implementation status](_rewrite/docs/modernization/README.md),
[capability matrix](_rewrite/docs/modernization/capability-matrix.md) and
[next implementation tasks](_rewrite/docs/modernization/next-steps.md).

<a id="overall-progress"></a>

## Active implementation — Phase A, unverified

Current Phase A continuation (2026-10-16): source Map monster catalog,
group/party query order and default-spawn slot allocation now feed native boot
and battle bindings, **INTEGRATED_UNVERIFIED** for allocation only. Native startup
refuses the first unported synchronous `CTMonster::Initialize` before listening;
round-robin prototype population is excluded. Full AI/monster/gameplay parity
remains incomplete.079/grants/publisher are written only; next080.
[Current scope and exact handoff](_rewrite/docs/modernization/evidence/phase-a-map-monster-allocation-20261016/README.md).

Previous Phase A continuation (2026-10-16): native Map battle-region boot now
feeds actual HEROLIST and source territory-based NPC access. The explicit078
PostgreSQL projection preserves source order, bytes, times and map/spawn bindings,
**INTEGRATED_UNVERIFIED**. Battle effects/native guild mutations/full bootstrap
remain incomplete; unsupported World changes retire cached authority and the
link. Written078/grants/publisher unexecuted; next079.
[Current scope and exact handoff](_rewrite/docs/modernization/evidence/phase-a-map-battle-regions-20261016/README.md).

Previous Phase A continuation (2026-10-16): native World loads source battle
charts/calendar and runs the original defined timer branches through a bounded
batch queue. Peace effects and conditional guild last-record output are wired,
**INTEGRATED_UNVERIFIED** within World. Actual Map battle regions and full source
bootstrap remain incomplete. Written077/grants/publisher are unexecuted; next078.
[Current scope and exact handoff](_rewrite/docs/modernization/evidence/phase-a-world-battles-20261016/README.md).

Previous Phase A continuation (2026-10-16): source channel topology now fences
native Map startup and World authorization; offline routing publication retires
stale process tokens. Source battle selectors are corrected. These paths are
**INTEGRATED_UNVERIFIED**; source registration/full bootstrap remain incomplete.
Written076/grants are unexecuted; next077.
[Current scope and exact handoff](_rewrite/docs/modernization/evidence/phase-a-map-topology-20261016/README.md).

Previous Phase A continuation (2026-10-16): ordinary native Map RPS now includes
start-item debit, source rounds, real World decisions and atomic reward mail,
**INTEGRATED_UNVERIFIED**. BR/late replica parity, original admission and full
World bootstrap remain incomplete. Written075/grants are unexecuted; next076.
[Current scope and exact handoff](_rewrite/docs/modernization/evidence/phase-a-map-rps-20261016/README.md).

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
remain OPEN. [Source contract and deferred acceptance](_rewrite/docs/modernization/evidence/phase-a-finish-terminal-20261015/README.md).

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
[Native cast authority and source exceptions](_rewrite/docs/modernization/evidence/phase-a-native-cast-authority-20261015/README.md).

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
[Bounded CURE death and deferred acceptance](_rewrite/docs/modernization/evidence/phase-a-cure-death-20261015/README.md).

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
[Signed numeric CURE and deferred acceptance](_rewrite/docs/modernization/evidence/phase-a-cure-signed-20261015/README.md).

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
[Source DWORD arithmetic and actual default branches](_rewrite/docs/modernization/evidence/phase-a-cure-dword-20261015/README.md).

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
[Actual cure combine semantics and checked completion](_rewrite/docs/modernization/evidence/phase-a-cure-combine-20261015/README.md).

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
[Actual exponential metadata and checked cure arithmetic](_rewrite/docs/modernization/evidence/phase-a-actitem-exponential-20261015/README.md).

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
[Actual trigger rank, critical and cure values](_rewrite/docs/modernization/evidence/phase-a-actitem-trigger-20261015/README.md).

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
[Source MISS, actual802 lifecycle and deferred acceptance](_rewrite/docs/modernization/evidence/phase-a-native-self-miss-20261015/README.md).

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
[Party self-cure contract and deferred acceptance](_rewrite/docs/modernization/evidence/phase-a-party-self-cure-20261015/README.md).

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
[Action-item source contract, publication and deferred acceptance](_rewrite/docs/modernization/evidence/phase-a-native-actitems-20261015/README.md).

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
[Source collision, persistence distinction and deferred acceptance](_rewrite/docs/modernization/evidence/phase-a-revival-replacement-20261015/README.md).

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
[Source contract, publication and deferred acceptance](_rewrite/docs/modernization/evidence/phase-a-native-death-20261015/README.md).

Previous code `d86d407` (2026-10-15), based on `1fe7404`, integrates ordinary
native PvE resource recovery and the original one-BYTE CHGMODE request through
the existing PostgreSQL authority/operation/checkpoint path. Actual formula,
powered equipment and passive HPR/MPR rates feed independent three-second clocks;
entry, local replica handoff, battle delay/timeout and supported cast/self-Defend
hooks retain their source distinctions. **INTEGRATED_UNVERIFIED**: static review
only; no build/test/SQL/service/client/container run, migration (next070), push or
deployment. Full combat/death/SoulHP, unsupported effect/guild/companion graphs,
other mode lifecycles and original-client acceptance remain OPEN.
[Source contract, local clocks and deferred acceptance](_rewrite/docs/modernization/evidence/phase-a-native-recovery-20261015/README.md).

Previous code `1fe7404` (2026-10-15), based on `1c83b5f`, moves native
REVIVAL out of the cache-only full-heal prototype and into the current PostgreSQL
mutation/checkpoint path. The bounded same-cell primary path uses actual source
30/40% bars, aftermath/stat/mode replies and validated protection800. Stored
penalty steps survive hydration; the local decay clock is reconstructed without
new wire fields. **INTEGRATED_UNVERIFIED**: static review only, no build/test/SQL/
service/client/container run, migration (next070), push or deployment. Cross-cell
AOI/replica/mode variants, unsupported800 data and BYTE decay underflow remain
OPEN, together with full death/combat/SoulHP/rewards/bootstrap and client acceptance.
[Source contracts, supported scope and deferred acceptance](_rewrite/docs/modernization/evidence/phase-a-native-revival-20261015/README.md).

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
[Exact storage domains and deferred acceptance](_rewrite/docs/modernization/evidence/phase-a-native-identity-20261015/README.md).

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
no push. [Source contracts, changes and deferred acceptance](_rewrite/docs/modernization/evidence/phase-a-party-stats-20261015/README.md).

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
[Source contracts, differences and deferred acceptance](_rewrite/docs/modernization/evidence/phase-a-party-recovery-20261014/README.md).

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
[Source contracts, differences and deferred acceptance](_rewrite/docs/modernization/evidence/phase-a-native-party-20261014/README.md).

Previous code `80b7698` (2026-10-14), based on `acbcaaf`, ports actual
Map–World and Control–World links to source Plain16, preserving source receive
semantics and current-socket/no-replay guards. The modern RW catalog ACK now reads
DWORD message keys. Control service types follow linked CTProtocol exactly
(World3/Patch5/Log7/Relay8), and World monitoring counts actual raw sessions.
**INTEGRATED_UNVERIFIED**, static review only; no build/test/SQL/service/client/
container run or new migration (next070). Original Map MW_CONNECT_ACK admission
and full bootstrap remain OPEN: the current RW-as-Map path is a modern extension.
Party/group producers, other service/operator framing, TLS and complete gameplay
remain required. [Source contracts, differences and deferred acceptance](_rewrite/docs/modernization/evidence/phase-a-world-framing-20261014/README.md).

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
framing correction and subsequent acceptance](_rewrite/docs/modernization/evidence/phase-a-control-map-20261014/README.md).

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
repairs and deferred acceptance](_rewrite/docs/modernization/evidence/phase-a-chat-tms-20261014/README.md).

Earlier code `c389976`, based on `75125a2`, connects original soulmate
SEARCH/REGREADY/REG through Map → World → PostgreSQL → original client replies.
Written067 binds exact prepared World replies to the current Map claim; purse,
optional caller, directed relationship and checkpoint commit atomically. World
reconciles committed cache projections and checks initial ready-primary level
gaps. Written068 adds verified native guild-level publication/loading, preserving
source level0. These changes are **INTEGRATED_UNVERIFIED**; SQL/tools are
**IMPLEMENTED_UNVERIFIED**. No build, test, SQL, service, client or container ran.
Full guild/World bootstrap, soulmate party XP/emergency HP and the wider original
gameplay inventory remain open. [Source review, differences and deferred acceptance](_rewrite/docs/modernization/evidence/phase-a-source-review-20261013/README.md).
World now also loads original51-row server-message bytes for relay/event/TMS,
repairs relay DWORD keys, and Map persists exact maintained timers at checkpoint/
logout/transfer. Full chat/TMS and general effect simulation remain open.

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
still incomplete. [Source contracts, repairs and compile evidence](_rewrite/docs/modernization/evidence/phase-a-native-social-20261013/README.md).

Earlier code `11e0df1` / `dd07164` repairs both mutual-friend acceptance
paths: two source directed edges commit together before cache/success responses,
with registered ready-primary checks before persistence and after await. Any
mutual persistence failure stops World without a write replay. World compiles
in Debug/Release; no tests or runtime ran. This preserves the existing schema
adapter and **does not complete native friend schemas/imports, World ownership
or the other best-effort social paths**. [Scope and remaining native work](_rewrite/docs/modernization/evidence/phase-a-mutual-friends-20261013/README.md).

Code `e84ece4` / `65be6d1` integrates quest SENDPOST and native offline bill
returns. QUESTEXEC/child hooks select original rewards into the fenced reserved
queue; World returns due offline bills with authoritative item ownership and
notification/audit in one transaction. Written064/065 and `--quest-mail` are
unexecuted. Map/World compile/link in Debug/Release; no tests, DB or runtime ran.
The backup appearance omission is explicitly repaired from CTItem; source gem/
grade losses and distinct online/offline sender-ID rules are preserved. Full
Phase A, PostgreSQL-only World bootstrap and original-client acceptance remain
incomplete. [Contracts, differences and compilation evidence](_rewrite/docs/modernization/evidence/phase-a-quest-mail-offline-bills-20261013/README.md).

Code `45d130d` integrates PostgreSQL reserved-mail delivery into the existing
World minute timer and Map notification path. Migration063 and the historical
queue importer are written, not executed. Quest/event/admin producers and full
native World startup remain incomplete.
[Source scope and compilation evidence](_rewrite/docs/modernization/evidence/phase-a-reserved-mail-20261013/README.md).

Code `85bf923` / `feea4a1` / `b4ff581` adds native ordinary player mail sending and
protected-list persistence through admission and Map transfer. Notification
receipts retain enqueue audit data and cancel obsolete recipient/type routing.
POSTRECV uses the recipient Map’s current second, as the source handler does. Migrations061–062/import tooling are written only.
Map and World compile/link
in Debug/Release; no new tests, database runs or cluster startup occurred.
Full World/social/economy/gameplay remains incomplete.
[Exact source scope and evidence](_rewrite/docs/modernization/evidence/phase-a-mail-send-protected-20261013/README.md).

New migrations **044–069** and native code extend quest state/actions, skill audit,
peer authentication, Patch metadata and Control persistence. Commit `ea7c811` adds
transactional completion/crafting rewards, magic item creation, turn-in consumption,
currency/title/skill rewards, ordinary experience/level progression and World/Map
level synchronization. Code `fa4c282` adds level benefits18182–18184, native EXP
entitlement persistence/import, effect903 lifecycle and atomic consumption through
the original ITEMUSE request. Player ENTER now carries maintained effects; CHARINFO
and ENTER sample their remaining time. Code `ff50e58` adds HP/MP/full-resource and
direct-EXP consumables, shared item cooldowns through CHARINFO/save/transfer,
migration053 and an offline TITEMUSEDTABLE importer. `6a91e4a` separates read-only
transfer confirmation from a promotion write after an unknown commit. These
bounded paths are **INTEGRATED_UNVERIFIED**.
Code `c4385af` adds Soul Lottery draw/settlement and effect820, money/AP items,
historical special-box publication/grants (migration054), original item sale and
single/equipment/all durability repair. Item/purse/title changes use the existing
native transaction and ownership fences. Follow-up `72d6504` preserves the
original security prompt before decoding protected item requests. Source differences and the bounded
ordinary-player profile are recorded in the
[implementation and compile evidence](_rewrite/docs/modernization/evidence/phase-a-lottery-boxes-20261011/README.md).
Account premium/PCBang, complete monster rewards and other item-use kinds remain
unfinished. See the [exact continuation](_rewrite/docs/modernization/full-rewrite-status.md#exact-continuation--implementation-gate).

Code `0457d3d` connects ordinary REFINE, WASTEREFINE and probability consumables
912/913 to the existing native item/effect/purse transaction. Control/World event
values reach Map and are sampled in operation receipts (migration055). World
requires an explicit Control IP allowlist. `750c797` serializes Control output,
event CRUD/tick/replay and exact-connection retirement. Conditional territory
bonuses, general effects and complete event recovery remain open. These paths
are **INTEGRATED_UNVERIFIED**; see [source contracts and limits](_rewrite/docs/modernization/evidence/phase-a-refine-events-20261011/README.md).

Code `0fe24d2` connects native GAMBLECHECK/GAMBLEOPEN and the ordinary ITEMUPGRADE
branches: identification, sealing removal, upgrade/downgrade, gems, magic, appearance,
wrapping, level reduction, refinement reset and color/effects. Probability consumables
905/911/919/921/922/939 join the existing item/effect lifecycle. New migrations056–057
and explicit import profiles preserve the complete historical gamble, gem and
accessory charts. Conditional territory/PCBang and BOW integration remain open.
These paths are **INTEGRATED_UNVERIFIED**; see [source contracts, repairs and deferred
acceptance](_rewrite/docs/modernization/evidence/phase-a-item-upgrades-20261011/README.md).

Code `3bc3bd1` connects NPC item/trainer lists and ordinary, quest, magic and PvP
purchases to the native transaction. Migration058 adds fenced PvP balances and
an explicit offline archival importer; CHARINFO and transfer retain the original
three point fields. Unknown-quest free purchases are explicitly rejected. BOW,
portal/monster shops, territory rules, expiration and full PvP remain open.
[Source scope and compile-only evidence](_rewrite/docs/modernization/evidence/phase-a-npc-shops-20261011/README.md).

Code `18ee496` adds carried-item alarms/expiry and expired-bag mail in the existing
native timer transaction. POSTLIST/POSTVIEW/POSTGETITEM/POSTDEL and entry POSTINFO
use original packets, durable attachment ownership, atomic item/fund claims and
GETITEM hooks. Migration059 and `--server-messages` publish exact backup message
bytes. Expiry, mail counters, empty-post deletion and owning-value repairs are
**INTEGRATED_UNVERIFIED**. Sending, returns, settlement, reserved/event mail and
broader gameplay remain unfinished. See the [source scope and acceptance backlog](_rewrite/docs/modernization/evidence/phase-a-item-mail-20261013/README.md).

Code `d3904c5` adds native POSTRETURN/payment, sender-owned online bill deadlines,
Map notification receipts and an explicit offline historical mailbox importer.
Migration060 preserves sender/recipient bytes and keeps the notification baseline
outside protocol fields. These paths are **INTEGRATED_UNVERIFIED**; importer and
migration are **IMPLEMENTED_UNVERIFIED**, never executed. POSTSEND, offline bill
jobs and World reserved/event/admin producers remain required.
[Source contracts, repairs and continuation](_rewrite/docs/modernization/evidence/phase-a-mail-returns-20261013/README.md).

Compilation results for the changed Map target are recorded with that increment.
Earlier evidence covers all six modern targets at its stated commits. Historical
import tools and migrations044–069 are written, not executed. No tests, database
migration, server startup or container run belongs to this increment. Existing
migrations001–068 remain unchanged; the next unused migration number is070.

## Historical verification through `2fa2396`

The following results describe their original commits, not the changed Phase A
code. The last tested increment added **native account secure codes**: all seven original requests,
PostgreSQL persistence, ten-attempt lockout, original CHARINFO fields, protected
inventory/skill purchases, same-login Map transfer and new-login relocking.
See the [bounded security contract](_rewrite/docs/modernization/evidence/security-contract.json)
and [local build/startup](deploy/README.md#native-account-security).
Final Debug, ASan/UBSan and installed Release each pass **1,455** focused native
checks; all **120** upgrade checks and six service smoke checks pass. These include
real PostgreSQL, original-byte TCP, races, rollback, restart and transfer recovery. Complete gameplay
and original-client acceptance remain unfinished; the client EXE/assets are absent.
That tested increment applied migrations **001–043**; they remain immutable.

Previous increment adds **native title lists and selection** through Map → PostgreSQL →
World → Map → original client response, including source gold-title transitions.
Migration **041** fences stale selections and skill purchases; reload and transfers
preserve original title flags. See the [title contract](_rewrite/docs/modernization/evidence/titles-contract.json)
and [local reproduction](deploy/README.md#native-title-lifecycle).
Final Debug, ASan/UBSan and installed Release each pass 1,243 focused native
checks; 97 migration checks and all six service smoke checks pass.
Complete title awards and actual-client acceptance remain unfinished.
That title increment used schema 041; current requirements are above.

Previous increment (2026-10-11) adds **native skill learning and rank upgrades** through
`CS_SKILLBUY_REQ`, the original trainer offers, prices and skill-point rules.
New migrations **039/040** publish backup trainer/title data and atomically persist
currency, ranks, titles and recovery. Debug, ASan/UBSan and installed Release each
pass 3,418 full native regression checks and a separate 1,177-check focused suite
(the suites overlap). All 41 sanitizer suites and 85 migration checks pass.
The local image is `localhost/fourstory:postgresql-skill-learning`. The bounded
scope and remaining trainer/title dependencies are tracked in the
[implementation status](_rewrite/docs/modernization/README.md#previous-increment-native-skill-learning-and-rank-upgrades-2026-10-11).

Previous increment (2026-10-11) integrates the existing **permanent postures
131/132 with ACTION and self-cure finishes**. Their backup erase flags preserve
them during ACTION. Cure and charged First Aid now run with either posture;
Defend removes eternal postures at zero MP before its response. Migration **038**
records the ordered effect change and each response audience atomically with
healing and recovery. END reaches neighboring cells, DEFEND/HPMP nearby players,
and character statistics only their owner. See [the finish/effect contract](_rewrite/docs/modernization/evidence/finish-effects-contract.json)
and [local reproduction](deploy/README.md#native-skill-finishes-with-postures).
General effects and complete combat remain unfinished. That increment used:
`localhost/fourstory:postgresql-finish-effects`.

**3,276 native PostgreSQL/network checks pass per configuration** in Debug,
ASan/UBSan and installed Release (installed daemons with the Debug backend test):
975 Map, 29 pool/TLS and 2,272 TCP checks. The new 61-check wire fixture verifies
both equipment postures, charge, exact cure bytes, empty targets, zero-MP cleanup,
late SQL rollback and SIGKILL/reconnect. Backend tests also verify a primary
handoff, concurrent finishes and stale-save rejection. Debug CTest has 188 actual
passes and 15 separately recorded fixture skips; all 40 sanitizer suites and 65
additive upgrade checks pass. Seven-socket controlled tests verify recipient
scope and private statistics. Six installed services pass health/DNS/SIGTERM.
These results certify this bounded scope; original-client acceptance remains
blocked by missing EXE/assets.

Previous increment (2026-10-11) replaces native **ACTION** placeholder success with
source learned-skill, cooldown and resource validation. Migration **037** records
charge provenance and binds it once to an accepted cast. **First Aid 240** now
finishes its own-PC cure after the backup's 300ms charge; ACTION itself deducts no
resources. Cancellation, stale-owner fencing and recovery extend the existing
native transaction path. Aid 629 requires a recall target and remains outside
own-PC healing. See [action/charge evidence](_rewrite/docs/modernization/evidence/skill-actions-contract.json)
and [local startup](deploy/README.md#native-skill-actions-and-charging).
Percentage costs now use the original pure HP/MP maxima, excluding aftermath,
equipment and effects; the previous rewrite used modified visible maxima.
Full charge interruptions, targets, combat and original-client acceptance remain
unfinished. That increment used `localhost/fourstory:postgresql-skill-actions`.

**3,101 native database/network checks pass per configuration** in Debug,
ASan/UBSan and installed Release (installed daemons with the Debug backend test).
This includes 938 native Map, 29 pool/TLS and 2,134 TCP checks. The independent
ACTION fixture verifies all 17 ACK bytes, source percentage costs with aftermath,
charged First Aid, cancellation, old-login rejection, SIGKILL/reconnect and late
SQL rollback. Backend tests cover concurrent actions, stale snapshots, early
finish rejection and charge across primary handoff. Debug CTest has 188 actual
passes and 15 fixture skips; all 40 sanitizer suites and 54 schema-upgrade checks
pass. Six installed daemons pass health/DNS/SIGTERM. Original-client acceptance
is still blocked by missing EXE/assets; these results certify only the stated scope.

Previous increment (2026-10-11): the original **CS_FINISHSKILL_ACK** path is integrated.
Client `ShotSkill`/`FinishSkill` sends this message for ordinary PC skills; most
player hits do not use `CS_DEFEND_REQ`. Migration **036** adds one-use finish
receipts and stale-snapshot fencing. The first bounded effect is learned skill
509 **Cure**, on oneself without active effects and with a zero party field. Full combat and
other finish branches remain incomplete. Verification is recorded in
[the finish contract](_rewrite/docs/modernization/evidence/skill-finish-contract.json).
Native `DEFEND` requests now refuse the unfinished client-authoritative prototype.
No supported original executable build is claimed. That increment used
`localhost/fourstory:postgresql-skill-finish`; see the
[local build/startup procedure](deploy/README.md#native-skill-finishes).

**2,890 native database/network checks pass per configuration** in Debug,
ASan/UBSan and installed Release (installed daemons with the Debug backend test).
Debug CTest has 188 actual passes and 15 fixture skips out of 203 entries;
all 40 sanitizer suites and 43 additive schema-upgrade checks pass. Six installed
services pass health, container DNS and SIGTERM checks. Native wire verification
covers exact original Cure responses, crash/reconnect, old-login replay, empty
filtered targets, late SQL rollback and rejection of forged prototype DEFEND.
Backend tests also cover concurrent finishes and pending casts across primary
transfer. These are synthetic-client/native-PostgreSQL results; original-client
acceptance and full gameplay remain pending.

Updated 2026-10-11. These scopes describe executed tests; historical handler counts
and completeness percentages are not evidence of native PostgreSQL feature parity.

| Area | Verified scope | Remaining work |
|---|---|---|
| Linux containers | Six installed daemons, health endpoints, Map → World DNS connection and graceful SIGTERM shutdown | Full persistent deployment acceptance and pinned build dependencies |
| Login | Native PostgreSQL authentication/session transactions, process ownership, duplicate protection, encrypted synthetic TCP and bounded email confirmation | Historical credential import, additional client profiles and original executable acceptance |
| Characters | Native creation/list/deletion, starter inventory, atomic item IDs, fresh world admission, client hydration, core checkpoints, logout and reconnect | Complete item/economy mutations and ancillary character state |
| Character statistics | Native 87-byte self-inspection and two-Map round-trip preservation using backup formulas and equipment | Advanced effects, special equipment branches, complete remote-player and original-client acceptance |
| Map content | Pinned source catalogs, real monster attributes, routing and actor catalogs loaded by the actual daemon | Complete spawn, movement validation, entity visibility and gameplay parity |
| Primary Map handoff | Movement-triggered transfer between two Maps, one database writer, exact state transfer, skill timers, return trips and recovery after process replacement | Full effect/quest/companion simulation, multiple-neighbor changes and special transfer branches |
| Inventory/equipment | Native carried moves/splits/merges and ordinary equip/unequip/swap; atomic displacement, IDs, HP/MP clamp and graph recovery | General effect lifecycle, advanced equipment, drops, timed/security/trade/store cases and full economy |
| Log | Native LP_LOG PostgreSQL migration, append-only runtime grants, exact raw bytes, native audit queries and process restart | Durable spool/reconciliation, closed-pool recovery, LP_CHAT and original tool/data acceptance |
| World, Control and Patch | Modern daemons and handler/transport tests; World handoff coordination | Remaining native repositories, social persistence and tool acceptance |

The preceding increment integrated **accepted native cast transactions** (migration
**035**) into the existing SKILLUSE/LOOPSKILL handler. Cost-only and free casts now
join item-consuming casts in an immediate PostgreSQL commit containing core
resources, cooldowns, raw request/ACK, inventory receipts and recovery state.
A per-character ledger chain rejects stale snapshots. Ordinary casts remove
supported permanent postures at zero MP after computing outgoing powers; loops
retain them, as in the original source.

**2,761 native checks pass per configuration** in Debug, ASan/UBSan and installed
Release (installed daemons with the Debug backend test). Debug CTest records 188
actual passes and 15 fixture skips; all 39 sanitizer suites and 35 schema-upgrade
checks pass. All six installed services pass health, DNS and SIGTERM checks. See the
[accepted-cast contract](_rewrite/docs/modernization/evidence/accepted-casts-contract.json).
That increment used `localhost/fourstory:postgresql-accepted-casts`;
[local build/startup procedure](deploy/README.md#native-accepted-cast-transactions).
This implements accepted-cast persistence; authoritative hit consumption and
complete Defend remain unfinished. The original client EXE/assets are unavailable.

The preceding increment replaces placeholder attack fields in **native SKILLUSE and
LOOPSKILL** with the original learned-rank instance-skill calculation. Physical,
ranged and magic powers, attack level, critical and aid country come from pinned
PostgreSQL catalogs and current equipment/effects. Profiles refresh on equipment
changes, cancellation, reconnect and Map handoff. The ordinary/loop AL distinction
and original packet layouts are preserved. See the [cast-profile contract](_rewrite/docs/modernization/evidence/cast-powers-contract.json)
and [local build/startup procedure](deploy/README.md#native-cast-attack-profiles).
Previous verified image: `localhost/fourstory:postgresql-cast-powers`.

**2,582 native database/network checks pass per configuration** in Debug,
ASan/UBSan and installed Release. Debug CTest has 188 actual passes and 15 fixture
skips; all 39 sanitizer suites pass. Six installed services pass health, DNS and
SIGTERM. The previous image fails the new independent outgoing-power assertion.
This projection alone does not complete combat. Accepted-cast persistence is
tracked above; authoritative Defend, full effects and unchanged-client acceptance
remain unfinished.

The preceding increment implements **client-requested cancellation of permanent warrior
postures** through the existing Map handler and native PostgreSQL (migration **034**).
The original 19-byte `CS_SKILLEND_REQ` removes the first matching effect. Replicas
answer without writing; only the current primary commits the effect, statistics,
core and recovery receipt. Absent matches retain the original ACK-only behavior.
Actual two-Map TCP scenarios cover a live posture handoff, cancellation by the new
owner and return transfer. See the [cancellation contract and results](_rewrite/docs/modernization/evidence/effect-end-contract.json)
and [local build/startup procedure](deploy/README.md#native-effect-cancellation).
That increment used `localhost/fourstory:postgresql-effect-end`.

**2,470 native database/network checks pass per configuration** in Debug,
ASan/UBSan and installed Release (Release daemons with the Debug backend test).
Debug CTest records 188 actual passes and 15 fixture skips; all 39 sanitizer suites
pass. Additive schema preservation passes 26 checks, and all six installed services
pass health, container DNS and SIGTERM checks. Skipped and unavailable acceptance
scenarios are listed separately in the contract.

The preceding **durable warrior postures** increment (033) integrated automatic
131/132 creation, equipment-driven removal, original packet ordering and native
recovery. Its recorded baseline is 2,221 native checks per configuration; see the
[posture evidence](_rewrite/docs/modernization/evidence/postures-contract.json).

Complete active-effect lifecycle, combat and gameplay remain unfinished. General
cancellation side effects, timed expiry, zero-MP removal outside the bounded
self-cure `Defend` path (including item use), dependent posture skills and remote
visibility still require implementation and acceptance.
Only uncompiled client sources are available; no supported executable build/hash,
assets or original-client acceptance is claimed. Backups and applied migrations
**001–043** are immutable; next migration **044**.

The preceding **native equipment** increment (032) verified ordinary equip,
unequip, swap, displacement, splits and HP/MP clamping with 2,111 native checks per
configuration. See the [equipment evidence](_rewrite/docs/modernization/evidence/equipment-contract.json)
for its original Debug, sanitizer and installed Release results.

The preceding increment adds **safe offline actor-catalog upgrades** (migration 031).
A verified certificate lets the existing Map server restore old graph checkpoints
under the extended statistics catalog. Publication preserves original saved packets,
fences old writers and refuses live Maps or incompatible data. Tests use an actual
previous installed Map to create logout, crash and interrupted-transfer states.
See the [upgrade contract](_rewrite/docs/modernization/evidence/actor-transition-contract.json)
and [local upgrade procedure](deploy/README.md#offline-actor-catalog-transition).
That increment used `localhost/fourstory:postgresql-actor-transition`.
The upgrade suite passes **852 checks per configuration** in Debug, ASan/UBSan and
installed Release; the existing **1,915-check** native regression suite also passes
in each configuration. Complete gameplay and actual-client acceptance remain open.

The preceding increment adds **source-derived character statistics** to the existing
Map server: the original 87-byte inspection response, equipment attributes and
World relay handlers. Migration **030** extends the pinned actor catalog to four
tables and 9,773 backup rows. Native TCP checks cover grade/gem/aftermath, broken
weapons, reconnects and preservation across two Map transfers.

**1,915 native checks pass in each of Debug, ASan/UBSan and installed Release.**
Debug CTest has **188 actual passes** (203 entries, 15 fixture skips); all **39
sanitizer suites** pass. The Release run uses installed daemons and a Debug backend test. See the
[statistics contract](_rewrite/docs/modernization/evidence/character-statistics-contract.json)
and [native Map startup instructions](deploy/README.md#native-map-character-statistics).
Complete gameplay, advanced stat effects and original-client acceptance remain
unfinished. Only client sources are currently available; no executable build or
asset identity has been verified.

Migration **020** adds the primary transfer journal, authority epochs and complete
transfer-state checkpoints. A stale source cannot overwrite its successor's state.
Original CONNECT/CONREADY packets switch the primary connection without duplicate
CHARINFO. Tests cover rollback, interrupted transfers, source/target replacement,
logout/relogin and client disconnect during commit.

The latest native-primary-transfer verification records:

- **197 Debug CTest entries:** 183 passes, eight internal legacy fixture skips,
  six explicit native fixture skips and zero failures.
- **32 ASan/UBSan CTest entries:** all passed.
- **824 native database/network checks per configuration:** Debug, ASan/UBSan and
  installed Release, using isolated PostgreSQL and source-derived synthetic peers.
- Six installed daemon health/shutdown checks and the container DNS connection.

See the [transfer contract and test reports](_rewrite/docs/modernization/evidence/native-primary-transfer-contract.json)
and [installed image evidence](_rewrite/docs/modernization/evidence/native-primary-transfer-container-verification.json).
The tested local image is `localhost/fourstory:postgresql-native-primary`; it is
not a published registry image. The original client executable has not been run.

The subsequent branch consolidation also passes **200 Debug CTest entries**
(186 passes, eight internal skips and six explicit skips), plus **87 Control
checks across three ASan/UBSan suites**. See the [merge verification report](_rewrite/docs/modernization/evidence/main-consolidation.json)
for the integrated branch tips, container smoke results and remaining Control
persistence limits.

The preceding increment persists **skill cooldowns for fresh characters that never
transfer between Maps**. Migration 021 adds atomic core/skill receipts, exact retry
checks and crash recovery. Debug, ASan/UBSan and installed Release runs each pass 181 Map,
29 pool/TLS and 191 outer TCP checks, plus the existing World/two-Map matrix.
The previous installed image fails the new online-countdown assertion. See the
[skill checkpoint contract](_rewrite/docs/modernization/evidence/skill-checkpoints-contract.json)
for source evidence, deployment requirements and limitations. The full Debug suite
remains at 200 entries (186 passes, 14 skips); all 32 sanitizer entries pass.
That increment was verified in `localhost/fourstory:postgresql-skill-checkpoints`;
its six services pass health/DNS/SIGTERM checks. See [container evidence](_rewrite/docs/modernization/evidence/skill-checkpoints-container-verification.json).

The preceding resource increment enforces **learned-skill ownership and rank-based HP/MP
costs** in native Map casts. Definitions come from the pinned backup catalogs;
checks and resource deduction are atomic. Success packets carry the learned rank,
and forged or incomplete requests cannot charge a cast. Source skill 134 at rank 2
costs exactly 88 MP in the encrypted PostgreSQL test, including durable save.
See the [skill resource contract](_rewrite/docs/modernization/evidence/skill-costs-contract.json).
No schema migration or packet change is required. Verification passes 200 Debug
CTest entries (186 actual passes, 14 skips), all 33 ASan/UBSan entries, and 880
native database/network checks per Debug, sanitizer and installed Release run.
That increment used `localhost/fourstory:postgresql-skill-costs`;
all six daemons pass health/DNS/SIGTERM checks. See
[container evidence](_rewrite/docs/modernization/evidence/skill-costs-container-verification.json).
Active effects and combat damage remain unfinished.

The preceding increment generates **native cooldowns on normal skill use** from
learned rank, source attack delay, powered weapon slots and passive/item speed
rates. Same-kind timers extend atomically without shortening a longer timer.
Actual encrypted tests cover immediate-repeat rejection, expiration and durable
save. Debug, ASan/UBSan and installed Release each pass **906 database/network
checks**; 200 Debug CTest entries have 186 passes and 14 skips, and all 33 sanitizer
entries pass. The preceding image fails the new repeated-cast assertion.

That increment used `localhost/fourstory:postgresql-skill-timing`.
Its six daemons pass health/DNS/SIGTERM smoke. See the
[timing contract](_rewrite/docs/modernization/evidence/skill-timing-contract.json) and
[container evidence](_rewrite/docs/modernization/evidence/skill-timing-container-verification.json).
Buff-dependent timing, effect expiry, cancellation and the original client
executable remain pending; the loop extension is described below. Speed-dependent casts on buff-bearing transferred
characters close before mutation; no buff speed is guessed.

The preceding increment samples **current skill cooldowns during admission**.
CHARINFO subtracts time since character loading, and CONREADY samples again for
the initial PostgreSQL checkpoint. A short cooldown can expire during admission;
neither message restarts it or changes learned ranks. A missing runtime tracker or
an active timer for an unlearned skill refuse admission. Packet layouts stay
unchanged. See the [admission timer contract](_rewrite/docs/modernization/evidence/admission-timers-contract.json).

Verification: **186 Debug passes** (200 entries, 14 skips), **33 ASan/UBSan passes**
and **910 native database/network checks per Debug, sanitizer and installed Release
run**. The previous image fails the delayed-readiness regression. That increment used
`localhost/fourstory:postgresql-admission-timers`; all six
installed daemons pass health/DNS/SIGTERM checks. See
[container evidence](_rewrite/docs/modernization/evidence/admission-timers-container-verification.json).

The preceding increment implements **native repeated skill casting** with the original
`CS_LOOPSKILL_REQ` / `CS_LOOPSKILL_ACK` layouts. Loops use source `dwLoopDelay`,
check existing cooldowns before HP/MP costs, retain learned ranks and share the
same live timer with ordinary casts. They do not rearm other same-kind skills.
Item-free weapon checks honor the original mask and durability; missing active
prerequisites return the original result code. Consumable and active-effect
branches remain explicitly unsupported. See the
[loop contract](_rewrite/docs/modernization/evidence/skill-loop-contract.json).

Verification passes **186 Debug tests** (200 entries, 14 fixture skips), all
**33 ASan/UBSan tests** and **929 native database/network checks** in each Debug,
sanitizer and installed Release run. The previous image fails the loop-ACK
regression. That increment used `localhost/fourstory:postgresql-skill-loop`;
all six installed services pass health/DNS/SIGTERM smoke. See
[container evidence](_rewrite/docs/modernization/evidence/skill-loop-container-verification.json).

The preceding implementation adds **ordinary native cast requirements**: source map
restriction, the distinct previous-active-effect requirement and non-consuming
weapon eligibility. Rejection codes and ordering follow the original server.
An unsuitable weapon on normal use retains the source own/shared-kind cooldowns
without charging HP/MP; the loop branch keeps its separate behavior. Unsupported
consumable or buff-dependent work closes before mutation. See the
[cast requirement contract](_rewrite/docs/modernization/evidence/skill-gates-contract.json).

This increment passes **186 Debug tests** (200 entries, 14 fixture skips), all
**33 ASan/UBSan suites** and **933 native database/network checks** in each Debug,
sanitizer and installed Release run. The previous installed image fails the map
restriction regression. That increment used `localhost/fourstory:postgresql-skill-gates`; all six services pass health/DNS/SIGTERM smoke. See
[container evidence](_rewrite/docs/modernization/evidence/skill-gates-container-verification.json).

An earlier increment adds **durable single-reagent consumption** for ordinary
and repeated casts on fresh primary characters. Migration **022** commits the item
count or deletion, HP/MP, timers and a receipt in one PostgreSQL transaction before
the original inventory and success packets. Checkpoint coordination prevents an
older save from undoing a cast; uncertain outcomes retain the reservation for
recovery. Depleted stacks reject further casts without another charge.

Verification: **186 Debug passes** (200 entries, 14 fixture skips), **33 ASan/UBSan
passes** and **973 native database/network checks per Debug, sanitizer and installed
Release run**. That increment used `localhost/fourstory:postgresql-skill-reagents`;
six services pass health/DNS/SIGTERM checks. See the
[reagent contract](_rewrite/docs/modernization/evidence/skill-reagents-contract.json) and
[container evidence](_rewrite/docs/modernization/evidence/skill-reagents-container-verification.json).
Migration 022 and updated Map grants introduced that storage contract.
The original client executable has not been run.

An earlier increment extends **durable reagent consumption to transferred
characters**. Migration **023** records hashes of the complete state before and
after a cast.
The current primary server validates the exact item and authority epoch, then
commits the inventory change, character core, cooldowns and audit receipt together.
The updated graph survives transfer back, relogin and process recovery, including
last-item deletion. Full unsigned item IDs and original item fields are preserved.

After transfer, the complete checkpoint is authoritative: this path leaves stale
normalized item/skill rows untouched and never uses them to restore consumed items.
Apply migrations through **023** before deploying these binaries; existing Map
runtime grants suffice. See the
[graph reagent contract](_rewrite/docs/modernization/evidence/graph-reagents-contract.json).
Equipped reagents, premium-item overrides, inventory movement and full combat remain pending.
Verification uses encrypted test peers derived from the original source; the
original client executable has not been run.

Verification passes **186 Debug tests** (200 entries, 14 fixture skips), all
**33 ASan/UBSan suites** and **1,046 native database/network checks** in each Debug,
sanitizer and installed Release run. All six container services pass health,
DNS and SIGTERM checks. That increment used
`localhost/fourstory:postgresql-graph-reagents`. See
[container evidence](_rewrite/docs/modernization/evidence/graph-reagents-container-verification.json).

The previous increment adds **source multi-attack target expansion** to native
ordinary and loop casts. Pinned `TSKILLDATA` and the learned rank determine the
hit budget; random per-target duplication and first-target padding reproduce the
original target order. All valid ranks of the three recovered templates (324,
412, 1407) require at most seven hits. Larger derived budgets above 16 fail closed.

Expanded ammunition uses the existing atomic per-bag stack transaction. The
locked database validator recomputes its full hit budget from authoritative learned
state; a transferred graph remains authoritative over stale normalized skill rows.
Resources and cooldowns apply once per cast, while ammunition is charged per
expanded hit. Migration **026** records `hit_mode=expanded` alongside the grouped
item receipts; existing receipts retain `direct`.

Verification passes **186 Debug tests** (200 entries, 14 fixture skips), all
**33 ASan/UBSan suites** and **1,589 native database/network checks** in each
Debug, sanitizer and installed Release run. Twelve migration-upgrade checks
preserve existing receipts; the previous image fails the new expansion regression.
All six container services pass health, DNS and SIGTERM checks. The local image is
`localhost/fourstory:postgresql-multi-attack` (then also `:main`), running as
UID/GID 10001:10001. The installed test uses Release daemons with verified Debug
backend/pool integration executables. See
[container evidence](_rewrite/docs/modernization/evidence/multi-attack-container-verification.json).

See the [multi-attack contract](_rewrite/docs/modernization/evidence/multi-attack-contract.json).
Complete combat damage/target/AOI rules, active effects, equipment mutation and
original-client executable acceptance remain pending. The random outcome range is
preserved; the original Windows process seed sequence is not reproduced.

The preceding increment adds **native inventory moves**: whole stacks can move to
an empty carried-bag slot, and different item templates can swap positions within
or between bags. IDs, counts, raw attributes and client descriptors are conserved.
The source five-byte request and DEL/ADD or ordered UPDATE responses are retained;
MOVEITEM success follows confirmed commit. Bag capacities come from the backup
(default bag template 3 has 16 slots; template 4 has four).

Migration **027** makes the existing unique item-slot constraint deferrable inside
a swap and adds grouped movement receipts. Fresh rows or the complete transferred
checkpoint commit atomically with core and timers. Recovered graphs stay authoritative
over stale normalized item positions. Apply migration 027 and the updated Map grants.

Verification passes **186 Debug tests** (200 entries, 14 fixture skips), all
**33 ASan/UBSan suites** and **1,730 native database/network checks** in each
Debug, sanitizer and installed Release run. Thirteen migration-upgrade checks
verify preservation, atomic swaps and receipt constraints; the previous image
fails the new inventory packet regression. All six container services pass
health, DNS and SIGTERM checks. The local image is
`localhost/fourstory:postgresql-inventory-moves` (then also `:main`), running as
UID/GID 10001:10001. The installed test uses Release daemons with verified Debug
backend/pool integration executables. See
[container evidence](_rewrite/docs/modernization/evidence/inventory-moves-container-verification.json).

[Inventory move contract](_rewrite/docs/modernization/evidence/inventory-moves-contract.json)
records the preceding scope and tests. The stack increment below adds split/merge
and same-template unequal swaps. Item dropping, timed bags, secured inventories
and equipment changes remain unsupported.
Original-client executable acceptance and complete gameplay remain unfinished.

The current increment adds **native stack splitting and merging** in the existing
MOVEITEM handler. Raw CTItem equality decides merge versus full-stack swap, even
for the same template. Splits atomically allocate a new ID from the same world
counter as character creation; merges retain destination identity and clamp to
pinned stack capacity. A full destination preserves the original successful no-op
packet sequence. Original UPDATE/DEL/ADD ordering follows confirmed commit.

Migration **028** adds paired stack receipts without rewriting earlier migrations
or historical data. Fresh rows or complete transfer graphs commit with the allocator,
core, timers and receipts. Actual encrypted tests split/move on the second Map and
merge after returning to the first. Concurrent split, late rollback, disconnect
and recovery tests preserve quantities and IDs.

Verification passes **186 Debug tests** (200 entries: eight internal legacy skips
and six explicit native fixture skips), **34 ASan/UBSan suites**, and **1,861
native database/network checks** separately in Debug, sanitizer and installed
Release runs. Seventeen migration-upgrade checks preserve existing rows/receipts
and verify stack constraints. The prior image fails the new split request after
a real primary transfer. All six services pass health/DNS/SIGTERM smoke.
The local image is `localhost/fourstory:postgresql-inventory-stacks` (also `:main`),
UID/GID 10001:10001. Installed verification uses Release daemons with verified
Debug backend/pool integration executables.
See the [stack contract](_rewrite/docs/modernization/evidence/inventory-stacks-contract.json)
and [installed image evidence](_rewrite/docs/modernization/evidence/inventory-stacks-container-verification.json).

**No original client binary build is currently certified.** The owner has only
uncompiled client source, without game data. Protocol source version `0x2918`
does not identify an executable release. The [client acceptance gate](_rewrite/docs/modernization/client-acceptance.md)
records the missing executable/data/environment and all blocked scenarios. Complete
gameplay, remaining service persistence and tools remain unfinished; no overall
completion percentage or full compatibility claim is made.

## Database authority

`TGAME_RAGEZONE.bak` and `TGLOBAL_RAGEZONE.bak` are the immutable historical source
of truth. Keep their hashes and recovered values unchanged. When server contracts
need adaptation, change the derived PostgreSQL schema through a new migration.

- `legacy_game`, `legacy_global` and `legacy_game_tgame` preserve recovered data.
- `content` and the compatibility views expose explicit, versioned projections.
- `app_global` and `app_world` own mutable application and operational state.
- Applied migrations **001–028 are immutable**; the next schema change starts at 029.
- Backups, credentials and private extraction output stay outside Git. Historical
  accounts, player records and missing combat values are never invented.

See [database setup and contracts](database/README.md),
[forensic reconstruction](_rewrite/docs/database-reconstruction/README.md) and
[preservation checks](_rewrite/docs/modernization/evidence/preservation.json).
SQL Server is unnecessary for the verified native runtime paths. ODBC comparison
paths and unported services remain in the repository and runtime image.

## Linux quick start

Install Git LFS when working with the original client/library assets:

```sh
git lfs install --local
git lfs pull
```

Run the six-service development topology with Docker Compose:

```sh
docker compose up --build -d
docker compose ps
docker compose logs -f
docker compose down
```

The committed Compose configuration uses in-memory services without a database.
It is a process/transport smoke environment; native PostgreSQL requires the
migrations, activated catalogs, dedicated database roles and private configuration
in [deploy/README.md](deploy/README.md). Keep deployment secrets in ignored
configuration files or private environment files.

Build and verify a runtime image with rootless Podman:

```sh
podman build --target runtime --build-arg BUILD_JOBS=2 \
  -t localhost/fourstory:linux .
python3 tools/container_smoke.py --engine podman \
  --image localhost/fourstory:linux
```

Each container runs one daemon as UID/GID `10001:10001`. The binaries are installed
under `/opt/fourstory/bin/`: `tloginsvr_asio`, `tmapsvr_asio`, `tworldsvr_asio`,
`tcontrolsvr_asio`, `tpatchsvr_asio` and `tlogsvr_asio`.

## Build and test

For a native Ubuntu 24.04 development environment:

```sh
sudo apt-get install build-essential cmake ninja-build pkg-config \
  libboost-system-dev libssl-dev openssl libspdlog-dev \
  libtomlplusplus-dev libsoci-dev unixodbc-dev libpq-dev
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
```

`linux-release` builds the servers without test targets. `linux-asan` builds and
runs the selected sanitizer suites:

```sh
cmake --preset linux-asan
cmake --build --preset linux-asan
ctest --preset linux-asan
```

Native integration tests need an owned disposable PostgreSQL lab. The
[deployment guide](deploy/README.md) documents `run_native_verification.py`, pinned
reference manifests and role grants. Tests needing absent fixtures are skipped;
a passing default CTest run does not imply that those database checks executed.

The [Linux CI workflow](.github/workflows/linux-container.yml) builds the container,
runs tests and verifies synthetic PostgreSQL Login/pool flows. Local evidence does
not establish the outcome of a GitHub Actions run.

The Windows CMake/vcpkg build path remains available:

```powershell
$env:VCPKG_ROOT = "C:\vcpkg"
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release
```

The latest native PostgreSQL runtime is verified on Linux; equivalent Windows
verification is pending. The stack uses Boost.Asio coroutines, SOCI/libpq,
OpenSSL, spdlog and toml++. Linux images use Ubuntu packages. The vcpkg baseline,
container base digest and package versions still need release-level pinning.

## Repository and documentation

| Location | Purpose |
|---|---|
| `Server/T*Svr/`, `Client/` | Original server/client behavior and protocol references |
| `Server/T*SvrAsio/` | Modern server implementations and component tests |
| `Lib/Own/FourStoryCommon/` | Shared database, cluster, logging and operations infrastructure |
| `Lib/Own/TNetLib/`, `Lib/Own/TProtocol/` | Transport, packet codec and protocol definitions |
| `database/postgresql/` | Numbered migrations and database compatibility contracts |
| `deploy/`, `Dockerfile`, `compose.yaml` | Linux deployment, sample configuration and role grants |
| `tools/database/` | Extraction, migration and isolated integration verification |
| `_rewrite/docs/modernization/` | Current status, protocol traceability, evidence and continuation |
| `_rewrite/docs/database-reconstruction/` | Backup analysis, provenance and unresolved historical contracts |

Component references: [Login](Server/TLoginSvrAsio/README.md),
[Map](Server/TMapSvrAsio/README.md), [World](Server/TWorldSvrAsio/README.md),
[Control](Server/TControlSvrAsio/README.md), [Patch](Server/TPatchSvrAsio/README.md),
[Log](Server/TLogSvrAsio/README.md) and [shared infrastructure](Lib/Own/FourStoryCommon/README.md).
The [older patch catalog](_rewrite/docs/PATCH_README.md) and
[legacy-to-modern changelog](_rewrite/docs/CHANGELOG_LEGACY_TO_MODERN.md) retain
historical context; current acceptance is tracked in the modernization documents.

Next priorities are active buff/timer semantics, skill cancellation and consumable inventory,
complete entity visibility, transactional item/economy
operations, native social persistence and execution of the supported original
client. Full feature parity is not yet established.

## License

See the original notices in `Server/TLoginSvr/` and the other legacy subtrees.
No separate project-wide license grant is established by this README.
