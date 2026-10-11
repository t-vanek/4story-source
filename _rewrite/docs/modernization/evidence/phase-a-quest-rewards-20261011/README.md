# Phase A — quest rewards and progression, compile-only evidence

Code revision: `ea7c811a0a092c65cd9883303e78b5b7d24db4ba` on local `main`, starting
from `b56b255`. **INTEGRATED_UNVERIFIED within the scope below. Phase A is not
complete. Phase B/C have not started.** No push, migration, database connection,
server/client session or container run was performed for this increment.

## Implemented scope

The existing QUESTEXEC request now reaches the native QT_COMPLETE/QT_CRAFT reward
transaction. The original source distinguishes a running objective quest from a
fresh crafting action; repeating valid recipe bytes is not treated as a duplicate.
The same current-owner transaction writes item/skill/title/currency/quest changes
and a ledger head before exposing ordered owner, neighboring-cell and World
messages. A stale checkpoint cannot replay a later reward.

`CreateQuestRewardItems` reproduces source class filtering, selected/probability/
random item rewards, BYTE random accumulation and split stacks. Magic rewards
retain raw magic percentages, grade/gem/durability/refine and all six DWORD
extensions. Inventory room is checked before consuming turn-in items. Source
item and completion hooks run inside the same transaction, including failure
rollback. Non-item RM_RANDOM and CHGCLASS/TITLE/SOUL/POINT switch omissions are
source behavior, not newly invented success handlers.

GOLD/QUEST title rules preserve the strict gold comparison, selected promotion,
ANY-owned early return and category replacement. Skill grants/rank upgrades use
the source distinction. Ordinary GainExp(FALSE) implements soul progress,
threshold jumps, source previous-level iteration, skill points, quest hooks,
full-resource level-up, map level-limit kick and original response ordering.
MW_LEVELUP remains DWORD character + DWORD key + BYTE level. World checks the
registered current main sender; ready secondary Maps confirm the native committed
level and recheck their connection after the database await.

The native one-second scheduler also handles hydrated/transferred rank1
skills918/923. Their templates/data are checked against the pinned character
catalog: pure status63/67, static, positive, no weapon/posture/hide/attack-erase or
zero-MP side effects. Expiration precedes AUTOEXP; a delayed tick grants one
charge rather than multiplying missed intervals. Source nation-dependent monster
EXP rate is used. END precedes private CHARSTATINFO on expiry. Existing equipment,
posture and zero-MP paths preserve these pure statuses. General effects remain
explicitly unsupported.

## Required work and precise limits

- The level-benefit loop at indices8/18/28/... is still rejected when the backed
  benefit template exists. Source items18182/18183/18184 require character EXP
  entitlement persistence, account premium/PCBang state and skill903 before
  skills923/918 can be created by that path. The whole quest/timer transaction
  rolls back; no partial item/money award or successful completion is returned.
- Continue from TPlayer.cpp:252–390 (HangPremiumItem, CheckDuringItem,
  SetDuringItem, HangExpBuff), TPlayer.cpp:4395 (IsExpBenefit), TObjBase.cpp:2953
  (UpdateBuffSkill), :4397 (ForceMaintain) and :2521 (CheckMaintainSkill).
  TSKILLDATA has status41 for903, status63 for918 and status67 for923; values
  must come from the catalog. 918/923 creation at level-up is not yet integrated.
- CTBLExpItem reads **TEXPITEMTABLE by character**. CTBLDuringItem reads
  **TVIEW_DURINGITEMTABLE by account**. TSaveExpItem appends TTEMPEXPITEMTABLE;
  TSaveDuringItem delegates to TGLOBAL_GSP.TSaveTempDuringItem. Preserve the
  staged-save/finalization meaning with atomic native persistence; do not merge
  character and account ownership or invent historic entitlements.
- Remaining effect time currently comes from the last committed scheduler
  observation. Original GetRemainTick also samples at checkpoint/logout/handoff.
  Exact sub-tick sampling, delayed/interrupted handoff and crash clock boundaries
  still require implementation and later acceptance. Offline time earns no XP;
  local charge observations reset when a new player object is hydrated.
- World sends occur after commit. Failure closes the source client without
  rerunning rewards. Durable delivery/reconciliation and remaining native World
  social effects are not complete.
- Party/recall/guild statistics, Soul Lottery/effect820, monster vitality, general
  combat/effects, remaining quest actions/producers and the broader master scope
  remain open. Quest mail needs TRESERVEDPOST/TGetReservedPost plus item storage2;
  current carried-only native storage cannot silently stand in for attachments.

## PostgreSQL and data authority

