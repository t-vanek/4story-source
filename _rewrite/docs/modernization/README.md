# Modern server implementation status

Updated 2026-10-09 (Europe/Prague). Target: original client → compatible C++20 server → native
PostgreSQL, primarily Linux containers. The overall reimplementation is **in
progress**. No claim of complete gameplay or real-client compatibility is made.
Older C#/.NET plans and percentage estimates in `GAP_ANALYSIS.md`,
`COMPLETENESS_ANALYSIS.md` and `PROTOCOL.md` are historical planning material;
use current code and the evidence linked here to assess the C++ rewrite.

Repository consolidation on 2026-10-09 integrates the previously unmerged Control
service-management work and preserves every existing branch tip in `main`.
The subsequent Debug run has 200 entries: 186 actual passes, eight internal
legacy skips, six explicit native skips and no failures. Three additional Control
ASan/UBSan suites pass 87 checks, including inventory replacement during an
awaited status query. Container configuration leaves service lifecycle to
Compose/Podman. The optional Control registry adapter remains distinct from
native PostgreSQL acceptance. See [consolidation evidence](evidence/main-consolidation.json)
for its original image digest and branch integration; the feature
reports below retain their original tested image identities and counts.

## Current verified increment: native stack split/merge and two-Map inventory

The existing native inventory handler now implements the source partial-copy and
merge branches, plus same-template unequal swaps. Raw magic and all six DWORD
extensions participate in equality; grade effect/count/identity do not. Splits get
one real database ID, retain raw fields and publish UPDATE/ADD/MOVEITEM. Merges
retain destination identity, clamp to bStack and publish source UPDATE/DEL followed
by destination UPDATE/MOVEITEM. Full destinations preserve the source successful
no-op. Over-capacity corrupt destinations are explicitly refused.

Migration 028 adds paired stack receipts. The same transaction fences the current
primary, validates the complete graph or locked owned rows, allocates from the
shared Login/Map high-water, writes items or authoritative graph, then commits core,
timers and both receipts. Failed second receipts roll back allocation and deletion.
Concurrent requests from one snapshot yield one split. Unknown outcomes never retry
or permit stale final saves. Disconnect during confirmed commit saves the new ID.

Verification passes **186 Debug tests** (200 entries: eight internal legacy skips
and six explicit native fixture skips), **34 ASan/UBSan suites**, and **1,861
native database/network checks** separately in Debug, sanitizer and installed
Release runs. Seventeen migration-upgrade checks preserve existing rows/receipts
and verify stack constraints. The prior image fails the new split request after
a real primary transfer. All six services pass health/DNS/SIGTERM smoke.
The local image is `localhost/fourstory:postgresql-inventory-stacks` (also `:main`),
UID/GID 10001:10001. Installed verification uses Release daemons with verified
Debug backend/pool integration executables.
Image ID: `e5ae8ab0df918cd1863b1d7984186bf95b2980ad62026794a36df8bcaf7c4fe8`.

Native totals comprise 589 backend, 29 pool/TLS, 402 outer TCP, 53 skill lifecycle,
295 World handoff, 66 secondary, 141 reagent two-Map, 141 ammunition two-Map,
24 fresh ammunition, 30 batch, 23 multi-attack, 29 whole-stack move, 32 stack
and seven rejection checks. Two-Map tests now split/move on the promoted Map,
transfer the graph back and merge on the new primary without private packets to
replicas or normalized-row fallback. Native process replacement/relogin preserves
both an unsigned source and its newly allocated child. Existing generic TCP
SIGKILL/database/World-loss coverage remains; this increment does not claim a
new split-specific SIGKILL wire scenario.

Backups and migrations 001–027 retain hashes; 028 is now immutable and next is 029.
Apply 028 and current Map grants before these binaries. See the
[stack contract](evidence/inventory-stacks-contract.json),
[container evidence](evidence/inventory-stacks-container-verification.json),
[build fingerprints](evidence/inventory-stacks-build-fingerprints.json) and
[owned-lab cleanup](evidence/inventory-stacks-cleanup.json).

The owner confirms only uncompiled client source and no additional client files.
No supported executable build or original-client run is declared; see the
[acceptance gate](client-acceptance.md) and [current code audit](evidence/modernization-status-audit.json).
Equipment, active effects/combat/quests/economy/social/modes, remaining service PG
paths and original tools remain in scope and unfinished. All versioning stays local
on main; no push/publication/external deployment.

## Earlier verified increment: native carried inventory moves

The actual native `CS_MOVEITEM_REQ` path now moves a whole stack into an empty
slot or swaps different item templates. Both preserve original uint64 identity,
quantity, magic and full extended attributes. Source count clamping is preserved:
a move accepts a requested count larger than the stack, and a different-template
swap exchanges entire stacks even when the request says one. Source missing-bag,
missing-item, zero-count and same-position result codes are retained.

Bag capacity is loaded from pinned `TITEMCHART` on fresh and transfer hydration;
the default template 3 provides 16 slots and template 4 provides four. Positions
outside that capacity are refused instead of reproducing the original unchecked
FindTItem behavior. At that increment splitting/merging were deferred; 028 above extends them.
Dropping, equipment, timed bags and secure-code inventories remain unsupported.

Migration 027 adopts the existing unique `item_slot` index as a deferrable,
initially immediate constraint. A swap defers and revalidates it inside its single
transaction, with no temporary fake storage or slot. Fresh storage locks and
checks both exact owned rows, counts/templates and destination occupancy. Graph
storage compares the full authoritative checkpoint and preserves unrelated state.
Grouped `inventory_movements` receipts, core, cooldowns and recovery state commit
before any response. An unknown outcome keeps the reservation and refuses stale
final saves, with no automatic retry. The updated Map grants permit only the two
new position columns and the append-only movement ledger/sequence.

**1,730 native checks pass separately in Debug, ASan/UBSan and installed
Release runs:** 546 backend, 29 pool/TLS, 380 outer TCP, 53 skill lifecycle,
295 World handoff, 66 secondary lifecycle, 124 reagent two-Map, 124 direct
ammunition two-Map, 24 fresh single-hit, 30 fresh batch, 23 fresh multi-attack,
29 fresh inventory and seven rejection checks. Debug CTest has 186 actual
passes (200 entries, eight internal legacy skips and six explicit native
fixture skips); all 33 sanitizer suites pass. Thirteen migration-upgrade
checks verify preservation, atomic swaps and receipt constraints. The previous
installed image fails at the first inventory error response because it has
no native MOVEITEM handler.

The installed run uses Release daemons with verified Debug backend/pool test
executables. Six-service health/DNS/SIGTERM smoke passes. Local image
`localhost/fourstory:postgresql-inventory-moves` (then also `:main`), ID
`1edff25ea38aaaeafb3a7d868880649a9b5e3b88976ce63a67d4ef0783c6cea2`,
UID/GID 10001:10001. See [container evidence](evidence/inventory-moves-container-verification.json).

Native fixtures cover fresh and graph moves, stale hashes/counts, occupied target,
late second-receipt rollback, unsigned graph IDs, return transfer, process recovery
and fresh relogin. Handler tests also exercise a disconnect during commit, unknown
outcome, unchanged live timers and private ACK order. Encrypted TCP tests use fresh
state for whole-stack cross-bag moves and swaps, delayed commit, exact descriptors,
error codes, capacity/split refusal and relogin. That increment verified the service/transfer boundary; 028 above adds actual
two-Map inventory packet acceptance.

Backups and migrations 001–026 retain hashes; 027 became immutable; 028 above extends the schema.
Apply 027 and updated `map-runtime-grants.sql` before these binaries. See
[source contract](evidence/inventory-moves-contract.json),
[build fingerprints](evidence/inventory-moves-build-fingerprints.json) and
[owned-lab cleanup](evidence/inventory-moves-cleanup.json).
Only local commits and local images are produced; no GitHub push or deployment.
Full equipment/stat invalidation, effects, combat and original-client acceptance
remain unfinished.

## Earlier verified increment: source multi-attack expansion

Native owned-PC ordinary and loop casts now derive `IsMultiAttack` and
`GetCountMultiAttack` from pinned source rows and the learned rank. The source
ability sum uses only SA_ONCE/SDT_ABILITY/MTYPE_EFC rows, signed INT deltas and
BYTE narrowing. Original random per-target duplication and first-target padding
build the final defender list. Source templates 324/412/1407 require respectively
3–6, 3–7 and six hits at their valid learned ranks. Budgets above 16 are refused;
that avoids the original inner random loop's possible MAX_TARGET overrun outside
this recovered content. Original Windows seed sequences are not reproduced.

Final expanded targets determine ammo quantity. The fresh transaction locks the
learned skill and validates its rank against the plan; transferred characters use
the complete checkpoint's learned rank even when normalized skill rows disagree.
All stack debits, core, timers, checkpoint and receipts still commit together
before private item ACKs and cast success. Migration **026** adds `hit_mode` and
preserves existing receipts as direct; no extra Map grants are needed.

**1,589 native checks pass separately in Debug, ASan/UBSan and installed
Release runs:** 478 backend, 29 pool/TLS, 336 outer TCP, 53 skill lifecycle,
295 World handoff, 66 secondary lifecycle, 124 reagent two-Map, 124 direct
ammunition two-Map, 24 fresh single-hit, 30 fresh batch, 23 fresh multi-attack
and seven rejection checks. Debug CTest has 186 actual passes (200 entries,
14 fixture skips); all 33 sanitizer suites pass. Twelve migration-upgrade
checks preserve existing receipts. The previous installed image fails exactly
at the six-hit target expansion regression.

The installed run uses Release daemons with verified Debug backend/pool test
executables. Six-service health/DNS/SIGTERM smoke passes. Local image
`localhost/fourstory:postgresql-multi-attack` (then also `:main`), ID
`528ec324da8943c252206ec3588342cd8db9a397a524fa9313d47e6e04e5507e`,
UID/GID 10001:10001. See [container evidence](evidence/multi-attack-container-verification.json).

