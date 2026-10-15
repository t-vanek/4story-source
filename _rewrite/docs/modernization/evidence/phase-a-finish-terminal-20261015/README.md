# Native CURE terminal returns and already-dead targets

2026-10-15, local `main`, baseline `7a8dfeec096a7395e84ccff2aa929174437d312d`.
Gameplay is **INTEGRATED_UNVERIFIED**; new migration073 is
**IMPLEMENTED_UNVERIFIED**, written only. The full Phase A gate remains open.
The initial worktree contained only untracked `_rewrite/docs/audits/`, left
untouched and excluded from the local commit. No AGENTS.md was found in the
repository or its ancestor directories. README's owner policy requires local
`main` commits and prohibits publication and verification runs.

## Source contract and changes

`Server/TMapSvr/CSHandler.cpp::OnCS_FINISHSKILL_ACK`, lines20527–20665,
filters dead targets before the per-target exception/Defend work. For skill3604,
lines20607–20614 look up the session player's actual learned3603 object: absence
returns from the entire handler, bypassing both Defend and terminal
`pPlayer->CheckEquipSkill()`. A present rank0 is an object, not absence. Empty
target lists never reach the lookup and still reach CheckEquipSkill.
`TSkillTemp.cpp::CanDefendAtDie`, lines505–519, admits REVIVAL or AI TEMPT,
neither of which occurs in the supported all-SA_ONCE pure HP/MP CURE profile.
`TPlayer.cpp::CanDuel`, lines4277–4301, does not refuse an own-PC dead target
before that dead-target filter. Source FINISH has no blanket living-caster gate.

Source `OnCS_ITEMUSE_REQ`'s IT_ACTITEMS branch (CSHandler.cpp9264–9293) sends
ACTION and does not consult learned3603. `OnCS_ACTITEMSUSE_REQ`, lines19875–20196,
also never looks up3603: activity/erasure, item reuse and debit occur before its
SKILLUSE ACK. A later FINISH return cannot refund those earlier effects.

The former modern resolver threw on missing3603; both item preflights therefore
refused the operation and nonempty FINISH closed uncertain without a terminal
receipt. FINISH also refused an already-dead actor before the target filter.

- `services/postgresql_map_actitems.cpp::ResolveNativeCureEffectRank` now returns
  an optional rank. Absence means the source early return; present0 remains0.
  `PreflightActItemCure` skips future cure/MISS802 evaluation in that case, so
  ITEMUSE initiation and ACT's existing hooks/reuse/debit can proceed. Supported
  catalog/actor/effect guards remain mandatory; this is not arbitrary3604 combat.
- `services/postgresql_map_checkpoint.cpp::FinishSkill` resolves this per living
  occurrence before Defend activity, cure RNG and MISS802. Missing3603 records
  `SourceMissing3603`, bypasses terminal equipment cleanup and restores the input
  snapshot before advancing only its finish receipt head. No substituted rank,
  DEFEND/HPMP/END/STAT, death, cure roll or additional recovery activity is emitted.
  Ordinary persistence sampling of elapsed timers in PersistenceSnapshot remains.
- Already-dead native own-PC FINISH now reaches the source dead-target filter.
  It requires coherent HP0/dead state and the same accepted ordinary/loop/ACT
  receipt, supported graph, route and current primary authority. Every target
  occurrence skips before exception lookup, cure/MISS work and activity clocks;
  CheckEquipSkill still runs, including for an empty list. Any actual equipment
  erasure retains its original packets and audiences. There is no revival or
  second death transition. `ReadActItemSkill` can read the definition after death;
  its real ACT caller retains an explicit living-actor capability guard, and
  ITEMUSE retains its existing dead result branch. Dead ACT initiation remains
  an unported source variant, not a completed feature.
- `services/finish_skill.h` carries the server-only terminal disposition.
  `handlers/maintained_effect.cpp::OnFinishSkill` validates it inside the existing
  uncertain-result boundary, stores the committed snapshot and returns without
  fanout for the source return. Completed empty/dead finishes keep the ordinary
  ordered output path. No field or opcode is added to the client protocol.

## Actual integration and persistence

Incoming `handlers/dispatch.cpp::DispatchInner` already routes FINISHSKILL to
OnFinishSkill, with Ready identity/role/epoch and gameplay-operation fencing.
OnFinishSkill freezes state, samples it and offloads the existing PostgreSQL
service; no blocking SQL is moved onto the network executor. ITEMUSE dispatch
continues through ChangeQuest/UseNativeItem and the existing preflight; ACT
continues through ChangeQuest/UseNativeActItems and that same preflight. All
modified translation units are already in `Server/TMapSvrAsio/CMakeLists.txt`;
this is not evidence of compilation.

