# Native cast tenure and pure-CURE exceptions

Status: **IMPLEMENTED / INTEGRATED_UNVERIFIED** on baseline `5f81b9f`.
Only source, declarations, callers and diffs were inspected. No build, test,
SQL execution, import, extraction, service or client was run. No migration,
protocol field, public service interface or secondary-state polling was added.

## Source contract and the actual dispatch gap

The original client sends ACTION, SKILLUSE and LOOPSKILL to all connected Maps:
`Client/TClient/CSSenderAll.cpp:319–340`, `489–617` and `620–861`.
The source own-PC ACTION branch requires the main actor with an actual Map
(`Server/TMapSvr/CSHandler.cpp:1233`, `1276–1280`). SKILLUSE and LOOPSKILL resolve
PC actors only when main and mapped before looking up the learned skill or
charging it (`2512–2538`, `3111–3143`). Their monster, recall, companion, self
object and foreign-actor contracts are separate from this native own-PC slice.

The initial Ready-replica opcode filter already exists in modern
`handlers/dispatch.cpp`: only MOVE and SKILLEND pass. A fresh partial replica
therefore does not normally reach prototype SKILLUSE/ACTION/DEFEND merely
because its payload is absent. No direct production exploit from that initial
state is claimed here, and no immutable-backend/public capability was added.

There was a distinct await boundary: dispatch captured the initial identity,
checked the role allowlist, then waited for `BeginGameplay`. The registry method
checks only Ready and absence of a checkpoint; it does not compare the request's
original role or authority epoch (`services/session_registry.h:185–191`). A
whole transfer can change Primary/epoch to Replica/new epoch and return to Ready
while this request is suspended. The demotion path is real:
`handlers/session.cpp:147–167` waits for the committed outgoing transfer,
calls `FinishTransfer(Replica, epoch+1)`, then drops the old full payload.
`BeginTransfer`/`FinishTransfer` in `services/session_registry.h:149–167` define
these transitions. Successful native PostgreSQL writers already reject a
non-primary claim; the correction also prevents a stale request from reaching
planning, error ACKs or prototype fallback under a different tenure.

Dispatch now compares current open/Ready state plus character, user, key,
channel, connection, role and authority epoch against the captured identity on
each unsuccessful lease acquisition and after a successful `BeginGameplay`.
The original secondary opcode allowlist remains unchanged. After successful
acquisition, the RAII `SessionOperation` exists before the comparison, so stale
early return releases precisely that operation. A request cannot inherit a new
role/epoch after waiting. This is a modern generation fence around the original
main-actor eligibility, not a claim that local operations prevent transfer from
starting after the check; existing PG claim fences and output projections remain
responsible for later awaits.

## Source CURE exceptions

Original FINISHSKILL first resolves the outgoing skill's learned trigger and
samples critical and powers (`Server/TMapSvr/CSHandler.cpp:20380–20425`). Target
eligibility and `GetAtkHitType` occur before the hardcoded exceptions
(`20535–20614`). The outgoing pure positive HP/MP CURE profile remains required;
no arbitrary skill is admitted merely because its ID matches an exception.

For outgoing128 or229, source checks `FindMaintainSkill(skill+1)` and erases
matching maintained129 or230 rows when present (`20601–20605`);
`FindMaintainSkill` finds the first row, while `EraseMaintainSkill(WORD)` removes
all matching rows (`TObjBase.cpp:2577–2594`, `2767`). The current native
`SupportedPostures`/`ReadProgressEffects` graph admits only the explicitly
published posture and progress/marker definitions, not129/230
(`services/posture_effects.h`, `services/postgresql_map_load.cpp:98–147`). The
new helper checks the actual supported effects vector and rejects a present
partner branch. Absence is the supported source no-op; no END, STAT, guessed
effect, or silent removal is synthesized. Rich partner-effect removal remains
**OPEN** and requires its actual lifecycle before that graph can be admitted.

For outgoing3604, source looks up learned3603 immediately before Defend. A
present object supplies its actual BYTE rank, including0; a missing object
returns before Defend and the final CheckEquipSkill (`20607–20614`).
`CTObjBase::FindTSkill` tests map presence without a positive-rank condition
(`Server/TMapSvr/TObjBase.cpp:2486–2493`). Native derivation requires an actual
pinned chart for every learned row; 3603 is used only for this rank, never for
its own cost, reuse, weapon eligibility, instance ability or attack profile.