Native fixtures derive all three recovered templates, validate stale rank and
wrong-count rejection, rollback, return transfer and process replacement/relogin.
Encrypted TCP tests exercise skill 324 rank 1 and 1407 rank 1 in ordinary and loop
paths, exact target tuples, atomic mixed-stack arrows, one MP charge per cast and
cooldowns across relogin. Multi-attack graph consumption is additionally verified
at the service/transfer boundary; actual multi-attack TCP in this increment uses
fresh state. Existing two-Map TCP continues to cover direct multi-target ammo.

That increment preserved backups and migrations 001–025 and made 026 immutable.
Migration 027 now adds inventory moves above. Source zero-target ammo, reagent-plus-weapon combinations,
premium overrides, equipment/durability mutation, active effects and full combat
remain pending. No original client executable was run. See the
[source contract](evidence/multi-attack-contract.json),
[build fingerprints](evidence/multi-attack-build-fingerprints.json) and
[owned-lab cleanup](evidence/multi-attack-cleanup.json). Only local commits on `main`
are authorized until complete gameplay is finished; no GitHub push or deployment.

## Earlier verified increment: atomic ammunition batches

Owner policy: commit only locally on `main`; GitHub pushes, publication and
remote deployment remain paused until complete gameplay is finished.

Owned-PC ordinary and loop casts now charge 1–16 flagged non-expanded targets
across multiple stacks in a single bag. Original `UseSkillItem` ordering, BYTE
accumulation, same-kind template selection and no cross-bag aggregation are
preserved. Selection and durable validation agree on the exact ordered debits.

All stack writes, core, timers, checkpoint and audit rows commit together before
any item response or cast success. Migration **025** groups per-stack receipts by
`cast_id` and records `hit_count`; reagent receipts still consume one unit.
Existing Map grants suffice. Fresh storage revalidates owned rows and the loaded
weapon; transferred storage validates the complete original graph and preserves
raw extensions and unrelated data without writing stale normalized children.
The same behavior survives return transfer, relogin at epoch zero and recovery.

Tests inject stale second-stack hashes and second-receipt failures, reject wrong
quantities/order/duplicates, and prove whole-transaction rollback. Encrypted
ordinary/loop tests cover 4 and 16 targets, unflagged and overflow tuples, mixed
arrow templates, bolt stacks, delayed commit, cooldown and depletion. Two-Map
network tests charge four targets before and after the return transfer; service
fixtures additionally cover multi-stack complete-graph mutation and recovery.

[Debug](evidence/native-ammo-batch-debug.json),
[ASan/UBSan](evidence/native-ammo-batch-asan.json) and
[installed Release](evidence/native-ammo-batch-release.json) each pass **1,472 checks**:
406 native Map, 29 pool/TLS, 314 outer TCP, 53 skill, 295 World handoff, 66 World
secondary, 124 reagent two-Map, 124 multi-target ammunition two-Map, 24 fresh
single-hit, 30 fresh batch and seven rejection checks. There are 186 actual Debug
CTest passes (200 entries, 14 fixture skips) and 33 passing sanitizer suites.
Nine separate migration-upgrade checks preserve old receipts and reject invalid
or duplicate debits. The previous single-hit image fails the new regression.

The installed run uses Release daemons and verified Debug backend/pool test
executables. Six-service health/DNS/SIGTERM smoke passes. Local image
`localhost/fourstory:postgresql-ammo-batch` (then also `:main`), ID
`1766f88f4fafcec132022d0e1913ac9f42af54470a306c7eee7645b4bab44531`,
UID/GID 10001:10001. See [container evidence](evidence/ammo-batch-container-verification.json).

This increment preserved backups and migrations 001–024, then made 025 immutable.
It left source multi-attack expansion to migration 026, verified above. Zero-target
ammunition, premium overrides, reagent-plus-weapon combinations, equipment
mutation, effects and full damage remain pending. No original client executable was run. See the
[batch contract](evidence/ammo-batch-contract.json),
[build fingerprints](evidence/ammo-batch-build-fingerprints.json) and
[owned-lab cleanup](evidence/ammo-batch-cleanup.json).

## Earlier verified increment: single-hit ammunition

Ordinary and loop casts now consume one arrow or bolt when the pinned skill has
no separate reagent and its first compatible powered weapon requires one unit.
Selection uses the item's source kind and unsigned bag/slot order. Exactly one
flagged, non-expanded target is required; source `SDT_ABILITY/MTYPE_EFC` attacks
remain unsupported. Missing ammo returns original UNSUITWEAPON, preserving normal
versus loop timer side effects. Other resource and cooldown gates remain intact.

The existing private cast plan and checkpoint lease cover both storage paths.
Fresh transactions lock selecting inventory and validate the loaded weapon hash;
contract-2 transactions use the complete graph. Item decrement/deletion, core,
cooldowns and typed audit receipt commit before original inventory/success packets.
Stale owners, epochs, weapon state, item kind or fingerprints cannot charge ammo.
Graph consumption continues to preserve raw fields and unrelated sections, with
no fallback to stale normalized children after transfer or relogin.

Migration **024** adds `consumption_kind`, leaving existing receipts as `reagent`.
Apply it before deploying the new binaries; existing Map grants suffice. Backups
and migrations 001–023 retain hashes; 024 is now immutable, and the next migration
is **025**. The backup has 142 ammunition weapons (65 arrows, 77 bolts), each with
count one. Windows premium templates 25020–25022 are absent; those overrides need
an explicit future content decision. No historical templates were fabricated.

[Debug](evidence/native-ammunition-debug.json),
[ASan/UBSan](evidence/native-ammunition-asan.json) and
[installed Release](evidence/native-ammunition-release.json) each pass **1,341 checks**:
349 native Map, 29 pool/TLS, 270 outer TCP, 53 skill, 295 World handoff, 66 World
secondary, 124 reagent two-Map, 124 ammunition two-Map, 24 fresh arrow/bolt and
seven rejection checks. CTest has 186 actual Debug passes (200 entries, 14 fixture
skips) and all 33 sanitizer suites pass. The existing independent packet golden
fixtures pass with unchanged layouts.

Native tests cover broken/stale equipped weapons, wrong ammo kind, claim/epoch
fences, rollback, audit classification and complete-graph recovery. Encrypted tests
use source skill32 with bow701/arrow8401 and crossbow801/bolt8402. Admission tests
also prove zero/multiple ammo hits cannot consume or arm timers before closing.
Service-level buff retention in a graph does not establish live buff simulation.

The installed run uses Release daemons and the verified Debug backend/pool
integration executables. The previous graph-reagents image fails the new delayed
ammunition transaction assertion. Current local image:
`localhost/fourstory:postgresql-ammunition` (also `:main`), ID
`0eac33bc3a33a7981dba32a33e22e21a9f4dd50d711bf3fc661e467815ca09d2`,
UID/GID 10001:10001. All six services pass health/DNS/SIGTERM smoke. See
[container evidence](evidence/ammunition-container-verification.json),
[build fingerprints](evidence/ammunition-build-fingerprints.json) and
[owned-lab cleanup](evidence/ammunition-cleanup.json).

See [source contract and acceptance boundaries](evidence/ammunition-contract.json).
Multi-hit/stack charging, reagent-plus-weapon combinations, equipment/durability
mutation, active effects, combat damage and original-client execution remain pending.

## Earlier verified increment: reagents in complete transferred state

Migration 023 extends durable single-reagent consumption to the current primary
after transfer, transfer back and graph-backed relogin. Contract-2 transactions
verify the complete persisted checkpoint and canonical original item bytes. They
allow exactly one bag-item decrement/deletion alongside current core and sampled
cooldowns; learned IDs/ranks and all other graph sections must match. Raw magic,
DWORD extensions, unsigned 64-bit item IDs and same-template cabinet items survive.

The complete graph is authoritative even when normalized `TITEMTABLE` and
`TSKILLTABLE` rows are stale. This path neither rewrites nor reloads those rows.
The new graph, core, timers and before/after audit hashes commit together before
original private UPDATEITEM/DELITEM, MOVEITEM and success responses. Existing
checkpoint coordination and uncertain-outcome recovery protections remain in force.

The encrypted two-Map fixture consumes on the promoted primary, returns and
consumes with LOOPSKILL, then verifies count/deletion and exhausted rejection
across relogin. It checks that the former primary receives no private item ACK.
A delayed ledger trigger verifies publication after commit. Native tests cover
stale epochs/process owners/hashes, unrelated graph drift, rollback, full uint64
identity and recovery of a confirmed transaction before the next checkpoint.

[Debug](evidence/native-graph-reagents-debug.json),
[ASan/UBSan](evidence/native-graph-reagents-asan.json) and
[installed Release](evidence/native-graph-reagents-release.json) each pass **1,046
checks**: 276 native Map, 29 pool/TLS, 196 outer TCP, 53 skill, 295 World handoff,
66 World secondary, 124 two-Map and seven rejection checks. The installed run uses
Release daemons with the verified Debug backend/pool integration executables.
CTest has 186 actual Debug passes (200 entries, eight internal and six explicit
fixture skips) and all 33 sanitizer suites pass. The previous installed image
fails the new delayed-transaction graph cast assertion.

That increment used `localhost/fourstory:postgresql-graph-reagents`,
ID `7a25026d3c20798049579845d0ea653b313b2f45aa772222da49f54a5758adad`,
UID/GID 10001:10001. All six daemons pass health/DNS/SIGTERM smoke; see
[container evidence](evidence/graph-reagents-container-verification.json),
[build fingerprints](evidence/graph-reagents-build-fingerprints.json) and
[owned-lab cleanup](evidence/graph-reagents-cleanup.json).

See the [source contract and boundaries](evidence/graph-reagents-contract.json).
Apply migrations through **023** before deployment; existing Map grants suffice.
Both backups and migrations 001–022 retain their hashes; 023 is now immutable.
The next schema migration is **024**. Equipped-item consumption, ammunition/cash,
full inventory movement, effects and full combat remain unfinished. Original-client
execution remains pending; wire tests use source-derived encrypted peers.

## Earlier verified increment: durable skill reagents

