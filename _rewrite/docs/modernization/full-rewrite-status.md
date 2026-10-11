# Full rewrite — Phase A, incomplete

Owner instruction, 2026-10-11: finish the complete implementation before Phase B
verification/migration and Phase C container startup. **Phase A is still active;
the IMPLEMENTATION_COMPLETE_UNVERIFIED gate has not been reached.**

Starting revision: `2fa2396`, local `main`. All changes in this increment are
**INTEGRATED_UNVERIFIED** or **IMPLEMENTING** as scoped below. No pushes or
publication. Pre-existing untracked `_rewrite/docs/audits/` is outside this work.
Original server/client sources, backups, applied migrations001–043 and historical
evidence remain unchanged. New migrations044–050 are **written, not applied**.

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

1. Complete QT_COMPLETE from QuestComplete.cpp and TPlayer::OnQuestComplete:
   reward selection, magic reward item definitions, inventory capacity before
   consumption, gold/title effects, skills/ranks, GainExp/soul/level side effects,
   quest counters and child/event triggers in one native transaction. Continue
   remaining Quest*.cpp actions; connect kill/use-item/completion and timer events.
2. Implement source Map switches (QCT20), neutral NPC territory authority and
   conditional guild/hero trainer discounts. No substitute country/switch values.
3. Finish Control one-shot event delivery/recovery with native World recipients;
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

All six modern C++20 Debug targets compile/link with BUILD_TESTING=OFF. Static
Python AST parsing covers the six changed/new database tools. `git diff --check`
passes. Compilation does not establish gameplay, persistence or protocol parity.
Exact configuration, commands and logs are retained in
[evidence/phase-a-20261011](evidence/phase-a-20261011/README.md).

**Not executed:** unit/integration/regression/protocol/PG tests, CTest/pytest,
sanitisers/fuzz/benchmarks, migrations/import/reconciliation, server/client
processes, Docker/Podman builds/startups or cluster restart tests. Phase B/C are
not started. No historical evidence was rewritten. Release for these changes
has not been compiled; previous Release evidence applies only to its own revision.

Only client source exists. No supported executable build/hash or matching asset
set can be named; real-client/launcher acceptance remains blocked. This is an
acceptance/data blocker, not a reason to stop source-backed server implementation.
