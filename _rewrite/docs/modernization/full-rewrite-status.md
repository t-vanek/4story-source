# Full rewrite — Phase A, incomplete

Owner instruction, 2026-10-11: finish the complete implementation before Phase B
verification/migration and Phase C container startup. **Phase A is still active;
the IMPLEMENTATION_COMPLETE_UNVERIFIED gate has not been reached.**

Starting revision: `2fa2396`, local `main`. All changes in this increment are
**INTEGRATED_UNVERIFIED**, **IMPLEMENTED_UNVERIFIED** or **IMPLEMENTING** as scoped below.
The preceding one-time GitHub publication ended at `2e406dc`. The owner has now
explicitly renewed local Git only: no push or deployment. The incomplete Phase A
gate and prohibition on builds/tests/runtime/SQL remain in force.
Pre-existing local `_rewrite/docs/audits/` and private extracts remain outside Git.
Original server/client sources, backups, applied migrations001–043 and historical
evidence remain unchanged. New migrations044–072 are **written, not applied**; next073.
Latest local increment is based on `42451e1`: basic-party own positive cures,
source-unused finish party metadata including SOLO client raw-ID behavior,
current local party context for output/MANSTAT and ordinary ACTION current
actor/recipient grid guards. No new migration; next073. World membership stays
independent of supplied finish metadata. Other-target/group/corps/guild/mode
healing, general combat and original-client acceptance remain OPEN. No build,
test, SQL, import, service, client, container execution or push.
Previous code `42451e1`, based on `a3fa3ef`: original action-item initiation,
ACT consumption and source positive own-PC cure completion; strict local item
reuse, truthful late failures, distinct accepted origins and actual route proof.
Written071 adds origin columns; written072 and an offline publisher bind complete
guild-skill membership to current character/routing releases. Both are unapplied;
ordinary FINISHSKILL also requires this publication. Next073. Full action-item
combat, party/learned-trigger/richer effects, DWORD wrap and client acceptance
remain OPEN. No build/test/SQL/import/service/client/container run or push.
Previous code `a3fa3ef`, based on `d5377dd`: actual positive maintained-status
collision semantics, STATUS_NONE800 append during ordinary revival, independent
duplicate clocks, per-row ACTION/cast erasure events and native cancellation
publication fences. No migration; next071. Exact native checkpoint/transfer state
is retained, with source save-only first-eligible duplicate collapse explicitly
OPEN alongside general effects, full combat/SoulHP/bootstrap and client acceptance.
Previous code `d5377dd`, based on `d86d407`: original own-PC environmental
damage and bounded ordinary death in the native PostgreSQL mutation path;
silent nonstatic removal, old-MP packet order, dead ENTER action, prototype AI
isolation and current actor/recipient projection guards. New070, offline source
publisher and separate Map view grants were written only. At that revision,
full combat/SoulHP, special death and retained800 revival remained OPEN.
Previous code `d86d407`, based on `1fe7404`: native ordinary recovery,
actual HPR/MPR rates, client mode request and battle timeout with private local
connection clocks and supported cast/self-Defend producers. Existing PostgreSQL
operation/core/effect/checkpoint authority commits before resource/mode replies.
No schema change or execution; full combat/death/SoulHP and richer graphs remain OPEN.
Previous code `1fe7404`, based on `1c83b5f`: PostgreSQL-backed same-cell native
revival, source30/40% bars and ordinary aftermath decay, source step-preserving
load/transfer clock reconstruction and validated protection800 lifecycle.
No schema change or execution; unsupported variants/data/underflow and full
combat/death/SoulHP/bootstrap remain OPEN.
Previous code `1c83b5f`, based on `199fa5e`: native Login account/key guards
and signed INTEGER binds across Map admission/replicas/load/checkpoints/transfer/
release; mixed gameplay checks preserve unsigned BIGINT receipt/unlock keys.
No schema/sequence change or execution. Positive-only character allocation and
all other unported responsibilities remain OPEN.
Previous code `199fa5e`, based on `b251cdf`: supported basic-party statistic
context and process-local real unsent departure deltas, SQL-ready FIFO drain,
primary metadata repair after replica triggers, pending-membership guards and
post-await route readiness checks. Source fields and ordinary message order stay
unchanged; delayed succession ordering is documented. No execution or migration.
Full roster/bootstrap, rewards/combat and high-bit identity remain OPEN.
Previous code `b251cdf`, based on `622fb2e`: manual party chief/type paths,
post-ready primary/replica attribute recovery through the existing entered ACK,
sorted native replica authority and handoff-start serialization. Source wire
fields remain unchanged; no unknown replica bars enter MANSTAT. No execution or
new migration; full roster/bootstrap/gameplay remain OPEN.
Previous code `622fb2e`, based on `2e406dc`: basic native party requests,
source membership/client projections, party chat and post-ready handoff ATTR
repair under SQL/current-peer guards. Historical account counts survive logout.
No execution or new migration; full party dependencies and Phase A remain OPEN.
Previous code `80b7698`, based on `acbcaaf`: source Plain16 Map–World and
Control–World transport, DWORD catalog keys, exact Control inventory types and
raw World session monitoring. Original Map MW_CONNECT_ACK/full bootstrap remains
OPEN; modern RW admission is not its source replacement. Static source review
only. No build,
test, SQL, server, client, container or runtime verification occurred.
Previous code increment: `0b97c18` (native chat/TMS/bans/admin-event text), based on
`c389976`.
Previous code increment: `c389976` (soulmate economy, guild-level/message loading
and maintained timers), based on `75125a2`.
Previous code increment: `1f1eb59` / `e709281` (native World social persistence and original Map relay), based on `374d56a`.
Previous code increment: `11e0df1` / `dd07164` (atomic mutual friend adapter repair), based on `5fe42f8`.
Previous code increments: `e84ece4` / `65be6d1` (quest mail and offline bill returns), based on `c537883`.
Previous code increment: `45d130d` (World reserved-mail delivery/importer), based on `0f05d33`.
Previous code increments: `85bf923` / `feea4a1` / `b4ff581` (player mail send/protected lists/importer), based on `8cd91ff`.
Previous code increment: `d3904c5` (native mail returns/payment/notifications/importer),
based on `802a232`. Previous `18ee496` implements item expiration and mail, based on `abc40d4`.
Previous code increment: `3bc3bd1` (native NPC shops and PvP balances), based on
`f2a7060`. Earlier `0fe24d2` adds item gambling/upgrades and probability consumables. Earlier code `0457d3d` and `750c797` and all
historical evidence retain their original scope.

## Native basic-party self cure — source review, 2026-10-15

Latest local Phase A work (2026-10-15), based on `42451e1`, permits the
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