Fresh owned-PC ordinary and repeated casts now consume exactly one required bag
item when the pinned skill has a reagent and no weapon mask. Selection follows
unsigned bag/slot order, including default inventory255. A PostgreSQL transaction
locks the exact item and checks its full-row fingerprint, process/claim ownership,
generation, catalog release and contract-3 receipt. Item decrement/deletion, core
resources, learned timers and an append-only receipt commit together.

The server plans the cast privately, then publishes after confirmed COMMIT.
Original private UPDATEITEM or DELITEM and MOVEITEM packets precede cast success
and charged HPMP. Ordinary missing-item rejection retains source own/shared timers;
loop rejection arms none. A checkpoint lease prevents older periodic snapshots
from overwriting an immediate consumption. Unknown write outcomes close the client,
retain the claim and refuse stale final saves until process recovery.

[Debug](evidence/native-skill-reagents-debug.json),
[ASan/UBSan](evidence/native-skill-reagents-asan.json) and
[installed Release](evidence/native-skill-reagents-release.json) each pass **973 checks**:
228 Map, 29 pool/TLS, 196 outer TCP, 53 skill-cast, 295 World handoff, 66 World
secondary, 99 two-Map and seven rejection checks. Full CTest passes 186 Debug tests
(200 entries, eight internal and six explicit fixture skips) and all 33 sanitizer
suites. Actual encrypted tests use source skill1623/item31238 on a disposable
character and verify full descriptors, two consumed units, exhausted-stack rejection,
unchanged other item rows, logout/relogin and restart. Native tests cover full-row
drift, late-trigger rollback and committed process recovery. Runtime tests cover
checkpoint waiting, disconnect during commit and a modeled unknown outcome.

That increment used `localhost/fourstory:postgresql-skill-reagents`,
UID/GID 10001:10001. See [container evidence](evidence/skill-reagents-container-verification.json),
[build fingerprints](evidence/skill-reagents-build-fingerprints.json) and
[owned-lab cleanup](evidence/skill-reagents-cleanup.json). Migration **022** and updated
Map grants are required. Backups and migrations 001–021 retain hashes; 022 is now
also immutable, and the next migration is 023.

See the [source contract and boundaries](evidence/skill-reagents-contract.json).
The graph extension above supersedes the original fresh-only boundary. Equipped
reagents, ammunition/cash and trade/store branches, active effects, full combat
and original-client execution remain unfinished.

## Earlier verified increment: native ordinary cast requirements

Normal owned-PC casting now enforces the source map restriction, previous active
effect and non-consuming weapon eligibility. `wMapID=-1` remains the original
unsigned `0xffff` sentinel. The error order is map, MP, HP, previous active effect,
reuse, then weapon. The normal prerequisite uses `wPrevActiveID`; loop retains
`wTargetActiveID` and its own source ordering. Learned skills are not active buffs.

Original normal `SkillUse` arms own and same-kind cooldowns before `UseSkillItem`
rejects an unsuitable weapon. The implementation preserves that side effect
without deducting HP/MP or sending updated bars. Loop rejection still arms nothing.
Both paths preflight unsupported consumable/buff work before changing state;
reagents, ammunition, cash exceptions and authoritative active effects still need
their full transactional/lifecycle port. Full combat and original-client execution
remain unfinished. See [source contract](evidence/skill-gates-contract.json).

The new encrypted wire fixture uses source skill1329 (map550 only) and skill736
(requires maintained733). All three skills are learned only on a disposable
character; no source chart or active effect is fabricated. The previous installed
loop image wrongly accepts the off-map request and fails this regression. Existing
powered/broken weapon hydration tests now check both normal and loop fields.

[Debug](evidence/native-skill-gates-debug.json),
[ASan/UBSan](evidence/native-skill-gates-asan.json) and
[installed Release](evidence/native-skill-gates-release.json) each pass **933 checks**:
203 native Map, 29 pool/TLS, 196 outer TCP, 38 skill-cast, 295 World handoff,
66 World secondary, 99 two-Map and seven rejection checks. Full suites pass 186
actual Debug tests (200 entries, eight internal legacy and six explicit fixture
skips) and all 33 sanitizer suites. Plain/encrypted admission tests cover gate
precedence, matching map/sentinel, learned versus maintained prerequisites,
normal weapon-failure timers and unsupported-state drain.

Image used for that increment: `localhost/fourstory:postgresql-skill-gates`,
ID `745e22cc4acafd7dd304a5fbc60bd1292c8869d690e13ae06412a45b386ad85d`,
UID/GID 10001:10001. All six services pass health/DNS/SIGTERM smoke. See
[container evidence](evidence/skill-gates-container-verification.json),
[build fingerprints](evidence/skill-gates-build-fingerprints.json) and
[owned-lab cleanup](evidence/skill-gates-cleanup.json). Both backups and migrations
001–021 retain their hashes. No migration or client packet change was needed.

## Earlier verified increment: native repeated skill casting

The original 23-byte LOOPSKILL request and 45-byte ACK are integrated for native
owned PCs. Cooldown precedes affordability; HP equality is allowed by this source
branch. Loop delay has no rank increment and only arms the cast skill. Ordinary
and repeated casts share the same timer, so switching packet types cannot bypass
reuse. Learned rank, source HP/MP costs, finite positions and complete target
parsing remain authoritative.

Fresh and transfer hydration cache the original loop delay, active prerequisite
and equipment eligibility. Non-consuming weapon checks preserve source masks,
first matching equipment and durability. Missing effects return NEEDPREVACT;
consumable/ammunition branches and buff-backed prerequisites close before mutation.
The full effect, cancellation and inventory mutation lifecycles remain unported.
See [source contract and verification](evidence/skill-loop-contract.json).

[Debug](evidence/native-skill-loop-debug.json),
[ASan/UBSan](evidence/native-skill-loop-asan.json) and
[installed Release](evidence/native-skill-loop-release.json) each pass **929 checks**:
203 native Map, 29 pool/TLS, 196 outer TCP, 34 skill-cast, 295 World handoff,
66 World secondary, 99 two-Map and seven rejection checks. Full suites have 200
Debug entries (186 actual passes, eight internal and six explicit fixture skips)
and 33 sanitizer entries, all passing. The previous image cannot answer LOOPSKILL.
Tests also cover packet golden bytes, HP equality, missing maintained effects,
powered/broken weapon eligibility, unsupported-state refusal and durable timers.

That increment used `localhost/fourstory:postgresql-skill-loop`,
ID `d4a8bc5990d871e051853c37097842288f6886b6d5c4e1e8d209bd9cca7b873e`,
UID/GID 10001:10001. All six installed daemons pass health/DNS/SIGTERM smoke.
See [container evidence](evidence/skill-loop-container-verification.json),
[build fingerprints](evidence/skill-loop-build-fingerprints.json) and
[owned-lab cleanup](evidence/skill-loop-cleanup.json). Backups and migrations
001–021 retain their hashes; no migration or client protocol change is required.

## Earlier verified increment: admission timer refresh

CHARINFO now samples remaining skill durations immediately before encoding, after
the channel notification. CONREADY independently samples the same live tracker for
the initial PostgreSQL checkpoint. Sampling copies the payload and preserves timer
origins, learned IDs/ranks and the existing packet layout. Expired skills send and
persist zero; missing runtime state or an unknown active skill refuses admission.
Duplicate World metadata and CONREADY do not resend hydration or create another
initial checkpoint. See [source contract and verification](evidence/admission-timers-contract.json).

[Debug](evidence/native-admission-timers-debug.json),
[ASan/UBSan](evidence/native-admission-timers-asan.json) and
[installed Release](evidence/native-admission-timers-release.json) each pass
**910 checks**: 199 native Map, 29 pool/TLS, 195 outer TCP, 20 skill-cast,
295 World handoff, 66 World secondary, 99 two-Map and seven rejection checks.
Standard suites pass 186 of 200 Debug entries (eight internal and six explicit
fixture skips) and all 33 sanitizer entries. The prior installed image fails
the new delayed-CONREADY check; source-derived plain/encrypted admission tests
also verify CHARINFO after an injected three-second delay and timer expiry.

That increment's image: `localhost/fourstory:postgresql-admission-timers`,
ID `7007a8af428eef454ef1033e286d6b7a2a8ec32b1c6b2f70aaa56833a8f30c52`,
UID/GID 10001:10001. All six installed daemons pass health/DNS/SIGTERM checks.
See [container evidence](evidence/admission-timers-container-verification.json),
[build fingerprints](evidence/admission-timers-build-fingerprints.json) and
[owned-lab cleanup](evidence/admission-timers-cleanup.json). No migration was added;
both backups and migrations 001–021 are unchanged. Active buff/timer simulation,
cancellation, active loop effects and original-client execution remain pending.

## Earlier verified increment: native normal-cast cooldown generation

Normal native skill use now generates cooldowns from learned rank, original
physical/ranged/magic attack formulae, powered weapon slots and passive/item speed
rates. Same-kind learned timers extend together and cannot shorten a longer timer.
The character and cooldown locks cover affordability, all timer changes and HP/MP
deduction. Fresh and transferred characters rebuild timing from pinned catalogs;
existing receipts and snapshots persist the generated durations. No migration,
SQL grant or wire field change is required.

[Debug](evidence/native-skill-timing-debug.json),
[ASan/UBSan](evidence/native-skill-timing-asan.json) and
[installed Release](evidence/native-skill-timing-release.json) each pass **906 checks**:
198 native Map, 29 pool/TLS, 192 outer TCP, 20 skill-cast, 295 World handoff,
66 World secondary, 99 two-Map and seven rejection checks. Tests cover powered and
broken weapons, projected speed bonuses, transfer hydration, source skill 102 at
rank 2, immediate-repeat rejection, the 600ms skill expiration and durable timers.
The [previous image](evidence/skill-timing-before.json) fails the new repeated-use
assertion. Standard suites pass **200 Debug entries** (186 actual passes, eight
internal legacy skips, six explicit native skips) and **33 sanitizer entries**.
The deterministic timing suite has 56 checks, including same-kind concurrency.