The source override changes the rank used by outgoing CURE calculations and
the DEFEND rank byte. It does not retroactively change trigger critical,
outgoing attack type, attack level or powers. The outgoing template retains its
own Calc2 start/next/growth metadata. Source's mutable rank persists into later
target hit tests, but all admitted pure-CURE profiles use the existing
guaranteed HITTEST1. After the mode2/critical255 MISS gates, kind21 returns
NORMAL early; ordinary HITTEST1 still rolls `rand()%100`, but HitInit100/HitInc0
makes its result independent of rank. This does not claim the same RNG stream.
A copied power profile with only its rank changed per eligible target is
therefore equivalent in this scope. The separate
3604 ABILITY damage branch in `TObjBase.cpp:826–889` remains unavailable because
every admitted effect row is HP/MP CURE, never ABILITY.

## Actual production callers and persistence

`PostgreSQLMapService::ResolveNativeCureEffectRank` is private and shared by
`PreflightActItemCure` and the per-target native FINISH loop. It validates
payload/effects/support before the partner-effect lookup and does not infer
missing3603 from rank0. It is deliberately absent from `ReadActItemSkill`.

The real action-item path remains ITEMUSE type25 → ACTION → ACTITEMSUSE →
accepted cast → FINISHSKILL. ITEMUSE init (`postgresql_map_item_use.cpp`) and
ACT acceptance (`postgresql_map_actitems.cpp`) refresh actual equipment/stats,
read the pinned outgoing profile, then call preflight before new hooks/debit.
Preflight resolves effect rank even for anticipated MISS because the source
3603 lookup precedes Defend. MISS still skips numerical evaluation and keeps
the existing actual802 capability check. For NORMAL, the checked cure helper
uses the resolved rank before item consumption, including actual Calc2 and
signed source variance bounds. The ACT wire ACK remains rank1/critical0/zero
powers, and no ordinary skill cost/reuse/charge is borrowed.

Shared `FinishSkill` (`services/postgresql_map_checkpoint.cpp`) retains primary
claim/account locks, current owner, pinned catalogs, skill-context publication,
physical cell, inventory/checkpoint consistency and one-use accepted occurrence
selection for ordinary, loop and ACT origins. After its actual current refresh,
each eligible living target resolves the effect rank before activity clocks,
RNG, effects or output are changed. The resolver covers NORMAL and MISS. Only
the copied effect profile's rank is used by `native_cure::Delta` and
`EncodeCureDefend`; original critical, powers, attack type and attack level are
retained. Unknown Calc/combine source behavior, signed DWORD arithmetic, final
death, zero-MP cleanup and packet ordering are unchanged.

Empty FINISH does not call this per-target resolver, needs no learned3603 and
still consumes its matched occurrence and runs CheckEquipSkill. A later target
after an actual death is skipped before the resolver, as before. Missing3603
for a nonempty eligible target is an explicit native unsupported refusal before
mutation/receipt completion. This differs from the source's silent early return;
the existing failure policy does not fabricate success, refund an earlier ACT
debit, consume the finish receipt or replay an uncertain write. Init/ACT also
preflight this capability earlier than source, so they can refuse before debit.
Current source data, not a seeded fixture or ID assumption, must satisfy every
profile and publication requirement; no actual chart availability was executed
or claimed in this increment.

## Remaining boundaries and deferred acceptance

Secondary HP/MP/dead state convergence stays **OPEN**. Original ENTERCHAR carries
no PC HP/MP/maxima; the HPMP client consumer updates client UI and optional
CORPS reporting, not another Map's actor cache. A fresh native partial replica's
zero bars mean unknown, while a demoted replica may retain stale old bars after
its payload is dropped. Neither is an authoritative living/dead or full-stat
snapshot. Promotion still obtains durable full core/graph and retains actual
target-local cluster/mode/recovery clocks. No polling, new sequence, wire field
or client-output cache writer was introduced. General NPC/foreign-actor DEFEND,
other-target cures, richer effects and original full source bootstrap remain
**OPEN**.

Phase B must exercise an actual original-client ITEMUSE/ACT/FINISH flow with
published definitions, alongside ordinary and loop receipts, and cover:

- checkpoint wait spanning full demotion/return to Ready: old tuple never
  reaches switch, emits a cast ACK or mutates state; unchanged tuple proceeds;
- full char/user/key/channel/connection/epoch/role drift, close, initial replica
  allowlist, and RAII operation release on stale return;
- 128/229 with absent partner effects and explicit refusal for a present richer
  partner graph, without a fabricated END;
- 3604 with missing3603, actual rank0 and positive rank; trigger rank differing
  from effect rank; physical/magical trigger critical and outgoing powers stay
  independent of3603;
- NORMAL Calc0..3/default/combine behavior using outgoing formula metadata and
  resolved current rank; MISS carries that rank but does not evaluate magnitude;
- empty FINISH with no3603, repeated eligible targets, post-death target skip,
  final equipment cleanup, one-use finish receipt and unknown-commit no-replay;
- no assumption that client DEFEND/HPMP/DIE updates secondary server state.

These checks are deferred. No runtime, encrypted client or database result is
claimed by this static implementation.
