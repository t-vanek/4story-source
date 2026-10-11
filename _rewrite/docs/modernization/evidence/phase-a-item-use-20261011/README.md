# Resource items, shared cooldowns and transfer confirmation — Phase A

Code `ff50e58`, following `6a91e4a`, local `main`. **INTEGRATED_UNVERIFIED** for
the bounded runtime paths below; the offline importer is **IMPLEMENTED_UNVERIFIED**.
The full Phase A gate remains open. No test, database migration/import, server,
client or container was run. No push, publication or external deployment.

## Source contract and connected implementation

- `Server/TMapSvr/CSHandler.cpp::OnCS_ITEMUSE_REQ`, `TMapSvr.cpp::UseItem`,
  `CSSender.cpp::SendCS_ITEMUSE_ACK` and `Lib/Own/TProtocol/include/NetCode.h`
  establish the ordinary HP26/MP27/MAXHP42/MAXMP43/GAINEXP81 branches, added to
  EXPBONUS56. Actual item definitions supply `wUseValue`, `dwSpeedInc`, delay
  group/delay, minimum level, use type and consumable flag. No gameplay values
  were invented. Source kinds outside this set still require their own branches.
- HP/MP only changes when below its derived maximum. Regular potions preserve
  the source DWORD addition then clamp; MAXHP/MAXMP set the maximum. A full
  resource returns IU_FULL=3 with the template delay and no debit/rearming.
  GAINEXP invokes the existing ordinary GainExp path, including level quest
  actions. Source target pairs are parsed and ignored for these six item kinds.
- The source orders HPMP/EXP effects, reuse arming, conditional one-item debit,
  success and USEITEM quest hooks. The native existing dispatcher/quest handler
  reaches `UseNativeItem` and the ownership-fenced transaction; all writes and
  the ordered response receipt commit together. Same bytes can be a later
  legitimate use; there is no byte-content deduplication. Unknown commits close
  the session with uncertain persistence rather than replaying the mutation.
- `CSSender.cpp::SendCS_HPMP_ACK` and `SSSender.cpp::SendMW_PARTYMANSTAT_ACK`
  establish the separate World party resource message. Its original WORD party,
  DWORD character, BYTE type/level and four DWORD resources are retained. This
  encoding does not certify complete native party admission/state/persistence.
- `SSHandler.cpp::OnDM_LOADCHAR_ACK` omits all tutorial timers and durations
  >=3600000ms, on both fresh load and transfer. `CSSender.cpp` includes all live
  groups in CHARINFO as BYTE count plus WORD group/DWORD remaining, even expired
  entries as zero. The independent `Client/TClient/CSHandler.cpp` reader agrees.
  `SSSender.cpp` includes only positive remaining groups for SAVE/RELEASEMAIN.
- Native `ItemCooldownState` carries local sample metadata outside the wire.
  `PersistenceSnapshot` samples remaining durations for ordinary requests,
  periodic/final checkpoints and handoff. Shared groups gate reuse. Fresh native
  writes compare old groups/durations; a core save cannot arm or lengthen one.
  The quest head fences a snapshot captured before newer item use. Graph-backed
  inventory/reagent validation permits only countdown, preserving the graph as
  authority instead of restoring its stale normalized projection.
- `ValidateSkillCast` calculates skill reuse on a relative clock. Quest/item
  observations are rebased for that calculation and restored to the live local
  clock before committing; a successful cast must not make the next request
  treat the quest's sample as a process-start timestamp.
- `6a91e4a` changes `OnMWTransferEnterReq` to attempt promotion once. After an
  exception, `ConfirmAcceptedTransfer` can return only an exact already-consumed
  receipt for the current target claim and checkpoint. Account/receipt locks
  serialize the observation with the original transaction. Missing, expired or
  prepared receipts cannot reach promotion DML. The server adds no client field.
- Prepared-transfer process recovery now reconstructs the entitlement/cooldown
  metadata required by the common save fence from the exact durable graph. It
  does not run fresh-load filtering or sample away any prepared duration.

## PostgreSQL and historical authority