That increment's local image: `localhost/fourstory:postgresql-skill-timing`,
ID `81b6c61dca05fae4150e2079de58ba7e6adc685aaaa79e087430974bee3f32ec`,
UID/GID 10001:10001. Six daemons pass health/DNS/SIGTERM checks. See the
[source contract](evidence/skill-timing-contract.json),
[container evidence](evidence/skill-timing-container-verification.json),
[build fingerprints](evidence/skill-timing-build-fingerprints.json) and
[owned-lab cleanup](evidence/skill-timing-cleanup.json).

Active buff/disarm/disguise timing and expiry are unported. Such transferred
characters retain their graph, but speed-dependent casts fail before mutation.
The pinned release has no passive speed rows; nonzero passive arithmetic uses
explicit deterministic fixtures. Active loop effects/cancellation, equipment/skill mutation with
cache refresh, full target validation and original-client execution remain pending.
Both backups and migrations 001–021 retain their hashes.

## Earlier verified increment: native learned-skill resource costs

Native Map casts resolve learned ownership and rank from the character and static
resource definitions from pinned backup catalogs. Source type-1 HP/MP costs now
apply the FLOAT-rounded rank exponent; type-2 costs preserve DWORD arithmetic.
Resource checks, existing cooldown rejection and deduction run under one character
state lock. Unknown skills return NOTFOUND; forged PC/channel/map identities and
malformed casts cannot consume resources. Original success packets carry the actual
learned rank. Fresh and transfer hydration both rebuild the static definitions.

The [contract](evidence/skill-costs-contract.json) records source arithmetic,
PostgreSQL/encrypted TCP evidence and precise limits. No new migration, catalog
mutation or packet field is introduced. New native cooldown generation still needs
rank, attack-speed and shared-kind semantics; learning, effects, full target checks
and original-client execution remain pending.

[Debug](evidence/native-skill-costs-debug.json),
[ASan/UBSan](evidence/native-skill-costs-asan.json) and
[installed Release](evidence/native-skill-costs-release.json) each pass **880 checks**:
181 native Map, 29 pool/TLS, 192 outer TCP, 11 skill-cast, 295 World handoff,
66 World secondary, 99 two-Map and seven rejection checks. Skill 134 at rank 2
consumes exactly 88 MP; its later unaffordable casts leave resources unchanged,
and a periodic checkpoint saves the charged value. The
[previous image](evidence/skill-costs-before.json) fails the new unlearned-skill
rejection. Standard suites pass **200 Debug entries** (186 actual passes, eight
internal legacy skips, six explicit native skips) and **33 sanitizer entries**.

That increment used installed image `localhost/fourstory:postgresql-skill-costs`,
ID `42ba521c5b853387551f80892d7977fcc3391813e42d2c45c779a555a2f0f6dc`,
UID/GID 10001:10001. Six services pass health/DNS/SIGTERM smoke. See
[container evidence](evidence/skill-costs-container-verification.json),
[build fingerprints](evidence/skill-costs-build-fingerprints.json) and
[owned-lab cleanup](evidence/skill-costs-cleanup.json).

## Earlier verified increment: fresh-primary skill cooldown persistence

Migration **021** adds contract-3 core/skill checkpoints for characters that have
never transferred between Maps. Periodic and final snapshots sample live remaining
durations; PostgreSQL updates only existing skill timers, preserving learned IDs
and ranks. Core, timers, receipts and logout audit commit together. Exact retries
include timer values, and durable skill drift blocks writes, transfer and recovery.
Fresh relogin keeps the ordinary database load path. Full transfer graphs keep
contract 2. Apply 021 and the updated Map grants before deploying the new binary.

[Debug](evidence/native-skill-checkpoints.json),
[ASan/UBSan](evidence/native-skill-checkpoints-asan.json) and
[installed Release](evidence/native-skill-checkpoints-release.json) each pass **181 native Map,
29 pool/TLS, 191 outer TCP, 295 World handoff, 66 World secondary, 99 two-Map and
seven rejection checks**. Tests include new use, expiration, full DWORD durations,
rollback after audit failure, changed-revision rejection, relogin and SIGKILL
recovery. The [previous installed image](evidence/skill-checkpoints-before.json)
fails the new online-countdown assertion. Standard suites pass **200 Debug entries**
(186 actual passes, eight internal skips, six explicit skips) and **32 sanitizer
entries**. The original client executable has not been run.

That increment used image `localhost/fourstory:postgresql-skill-checkpoints`,
ID `6535b80498bf7dca024a7dce9f140a1fb3b8c40c4dc6d62f1114ad0b83777c9e`,
UID/GID 10001:10001, passes all six daemon health/DNS/SIGTERM checks.
See [container verification](evidence/skill-checkpoints-container-verification.json),
[build fingerprints](evidence/skill-checkpoints-build-fingerprints.json) and
[lab cleanup](evidence/skill-checkpoints-cleanup.json).

The [contract](evidence/skill-checkpoints-contract.json) traces backup `TSaveSkill`,
`TLogout` and the commented skill copy in `TSaveCharDataEnd`; periodic skill
persistence is an explicit modern durability improvement. Both backups and
migrations 001–020 retain their hashes; verified 021 is now pinned. No client packet
changes or historical player imports. Learning/rank changes, full effect and
quest/recall/companion timers, ancillary synchronization, full AOI and gameplay
remain on the [acceptance backlog](next-steps.md).

## Earlier verified increment: native primary Map handoff and graph recovery

Movement across a cell boundary now drives the actual World/Map primary handoff.
The source freezes its live state; a ready replica consumes the exact original
transfer body in PostgreSQL and becomes the sole writer. Authority epochs fence
old writes even when the character returns to the same socket. Existing CONNECT
and CONREADY packets switch the client session without another CHARINFO.

Migration **020** adds the transfer journal and version-2 graph checkpoints.
Transferred core, raw inventory and remaining skill timers survive handoff,
logout/relogin and source/target process replacement. Prepared aborts save their
exact frozen graph. Teardown waits for ownership publication when a client closes
during COMMIT. Gameplay draining has a three-second deadline so a blocked client
sender cannot indefinitely block the World receive loop. Full typed extra sections
are preserved; their gameplay implementations remain separate work.

[Debug](evidence/native-primary-transfer-debug.json),
[ASan/UBSan](evidence/native-primary-transfer-asan.json) and
[installed Release](evidence/native-primary-transfer-release.json) each pass
**154 native Map, 29 pool/TLS, 174 outer TCP, 295 World handoff, 66 World secondary,
99 two-Map protocol and seven rejection checks**. The 197-entry Debug suite has
183 actual passes, eight internal legacy skips and six explicit native skips;
all 32 sanitizer entries pass. The [previous image](evidence/native-primary-transfer-before.json)
fails the new movement-driven handoff. Tests use synthetic accounts and an
explicit synthetic unit partition; historical rows remain untouched and the
original routing view is restored. The original client has not been executed.

Installed image: `localhost/fourstory:postgresql-native-primary`, ID
`209f8041b264fe5f59a8338f124cedee77c008c0163a5422320ac78264107bf7`, UID/GID 10001:10001. All six daemons pass
health, container DNS and SIGTERM checks. See the
[contract and remaining work](evidence/native-primary-transfer-contract.json),
[container verification](evidence/native-primary-transfer-container-verification.json),
[fingerprints](evidence/native-primary-transfer-build-fingerprints.json) and
[lab cleanup](evidence/native-primary-transfer-cleanup.json).
Both `.bak` and migrations 001–019 retain their hashes; verified 020 is now pinned.

Active effects, quests, summons, companions, native item/economy/social mutations,
full AOI and original-client acceptance remain unfinished. Fresh characters that
never transfer now use contract-3 skill checkpoints in the later increment above. Continue the
[acceptance backlog](next-steps.md); the overall mission remains active.

## Earlier verified increment: Map transfer state and native skill timers

The complete original RELEASEMAIN/ENTERSVR body now has an explicit C++ DTO and
bounded little-endian codec. An independent **861-byte, 319-field fixture** includes
all variable sections, high integer bits, raw text bytes and signed time sentinels.
The **43 codec checks** verify every encoded byte and reject every truncated prefix.
Native item loading now retains raw magic and full DWORD extensions alongside
client projections, so transfer code will not need to reconstruct lost values.

Fresh native admission also restores the remaining skill cooldowns into live
server timers. These gates apply without the optional full gameplay catalog and
survive a different process clock origin; successful teardown removes the timers.
The [previous installed daemon](evidence/main-transfer-cooldown-before.json)
reproduces a premature skill-use success. The current Debug, sanitized and installed
Release daemons reject that same encrypted request with original `SKILL_SPEEDYUSE`.
See the [state contract](evidence/main-transfer-state-contract.json).

**197 Debug entries** complete with **183 actual passes, eight internal legacy
skips, six explicit native skips and no failures**. All **31 ASan/UBSan entries**
pass. Separate [Debug](evidence/native-main-transfer.json),
[sanitizer](evidence/native-main-transfer-asan.json) and
[installed Release](evidence/native-main-transfer-release.json) runs each pass
**98 native Map, 29 pool/TLS and 156 outer TCP checks**, plus **295 World handoff,
66 World secondary, 36 native replica and seven second-Map rejection checks**.
These use source-derived and explicitly synthetic fixtures; the original client
has not been executed.

Installed image: `localhost/fourstory:postgresql-main-transfer`, ID
`92e23c3012003086f3e03b9cd143a6ee7523b2072fa580b93c32c0911f395e7f`, UID/GID 10001:10001.
All six daemons pass health/DNS/SIGTERM verification. See
[container evidence](evidence/main-transfer-container-verification.json),
[fingerprints](evidence/main-transfer-build-fingerprints.json) and
[lab cleanup](evidence/main-transfer-cleanup.json). Both `.bak` files and migrations
**001–019** retain their hashes; this increment adds no migration.

Full live graph capture/restoration, fenced PostgreSQL primary ownership transfer,
recovery and movement-triggered routing remain **IMPLEMENTING**. Embedded native
ENTERSVR is still refused; the codec alone does not authorize promotion. Core
checkpoints do not yet persist newly used cooldowns or other child mutations.
Buffs, quests, companions and other fields represented by the codec still require
native gameplay/persistence implementations. Continue with [next steps](next-steps.md).

