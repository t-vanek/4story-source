# EXP entitlements and ITEMUSE — Phase A, unverified

Code: `fa4c282`, based on `39743c8`, local `main`. No push or external deployment.
This is a bounded **INTEGRATED_UNVERIFIED** increment. The complete Phase A gate
is still open; neither Phase B nor Phase C has started.

## Source contract and implementation

- `TPlayer.cpp::GainExp` grants18182/18183/18184 for the source previous-level
  loop indices, a half-hour TIME entitlement, effect923 for half an hour and918
  for three hours at the source threshold. Missing item definitions retain the
  source `continue`, including skipped quest/skill-point work for that iteration.
- `SetDuringItem/HangExpBuff/CheckDuringItem` distinguish remaining seconds from
  skill milliseconds and calendar expiry. LOADCHAR derives TIME/DAY from the
  item template, preserves the original expiry and skips tutorial entitlements.
  Timer order remains maintain expiry/AUTOEXP, then entitlement expiry/decrement.
  Delayed entitlement ticks debit at most ten whole seconds, as the source does.
  The inactive timer still updates the player's local clock observation.
- `TObjBase.cpp::ForceMaintain/UpdateBuffSkill` provide the self-target status
  fields and ordered collision removal. Only pinned pure903/918/923 templates
  are admitted here. `CheckMaintainSkill` rehangs903 after its expiry; removal
  emits SKILLEND followed by private CHARSTATINFO. Their zero-MP behavior remains
  distinct from weapon postures. This is not a general buff implementation.
- `SSHandler.cpp` discards restored STATUS_PCBANG visuals. Pending native entry
  suppresses903 in CHARINFO/ENTER and retires the old durable projection without
  an extra removal response; EnterMAP recreates it from the entitlement after
  AOI exchange. Entry owns the existing exclusive session lease through commit.
- `OnCS_ITEMUSE_REQ`, `TMapSvr.cpp::UseItem` and the independent client sender/
  receiver establish the actual seven-byte header and eight-byte reply. The
  header contains a WORD delay group despite the stale CSProtocol.h comment.
  The native dispatcher now handles ordinary zero-delay IT_USE/IK_EXPBONUS.
  It checks security, template/slot, full DWORD wrapping, riding, delay group,
  death, level and overlap. Target records are parsed but ignored by this source
  branch. Effects, a one-item debit, success and USEITEM quest events are ordered
  in a single owner-fenced transaction. Other item kinds are still unimplemented.
- `CSSender.cpp::SendCS_CHARINFO_ACK/SendCS_ENTER_ACK` use the same maintained
  field layout. The modern ENTER encoder previously always emitted count0.
  Both encoders now preserve those fields and sample current remaining time.
  No opcode, client field, text encoding or required client metadata was added.

Native code is in `Server/TMapSvrAsio/services/postgresql_map_during.cpp` and the
existing progression/checkpoint/load/transfer/handler paths. The production
Map CMake target includes it. [Exact files and hashes](implementation.json).

## PostgreSQL and historical data

Migration052 creates `app_world.exp_entitlements`, `app_global.during_items`, a
checkpoint state function/column and an import receipt. The source EXP staging
chain (`TSaveCharDataStart`, `TSaveExpItem`, `TSaveCharDataEnd`) becomes one atomic
slot replacement together with item/effect/quest changes. Ordinary core saves
cannot overwrite a newer slot. The quest ledger fences stale snapshots. After
primary transfer, the full transfer graph remains authoritative; normalized old
rows cannot recreate an already consumed bonus or inventory item.

`tools/database/import_during_items.py` is written and AST-parsed, **not run**.
Its required inputs are `--manifest`, `--world`, `--character-map`, `--account-map`
and `--source-timezone`. Phase B must supply a complete verified archival
manifest and reviewed exact identity maps, including explicit empty maps for an
empty source. It preserves signed WORD/DWORD bit patterns, rejects ambiguous
EXP slots and ambiguous/nonexistent local times, locks all native world owners
and refuses live sessions or existing state/recovery. It creates no identities.
Map grants include only the new necessary reads/writes/function execution.
Account premium records can be projected, but premium/PCBang gameplay admission
is explicitly rejected until its remaining contracts are implemented.

Two explicit modern differences require later acceptance: item-use quest credit
must name the actual owned template (the source allowed a different valid ID),
and new entitlement expiry retains seconds instead of SQL Server SMALLDATETIME
minute rounding. Historical timestamps keep their original representation in
the archive; their epoch transform requires the source timezone. Backups and
migrations001–051 were not edited. The new migration was not applied.

## Compilation only

Both commands completed with exit0 and `BUILD_TESTING=OFF`:

```sh
cmake --build build/phase-a-compile --target tloginsvr_asio tworldsvr_asio tmapsvr_asio tcontrolsvr_asio tpatchsvr_asio tlogsvr_asio -j2
cmake --build build/phase-a-quest-release --target tloginsvr_asio tworldsvr_asio tmapsvr_asio tcontrolsvr_asio tpatchsvr_asio tlogsvr_asio -j2
```

[Debug log](debug-build.log), [Release log](release-build.log),
[compiler/cache settings and binary hashes](implementation.json),
[static inspection](static-inspection.txt). Existing extracted Boost/SOCI/fmt/
spdlog/libpq dependencies under `/tmp/fourstory-phase-a-deps/usr` were reused.
The final commands used the existing configured build directories. Compiler
warnings are retained in the logs. A successful link is not gameplay evidence.

Tests executed: **0**. No CTest, pytest, sanitizers, fuzzing, benchmarks, database
connections/migrations/imports/reconciliation, server/client sessions or
Docker/Podman operations were performed. No complete cluster was started.
There is still no client executable/hash/assets or supported binary build claim.

## Remaining implementation and later acceptance

Continue `TMonster::OnDie/GetExp` bonus multiplication and party/owner rewards;
account premium/PCBang/day/pet behavior; other item-use kinds/cooldowns and
tournament/deal/store dependencies. Complete general effects, tutorial-wide
restoration, Soul Lottery and the other outstanding quest actions. Durable
effect remaining time still comes from the last committed tick: packet-time
sampling is not precise checkpoint/logout/handoff sampling. Interrupted transfer
time, complete World/social/economy/mode coverage and client acceptance remain.

Phase B scenarios: each source level threshold and nested reward; use item then
relogin/restart/transfer; stack1 versus stackN; TIME/DAY boundary and zero remain;
903 expiry/rehang ordering and full ENTER fields; secure/wrapped/dead/riding/low
level/overlap results; spoofed template IDs and malformed target counts; repeated
valid item requests after expiry; concurrent quest/cast/checkpoint/transfer;
commit acknowledgement loss and process/database failure; import ambiguity,
signed bits, timezone, repeated import, ownership and rollback. Derive packets
independently from the listed source/client routines. None of these scenarios
has been executed for this increment.