The existing account/claim lock, catalog context, full inventory/learned-rank
validation, current physical cell, checkpoint and operation heads precede the
new disposition. FIFO matching still selects an outstanding accepted cast in
this login. A terminal no-effect return consumes that accepted cast exactly once
through the existing unique `accepted_cast_id`, without client success output.
This one-use ledger is the existing modern authority policy, not a new legacy
database procedure. Identical later accepted casts are not byte-hash duplicates.
The original protocol cannot universally distinguish their replayed FINISHes.

New `database/postgresql/073-native-finish-disposition.sql` adds only nullable
`app_world.skill_finish_operations.disposition`. Old receipts retain NULL;
new writers explicitly use `completed` or `source_missing_3603`. The latter
requires source3604, a nonempty target list, unchanged positive HP/MP and
effects, and empty responses/audiences. Client serialization in
`Client/TClient/CSSender.cpp::SendCS_FINISHSKILL_ACK`, lines2980–3020, independently
establishes little-endian skill bytes20/14 at offsets21/22 and target count at33;
the Windows BOOL fields remain four bytes. No archival data is modified.

Receipt insertion, finish-head advancement, WriteCore, RecordCheckpoint and
StoreTransferCheckpoint stay in one existing transaction before output/cache
publication. Fresh/graph loaders and handoff already reload the maximum finish
head; CheckCastHead rejects older snapshots. No response replay, compensating
debit or automatic retry follows uncertain COMMIT. Table-level Map privileges
cover the additive column; no role/grant or importer is introduced. Existing
tools/database/migrate.py discovers numbered SQL files; no parallel migrator.
Migrations001–072 remain unchanged;044–073 are unapplied, next074. During Phase B
stop old Map writers and apply the existing checksummed migration workflow to
an isolated copy before running the new binary. No SQL/tool/import was executed.

## Deferred acceptance and remaining work

All following scenarios are **NOT RUN**. Expected branches come from the source
locations above, rather than only from the new implementation.

| Scenario | Required result and independent oracle |
| --- | --- |
| Actual eligible3604 item, no learned3603 | ITEMUSE ACTION; ACT's ordered activity/erase/reuse/debit and SKILLUSE ACK; nonempty FINISH emits nothing, does not run CheckEquipSkill and records source return. Compare source9264/19875/20607. |
| Actual3603 rank0 or rankN | Present0 must not take source return; current rank drives outgoing Calculate/DEFEND only. Retain trigger critical and outgoing powers. Compare source20607 and TSkillTemp::GetValue. |
| Remove/add3603 between ACT and FINISH using authorized state changes | Decide using current validated learned membership; never a rank frozen at ACT. Out-of-band SQL drift must be refused, not treated as authoritative absence. |
| Missing3603 with GOHOME2/critical255 and unsupported actual802 metadata | Source return precedes Defend/MISS802; no forced802 or numeric cure work. A present3603 retains the existing unsupported802 refusal. |
| Empty FINISH and unsuitable retained posture | No3603 lookup, regardless of presence; CheckEquipSkill executes its real removal/END/private STAT and conditional HPMP path. Compare source loop and epilogue separately. |
| Accepted cast, ordinary DROP/CURE death, delayed FINISH | HP0/dead survives; each pure HP/MP target skips with no cure/death/802/activity; equipment epilogue still runs. Repeat for ordinary/loop/ACT, empty/duplicate targets and fresh/transferred graphs. Compare CanDefendAtDie and source20527. |
| Restart/transfer/stale snapshot/concurrent FINISH | One receipt per accepted cast; head/checkpoint/fullgraph survive; stale writes and revoked/replaced primary claims fail. Identical separately accepted operations stay distinct. |
| Fault before/after INSERT/checkpoint/COMMIT and lost result | Rollback is atomic; uncertain commit closes/quarantines the session without retry/debit/refund. Reconcile through existing authoritative load. Inspect actual SQL receipts and socket output. |
| Upgrade with historical036/038 receipts | NULL dispositions and all old bytes remain identical. Invalid source-return skill/count/changed bars/effects/output is rejected by073; valid zero-output records commit. Use an isolated real PostgreSQL copy. |
| Actual client and all required builds | Build full targets/configurations, then independent packet/state comparison, original client when available, and later local cluster acceptance. Synthetic packets alone do not close client compatibility. |

No build, compile, test, sanitizer, benchmark, database, SQL, migration, import,
server, client, service or container ran; no push/deployment. Only source,
call/declaration/CMake/SQL-text and diff/CSV/Markdown metadata inspection was used.
Source/backup/applied migrations/private audits and historical evidence remain
untouched. Full secondary vitals/AOI, negative/foreign combat, richer129/230
erasure, dead ACT initiation, original Map admission/native World bootstrap,
all unported modes and actual client/data acceptance remain open. The current
[queue](../../next-steps.md) remains authoritative; no overall completion
percentage is claimed.