## Earlier verified increment: World handoff and TCP session ownership

World now accepts CHECKMAIN, RELEASEMAIN and ENTERSVR responses only from the
expected registered Map connections in the expected phase. Fresh load completion
and transfer responses are consumed once. A five-second total deadline retires
unfinished transfers, including loss of the old source after it leaves the live
connection set. The exact opaque transfer body is forwarded unchanged. A regression
reproduced unsolicited primary reassignment against the previous Release daemon;
that exchange now preserves the valid primary.

Broader sanitizer verification exposed a separate plain TCP transport defect:
concurrent close accessed the same Asio socket unsafely. The session now serializes
internal socket and queue operations on one strand. Atomic liveness checks replace
raw socket queries in native Login/Map. Shutdown also cancels producers waiting on
a full queue, a second reproduced failure. The multithreaded regression checks
400 ordered packets and 24 close races with pending reads and backpressured writes.

The [handoff contract](evidence/world-handoff-contract.json) records source
references, before/after failures, exact scope and unresolved contracts.
**196 Debug entries** complete with **182 actual passes, eight legacy internal
skips, six explicit native skips and zero failures**. All **29 ASan/UBSan entries**
pass. The World C++ handoff fixture exercises **117 checks**, the transport suite
**33 checks**, and plain/encrypted admission suites each exercise **173 checks**.

[Debug](evidence/native-world-handoff.json),
[sanitizer](evidence/native-world-handoff-asan.json) and
[installed Release](evidence/native-world-handoff-release.json) each pass **96 native
Map, 29 pool/TLS and 153 Login/World/two-Map TCP checks**, plus **295 World handoff,
66 World secondary, 36 replica-wire and seven second-Map rejection checks**.
World handoff peers and the opaque transfer graph are synthetic. Native replica
checks separately use an explicit temporary same-channel partition, restore the
compatibility view and preserve historical rows. The original client has not run.

The separate [sanitized Login regression](evidence/world-handoff-login-regression.json)
passes 68 native authentication, 29 pool/TLS, 15 encrypted Login and 54 encrypted
security checks, plus SMTP timeout, owner loss/replacement and shutdown scenarios.
This covers the shared transport change beyond the Map admission flow.

Installed image: `localhost/fourstory:postgresql-world-handoff`, ID
`d29f4c8869b897fdaa3ddd947ff4aeea067316cd34a50d67f844882b2ed918c9`, UID/GID
10001:10001. All six installed daemons pass health/DNS/SIGTERM checks. See
[container verification](evidence/world-handoff-container-verification.json),
[source/build fingerprints](evidence/world-handoff-build-fingerprints.json) and
[owned lab cleanup](evidence/world-handoff-cleanup.json). Release verification uses
installed daemons and mounted Debug native integration executables.
Both `.bak` files and migrations **001–019** retain their fingerprints; no migration
was added by this increment.

That increment completed the World coordinator prerequisite. The subsequent Map
state codec is verified above; durable PostgreSQL ownership transfer/recovery,
dynamic routing and full distributed gameplay remain unfinished. The original CHECKMAIN acknowledgment
has no wire epoch; the phase/socket guards do not distinguish every possible delayed
reply across later rounds on the same socket. The future native transfer must also
fence database ownership. See [next steps](next-steps.md) for the full continuation.

## Earlier verified increment: native replicas and client hydration order

Migration **019** adds separate one-use replica grants bound to both Map process
owners, the primary connection, advertised target endpoint and pinned catalogs.
The actual Login/World/two-Map flow now admits a fresh secondary through the
original ADDCONNECT → CONNECT → CHARDATA → ENTERCHAR → CONREADY sequence.
Only the primary loads the supported character graph and writes core checkpoints
and logout state.
Replicas hold the source summary, cannot overwrite primary state, and retire
through their own exact connection identity. Target restart and primary closure
invalidate their grants without inventing historical records.

A source-derived regression also exposed incorrect native packet order: CHARINFO
had followed CONNECT/CONREADY, after the original client activates its map. The
primary now sends CHGCHANNEL and full CHARINFO before CONNECT, matching the
source, and never duplicates hydration after CONREADY. The regression failed
against the preceding binary and passes in the final builds.

The [replica contract](evidence/map-replica-contract.json) records source references,
roles, failure cases and limitations. **195 Debug entries** complete with **181
actual passes, eight legacy internal skips, six explicit native skips and zero
failures**. All **24 ASan/UBSan preset entries** pass. Plain and encrypted admission
suites each exercise **173 checks**.

[Debug](evidence/native-map-replica.json),
[sanitizer](evidence/native-map-replica-asan.json) and
[installed Release](evidence/native-map-replica-release.json) each pass **96 native
Map, 29 pool/TLS and 153 Login/World/two-Map TCP checks**, plus **36 replica-wire,
66 synthetic World-peer and seven ungranted-second-Map rejection checks**.
These are independent source-derived peers; the original client has not been run.
The replica tests explicitly substitute a synthetic same-channel cell partition
and restore the original compatibility view. That partition is not claimed to
exist in the authoritative backup. Both `.bak` files and migrations 001–018 retain
their fingerprints; 019 is now immutable too.

Installed image: `localhost/fourstory:postgresql-map-replica`, ID
`bebc88410bb885dd7eb6ee0a2b0faa49454681ed5cd3a56b54ce4d6fd238664f`, UID/GID
10001:10001. All six installed daemons pass health/DNS/SIGTERM checks. See
[container verification](evidence/map-replica-container-verification.json),
[source/build fingerprints](evidence/map-replica-build-fingerprints.json) and
[owned lab cleanup](evidence/map-replica-cleanup.json). Release verification uses
installed daemons with mounted Debug native integration executables.

Dynamic movement-triggered routing, primary transfer/recovery, full distributed
AOI/gameplay, guild/recall hydration and ancillary initial client packets remain
unfinished. Fresh replica admission is one implemented part of the complete
server objective. Older increment records below retain their historical scope,
test counts and images; use this section and [next steps](next-steps.md) for current
status.

## Earlier verified increment: safe Map retirement

A failed claim on a second native Map previously sent CLOSECHAR during teardown
and removed the first Map's valid character from World. The regression was
reproduced before the fix with two real Map processes over PostgreSQL. Map now
records whether a connection was announced to World and suppresses close-all for
unannounced or World-retired connections. World requires an accepted connection
on the exact registered Map peer before honoring close-all.

INVALIDCHAR, DELCHAR and CLOSECHAR now retire the exact local identity through the
actual World dispatch path, preserving the original client replies and rejecting
malformed bodies/stale keys. Teardown saves a ready native primary even when the
DELCHAR save byte is zero; this is an explicit modern persistence policy.
Retirement received during an awaited final save suppresses its close-all echo.
No migration was added; both backups and migrations 001–018 are unchanged.

The [retirement contract](evidence/map-retirement-contract.json) records the failing
regressions and passing checks. **195 Debug entries** complete with **181 actual
passes, eight legacy internal skips, six explicit native skips and zero failures**;
all **24 ASan/UBSan preset tests** pass. The plain and encrypted Map admission
tests each exercise 140 checks, and the World secondary suite now has 31 checks.

[Debug](evidence/native-map-retirement.json),
[sanitizer](evidence/native-map-retirement-asan.json) and
[installed Release](evidence/native-map-retirement-release.json) each pass 59 native
Map, 29 pool/TLS, 134 Login/World/two-Map TCP, 66 synthetic World-peer checks and
seven additional checks that second-Map rejection preserves the primary. These
use independent synthetic protocol peers; the original client has not been run.
Successful native replicas, transient synchronization and primary transfers remain
unfinished, so neighboring-map admission stays disabled.

Installed image: `localhost/fourstory:postgresql-map-retirement`, ID
`7e05599b59b6e91e722fd1476d1875ba40b430037c9b6295abd8d8de39aa2a6e`, UID/GID
10001:10001. All six installed daemons pass health/DNS/SIGTERM checks; see
[container verification](evidence/map-retirement-container-verification.json),
[source/build fingerprints](evidence/map-retirement-build-fingerprints.json) and
[owned lab cleanup](evidence/map-retirement-cleanup.json). Release verification
uses installed daemons and mounted Debug integration executables.

## Earlier verified increment: World writes and expected secondary admission

World→peer sends now own their buffers and serialize each complete write, with a
per-peer limit of 256 outstanding frames / 4 MiB including headers. Overflow or
transport failure retires the writer and releases waiters. The regression test
failed against the previous shared buffer with a corrupted checksum; the fixed
path passes 147 checks, including maximum frames, stalled readers, reset, closure,
backlog limits and EOF. Runtime access stays on its single I/O thread.

World now validates an existing character's secondary ADDCHAR against the
registered Map, planned endpoint, key/account and live primary. Only completion
of every expected connection triggers CHARDATA from the primary. Invalid or
repeated secondary messages preserve the valid primary and return INVALIDCHAR;
this is an explicit change from legacy CloseChar on rejection. It changes no
client field or opcode. Native Map replicas and primary transfers remain pending,
so cells needing neighboring connections are still refused. No migration added;
both backups and migrations 001–018 retain their fingerprints.

[Contract and source trace](evidence/world-writes-contract.json) records the scope,
24 C++ secondary-admission checks, and the original failing regressions. The full
Debug suite now has **195 entries: 181 actual passes, eight legacy internal skips,
six explicit native skips, zero failures**. All **24 ASan/UBSan preset tests** pass.
Fifteen existing secondary-connection fixtures now use ROUTE→ADDCONNECT→ADDCHAR→
CHARDATA setup instead of unplanned connections, preserving their downstream tests.

[Debug](evidence/native-world-writes.json),
[sanitizer](evidence/native-world-writes-asan.json) and
[installed Release](evidence/native-world-writes-release.json) verification each
exercise 59 native Map, 29 pool/TLS, 131 Login/World/Map TCP and 66 additional World
wire checks. The World wire checks use three independent synthetic Map peers and
verify empty rank/tournament registration replay as well as secondary admission.
They do not establish native Map replica persistence or original-client execution.