Migration051 is additive and **written only**. Migrations001–050, source backups,
original server/client sources and old evidence were not changed. The extended
quest publisher checks the archival run, both pinned backup identities, shared
Map/character/actor table hashes and unambiguous reward/portal keys. Catalog
replacement uses the existing offline owner barrier.

TGetLimitedLevel and TGetNation reference TGLOBAL_GSP; the available authoritative
backup is named TGLOBAL_RAGEZONE. The operator must explicitly acknowledge this
mapping. For Phase B, after an actual verified full archive import, the written
command interface is:

```sh
python3 tools/database/activate_quest_catalog.py \
  --manifest /path/to/full-archive/manifest.json \
  --global-database-alias TGLOBAL_GSP=TGLOBAL_RAGEZONE
```

This command was **not executed**. No successful PostgreSQL migration, import,
reconciliation, role acceptance or PostgreSQL-only whole runtime is claimed.
New item expiration uses the documented UTC policy; historic timestamps remain
archival. Source undefined iterator endpoints, zero-capacity item loops and
unrepresentable numeric conversions are rejected explicitly.

## Compilation actually performed

GCC16.2.1, CMake4.3.0, Ninja1.13.2, C++20. Existing dependency prefix:
`/tmp/fourstory-phase-a-deps/usr` (Boost1.90, SOCI4.1.2, libpq18, fmt11.2,
spdlog1.15.3). No runtime dependency package was installed for these changes.
Release configuration, exit0:

```sh
cmake -S . -B build/phase-a-quest-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
  -DFOURSTORY_BUILD_LEGACY_TNETLIB=OFF -DFOURSTORY_ENABLE_POSTGRESQL=ON \
  -DCMAKE_PREFIX_PATH=/tmp/fourstory-phase-a-deps/usr \
  -DCMAKE_LIBRARY_PATH=/tmp/fourstory-phase-a-deps/usr/lib64 \
  -DCMAKE_CXX_FLAGS=-I/tmp/fourstory-phase-a-deps/usr/include \
  -DCMAKE_EXE_LINKER_FLAGS=-L/tmp/fourstory-phase-a-deps/usr/lib64 \
  -DCMAKE_DISABLE_FIND_PACKAGE_SOCI=ON
```

Both commands below completed with exit0 at the stated code revision:

```sh
cmake --build build/phase-a-compile --target tloginsvr_asio tworldsvr_asio tmapsvr_asio tcontrolsvr_asio tpatchsvr_asio tlogsvr_asio -j2
cmake --build build/phase-a-quest-release --target tloginsvr_asio tworldsvr_asio tmapsvr_asio tcontrolsvr_asio tpatchsvr_asio tlogsvr_asio -j2
```

Both caches have BUILD_TESTING=OFF. Debug reused the existing Phase A build tree.
[Debug log](debug-build-ea7c811.log), [Release log](release-build-ea7c811.log),
[initial full Release build](release-full-build.log) and
[Release configuration](release-configure.log) are retained. Existing SOCI
binding/deprecation and misleading-indentation warnings remain visible.
`implementation.json` records exact commands, binary hashes, scope and omissions.
Python AST parsing and `git diff --check` completed; the activation module was
not imported or executed. No extensive new test suite was written in Phase A.

## Phase B acceptance scenarios — recorded, not run

1. Independent source packet fixtures for completion versus crafting, forced
   errors, missing target, reward selection, random BYTE wrap and source defaults.
2. Full inventory before consumption; magic duplicates/missing definitions,
   six full extensions, split/merge placement, equipped turn-in and UTC expiry.
3. GOLD/QUEST title ordering, early-return selection, skill grant/upgrade byte
   boundaries, source no-op rewards and child reward-selection reset.
4. Soul interval/cap, ordinary and multi-level gain, BYTE/WORD/DWORD arithmetic,
   missing/undefined source levels and the still-guarded benefit boundary.
5. Timer expiry/charge boundary, delayed ticks, zero MP, equipment changes,
   active918 quest bonus, inactive/dead character, nation rates and nested events.
6. Concurrent stale snapshots, repeated valid crafting, interrupted commit,
   reconnect/restart/transfer, catalog change and failed World delivery.
7. Fresh install and additive schema upgrade with real PostgreSQL; complete
   source manifest/value/byte reconciliation and runtime-role permissions.
8. Corresponding unmodified original executable plus assets. Neither exists in
   the supplied environment; no exact client build/hash or compatibility result
   can be asserted.

**Tests run:0.** Unit/integration/regression/protocol/PG tests, CTest/pytest,
sanitizers/fuzz/benchmarks, migrations/imports, server/client processes and all
Docker/Podman/cluster operations were not run. Compilation establishes only that
the selected production targets build, not gameplay, persistence or compatibility.