## Native action items — source review, 2026-10-15

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

## Native maintained-status collisions and retained800 — source review, 2026-10-15

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

## Native environmental damage and death — source review, 2026-10-15

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

## Native recovery and mode — source review, 2026-10-15

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

## Native revival and aftermath — source review, 2026-10-15

Code `1fe7404` (2026-10-15), based on `1c83b5f`, moves native
REVIVAL out of the cache-only full-heal prototype and into the current PostgreSQL
mutation/checkpoint path. The bounded same-cell primary path uses actual source
30/40% bars, aftermath/stat/mode replies and validated protection800. Stored
penalty steps survive hydration; the local decay clock is reconstructed without
new wire fields. **INTEGRATED_UNVERIFIED**: static review only, no build/test/SQL/
service/client/container run, migration (next070), push or deployment. Cross-cell
AOI/replica/mode variants, unsupported800 data and BYTE decay underflow remain
OPEN, together with full death/combat/SoulHP/rewards/bootstrap and client acceptance.
[Source contracts, supported scope and deferred acceptance](evidence/phase-a-native-revival-20261015/README.md).

## Native Login/Map identity domains — source review, 2026-10-15

Code `1c83b5f` (2026-10-15), based on `199fa5e`, repairs native
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

## Basic party statistics and unsent departures — source review, 2026-10-15

Code `199fa5e` (2026-10-15), based on `b251cdf`, permits supported
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

## Party attributes and manual changes — source review, 2026-10-14

Code `b251cdf` (2026-10-14), based on `622fb2e`, connects manual
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

## Basic native parties — source review, 2026-10-14

Latest local Phase A work (2026-10-14), based on `2e406dc`, connects source
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

## World framing and Control types — source review, 2026-10-14

Actual Map/World links use source Plain16, total16–65534, fresh zero metadata and
source receive ignoring sequence/checksum, including cloned client numbers.
Current-socket writes, no replay and disconnect drain remain; invalid World output
closes its writer. Modern RW ACK parsing now agrees with the source DWORD keys.
Control selects Plain16 for Map4/World3, refuses unavailable TLS and routes with
direct CTProtocol inventory IDs instead of guessed installer values. World raw
session monitor/current-Control/exact-body reset is connected to the listener.

Original Map MW_CONNECT_ACK and its full native bootstrap are **OPEN**. The source
has no registration ACK and ambiguous channel count/key casts; the retained modern
RW-as-Map gate is explicitly an extension. Other service/operator framing, party/
guild/mode producers and complete native World/gameplay remain incomplete. Existing
fixtures were statically aligned only, never run; 001–069 are unchanged/unexecuted.

[Source mapping and deferred acceptance](evidence/phase-a-world-framing-20261014/README.md).

## Direct Control–Map transport and operations — source review, 2026-10-14

- Source CPacket is16 bytes, unencrypted for the fixed Control-IP server role.
  Existing modern Control8 and encrypted Map client transport were incompatible
  with that path. The real Map listener now selects one configured/current raw
  Plain16 Control connection before client admission/RC4, and Control's Map
  dialer selects its matching profile. The empty handshake has no invented ACK.
- Five source operations now reach actual Map effects: announcements via raw059
  caption6/groupWORLD3/sender0/protectedCHAT1; exact ready primary/replica name kick
  via native claim proof and ordinary teardown; real session/accepted-player/
  historical unique-account counters; data clear rebuilding the account set.
- Native owner checks use pooled live advisory/row proof, independent from the
  dedicated heartbeat connection. Current Control/actor/socket/epoch gates span
  awaits; shutdown drains Control operations and client saves. Dirty/unknown
  reservations remain retained; no direct claim deletion or write replay.
- Control now sends real one-second monitor probes, consumes one four-DWORD reply
  without another echo, preserves source >= peak refresh/live counts, rejects
  stale service machine/port/socket topology and marks kick audit Requested.
- Accepted CS identity clears pre-auth timeout even during legitimate World load;
  monitor sweeps retain source >5s/>10s stages. Busy Control announcements skip a
  recipient instead of closing it. Original full strings and frame caps remain.
- Other modern8 server/operator profiles are **not original framing evidence**;
  their source16 port remains required. Native Map–World framing is now integrated;
  original Map admission/full bootstrap remains OPEN.
  Direct Map event/cash replay has a proved original producer/consumer conflict;
  Control refuses active unresolved replay before connection publication. No
  unsupported opcode becomes a successful no-op. TLS, remaining Map Control
  operations, combat/logout penalties and transfer kick parity remain OPEN.
- No new migration/build/test/runtime. Full Phase A and whole native World/
  gameplay remain incomplete. Private manifests, missing Patch SQL and original
  executable/assets remain inputs; next unused migration070.

[Source proof, exact boundaries and Phase B acceptance](evidence/phase-a-control-map-20261014/README.md).

## Native chat, TMS, bans and notices — source review, 2026-10-14

- Real CS chat/TMS/ban dispatch and MW response consumers are connected to native
  current primary/replica claims. Source local neighbors/country/spam/whisper,
  original type/group fields and full message bytes are retained. Recipient
  CHAT1/INVITE2 protection uses committed native state. Normal connected Relay
  and Map group/mode producers remain open.
- Written069 and an offline exact operator importer require the complete pinned
  archival manifest/checkpoint and native checksum. Source operator count0 is
  legitimate evidence; missing publication is an error, never an implicit empty
  authority or generated GM account. Mutable operator tools are still required.
- Native World commits ban/deadline/current-generation audit before projection
  or result, restores ban on entry and distinguishes MW stacking/zero-clear from
  Control replacement/zero-now. Persistence across World restart is an explicit
  extension of the source memory-only list. Unknown writes stop without replay.
- Independent communication serialization and member/pending keys guard TMS
  across awaits/logout. The source target-ID/recipient-key defect is repaired,
  unsigned roster order and BYTE255 bounds retained. Map CONREADY holds its
  primary lease before snapshot/Ready; names convert losslessly to source bytes.
- Authenticated existing Control targeted and event text routing now reaches
  current typed Maps and actual primary clients with published message6 and
  source NetString. Event text changes no reward/gameplay state. Full Control
  correlation/recovery, RW ban leg and complete event system remain open.
- Full original inventory and native World bootstrap are incomplete. This code
  has no build/test/runtime evidence. New044–069 remain unapplied; next070.

[Contracts, deliberate changes, source mapping, required OPEN work and later
acceptance](evidence/phase-a-chat-tms-20261014/README.md).

## Soulmate economy, World catalogs and timers — source review, 2026-10-13