Migration053 is additive and **not applied**. Migrations001–052 and both original
backups are unchanged. `app_world.item_cooldowns` uses unsigned WORD/DWORD ranges,
character FK and unique group ownership. The checkpoint function includes its
state for fresh ownership; contract2 still uses the complete graph. Explicit
runtime grants were added to the existing role script.

The inspected TGAME backup definitions show `TSaveItemUsed` inserts into
TTEMPITEMUSEDTABLE; TSaveCharDataStart clears staging and TSaveCharDataEnd replaces
TITEMUSEDTABLE from staging. The native item/checkpoint transaction replaces that
meaning without SQL Server runtime or a second staging save window. Historical
TITEMUSEDTABLE has a composite character/group PK and signed smallint/int storage.

`import_item_cooldowns.py` requires the complete pinned archive, verified import
receipt/value hash and exact injective character map. It preserves every source
row, including zero/long durations, with WORD/DWORD bit conversion. Original
load filters are a later runtime rule, not silent historical import loss. It
locks the target world's Map owner slots and refuses current sessions, existing
target rows/checkpoints, changed archive or repeated import. It creates no
characters. Connection uses existing PG environment settings. Neither this tool
nor an import/reconciliation helper was executed; only Python AST parsing ran.

## Explicit differences and remaining scope

- The earlier owned-template check closes source cross-item quest credit spoofing.
- Cooldowns use a 64-bit monotonic observation rather than DWORD end ticks;
  source tick wrap cannot make an outstanding delay immediately reusable.
  Offline and transfer transit time remain paused as in source remaining-time
  load. A backwards observation preserves the outstanding duration.
- A GAINEXP-triggered quest may mutate item stacks. Consumption re-reads the same
  instance/count/fingerprint afterwards; if that instance was deleted, the whole
  transaction rolls back rather than using a dangling original pointer.
- Unrepresentable CHARINFO counts/duplicate groups are rejected explicitly.
  Source backups/archives are retained even when runtime admission rejects data.
- General item kinds, action items, premium/PCBang/pets, tournament/deal/store
  authority, BOW/BR bonus-point branches, full World party integration, monster
  reward/loot and complete combat/effect/quest/economy/social systems remain open.
  Do not infer full consumable or full PostgreSQL-only server completion.
- Exact maintained-effect sub-tick persistence is still pending. The completed
  sampling here concerns item cooldowns, not every timer in the player graph.
- No executable client or assets are available. No supported binary build/hash
  or original-client compatibility result can be declared from client source.

## Build evidence and deferred acceptance

All six C++20 modern targets compile/link in Debug and Release with
BUILD_TESTING=OFF. `implementation.json` records exact commands, code/source/log
hashes, dependency paths and produced binary hashes. Initial build logs and the
final incremental clock-fix builds are separate; warnings remain in the logs.
Compiler success is not gameplay, PostgreSQL, protocol or fault-recovery evidence.
Earlier `fa4c282` evidence is unchanged. Tests executed: **0**.

Phase B scenarios, not executed:

1. Fresh and transferred owners: HP/MP below/full/max, min-level/dead/riding/wrap,
   secure code, template mismatch, same group across templates, zero/nonzero
   delay, success versus full-result packet order and bytes from original client.
2. Direct EXP across multiple levels with quest hooks adding/removing the consumed
   stack; atomic rollback on unsupported actions; repeated legitimate requests.
3. Relogin, periodic/final save, same/two-Map transfers, source load filters,
   expired live groups, >DWORD clock span, backward observation and timed quest
   plus accepted cast while an item cooldown remains active.
4. Old snapshot after reuse rearm, stale owner/generation, simultaneous requests,
   process crash and DB outage before/after commit; item/HP/MP/EXP/timers agree.
5. Migration upgrade/new install, role grants, exact archive import/reconciliation,
   signed boundary values, missing identity, duplicate group, active owner and
   refusal to overwrite recovery. Original archive values must stay unchanged.
6. Lost target promotion COMMIT reply: consumed receipt confirms without writes;
   prepared/absent/mismatched receipt closes without another promotion. Repeat
   source/target crashes at each transfer phase and verify authority recovery.

Continue Phase A with remaining native item kinds/premium/monster rewards and
the broader backlog in `full-rewrite-status.md`; Phase B/C are not authorized by
this compile-only evidence while the complete implementation gate remains open.