Installed image: `localhost/fourstory:postgresql-world-writes`, ID
`39e62e27a5c1176e564a0e1e364fea28cb87a9d43ef7b5763f4b42218759c14f`, UID/GID
10001:10001. See [container verification](evidence/world-writes-container-verification.json),
[source/build fingerprints](evidence/world-writes-build-fingerprints.json) and
[owned lab cleanup](evidence/world-writes-cleanup.json). Native C++ integration
executables are mounted Debug builds when testing installed Release daemons.
The older increment records below retain their historical test/image counts.

## Implemented increment: native World-link loss and admission

The native Map listener now requires a complete registration acknowledgment from
its current World connection. TCP connection alone is insufficient. Missing ACKs
expire after five seconds; malformed counts, strings, trailing bytes and unexpected
pre-registration opcodes close the link. Anonymous transport cannot admit players.

After a detected World disconnect, Map closes all client sockets and awaits their
existing PostgreSQL save/release operations and in-flight background writes before
reconnecting. A committed ready transition is still saved even if its socket closes
while that transaction runs. Failed saves retain the snapshot, generation and
account reservation; they are never represented as successful logout. Successful
World replacement reopens admission only after its own acknowledgment. Map process
ownership remains unchanged. No new SQL migration or client packet is needed.

Map→World composed writes now hold a permit for the entire frame and retain their
original socket, preventing overlapping frame writes and replay on a replacement.
This runtime remains confined to its single I/O thread. The original Map requested
service shutdown on World loss; closing/saving clients and reconnecting this Map
process is an explicit modern recovery policy. See
[World-link contract](evidence/world-link-contract.json) for sources, verification
and remaining work. Original-client execution and full gameplay remain unfinished.

[Debug](evidence/native-world-link.json),
[sanitizer](evidence/native-world-link-asan.json) and
[installed Release](evidence/native-world-link-release.json) runs each pass 59 native Map,
131 encrypted three-daemon TCP and 29 pool/TLS checks. They exercise World SIGKILL
with a delayed final save, replacement registration, loss during the ready commit,
failed final-save rollback and retained-account rejection, plus the earlier Map
checkpoint/crash scenarios. The full Debug suite has 193 entries (179 actual
passes, eight legacy internal skips, six explicit native DB skips, zero failures).
All 22 sanitizer preset entries pass, including registration fault and concurrent
Map-frame transport tests.

The installed image `localhost/fourstory:postgresql-world-link`, ID
`529effcfb3c59321a8b3ccd1d7151b00161af6a9da374a0dbb0860dd0eee8f86`, runs as
UID/GID 10001:10001. Its actual Login/World/Map binaries pass the native fault flow;
all six installed daemons pass health/DNS/SIGTERM checks. Native C++ integration
tests are mounted Debug builds in the Release verification. See
[container evidence](evidence/world-link-container-verification.json),
[build fingerprints](evidence/world-link-build-fingerprints.json) and
[lab cleanup](evidence/world-link-cleanup.json).

## Implemented increment: native Map checkpoints and crash recovery

Migration **018** adds one latest core receipt per character, independently of
historical data. The actual native Map now saves an initial checkpoint atomically
with readiness, then performs bounded sequential checkpoint sweeps on DB workers.
The default interval is the original `CHAR_SAVE_TICK`: **30 minutes**, configurable
as `[native_map].checkpoint_interval_ms` from 100 to 1,800,000 milliseconds. Final
logout always saves. Checkpoints do not stamp an online account as logged out.

Each transaction commits the exact core, process/connection identity, monotonically
increasing revision and deterministic fingerprint together. Repeating an identical
checkpoint confirms it; conflicting or stale revisions fail. The exact final-save
receipt can confirm a lost acknowledgment without replaying a write. Failures close
the client for a final save, and shutdown drains checkpoint and final-save work.

Replacement ownership recovers only a `ready` claim with recovery contract 1, exact
identity and a receipt matching current durable core. It retains a `recovered`
receipt and releases that old login atomically. **Transient changes after the last
committed checkpoint are lost on a crash.** This is periodic core recovery, not an
inventory/economy event journal. Old claims without receipts, orphaned claims and
externally drifted core remain blocked. Final audit/recovery failure rolls back
rather than clearing the account. New recovery/logout timestamps record observed
closure in UTC; no exact crash time or historical timezone is inferred.

World now retires affected in-memory characters on unexpected Map TCP loss without
running the legacy bulk current-user delete. Map peer IDs also use the source
server-type byte `SVRGRP_MAPSVR=4`; the rewrite had incorrectly used the world ID,
which prevented the cleanup for world 1. Client packet layouts are unchanged.

[Checkpoint contract](evidence/map-checkpoint-contract.json) records the source,
modern recovery policy and limits. [Debug](evidence/native-map-checkpoint.json),
[sanitizer](evidence/native-map-checkpoint-asan.json) and
[installed Release](evidence/native-map-checkpoint-release.json) runs exercise actual PostgreSQL
and Login/World/Map processes, including SIGKILL during a checkpoint, same-World
relogin and SIGTERM while checkpoint/final-save transactions overlap. The supported
fresh-character scope from the preceding increment remains unchanged. Full gameplay,
transfers and original-client execution remain unfinished.

The installed image `localhost/fourstory:postgresql-checkpoint`, ID
`e061e3a704196ea884b28c20a557b9c5f40c9b777eab0f8df37cd313eea3d58b`, runs
as UID/GID **10001:10001**. Each of the three verification runs passes 59 native
Map checks, 79 encrypted three-daemon TCP checks and 29 pool/TLS checks. The Release
run uses installed Login/World/Map daemons and mounted Debug integration tests.
All six installed daemons pass health, Map–World DNS and SIGTERM checks; see
[container evidence](evidence/map-checkpoint-container-verification.json).
The full Debug suite has 192 entries: 178 actual passes, eight legacy internal
skips and six explicit native DB skips, with zero failures. All 20 sanitizer
preset entries and all 31 Python tests pass. See
[build fingerprints](evidence/map-checkpoint-build-fingerprints.json) and
[owned-lab cleanup](evidence/map-checkpoint-cleanup.json).

## Implemented increment: native Map claim, hydration and core persistence

Migrations **016–017** add durable process/connection ownership and an independent
actor reference release: 27 original item-magic and 719 skill-point rows. The
actual Map binary selects native PostgreSQL services for `[database].backend`,
requires all three explicitly published character/routing/actor hashes, and uses
its own least-privilege role and advisory control connection. Existing applied
migrations 001–015 and both `.bak` files remain unchanged.

The verified fresh normal-world flow now runs **Login → World → Map → CONREADY →
movement → disconnect → reconnect** through actual processes and source-derived
encrypted client packets. Map consumes exactly one unexpired Login reservation,
fences operations by process token and connection generation, and protects its
claim from delayed Login cleanup. It reads bags/items, skills/hotkeys and the
persisted ancillary graph consistently. CHARINFO sends actual lists, derived
maximum HP/MP, source experience thresholds, selected title and lucky number.
Zero current HP stays dead; it is never silently healed. Item `dwTime1..6` are
extension values, including custom texture in `dwTime6`, not timestamps.

World's missing CHARINFO/ROUTE/CHARDATA/ENTERCHAR completion is now connected to
the native Map path. CHECKMAIN uses original cell ownership. The World BOOL
trailer is four bytes, matching the Windows source. Core save and logout form
one transaction; failures retain the reservation, and core saves never replace
unloaded children. Disconnect also removes the character from World's registry.

This scope is **fresh normal characters without persisted guild membership or
active recalls, in cells requiring no secondary Map connection**. Unsupported
admission is refused without deleting durable data. Transfers, gameplay inventory
writes, active buffs, social persistence, companion admission and full combat
remain unfinished. The later checkpoint increment above now recovers eligible ready sessions.
Claims without that recovery contract remain orphaned. The test World uses its actual
in-memory registry, with no social database adapter. The original client executable
has not been run. See [contract](evidence/map-runtime-contract.json),
[native/TCP evidence](evidence/native-map-runtime.json),
[sanitizer evidence](evidence/native-map-runtime-asan.json) and continuation plan.

The full Debug suite has 192 entries: 178 actual passes, eight legacy internal
skips and six explicit native DB skips. Separate real-PG runs in Debug and
ASan/UBSan pass 36 Map checks, 57 three-daemon encrypted TCP checks and 29 pool/TLS
checks each. All 31 Python reconstruction/publication tests and all 19 entries
of the expanded sanitizer preset pass. The character regression also passes. Tests cover
rollback, simultaneous claims, stale process/generation rejection, process restart,
owner-connection loss and SIGTERM during the ready commit. The tested binaries are
mounted Debug/sanitizer builds. The later checkpoint Release image above includes
this native Map implementation and extends its verification.

## Implemented increment: native Login routing and pending Map handoff

Migrations **014–015** publish four original routing tables (1,440 rows) and
recreate the backed-up `TSVRCHART` view (358 joined rows). The three original
unmapped channel cells remain unchanged. This release is independent of the
15-table Map and 22-table character catalogs; both backups and migrations 001–013
retain their recorded fingerprints. No historical accounts or player rows are
imported. Configure the explicitly published hash under `[routing].manifest_sha256`
and map each operational world to its source chart group in `app_world.routing_worlds`.

The actual `CS_START_REQ` handler now calls the authenticated PostgreSQL path.
One fenced transaction checks the exact session, agreement, lock and character
ownership, resolves logical channel plus map cell, rotates the configured endpoint,
and binds the selected character. Missing ownership or routes fail without using
the first registered server. The original starter map 2010 has no cell route; its
spawn 15003 resolves to map 0 and the transaction applies the backed-up X/Z and
original Y=0 rule. Failure rolls back the complete selection and position change.

