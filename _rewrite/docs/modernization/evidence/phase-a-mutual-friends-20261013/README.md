# Phase A — atomic mutual friendship in the existing World adapter

Code `11e0df1` / `dd07164`, based on `5fe42f8`, local main only.
Both acceptance paths are **INTEGRATED_UNVERIFIED**. Complete native social
persistence and Phase A remain incomplete. No test/database/runtime ran.

Original `OnMW_FRIENDASK_ACK` (pending entries on both sides) and
`OnMW_FRIENDREPLY_ACK` send two DM_FRIENDINSERT operations. Backup TFriendInsert
inserts each missing directed edge with group0, retaining an existing edge's
group. The previous modern handlers changed memory first and ignored two
independent boolean writes, allowing a successful reply after partial persistence.

The existing IFriendRepository now exposes one required InsertMutualFriends
operation, implemented by the actual SOCI repository using one session/RAII
transaction. It preserves source existing-edge groups, fixed signed32 identity
bits and canonical unsigned pair order. Both runtime handlers await that commit
before changing either cached side or sending the original success packet.
Registered ready primary identity is checked before persistence and after await;
registry pointers/session keys are rechecked before publishing cached state.
These transport checks are not a substitute for the pending native World owner
and database authority fences.

Any mutual persistence failure, including unknown COMMIT, poisons that repository
instance's subsequent writes, emits a source/credential-safe critical diagnostic
and stops actual World main with exit1. There is no automatic replay or invented
error/success result. Fresh startup must inspect durable directed rows before
reconciliation. The deliberate modern changes are atomic pair persistence,
commit-before-cache/reply, primary checks and fail-stop instead of best-effort
continuation. Client layouts and existing sender functions remain unchanged.

The pre-existing fake adapter implements the new interface under one mutex using
copy/swap, preserving its existing directed-row/idempotence shape. It remains a
test adapter; no new production selection/fallback or test assertions were added.

**Limits:** the SOCI implementation still names the existing schema's tables.
There is no new PostgreSQL friend/group/soulmate schema, historical importer or
World process ownership implementation in this increment. Other group/soulmate/
erase handlers still have legacy best-effort behavior; LoadForChar still catches
errors and returns an empty graph. Those are required native replacement work,
not functioning native substitutes. Cross-process concurrency, stale World
writers and complete relation recovery/audit remain unresolved. Full World still
cannot be declared PostgreSQL-only. No original executable/assets are available.

## Compilation and deferred acceptance

```sh
cmake --build build/phase-a-compile --target tworldsvr_asio -j1
cmake --build build/phase-a-quest-release --target tworldsvr_asio -j1
```

Debug and Release compiled/linked with C++20, PostgreSQL enabled and
BUILD_TESTING=OFF; final commands exited0. Logs include compilation of the
changed existing fake adapter. Older warnings remain. No executable was run.
Current Map code is unchanged from the preceding mail compilation scope.
[Exact commit/file hashes and counts](implementation.json) identify this evidence.

Phase B must check both accept branches, existing/new edge/group combinations,
first/second-write failure, unknown COMMIT/restart, no partial row/cache/ACK,
primary replacement/key/registry/logout/handoff during await, correct response
order and parallel Worlds/social operations. Existing regression fixtures must
represent registered ready primaries; do not weaken assertions. Tests, real
PostgreSQL, sanitizers and original-client acceptance remain unexecuted.

Migrations001–065, backups, original source/client files and older evidence are
unchanged. Next free migration is066. Continue complete native World ownership,
friend/group/soulmate schema/imports and typed authorized transactions, all other
World repositories/bootstrap, then remaining full gameplay scope. Do not treat
this shared-adapter repair as completing the PostgreSQL port of social systems.