- Actual original CS SEARCH/REGREADY/REG dispatch now reaches the existing
  native Map operation, original MW World selection/preview and native finalizer.
  Written067 records exact request/reply plus current process/claim identities.
  World preparation writes no relationship; Map atomically commits purse,
  optional caller, directed row, social audit and checkpoint before responses.
- World ready-primary maintenance checks source initial own/incoming level gaps
  with exact-target END. Cyclic consumed-receipt scanning projects cache only;
  Map standalone state/END verifies native authority under a projection lease.
  Deadlines, no write replay and fail-closed uncertain outcomes remain explicit.
- Written068 and an offline tool publish a verified immutable15-column guild
  chart; the native typed loader is selected by actual World main and CMake.
  Source level0 is retained and duplicate cache counting repaired. Existing
  cache assertions are aligned by static review only, not run.
- World now loads all51 source server-message byte rows from existing059's
  full pinned publication. Real relay entries use the original DWORD keys;
  event operator names and TMS no-receiver envelopes use source text. Map chat/
  TMS and operator lists/whispers remain required source work.
- Periodic checkpoint/logout/RELEASEMAIN sample maintained timers at exact
  milliseconds with eight-field identity/monotonic validation. Transfer expiry
  emits original SKILLEND after native preparation. Periodic expiry is deferred
  to existing progression producers; broader general effect simulation and
  save-only filters remain open beyond the admitted pinned subset.
- Full guild lifecycle/Map membership hydration and other World bootstrap
  repositories remain unported; party soulmate XP/emergency HP depend on full
  reward/combat transactions. The source inventory still contains NOT_ANALYZED
  responsibilities. These are required OPEN implementation, not completed work.
- Full private source manifests, missing Patch pre-version SQL and real original
  client executable/assets remain external blockers. No historical values or
  substitute success packets were invented. Written067/068/grants/publication
  await later isolated acceptance; next free migration is069.

[Source mapping, modern differences, blockers and concrete Phase B plan](evidence/phase-a-source-review-20261013/README.md).

## Native World social persistence and original Map relay — `1f1eb59` / `e709281`

- Existing World main selects native PostgreSQL social repositories, holds a
  dedicated World process generation and fences social/mail transactions. Live
  Map owner/current-user/key/account locks protect directed writes and mutual
  acceptance. A coroutine permit serializes social validation/commit/projection.
  Stale loads reject explicitly; persistence failures stop World without replay.
- Friend/group packets now reach client→Map→World→PG→original result. Original
  limits/security/protection/BOW no-ops and group bucket semantics are retained.
  A server-side invitation key prevents unsolicited/stale REPLY from creating
  another player's friend row; protected refusal consumes that authorization.
- FRIENDLIST nonzero soulmate now includes all descriptor fields required by
  independent original client/server sources. Directed soulmate registration,
  end timestamps, load expiration, incoming projections and online level/end
  notifications replace the incorrect mutual-row/deletion model. Map standalone
  state and client END are connected; full NPC costs/consumption/XP and initial
  load-gap CheckSoulmateEnd remain **IMPLEMENTING**.
- Written066/grants/social importer and added World offline barriers are
  **IMPLEMENTED_UNVERIFIED**, not executed. New scope is
  **INTEGRATED_UNVERIFIED**; Map/World compile/link in Debug/Release. No test, SQL,
  runtime, client or container was run. Original sources/backups/001–065 remain.
  Next free migration is067. No executable build/assets can be named.
- Production World rejects missing database/workers; fake tests remain. Actual
  World still validates/constructs unported guild/war/event/rank/item/service
  repositories. No schema validator was bypassed. Full Phase A and PG-only
  runtime remain incomplete; next implement those bootstrap/repository contracts
  and complete source social/gameplay dependencies.

[Source contracts, explicit differences and deferred acceptance](evidence/phase-a-native-social-20261013/README.md).

## Atomic mutual friend acceptance — `11e0df1` / `dd07164`

- The existing pending-both ASK and explicit REPLY acceptance paths now call
  one IFriendRepository operation; source IF NOT EXISTS/group preservation is
  retained for both edges in a single transaction before cache mutation/ACK.
- Current registered ready primary, registry pointer and keys are checked around
  worker awaits. Failed persistence poisons subsequent repository writes and
  stops World with exit1/count-free safe diagnostic; unknown COMMIT is not replayed.
- Existing fake adapter implements the changed API atomically without new test
  assertions or production fallback. World Debug/Release compiled/linked; tests,
  DB, runtime and containers did not run. **INTEGRATED_UNVERIFIED**.
- This is an existing schema-adapter repair. Native friend/group/soulmate schemas,
  imports, World ownership, cross-process fencing and bootstrap remain required.
  Other best-effort writes and empty-on-error loading are not native replacements.
  Next free migration remains066; no migration or historical file changed here.

[Exact scope and deferred acceptance](evidence/phase-a-mutual-friends-20261013/README.md).

## Quest mail and offline bill returns — `e84ece4` / `65be6d1`

- Original QT_SENDPOST now reaches the existing native QUESTEXEC/child/reward
  transaction: source selection/order, raw item chunks, temporary versus delivery
  identities, exact title/message bytes and child gate. Queue and child effects
  share account/owner/epoch/head/checkpoint fences. Missing/NULL title is source
  SP return2 with no mail, while the original true bRun still executes children.
- Written064 and explicit offline `--quest-mail` publish the complete pinned
  TQTITLECHART byte/value profile against the active Map/character/actor receipts.
  Actual backup columns are varchar/Latin1, not assumed Unicode. Prefix lengths
  and post-prefix truncation are retained. Appearance takes actual CTItem state
  as a documented repair of the omitted NOT NULL field; grade effect/gem source
  losses remain0. The archive and migrations001–063 are unchanged.
- Actual World DB worker now runs recipient-offline TJobsPostReturn semantics:
  stored receive-time expiry, source retained dwSendID, atomic native item/post/
  notification/audit and shared Login account/mailbox barriers. Missing senders
  retain bills/items and produce a warning. Native minute/batch128 and normalized
  attachment authority replace the unknown SQL Agent schedule/obsolete table
  explicitly. Unknown commit stops World without replay.
- Map and World Debug/Release compile/link passed. No migration, publication,
  import, test, server, sanitizer or container ran. All new production scope is
  **INTEGRATED_UNVERIFIED**, SQL/tool publication **IMPLEMENTED_UNVERIFIED**.
- Next: complete native World bootstrap/ownership/social repositories; other
  quest map/monster/effect actions and non-request event producers; admin/event/
  BR/rank mail, notification retention and obsolete/staged import reconciliation;
  full combat/AI/economy/social/modes. Full Phase A and PG-only runtime are incomplete.

[Source scope, explicit differences and deferred acceptance](evidence/phase-a-quest-mail-offline-bills-20261013/README.md).