A durable **60-second pending reservation** protects the Login-to-Map boundary.
Repeated identical requests reuse its endpoint without extending expiry; conflicting
selection and subsequent lobby writes are refused. Login flushes the original
8-byte START ACK before closing. Disconnect preserves committed reservations,
including a close racing the transaction. Startup/login can recover expired,
unclaimed reservations; existing non-pending Map sessions are retained. TLOG keeps
its original login-time logout sentinel until actual expiry/termination.

[Contract and limits](evidence/map-handoff-contract.json),
[real PostgreSQL and TCP verification](evidence/native-map-handoff.json), and
[sanitizer verification](evidence/native-map-handoff-asan.json) cover this increment.
The full Debug suite has 191 entries: 178 passes, eight legacy internal skips
and five explicit native DB skips. Dedicated native runs pass 34 handoff checks,
23 encrypted TCP checks and 29 pool/TLS checks; all 28 Python tests and 11 sanitizer
CTest entries pass. Auth and character regression evidence is linked in the contract.

The subsequent Map increment above extends this verified Login selection into
native fresh-character admission and core save/reconnect. Transfers remain pending. BR/BOW routing,
active-character maintenance, scamming-post/pet legacy conversions and the
TEnterServer timed-title side effect remain explicit unfinished contracts.

## Implemented increment: Map admission and save ordering

Source CONNECT result codes, 29-byte parsing and DWORD/64-bit checksum behavior
now drive the existing Map handler. Sessions advance through pending, loading,
loaded, admitted and ready phases; the World verdict is the single client ACK.
World now initiates the missing fresh-login ENTERSVR request and unwinds failed
loads. Keyed messages, duplicate reservation and pre-gameplay guards prevent stale
responses or a second socket from overwriting the current session. Raw session
keys are removed from the touched logs and redacted at both audit sinks.

Map dispatch and World inbound handling await completion. Disconnect saves keep
the reservation until completion, and shutdown waits for client cleanup and World
operations. Failed writes retain runtime snapshots, block replacement and produce
nonzero daemon shutdown status. This is **not durable recovery**. Legacy SOCI save
now propagates failures and groups its existing character/directory updates in a
transaction; that repository is still not suitable for `app_world`.

See [the admission contract](evidence/map-admission-contract.json) for scope,
source evidence, tests and remaining work. The new TCP tests run 85 checks per transport using controlled World
and persistence interfaces with both plain and RC4/XOR framing. The full Debug
suite has 190 entries: 178 passed, eight legacy internal skips and four explicit
native database skips. Six current Debug daemons mounted in the non-root runtime
image passed health, Map–World DNS and SIGTERM checks. The subsequent increments
above implement native Login cell routing, pending handoff and fresh Map
ownership/load/core save. Full gameplay persistence and transfer recovery remain pending.
No new release image or original-client compatibility certificate is claimed here.
The previous verified native lobby image and its evidence remain below.

## Implemented increment: native character lobby and durable starter state

Migrations **011–013** add an independently selected 22-table character reference
release, empty mutable `app_world` state, global name ownership and the confirmed
world-zero item allocation floor **735812**. Both `.bak` files and migrations
001–010 remain unchanged. Original snapshot values and text bytes remain in the
historical schemas; no player/account/reservation corpus is imported. The original
15-table Map release is independently pinned and remains unchanged.

`PostgreSQLCharService` is wired into the existing Login executable using
`[characters].manifest_sha256`. It uses the same native PostgreSQL pool as accounts;
a second world database is refused for this mode. The whole create graph is one
transaction: directory, character, two inventories, title, cabinet, class skills,
chart-driven items, hotkeys, original welcome post, optional recall and account
mount. HP/MP, veteran levels, item attributes and mail payload/money come from the
backed-up procedures and charts. Missing reference relationships abort publication.
Every mutation checks the process owner and exact authenticated session key. Account
row versions serialize operations across worlds; item counters reserve complete
ranges atomically. Only confirmed serialization/deadlock aborts are retried.

List uses durable equipment (including the required 12-byte wire record). Delete
rechecks the wire credential under the account lock, blocks guild members, hard
deletes level <=5 with child cleanup, and soft deletes higher levels while retaining
their global name. `CS_CREATECHAR_ACK` now carries the backed-up live character count,
not remaining slots. Delete results match original values 0/1/2/3/4. Native create
supports the source US ASCII player profile, normal worlds and the three recovered
veteran options (19/29/93).

[Implementation contract](evidence/character-implementation-contract.json) separates
source facts, modern repairs and unresolved behavior. Explicit repairs include a
recall identity absent from the original table despite its procedure requiring one,
zero custom texture for new items, atomic starter creation and orphan-title cleanup.
New timestamp values are UTC; source zero-date sentinels stay `1900-01-01`. Historical
reservation ownership, complete SQL Server LIKE/collation equivalence and non-ASCII
names remain migration gates. Full guild/BR/BOW/item-transfer systems are pending.

[Debug verification](evidence/characters.json) and
[ASan/UBSan verification](evidence/characters-asan.json) cover actual native PG,
least-privilege deployment grants, fault rollback, concurrent name/slot creation,
starter recall/items/mail, process fencing and encrypted TCP create/list/delete.
The dedicated run passes 32 native character checks plus 29 pool/TLS checks and
31 encrypted TCP checks. The actual Login process restarts between creation and
re-reading the same character.
This earlier evidence proves lobby persistence. The later Map increment above
extends it to fresh world entry, core persistence and reconnect. Original-client
execution and complete feature parity remain unverified.

The installed Release image `localhost/fourstory:postgresql-characters`
(`684c7dbddae8411dfe1e5469b9ff86eead661cf21ee8ad00b6bb281b98336b9d`)
also passes the actual Login encrypted TCP lifecycle as UID 10001 and all six-daemon
health/DNS/SIGTERM checks. See [Release evidence](evidence/characters-release.json)
and [container checks](evidence/character-container-verification.json). The C++
integration executables in that run are mounted Debug tests; the daemon under test
is the installed Release executable. The installed Map binary also passes its
original 15-table catalog/startup check (`character-map-catalog-release.json`).
Owned databases/network and temporary credentials/private keys were removed;
private forensic metadata and snapshots remain for continuation (`character-cleanup.json`).

## Implemented increment: connection-bound source-client email verification

Migration `010-login-security-challenges.sql` adds empty operational challenge and
issuance-budget tables. Historical `TSECURECODE`, backup files and applied migrations
001–009 are preserved. This is an explicit modern opt-in policy: the original
server's issuance branch is commented out and the backup contains no verified
hardware-trust contract. It is not a claim that the recovered deployment used 2FA.

For the non-direct source `0x2918` profile, LOGIN issues a private connection-bound
challenge and emails a six-character code. Confirmation sends only the original
one-byte `SECURITYRESULT`; it creates no session. The source client then sends
LOGIN again with its form password. A confirmed private grant alone enables the
SHA1 normalization to the original initial-login wire credential before bcrypt
verification. The token never travels in game packets. Direct-login/JP/TW and
other versions fail closed for this feature; executable and non-ASCII acceptance
remain pending. No client source or packet layout was changed.

The existing auth transaction rechecks passwords, bans, agreement, duplicates and
full ACK fields on retry, then consumes the grant atomically with session/audit
writes. Denied or rolled-back handler attempts revoke it. Confirmation never adds
IP/device trust. Pending connections cannot access lobby/account handlers, replace
one another, or evict an authenticated account before confirming their own code.
Disconnect and process replacement revoke challenges. Budgets survive both:
five code attempts, five-minute challenge lifetime, three issuances/account/five
minutes, and thirty seconds for a confirmed retry.

SMTP now runs on the worker pool, retains coalesced multiline replies and uses a
fifteen-second transaction deadline. A silent relay leaves reactor health
responsive and causes challenge cleanup. SMTP diagnostics omit mail recipients,
codes and bodies. A trusted local/private relay is still required for the plain
SMTP hop; provider TLS is outside this adapter. Tests use an isolated mail sink
with no forwarding and remove captured messages afterward.

[Debug evidence](evidence/login-security.json) and
[instrumented evidence](evidence/login-security-asan.json) record native PostgreSQL,
actual Login TCP and private SMTP checks. Native and source-derived peer checks cover sequence,
wrong/expired codes, concurrent pending connections, substitution/replay, rechecked
bans/agreement, issuance budgets, duplicate ACK flush and mail failure cleanup.
A repeated full-suite run exposed a read-coroutine close guard holding a raw
session pointer during reactor destruction. The guard now retains shared ownership
until cleanup completes. The Asio echo suite is included in the sanitizer preset
and CI, with thirty repeated instrumented runs recorded for this regression.
The earlier [source review](evidence/security-source-review.json) is preserved as
the pre-fix analysis. Full character/world persistence and original-client
acceptance remain separate work.

## Implemented increment: Login process ownership and orderly shutdown

Migration `009-login-runtime-owner.sql` adds operational ownership metadata only.
A dedicated PostgreSQL session holds the single Login advisory lock. Startup
claims a random process token before stale-session cleanup or listener startup;
a second live Login is refused without touching the first process's sessions.
Native auth/session/routing transactions verify that token while holding a shared
row lock. Replacement ownership waits for preceding transactions, then prevents
old processes from writing through surviving ordinary pool connections. Loss of
the owner connection stops the daemon with exit status 1. This supports one active
Login and failover per application DB; it is not multi-active Login clustering.

Login dispatch awaits each packet handler before reading the next packet on the
same socket. Pipelined group requests therefore observe completed authentication.
Graceful shutdown stops accepts, closes sockets, awaits accepted DB work and
key-specific cleanup, then joins workers. Map-handoff cleanup retains its reason
and selected character. Missing DB configuration now refuses startup unless
`[development] allow_no_database=true` explicitly enables the smoke environment.
The `TCheckIP` pattern restriction runs after valid credentials, preserving the
backed-up procedure's error precedence.

Sanitizer verification found additional defects: signed overflow in the
shared body/header checksum arithmetic and Login version checksum, a health
endpoint destroyed after its `io_context`, and leaked OpenSSL provider references. Unsigned modular arithmetic preserves
wire bits, checked against independent fixed golden vectors and the Python TCP
peer; endpoint lifetime now stays inside the reactor's lifetime. Loaded OpenSSL
providers have explicit lifetime management and release their owned references. Original client
source and packet layouts remain unchanged.

