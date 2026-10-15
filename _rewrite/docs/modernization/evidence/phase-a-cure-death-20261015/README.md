# Bounded own-PC CURE final-death completion

2026-10-15, local `main`, baseline `1a9fa82`.
**IMPLEMENTED / INTEGRATED_UNVERIFIED**. The full Phase A implementation remains
incomplete. This increment extends the existing native own-PC odd-positive-template
pure HP/MP CURE from living results to ordinary final-HP0 death, under explicit
current actor/graph and verified map-context bounds.

Owner restrictions remain unchanged: local Git only; no push/deployment, build,
test, SQL execution, migration/import, backup extraction, server/client/service,
container or runtime verification. Only source/API/SQL-text/packet/diff and
CSV/Markdown metadata inspection was performed. Historical execution does not
verify this code. Original server/client/protocol/Lib, backups, older evidence,
applied001–043 and written/unapplied044–072 are unchanged. Private audit output
remains outside this change. No new schema, publisher, grant, public service
signature or client field is introduced; the next migration remains073.

## Production behavior

FINISH still resolves its current one-use accepted ordinary/loop/ACT receipt and
current source rank, maxima and effect profile inside the existing native Map
transaction. Signed row arithmetic and DWORD modulo are unchanged. All rows run
before the target cap; intermediate HP0 can still be restored by a later row.

For final HP0, the actual ordinary-state guard and existing070 map proof must
succeed before target cleanup/output. Source zero-MP cleanup then runs normally.
The target emits its 84-byte zero-damage DEFEND with LASTHIT4 and conditional
21-byte HPMP with both wire bars zero to nearby players. Silent ordinary death
sets HP0/MP0/bDead, removes actual nonstatic maintained rows, refreshes current
statistics and emits the existing five-byte DIE to neighboring cells. Source
temporary maxHP before OnDie has no observable intermediate consumer in this
bounded graph and is not published as an extra heal.

The changed-resource flag is captured before zero-MP cleanup. That cleanup may
already derive bDead from HP0 during stat refresh; it does not substitute for
the subsequent silent OnDie effect release. Static effects survive death unless
the earlier actual zero-MP or later equipment cleanup removes them by its own
source rule. Death release itself emits no END, STAT or extra HPMP.

Subsequent occurrences of the now-dead own-PC target are skipped before hit,
RNG, activity-clock and effect work, because pure HP/MP CURE cannot defend a
dead target. Terminal CheckEquipSkill still runs, including after death or an
empty target list. The accepted receipt is consumed once with final core,
effects, checkpoint and complete transfer graph before cache/output publication.

## Supported state and authority

The death guard requires actual hydrated statistics, effects, local recovery
and cluster state. It excludes guild/riding/tactics, arena/tournament,
maps500–532, recalls, unsupported effects and full-graph companion records/items,
recalls/buffs or nonzero local ID. It does not erase a rich graph to make it fit.

Source constructors initialize live duel, carried GodBall and self actors to
zero/empty; complete original ENTER/RELEASE do not transfer those live fields.
Current native fresh admission/promotion and both dispatch paths have no
activation producer for these states, and supported effect projection excludes
TRANS restoration. This is a proof for current native-owned PostgreSQL PvE
objects only. Historical duel records/scores may remain nonempty and are not
active-duel authority. Fresh `app_world` admission has no companion-record/item
authority input; archival `legacy_game` companion rows are not silently imported
as an empty active graph. Actual full-transfer companion input is retained and
explicitly refused. Every future activation/import producer must extend this
actual-state contract before admitting affected death.

The existing070 query checks verified publication matched to current routing
and absence of battle-zone rows for the actual map/global key0 and GodBall rows
for the actual map in one statement. Missing/stale publication is unavailable,
not an empty-chart proof. Existing `death-context-runtime-grants.sql` supplies
the same SELECT views; living FINISH does not acquire this death query. Existing
071 origin/route and072 skill-context prerequisites remain required.

Current account/owner/Ready claim/session/epoch, actual physical Map cell,
catalog/inventory/checkpoint/fullgraph and FIFO receipt authority are preserved.
The handler checks PvE, matching the PostgreSQL startup gate. Existing per-packet
audiences and current actor/recipient/party guards remain around awaited output.
The conditional owner HPMP feeds actual HP0/MP0 to current party membership.
Unsupported death rolls back FINISH without exposing staged packets. It does
not refund an earlier committed ACT debit or replay uncertain writes/output.

## Shared internals and source review

- [Original server/client and admission audit](analysis.md) proves complete
  Defend/OnDie order, state bounds, dead-target filtering, equipment epilogue,
  native transient lifetime, client consumers and remaining dependencies.
- [Map implementation review](map-death.md) records production call sites,
  private helper contracts, current guards, persistence and deferred acceptance.
- `RequireNativeOrdinaryDeathState`, `RequireNativeDeathMapContext` and
  `ApplyNativeOrdinaryDeath` are private Map internals. They share actual proof
  and silent state mutation between DROP and FINISH without choosing packets or
  changing clocks.
- DROP retains its own DROP9, old-MP HPMP21, DIE5 and HP/MP recovery+5000 rules.
  Positive-template Defend updates last-attack and preserves recovery deadlines.
- The unchanged original client reads LASTHIT4 in DEFEND, assigns the four
  DWORD HPMP fields and consumes DIE to begin its existing death action/UI.
  No damage entry, quest/XP/item penalty or new wire field is fabricated.

Static review covered declarations/callers, source/client operation order,
current actor/map/graph proof, shared DROP behavior, packet widths/audiences,
receipt/core/effect/transfer writes and the complete production diff. Metadata
checks cover 210 unique CSV data rows, 12 columns, LF, seven new contracts,
unchanged `ALL.RESPONSIBILITIES=NOT_ANALYZED`, added Markdown targets and
whitespace. No compilation or executable acceptance is implied.

## Remaining work and Phase B acceptance

Active duels/DuelLose, ANGEL and rich companions, carried GodBall, castle/local,
arena/tournament/BOW/BR, recalls/self actors/transforms and general source-native
interoperability remain unimplemented here. Negative-template/foreign combat,
hardcoded128/229/3604, linked/repeated effects, richer statuses, full admission/AI/
rewards, secondary Map death-state cache convergence and source DB-only save
filtering/collapse remain OPEN. Current native absence is not implementation of
those original systems. Client grid output does not add a secondary state writer.

Later acceptance must cover actual ordinary/loop/ACT finalHP0, MP0 versus nonzero
internal MP, LASTHIT→wireHPMP0/0→DIE order and audiences, zero-MP versus silent
release, static800/802 and equipment cleanup, duplicate target skipping, current
party vitals, recovery clocks, dead save/transfer/entry and ordinary revival.
Exercise missing/stale070 and excluded actor/map contexts, transaction rollback,
stale/duplicate receipt, unknown commit and awaited transfer/recipient changes.
Recheck DROP packet/clock behavior with the unchanged original client.

No services or tests were run. Full functionality and compatibility with a real
original client remain unverified.