## World reserved-mail delivery — `45d130d`

- Existing World main selects native PostgreSQL queue delivery on its actual
  PostgreSQL pool/group and schedules it on the original minute boundary, via
  existing DB workers. Batch limit30, missing-recipient break and status0/1/2
  retain source meaning. Full raw fields persist with new item IDs at delivery.
- Post/item/status/receipt/clock commit together; shared mailbox and allocator
  locking prevents duplicate concurrent batches. Known busy rollback leaves the
  minute available. Unknown COMMIT stops World with exit1 and is not replayed.
  Stable FIFO queue order, whole-batch atomicity, durable minute and UTC are
  documented modern changes; no new original-client fields are added.
- Written063/grants and the full pinned historical queue importer are unexecuted.
  Exact status/text/attribute/clock/identity mappings are required; terminal rows
  do not invent old delivery IDs. Original sources, backups and001–062 remain
  unchanged. Map/World Debug/Release compile/link passed; no test, DB or runtime.
- Remaining producers: quest/admin/event/BR/rank mail and original TQTITLE SQL
  conversion/prefix/truncation. A proved backup conflict requires explicit repair:
  TQuestSendPost omits NOT NULL/no-default wMoggItemID; its omitted bGradeEffect
  instead has the proved default0. Do not infer defaults from modern data.
- Full World bootstrap still validates legacy schema and constructs other legacy
  repositories. Delivery integration is INTEGRATED_UNVERIFIED; Phase A and the
  PostgreSQL-only runtime remain incomplete.

[Exact source scope and deferred acceptance](evidence/phase-a-reserved-mail-20261013/README.md).

## Player mail sending and native protected lists — `85bf923` / `feea4a1` / `b4ff581`

- Original POSTSEND53A2 now reaches the existing PostgreSQL primary transaction:
  source secure/tutorial/tournament/deal/store gates, saved countries, mailbox
  capacity, any-row protected blocking, funds/title checks, whole raw attachment,
  post/error audit and notification receipt. Commit precedes DELITEM, MONEY and
  POSTSEND53A3. Original item IDs/extensions and money DWORD arithmetic remain.
- PROTECTEDADD/ERASE/OPTION reach native remembered-name/options persistence.
  Fresh/recovered/transferred primaries hydrate SQL authority and capture the
  original protected transfer fields. Admission sends source PROTECTEDLIST after
  POSTINFO. World presence relay now requires the exact registered ready primary.
- Explicit security repairs reject privileged mail types on the player opcode,
  equipped-item mailing without equipment cleanup, embedded NUL and oversized
  responses. Non-ASCII mail requires an explicit ANSI profile; arbitrary native
  historical names/collation and client binary compatibility remain unproven.
- Migration061 adds protected state, original package gold-error audit, native
  notification enqueue provenance/superseding and the historical import receipt.
  The importer requires full pinned archives, exact reviewed identity/byte/codec
  maps and offline owners; it was AST-parsed only. Previous001–060 and backups
  are unchanged. All new scope is **INTEGRATED_UNVERIFIED** (tool/migration
  **IMPLEMENTED_UNVERIFIED**); no migration, test, server or container was run.
- Written062 corrects notification audit naming: the receiving Map sends its current
  second, because the original World message carries no timestamp. Map and World
  compile/link in Debug/Release; full World native social authority,
  reserved/quest/event/admin producers, offline bill jobs, receipt retention,
  trade reservations and other protected consumers remain implementation work.
  The full runtime is not PG-only and Phase A remains incomplete.

[Contracts, explicit differences and compilation evidence](evidence/phase-a-mail-send-protected-20261013/README.md).

## Mail return/payment and historical importer — `d3904c5`

- Original POSTRETURN53AC DWORD/BYTE reaches native recipient/read/view/security
  checks and the same owner transaction. Return retains post/item identity,
  exchanges recipient/sender and emits POSTDEL; payment atomically debits64-bit
  money, creates type4 payment mail and releases original attachments for claim.
- Online expiry is keyed by source **dwSendID**, not recipient. Ready primaries
  process source three-day deadlines and durable POSTRECV routing; inactive
  primaries use an internal mailbox poll without empty ledger writes. Source
  Tutorial suppression is retained. Separate checkpoint metadata prevents
  admission/list notifications from being counted again after handoff.
- STORAGE_POST is normalized authority; graph mail entries are read projections.
  The importer checks complete pinned archives, exact mappings/bytes/clock/codec
  profiles, native ownership and allocation indices; nonempty obsolete
  TPOSTITEMTABLE is an explicit unresolved contract. No import was executed.
- Migration060/grants are written only. World reserved/event/admin
  producers, offline expiry/job reconciliation and notification lifecycle remain.
  The full runtime is not PG-only. No test/server/container was run.
  [Exact source scope and compilation evidence](evidence/phase-a-mail-returns-20261013/README.md).

## Carried item expiration and native mail — `18ee496`

- Source strict30/20/10/1-minute alarm boundaries, unsigned bag/slot iteration,
  whole-instance deletion, equipped-item posture/stat responses and paused
  personal-store expiration reach the existing native timer transaction. Copy/
  SetItemAttr rearm alarms; merges/derivation retain them. Probability skills
  905/911/919/921/922/939 now also activate the regular expiration tick.
- Expired bags atomically move remaining items into groups of three attachments,
  allocate posts and remove the bag before INVENDEL/POSTRECV responses. Fresh
  rows and authoritative graphs retain original identities, raw attributes and
  current-owner/epoch/operation-head fencing. Unknown commits are not retried.
- Original POSTLIST/POSTVIEW/POSTGETITEM/POSTDEL requests use the same dispatcher,
  security/tutorial/tournament gates and transaction. Capacity precedes claim;
  attachment/fund/quest/title/checkpoint updates commit together. Source unbounded
  list pagination and unread-counter meaning are retained. POSTINFO is sent at
  native entry. Wider mail money amounts retain the original 64-bit contract.
- Migration059, role grants and `--server-messages` are implemented only. Existing
  item hashes remain stable across the added NULL attachment column. Backup
  message bytes are published without fabricated operator/title text.
- Explicit repairs: reject unpaid bill claims; protect all nonempty posts from
  deletion; derive native containment from actual funds/attachments because the
  backup's nullable bContain has no default/producer in TSavePost. Bound oversized
  packets; retain source data. Item mutation inputs now own their old item values.
- **INTEGRATED_UNVERIFIED**. Sending, returns/bill settlement, reserved/quest/event
  and offline mail import, full store/deal/tournament/effect producers remain
  required. No complete Phase A, PostgreSQL-only or executable-compatibility claim.

