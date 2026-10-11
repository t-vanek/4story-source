# Full rewrite — Phase A, incomplete

Owner instruction, 2026-10-11: finish the complete implementation before Phase B
verification/migration and Phase C container startup. **Phase A is still active;
the IMPLEMENTATION_COMPLETE_UNVERIFIED gate has not been reached.**

Starting revision: `2fa2396`, local `main`. All changes in this increment are
**INTEGRATED_UNVERIFIED** or **IMPLEMENTING** as scoped below. No pushes or
publication. Pre-existing untracked `_rewrite/docs/audits/` is outside this work.
Original server/client sources, backups, applied migrations001–043 and historical
evidence remain unchanged. New migrations044–058 are **written, not applied**.
Latest code increment: `3bc3bd1` (native NPC shops and PvP balances), based on
`f2a7060`. Earlier `0fe24d2` adds item gambling/upgrades and probability consumables. Earlier code `0457d3d` and `750c797` and all
historical evidence retain their original scope.

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
   EXPBONUS/MONEY/SPECIALBOX/pinned probability skills, tournament/deal/store producers and exact effect
   sampling at save/transfer/logout. Do not confuse packet-time samples with
   durable sub-tick persistence. Soul Lottery820 is integrated, not verified.
   Ordinary REFINE/WASTEREFINE and received event probabilities are now connected.
   Complete conditional guild/hero NPC bonuses/discounts, BOW-specific branches,
   complete economy integration and general item effects. Ordinary gambling and
   upgrade branches are now integrated at0fe24d2, not verified. Preserve the new
   full-stack/magic/caller behavior and exact per-operation World observation;
   it is not a complete event replay/recovery barrier.
   Continue QT_DROPITEM/SPAWNMON/TELEPORT/SWITCH/DIEMON/DEFENDSKILL/SENDPOST/REGEN
   and all non-request kill/use-item/position/spawn producers. Quest mail uses
   TRESERVEDPOST then TGetReservedPost and storage2; carried-only TITEMTABLE cannot
   receive attachments without an explicit storage/ownership extension.
2. Implement source Map switches (QCT20), neutral NPC territory authority and
   conditional guild/hero trainer discounts. No substitute country/switch values.
3. Finish Control one-shot event delivery/recovery with native World recipients,
   missing monster event12/13 side effects and deactivation after Control/World
   crashes. Frame/replay serialization is implemented; durable application
   receipts, full admin reply correlation and slow-peer deadlines remain open;
   complete missing pre-version contracts if further authoritative SQL is found.
   Operator/offline tools, service variants and remaining admin behavior still
   need per-responsibility review; native DB paths alone do not complete Control.
4. World friend/group/soulmate/guild/war/event repositories still require native
   world/session ownership, atomic mutual operations and response integration.
   Existing SpCall/unqualified legacy repositories must not be treated as PG-ready.
5. Complete general combat targets/effects/interrupts, AI and rewards; all remaining
   inventories/economy/crafting/mail/trade, social/companions/modes/BOW/BR; all
   source timers/internal messages/build variants/tools and historical data paths.
   Preserve existing bounded implementations; the full inventory gate is open.
6. Then do the whole Phase A static gate, Phase B validation/migration and fixes,
   and finally Phase C local cluster. **No full PostgreSQL-only runtime claim.**

## Activities actually performed

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