[Lifecycle evidence](evidence/login-lifecycle.json) exercises process exclusion,
transaction fencing across owner loss, route-counter writes, pipelined protocol
requests, responsive health during DB wait and shutdown during authentication.
That older routing fixture used explicitly synthetic endpoints. Native unkeyed
lookup is now refused; the later START contract above supersedes it. Full native
Map acceptance is covered by the later increment above; full persistent gameplay remains unported. See the verification section below for
instrumented and installed-image results.

## Implemented increment: native Login transactions

Migration `008-native-login.sql` adds an **empty** `app_global` application schema.
The historical schemas, source credentials, backups and migrations `001`–`007`
remain unchanged. It contains existing Login contracts and explicitly labelled
rewrite extensions; it does not assert those extensions existed in the backups.
The existing `SociAuthService` and actual daemon startup use native PostgreSQL.

Account-row locking and a unique current-user constraint serialize competing
logins. Session creation, last-login time and audit commit together. Logout matches
both account and session key, so delayed cleanup cannot delete a newer login.
Agreement persistence failures cannot open the in-memory gate. Disconnecting during
the DB operation cleans the committed session after the worker returns. Historical
agreement gifts and complete Map ownership/kick behavior remain outstanding.
Migration 009 enforces one active Login owner per application DB as described above.

The original `NetCode.h` and backed-up `TLogin` exposed incorrect rewrite result
values. Login now sends internal error **5**, IP restriction **6**, account/IP ban
**7**, agreement required **8**, and security required **10**. Duplicate login
sends **3**, flushes the reply, then closes both local connections. Wire IDs, field
order and the 41-byte ACK body are unchanged. SMTP fallback no longer logs codes
or claims delivery. The source-client retry conflict found in the initial
[source review](evidence/security-source-review.json) is addressed by migration 010
and the connection-bound verification increment above.

[Native Login evidence](evidence/native-login.json) covers actual PostgreSQL,
limited runtime grants, real daemon startup, synthetic encrypted TCP login,
fragmentation, denial codes, duplicate/reconnect, failure rollback, abandoned
login cleanup and SIGTERM. It uses source-derived RC4/MD5+XOR frames, not an original
client executable. [Source evidence](evidence/auth-source-contract.json) records
backed-up procedure fingerprints. Bcrypt hashes the synthetic wire credential;
historical credential import and non-ASCII client encoding remain gated. The ICU
account comparison policy is tested for ASCII case/trailing spaces, not exhaustive
SQL Server collation equivalence.

Use `--login-only` with `tools/database/run_native_verification.py` in an owned
PostgreSQL-only lab. Configuration: [Login example](../../../deploy/tloginsvr-postgresql.example.toml).
Provision a separate runtime role with [these grants](../../../deploy/sql/login-runtime-grants.sql)
and supply a verified TLS connection through `FOURSTORY_LOGIN_CONNECTION`.
The example disables unported persistent peer trust; it does not enable a complete
production cluster. Native character operations require the explicit character release and provisioned worlds described above. Full login→character→world→save→reconnect acceptance is still pending.

## Implemented increment: native catalog startup

The shared `SessionPool` now opens native SOCI/libpq connections. It bounds
connection establishment, defaults to TLS `verify-full`, supports exclusive RAII
leases, and suppresses connection options in failure diagnostics. T-SQL `SpCall`
is rejected on PostgreSQL before execution; enabling a driver does not translate
existing SQL Server repositories. No SQL Server service is needed for this slice.

`TMapSvrAsio/main.cpp` accepts a separate `[content]` connection and manifest
fingerprint. `LoadPostgreSQLCatalog` uses one repeatable-read, read-only transaction
for release metadata, all fifteen source table counts and the existing ten C++
cache services. Query/lock timeouts are 30/5 seconds. The caches are wired into
the actual handler context and static spawn manager before listeners start.
The content connection closes after initialization. No hot reload is implemented.

Migration `007` adds three read-only publication metadata views. Migrations
`001`–`006`, historical rows and original backups were not edited. Publication
requires the verified selected manifest; startup refuses a different fingerprint.
Activation recomputes source-value and original-byte hashes. Startup checks
publication provenance/counts, not an independent full hash of every source value.
Only administrators can change those underlying objects in the tested role model.

Actual PostgreSQL 18.6 verification imported **106,692 source rows in 15 tables**,
loaded **3,495 mapped monster attributes**, retained **39 missing mappings** and
reported **62 rewards referencing missing quests**. The current spawn algorithm
created **20,944 monsters**; 27 selected candidates lacked attributes, two lacked
templates and five spawn points had no link rows. These are diagnostic counts,
not a claim that spawn selection already matches all legacy rules.

The previous fabricated `100 * level` monster HP fallback was removed. Missing
attributes refuse that candidate, following the legacy refusal principle. Zero
HP is quarantined without inventing values (zero occurrences in this real run).
See [protocol and behavior review](protocol-compatibility.md) for the remaining
selection/ID/essential-spawn differences.

## Verification and limits

- [Native PostgreSQL evidence](evidence/native-postgresql.json): actual C++ pool,
  bound Unicode values, commit/rollback, constraint failure, concurrent updates,
  least privilege, trusted/untrusted TLS, actual catalog/domain conversions,
  manifest rejection, map health and graceful SIGTERM. The fixture has explicitly
  synthetic transaction records; catalog records come from the pinned backups.
- [Sanitizers](evidence/sanitizers.json): six crypto/codec/Login socket suites
  and [actual native Login email/ownership lifecycle](evidence/login-security-asan.json) passed
  ASan/UBSan with leak detection and failure-on-error. SOCI RTTI prevents `vptr`
  instrumentation; no full-cluster sanitizer or ThreadSanitizer claim.
- [C++ suite](evidence/cpp-tests.json): 187 registered tests, 176 passed, eight legacy
  internal DB skips and three explicit native DB skips. The native tests were then
  run with the real PG fixture above. No test failed.
- [Release container](evidence/container-verification.json): all six daemon health
  checks, Map→World DNS connection and SIGTERM passed; the installed Map binary
  as UID 10001 also passed the real PG startup and wrong-manifest refusal.
- [Earlier security Release image](evidence/container-security-verification.json):
  `localhost/fourstory:postgresql-security`, UID 10001, all six health/SIGTERM checks.
  [Installed Login](evidence/login-security-release.json) passed the native
  PostgreSQL, encrypted TCP and private SMTP verification, including ownership
  failover and in-flight shutdown. [Installed Map](evidence/security-catalog-release.json)
  loaded all 106,692 pinned source rows across fifteen tables, verified the manifest,
  refused a wrong manifest and passed health/SIGTERM.
- [Earlier lifecycle Release image](evidence/container-lifecycle-verification.json):
  `localhost/fourstory:postgresql-lifecycle`, UID 10001, all six health/SIGTERM checks.
  [Installed Login](evidence/login-lifecycle-release.json) passed PostgreSQL ownership,
  encrypted packet sequencing, owner-loss recovery and in-flight shutdown.
  [Installed Map](evidence/lifecycle-catalog-release.json) also loaded the pinned
  original catalog and passed health/SIGTERM and wrong-manifest refusal. Earlier
  image reports below remain separate historical verification increments.
- [Native Login Release image](evidence/container-login-verification.json):
  `localhost/fourstory:postgresql-login`, UID 10001, all six health/SIGTERM checks.
  [Installed Login verification](evidence/native-login-release.json) passed native
  transactions and encrypted TCP against PG using the checked-in deployment grants.
  The preceding Map catalog image/report remains separate above.
- [Database tests](evidence/database-tests.log): 22 reconstruction, preservation,
  migration, activation/snapshot consistency and permission tests, executed on a fresh disposable DB.
- [Synthetic pool evidence](evidence/native-pool-synthetic.json): the same native
  pool test without backups; suitable for CI. CI wiring is supplied, remote CI
  execution is not claimed.
- [Source index](evidence/source-inventory.json): 206 legacy translation units,
  231 production Asio translation units, all 1,542 source opcode values matching
  the modern enum, and nine preserved collision values. Lexical references are
  navigation aids, **not functional coverage**. Tools are included in the legacy
  index; they are not all runtime daemons.
- [Cleanup](evidence/security-cleanup.json): owned containers/network and temporary
  credentials/keys removed; source backups and private forensic extract retained.
- See [capability matrix](capability-matrix.md) and [continuation plan](next-steps.md)
  for pending implementation and acceptance criteria.

The original client was not run. Its exact binary build remains unknown; source
`TVERSION=0x2918` alone does not identify a supported executable. No client or
packet layout was changed in this increment. Native PostgreSQL authentication has the verified synthetic subset above; complete
Map character persistence, item transfers and reconnect gameplay remain pending.
The map health check proves startup, not client login readiness.

## Reproduce

Build the Debug targets using [Linux instructions](../../../deploy/README.md).
Install `psycopg[binary]==3.3.6` in a private Python virtual environment. Keep the
reference extract outside Git and use the original verified manifest, not a newly
fabricated catalog. The helper verifies ownership before removing containers.

```sh
python3 tools/database/disposable_environment.py start --work /tmp/fourstory-native-lab --postgresql-only
python3 tools/database/run_native_verification.py --work /tmp/fourstory-native-lab --snapshot /private/reference-snapshot/manifest.json
python3 tools/database/disposable_environment.py stop --work /tmp/fourstory-native-lab
```

Run through the Python environment containing psycopg. Use `--pool-only` instead
of `--snapshot` for synthetic driver tests. The default executable image is
`localhost/fourstory:build-deps`, with binaries under `build/linux-debug/bin`.
Certificates and credentials are temporary private files; only scrubbed reports
belong in Git. Stop the environment after either success or failure.

For deployment use [the example](../../../deploy/tmapsvr-postgresql.example.toml)
and the role/configuration instructions in [database/README.md](../../../database/README.md).
This increment does not grant game-server accounts access to historical schemas.