[Source contracts, exact compile results and Phase B backlog](evidence/phase-a-item-mail-20261013/README.md).

## NPC purchases and PvP balances — `3bc3bd1`

- Source item/trainer listings and ordinary/quest/magic/PvP purchases now use the
  existing native dispatcher, primary lease, PostgreSQL transaction and ordered
  responses. Capacity precedes caller consumption; price/title/item/PvP/quest
  changes share ownership/head fences and durable checkpoint recovery.
- Additive058 and the offline importer carry historical total/useable/monthly
  points, including signed source bit patterns. Fresh rows and authoritative
  transfer graphs stay separate; CHARINFO and handoff use actual balances.
  Ambiguous monthly-country rows abort runtime projection, not archival import.
- Original magic quantity/expiry and trainer-list quirks remain explicit. Unknown
  nonzero quests and remote BOW NPCs cannot select a free-purchase exploit.
- **INTEGRATED_UNVERIFIED**. Map Debug/Release compiled; no tests/imports/migrations
  or runtime executed. BOW, portal/monster shops, territory rules, native trade/
  tournament producers, item expiration/mail and complete PvP remain required.

[Source contracts, migration prerequisites and later acceptance](evidence/phase-a-npc-shops-20261011/README.md).

## Item identification, upgrades and probability consumables — `0fe24d2`

- GAMBLECHECK/GAMBLEOPEN now enter the existing native primary transaction:
  source quote precedence, unwrap/appearance reset, random template/magic/count,
  whole-stack failure, new item identity, pre-debit capacity, currency/titles,
  caller consumption and ordered GETITEM hooks. Uncertain commits are not retried.
- ITEMUPGRADE connects its ordinary source switch branches: numeric upgrade/
  downgrade, gems, appearance, magic/rare, clear magic, wrapping, equip-level
  reduction, refine reset, grade/cloak effects, color, undye and accessory scrolls.
  Source validation/results and random quirks are documented. Material/caller
  aliases and undefined arithmetic are rejected; consumed material fields remain
  owned values. This does not complete all economy, BOW or conditional territories.
- Pure skills905/911/919/921/922/939 use actual ITEMUSE self-Defend, maintained
  storage/transfer and the upgrade/magic/guard consumer. Lucky potion overlap and
  tutorial guard preservation follow the source. General effects remain required.
- Migrations056–057 and the existing offline publisher add complete gambling/
  upgrade content gates. Historical empty accessory content and all gem rows are
  preserved. Missing publication cannot silently become an empty gameplay chart.
- Status: **INTEGRATED_UNVERIFIED**. Map Debug/Release compile/link passed;
  test/migration/server/container execution remains deferred. Conditional guild/
  hero discounts and bonuses, premium/PCBang, BOW and shop/deal/tournament producers
  remain required. No full PostgreSQL-only or original-client compatibility claim.

[Exact contracts, changes, compile evidence and acceptance backlog](evidence/phase-a-item-upgrades-20261011/README.md).

## Refine, probability items and event delivery — `0457d3d` / `750c797`

- Original REFINE quote/execution and WASTEREFINE now reach the existing primary
  PostgreSQL transaction. Preserve source level/cost/narrowing/random order,
  full material-stack loss, raw magic collisions/reverse selection, preflight
  titles and caller consumption. Fresh item fingerprints or the authoritative
  full graph, owner/epoch/checkpoint/operation heads fence each mutation.
- Pure skill912/913 consumables have their original self-Defend entry, zero-MP
  lifecycle, maintained storage/transfer and one-use REFINE/TRANS probability
  consumption. General item skills and the full combat/regeneration consumers
  remain incomplete; these two skills do not certify the whole effect engine.
- Complete World EVENTINFO validation, original inner-value correction and
  ordered live/replay delivery feed16 source WORD values into Map. Native
  mutations sample their connection/revision/value context into migration055.
  The original ACK-before-replay sequence has no completion marker; full event
  recovery remains open. Monster events12/13 reject missing spawn/AI effects.
- World requires the explicit Control address/CIDR and same-connection CT role.
  Control has bounded owned frame buffers for plain/TLS, a shared event mutation/
  delivery lease, unpublished dialing through role/replay, and exact-connection
  monitor/cleanup. Old callbacks cannot remove a successor. Direct event/cash
  pushes require GMLevel3/All; source Relay announcement routing is restored.
- Migration055 and specific Map item-column grants are written only. Earlier
  migrations and historical sources remain intact. These changes are
  **INTEGRATED_UNVERIFIED**, not a complete Control/World/gameplay replacement.

[Source contracts, explicit changes, remaining scenarios and compilation](evidence/phase-a-refine-events-20261011/README.md).

## Soul Lottery, boxes, sale and repair — `c4385af`

- Original Soul Lottery requests now draw distinct source numbers and settle
  payouts/titles/effect820 through native owner/quest-head fencing. Draw fields
  retain their source player-object lifetime; they are not invented durable
  claims. Effect820 joins the timer and template-based zero-MP lifecycle. Backup
  status73 differs from GAINEXP69; no unsupported EXP multiplier is inferred.
- ITEMUSE handles MONEY51 and the original intentional AP28 no-stat-change branch.
  Special-box115 preserves source group/class/count/magic/extensions, capacity
  before debit and USEITEM ordering. Migration054 adds an explicit complete-chart
  publication gate and the offline activation tool's `--special-boxes` profile.
- Native ITEMSELL preserves source price/power formulas, optional NPC-call debit,
  currency/title changes and GETITEM count0 progress. Newly explicit shop/deal/
  tournament flags still lack their complete native producers; ordinary admitted
  players are the implemented scope, not full trade/shop/tournament integration.
- DURATIONREP quotes and repairs a single item, equipment or all inventory using
  source repair cost/raw magic percentages and reverse ACK ordering. Money/title
  checks, caller consumption, durability and derived equipment refresh share one
  transaction. Source UseMoney(FALSE)'s title side effect is retained. Conditional
  discounts and premium/PCBang remain gated by their missing authority.
- Follow-up `72d6504` restores the original secure-code prompt before decoding
  item use/sale/repair bodies, including malformed requests while locked.
- Same-instance NPC-call use-after-free and undefined numeric conversions are
  explicitly rejected. Special-box expiry and unordered SQL handling are recorded
  as differences. Earlier migration checksums and original sources are intact.

[Exact source references, differences and compile-only evidence](evidence/phase-a-lottery-boxes-20261011/README.md).

## Resource items, shared reuse and recovery — `ff50e58` / `6a91e4a`

- Original ITEMUSE now handles HP26, MP27, MAXHP42, MAXMP43 and GAINEXP81 in
  addition to EXPBONUS56. Pinned template values, original validation/results,
  resource/EXP responses, shared reuse, single-instance consumption and ordered
  USEITEM/level quest hooks use one native transaction. Source IU_FULL leaves the
  item and reuse untouched. Other kinds and tournament/deal/store remain open.
- Migration053 replaces TITEMUSEDTABLE/TSaveItemUsed staged persistence. Live
  remaining durations are sampled for CHARINFO, checkpoints, logout and transfer.
  Fresh rows are fenced by the checkpoint and quest head; a graph-backed owner
  keeps the full graph authoritative. Core writes can only decrease timers, not
  arm or extend them. Original LOADCHAR tutorial/one-hour filters are explicit.
- An offline importer preserves every historical cooldown row and unsigned bit
  pattern through an exact character map; it refuses active owners, existing
  recovery/state or changed archive. It was syntax-parsed only, never executed.
- The skill validator now distinguishes its relative calculation clock from the
  live quest/item timer origins. Prepared-transfer recovery reconstructs the
  server-only entitlement/cooldown metadata while preserving the durable body.
- After uncertain AcceptTransfer, the handler calls a read-only exact consumed
  receipt confirmation. A missing/prepared receipt never authorizes replaying
  the promotion. This does not prove fault recovery; Phase B remains pending.

[Exact source references, limitations and compile evidence](evidence/phase-a-item-use-20261011/README.md).

## EXP entitlement / ITEMUSE increment — `fa4c282`

- Native fresh loading, graph-backed transfer/recovery and timer transactions
  carry the original EXP slot (item/type/seconds/expiry). Migration052 replaces
  TSaveExpItem + TSaveCharDataStart/End staging for this slot with an atomic write.
  Core checkpoints compare the durable slot; quest operation heads fence older
  snapshots. After transfer, the complete graph remains authoritative.
- GainExp's18182/18183/18184 branch now sets the source half-hour entitlement and
  forces903/923, plus918 at the source threshold. Pure status collisions, expiry,
  refresh and END/CHARSTAT/DEFEND ordering are connected. LOADCHAR discards the
  old903 visual and EnterMAP recreates it after the visibility flood.
- CS_ITEMUSE_REQ handles source zero-delay IT_USE/IK_EXPBONUS templates with
  security, ownership, wrap, riding, dead/level and overlap checks. Force status,
  consume one item, success and USEITEM quest events share one PostgreSQL
  transaction. The requested template must match the owned item, closing a
  source quest-credit spoof. Other item kinds/tournament authority remain open.
- CHARINFO and player ENTER carry the source maintained-effect fields and sample
  remaining milliseconds when serialized. Previously ENTER always sent zero.
- import_during_items.py preserves both historical EXP and account entitlement
  rows through explicit identity maps and source IANA timezone. It rejects
  ambiguous EXP slots, DST instants, existing state/recovery and active owners;
  no import was executed. Account premium rows are preserved, but their gameplay
  admission remains explicitly blocked until premium/PCBang/pet semantics exist.

This is entitlement/visual/consumption integration, **not complete monster EXP
bonus or premium gameplay**. TMonster::OnDie/GetExp benefit multiplication, party
sharing, all general effects and the branches below still need native work.

## Quest reward/progression increment — `ea7c811`

- `QT_COMPLETE` resolves the first COMPQUEST term (absent means0), checks its
  running mission/terms and preserves force-run error suppression. `QT_CRAFT`
  checks a fresh recipe object and does not invent persistent completion counts.
  The existing request, native repository, ownership fences, ledger, checkpoint
  and ordered responses form one transaction; repeat valid crafting stays valid.
- Item/non-item reward selection preserves their different source switches,
  BYTE random accumulation, class filtering and early break. Magic rewards retain
  the six raw extensions, source magic percentages, expiry and stack splitting.
  Capacity is checked before consumption. Gold/quest title rules, learned skills,
  rank increases and source item/quest triggers share the same transaction.
- `GainExp(FALSE)` implements ordinary EXP/soul accounting, thresholds/skill
  points, LEVELUP quest hooks, full resources, map kick condition and original
  ordered World/owner/neighbor packets. World authenticates the main Map sender;
  secondary Maps validate the committed level before updating their projection.
- The existing Map scheduler advances pinned pure status effects918/923 once per
  second. Expiry precedes AUTOEXP; delayed ticks grant one charge as the source
  does. Effect/XP/quest changes share the quest ledger and current owner fences.
  Equipment/posture replacement and zero-MP checks preserve these non-posture
  effects. This is not general maintained-skill, premium or combat parity.
- Migration051 publishes backed quest item/title/level-limit/portal definitions
  against matching Map/character/actor manifests. Activation requires explicit
  `TGLOBAL_GSP=TGLOBAL_RAGEZONE` mapping for TGetLimitedLevel/TGetNation. It locks
  source/catalog rows, rejects ambiguous keys and retires stale offline owners.
  No catalog activation or data migration has run.

**Remaining reward/progression branches after `c4385af`:** account premium/PCBang
authority, daily timers/pets, complete monster EXP benefits, party/recall/guild
statistics and general effects remain open. Soul Lottery is now integrated but
unverified, including its820 status discrepancy and admission-lifetime reset. Ordinary EXP-item
level benefit creation no longer takes the earlier rollback guard.
No whole QT_COMPLETE, crafting, level progression or effect system is marked done.

## Implemented and connected in this increment

- **Map quest state:** native TQUESTTABLE/TQUESTTERMTABLE and operation ledger;
  loading, checkpoint fences, fresh-row/full-transfer graph authority, remaining
  timers, relogin/transfer serialization and source QUESTLIST/timer packets after
  CHARINFO. Live quest-level cache follows source creation/drop rules.
- **Map quest requests/actions:** NPC talk guards and progress; drop/end-timer;
  ordered source conditions/triggers; default talk, NPC talk, chapter, drop,
  mission/guild quest action, skill grant, item grant/delete, routing NPC item
  list. Item IDs, stack/slot
  planning, count changes and quest progress commit together before responses.
  This does **not** complete quest rewards, guild quests or all quest actions.
- **Default skill log:** successful trainer learning records all six TLogSkill
  arguments in app_world.TSKILLLOG in the purchase transaction. DEF_UDPLOG is
  undefined in the original default build; the alternate UDP skill body is
  commented out. This is not replaced by generic UDP logging.
- **Historical import tools:** explicit full-table `archive` extraction/import,
  preserving values and original bytes; pinned-backup checks; offline quest
  condition publication and character-mapped progress import. These tools have
  not extracted, migrated or reconciled any data in this phase.
- **Peer security:** existing PeerSecurityGate repository can load native trust
  rows, append outcomes and maintain counters/retention. New runtime_control
  tables are operational structures, not invented historical data.
- **Patch:** native catalog/import for TVERSION, TPREVERSION, TUSER_INTERFACE;
  original varchar bytes on the wire, initialization/schema checks, native
  minimum-beta policy, explicit environment connection configuration. No assets
  were invented or downloaded. Missing promotion contracts remain blockers.
- **Control:** native topology including TNETWORK; operator authentication;
  character-targeted account ban; event/cash-picker reads and event CRUD;
  regular patch updates; audit; peer registration, heartbeats, metrics, status,
  expiry and restart hydration. A dedicated PostgreSQL session owns Control,
  transactions fence its generation/backend identity, and unknown commits poison
  the instance until restart. Peer epochs use a noncycling native sequence;
  delayed cleanup/heartbeats cannot overwrite a newer epoch. Native paths do not
  issue T-SQL. Three missing pre-version procedures explicitly reject requests.
- **Control behavior corrections:** event IDs now match NetCode.h (XP1, cash2,
  spawn12, regen13, lottery14, gift15); original value serialization/restoration,
  ordered events, event-edit/scheduler exclusion and one-shot dispatch snapshots.
  Lost ACKs do not leave the event cache behind a committed database edit.
  Reloading topology now publishes a repeatable-read native snapshot; login lists,
  registry rebinding and peer dialing hold owning copies across await/offload.
  The connect timeout owns its cancellation state, removing dangling references
  when its handler is already queued at coroutine completion.
- **Production configuration:** Control, Patch and Log require an explicit DB.
  Existing native Log audit implementation (migration029) is preserved, including
  its unknown-COMMIT no-retry rule; only its stdout production fallback is removed.
  Fake services and existing tests remain available to the test targets.
- **Build compatibility:** stable SOCI parameter lifetimes/narrowed values in
  existing Login/World/Map/Control repositories and the PostgreSQL libpq include.

Concrete source symbols, variants, native counterparts and gaps are in
[legacy-modern-coverage.csv](legacy-modern-coverage.csv) and
[postgresql-object-map.csv](postgresql-object-map.csv). The broad existing
[source inventory](evidence/source-inventory.json), [capability matrix](capability-matrix.md)
and [historical object map](../database-reconstruction/evidence/source-object-mapping.json)
remain the navigation indexes. Their unreviewed entries are NOT_ANALYZED, not coverage.

## Explicit differences and unresolved contracts

- Quest/audit commits precede replies instead of asynchronous legacy saves.
  Equal request bytes do not identify duplicates. Ownership, ledger heads and
  current snapshots authorize writes. Full transfer graphs remain authoritative.
- Term keys retain the backup PK (quest/term ID), not an invented type dimension.
  Conflicting source rows are refused in runtime activation, preserved in archive.
- Monotonic remaining timers correct the source transfer's shadowed tick variable;
  end-timer also clears stored remaining time. New item expiry/calendar and audit
  timestamps use UTC; this does not reinterpret historical timestamps.
- Quest probability retains the Windows 15-bit `%100` distribution with secure
  independent draws; source RNG sequence is not reproduced. SQL tie ordering uses
  explicit quest/term IDs where the original only ordered main/type. Acceptance
  must assess these decisions, recursion256 and oversized count/input rejection.
- Progression effects currently checkpoint the remaining value observed at the
  last committed one-second timer tick; source GetRemainTick also samples at save
  and handoff. Exact sub-tick transfer/logout sampling and time spent in an
  interrupted handoff remain required. Charge clocks restart on hydration,
  matching the source constructor/load boundary; elapsed offline time earns no XP.
  Client CHARINFO/ENTER now sample current remaining time independently.
- New EXP expiry uses exact epoch seconds. Historical SMALLDATETIME values are
  untouched in the archive; import requires an explicit source timezone and
  rejects ambiguous/nonexistent local times. The runtime no longer rounds a new
  entitlement expiry to a SQL Server minute; Phase B must assess this precision
  difference. Ambiguous multiple EXP rows/unsupported templates are not guessed.
- ITEMUSE validates the claimed template against the actual owned slot. The
  original handler could use a different valid template as its quest trigger.
  Unsupported item kinds or gameplay dependencies fail without consuming items.
- Item reuse now uses 64-bit monotonic observation rather than DWORD deadlines;
  it does not grant early reuse when the original tick counter wraps. Offline
  time remains paused; only remaining milliseconds are persisted or transferred.
  GAINEXP rechecks the same item identity after level-triggered quest changes;
  a removed instance rolls back instead of dereferencing the original dangling
  pointer. These repairs require Phase B failure/compatibility scenarios.
- World level sends follow the database commit. A failed/unknown send closes the
  source client without replaying its reward; durable cross-process delivery and
  reconciliation are still a World integration requirement.
- Operator plaintext is transformed offline to salted PBKDF2-SHA256 (210000
  iterations). Password case is exact rather than SQL Server CI comparison;
  trailing-space comparison is retained. ID/path comparison uses the existing
  explicit ICU CI/AS policy, not a claim of full SQL Server collation equivalence.
- Control retains original multibyte strings and local event dates. The operator
  must choose the source IANA timezone at import and set matching TZ at runtime;
  no timezone is inferred from a backup. Event writes retain SMALLDATETIME minute
  rounding and TEventUpdate's 50-byte title parameter. Ambiguous bans, malformed
  data and unrepresentable input fail rather than selecting arbitrary characters.
- The original event parser loses monster coordinate signs, repeats the first
  lottery reward on restore and omits GIFTTIME message restoration. Those source
  quirks are explicit in event_value.h; malformed/uninitialized source cases are
  rejected. Source-defined topology exclusions are reported, not fabricated.
- Backup TMinBetaVer sets OUT parameter2, but original CSPMinBetaVer binds a return
  value without the required OUT argument. Native Patch follows the backup OUT
  value. **TPreCompleteAdd, TUpdatePreVersion, TBetaToVersion and TDeletePreVersion
  are absent from both inspected catalogs**; their complete semantics remain
  missing. OPTool_SMSEmergency has an entirely commented body: native logging
  preserves that intentional no-op and does not send SMS.

## Exact continuation / implementation gate

1. Continue the source-dependent progression/item contracts:
   TMonster::OnDie party and owner IsExpBenefit multiplication; account
   TDURINGITEMTABLE + TGetPcBangData/TSetPcBangData and premium/daily/pet behavior.
   Premium item templates7401/7402/7403 use skills900/901/902,7626/7627/7628 use
   914/915/916 in the available catalog; inspect their full data before admission.
   Complete remaining CS_ITEMUSE kinds beyond HP/MP/AP/full-resource/direct-EXP/
   EXPBONUS/MONEY/SPECIALBOX/pinned probability skills and tournament/deal/store
   producers. Exact effect sampling at save/transfer/logout is now connected,
   unverified; complete general expiry simulation and source save-only filters.
   Soul Lottery820 is integrated, not verified.
   Ordinary REFINE/WASTEREFINE and received event probabilities are now connected.
   Complete conditional guild/hero NPC bonuses/discounts, BOW-specific branches,
   complete economy integration and general item effects. Ordinary gambling and
   upgrade branches are now integrated at0fe24d2, not verified. Preserve the new
   full-stack/magic/caller behavior and exact per-operation World observation;
   it is not a complete event replay/recovery barrier.
   Continue QT_DROPITEM/SPAWNMON/TELEPORT/SWITCH/DIEMON/DEFENDSKILL/REGEN
   and all non-request kill/use-item/position/spawn producers. Quest mail uses
   TRESERVEDPOST then TGetReservedPost and storage2. Migration059 now provides
   native attachment ownership, expired-bag delivery and read/claim/delete paths;
   returns/payment and an offline importer are added byd3904c5. Quest mail and offline returns are integrated at e84ece4/65be6d1. Continue other
   reserved producers, staged/obsolete import reconciliation and exact source import
   blockers using that same ownership path. Do not add another mail backend.
2. Implement source Map switches (QCT20), neutral NPC territory authority and
   conditional guild/hero trainer discounts. No substitute country/switch values.
3. Finish Control one-shot event delivery/recovery with native World recipients,
   missing monster event12/13 side effects and deactivation after Control/World
   crashes. Frame/replay serialization is implemented; durable application
   receipts, full admin reply correlation and slow-peer deadlines remain open;
   complete missing pre-version contracts if further authoritative SQL is found.
   Operator/offline tools, service variants and remaining admin behavior still
   need per-responsibility review; native DB paths alone do not complete Control.
4. Native friend/group ownership, soulmate initial-load checks and Map atomic
   SEARCH/REGREADY/REG are now integrated, unverified. Complete party XP/emergency
   HP and full native guild/party/corps/tactics Map topology. Basic parties and
   normal party chat are now integrated but unverified; equipment/loot/XP/arena/
   instance/mode and roster recovery remain open. Port guild/war/event/rank/
   item/service repositories and actual schema validator/bootstrap. Chat/TMS and
   admin/event text now have bounded native Map/World paths; complete connected
   Relay/RW bans, group producers, BR team and BOW peace authority, mutable
   operator administration and Control response-loss/recovery correlation.
   Existing SpCall/unqualified legacy repositories must not be treated as PG-ready.
5. Complete general combat targets/effects/interrupts, AI and rewards; all remaining
   inventories/economy/crafting/mail/trade, social/companions/modes/BOW/BR; all
   source timers/internal messages/build variants/tools and historical data paths.
   Preserve existing bounded implementations; the full inventory gate is open.
6. Complete source16 framing for remaining service/operator links; direct Map
   event/cash conflict, other Map Control commands/TLS, admin-shell recovery,
   combat/logout penalties and entered/transferring kick variants remain required.
7. Then do the whole Phase A static gate, Phase B validation/migration and fixes,
   and finally Phase C local cluster. **No full PostgreSQL-only runtime claim.**

## Activities actually performed

Latest local work based on `1fe7404`: original resource recovery, formulas,
entry/transfer clocks, client mode/HPMP consumers and actual native timer/cast/
self-Defend/authority/packet/diff review only. See
[current recovery evidence](evidence/phase-a-native-recovery-20261015/README.md).
No build, test, SQL, server, client, service or container executed.
Previous local work based on `1c83b5f`: original revival/aftermath/protection,
client consumers and native authority/persistence/caller/diff review only. See
[current evidence](evidence/phase-a-native-revival-20261015/README.md). No build,
test, SQL, server, client, service or container executed. Previous local work
based on `199fa5e`: original DWORD account/key definitions,
Login/Map call paths, migration column types and diff review only. Source INT
keys and unsigned operational BIGINT keys were reviewed separately. See
[current evidence](evidence/phase-a-native-identity-20261015/README.md). No build,
test, SQL, server, client, service or container executed. Previous local work
based on `b251cdf`: original statistic/DEL/client consumers,
native graph capability and queued removal source/API/SQL-text/diff review only.
See [current evidence](evidence/phase-a-party-stats-20261015/README.md). No build,
test, SQL, server, client, service or container executed. Previous local work
based on `622fb2e`: original/server/client source reads,
declaration/call/CMake/SQL/packet/diff review, implementation and documentation.
`git diff --check` passes. No build, test, SQL execution, service/client/container
startup or runtime acceptance was performed. The following build results belong
only to their historical revisions.

The changed Map target at `0fe24d2` compiles/links in Debug and Release with
BUILD_TESTING=OFF and native PostgreSQL enabled. Static syntax/call-path inspection
only; migrations056–057 and the extended import profiles were not executed.
[Current commands, hashes and logs](evidence/phase-a-item-upgrades-20261011/README.md).

All six modern C++20 Debug and Release targets compile/link with BUILD_TESTING=OFF
for code commits `0457d3d` and `750c797`. This increment read existing historical
exports without running import/extraction. `git diff --check` passes. Compilation does not establish
gameplay, persistence or protocol parity.
Exact configuration, commands and logs are retained in
[evidence/phase-a-refine-events-20261011](evidence/phase-a-refine-events-20261011/README.md).
Earlier lottery/item economy compilation remains in
[evidence/phase-a-lottery-boxes-20261011](evidence/phase-a-lottery-boxes-20261011/README.md).
Previous resource/cooldown compile evidence remains in
[evidence/phase-a-item-use-20261011](evidence/phase-a-item-use-20261011/README.md).
Previous entitlement compile evidence remains in
[evidence/phase-a-during-items-20261011](evidence/phase-a-during-items-20261011/README.md).
Previous quest reward compilation remains in
[evidence/phase-a-quest-rewards-20261011](evidence/phase-a-quest-rewards-20261011/README.md).
Earlier Phase A compile evidence remains in
[evidence/phase-a-20261011](evidence/phase-a-20261011/README.md).

**Not executed:** unit/integration/regression/protocol/PG tests, CTest/pytest,
sanitisers/fuzz/benchmarks, migrations/import/reconciliation, server/client
processes, Docker/Podman builds/startups or cluster restart tests. Phase B/C are
not started. No historical evidence was rewritten. Compilation is the only
executed code validation for this increment; new gameplay remains unverified.

Only client source exists. No supported executable build/hash or matching asset
set can be named; real-client/launcher acceptance remains blocked. This is an
acceptance/data blocker, not a reason to stop source-backed server implementation.
