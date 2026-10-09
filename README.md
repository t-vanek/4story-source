# 4Story Emulator Server

**Development policy (owner instruction, 2026-10-09):** version changes only through
local commits on `main` until complete gameplay is finished. GitHub pushes,
publication and deployment are paused.

An incremental **C++20** reimplementation of the 4Story server cluster, targeting
**Linux containers and native PostgreSQL**. The compatibility goal is the original
client with unchanged packet layouts. The project is **in development**: selected
native database and protocol flows are verified, while complete gameplay and an
actual original-client session remain unfinished.

Original Windows sources live in `Server/T*Svr/`; the running modern implementations
live in `Server/T*SvrAsio/`. See the [implementation status](_rewrite/docs/modernization/README.md),
[capability matrix](_rewrite/docs/modernization/capability-matrix.md) and
[next implementation tasks](_rewrite/docs/modernization/next-steps.md).

<a id="overall-progress"></a>

## Verified functionality

Updated 2026-10-09. These scopes describe executed tests; historical handler counts
and completeness percentages are not evidence of native PostgreSQL feature parity.

| Area | Verified scope | Remaining work |
|---|---|---|
| Linux containers | Six installed daemons, health endpoints, Map → World DNS connection and graceful SIGTERM shutdown | Full persistent deployment acceptance and pinned build dependencies |
| Login | Native PostgreSQL authentication/session transactions, process ownership, duplicate protection, encrypted synthetic TCP and bounded email confirmation | Historical credential import, additional client profiles and original executable acceptance |
| Characters | Native creation/list/deletion, starter inventory, atomic item IDs, fresh world admission, client hydration, core checkpoints, logout and reconnect | Complete item/economy mutations and ancillary character state |
| Character statistics | Native 87-byte self-inspection and two-Map round-trip preservation using backup formulas and equipment | Advanced effects, special equipment branches, complete remote-player and original-client acceptance |
| Map content | Pinned source catalogs, real monster attributes, routing and actor catalogs loaded by the actual daemon | Complete spawn, movement validation, entity visibility and gameplay parity |
| Primary Map handoff | Movement-triggered transfer between two Maps, one database writer, exact state transfer, skill timers, return trips and recovery after process replacement | Full effect/quest/companion simulation, multiple-neighbor changes and special transfer branches |
| Inventory/equipment | Native carried moves/splits/merges and ordinary equip/unequip/swap; atomic displacement, IDs, HP/MP clamp and graph recovery | Warrior postures/active effects, advanced equipment, drops, timed/security/trade/store cases and full economy |
| Log | Native LP_LOG PostgreSQL migration, append-only runtime grants, exact raw bytes, native audit queries and process restart | Durable spool/reconciliation, closed-pool recovery, LP_CHAT and original tool/data acceptance |
| World, Control and Patch | Modern daemons and handler/transport tests; World handoff coordination | Remaining native repositories, social persistence and tool acceptance |

The latest increment adds **native equipment transactions** (migration **032**)
to the existing Map server. Ordinary equip/unequip/swap, eligibility, displaced
items and one-unit splits commit atomically with recalculated stats, HP/MP limits,
item IDs and recovery state. Responses retain the original item → EQUIP → MOVEITEM
→ CHARSTATINFO → HPMP → final MOVEITEM order. Actual two-Map tests preserve the
authoritative graph; disconnect during commit and subsequent reconnect retain the
new equipment.

**2,111 native database/network checks pass per configuration** in Debug,
ASan/UBSan and installed Release. Debug CTest has **188 actual passes**, eight
internal legacy fixture skips and seven explicit native skips (203 entries);
all **39 sanitizer suites** pass. Release uses installed daemons with the Debug
backend test. Six service health/DNS/SIGTERM checks also pass. See the
[equipment contract and evidence](_rewrite/docs/modernization/evidence/equipment-contract.json)
and [local build/startup procedure](deploy/README.md#native-equipment-transactions).
Current local image: `localhost/fourstory:postgresql-equipment`.

Warrior postures, active effects/cancellation, advanced equipment and complete
gameplay remain unfinished. Only uncompiled client sources are available; no
supported executable build/hash, assets or original-client acceptance is claimed.
Backups and applied migrations **001–032** are immutable; next migration **033**.

The preceding increment adds **safe offline actor-catalog upgrades** (migration 031).
A verified certificate lets the existing Map server restore old graph checkpoints
under the extended statistics catalog. Publication preserves original saved packets,
fences old writers and refuses live Maps or incompatible data. Tests use an actual
previous installed Map to create logout, crash and interrupted-transfer states.
See the [upgrade contract](_rewrite/docs/modernization/evidence/actor-transition-contract.json)
and [local upgrade procedure](deploy/README.md#offline-actor-catalog-transition).
That increment used `localhost/fourstory:postgresql-actor-transition`.
The upgrade suite passes **852 checks per configuration** in Debug, ASan/UBSan and
installed Release; the existing **1,915-check** native regression suite also passes
in each configuration. Complete gameplay and actual-client acceptance remain open.

The preceding increment adds **source-derived character statistics** to the existing
Map server: the original 87-byte inspection response, equipment attributes and
World relay handlers. Migration **030** extends the pinned actor catalog to four
tables and 9,773 backup rows. Native TCP checks cover grade/gem/aftermath, broken
weapons, reconnects and preservation across two Map transfers.

**1,915 native checks pass in each of Debug, ASan/UBSan and installed Release.**
Debug CTest has **188 actual passes** (203 entries, 15 fixture skips); all **39
sanitizer suites** pass. The Release run uses installed daemons and a Debug backend test. See the
[statistics contract](_rewrite/docs/modernization/evidence/character-statistics-contract.json)
and [native Map startup instructions](deploy/README.md#native-map-character-statistics).
Complete gameplay, advanced stat effects and original-client acceptance remain
unfinished. Only client sources are currently available; no executable build or
asset identity has been verified.

Migration **020** adds the primary transfer journal, authority epochs and complete
transfer-state checkpoints. A stale source cannot overwrite its successor's state.
Original CONNECT/CONREADY packets switch the primary connection without duplicate
CHARINFO. Tests cover rollback, interrupted transfers, source/target replacement,
logout/relogin and client disconnect during commit.

The latest native-primary-transfer verification records:

- **197 Debug CTest entries:** 183 passes, eight internal legacy fixture skips,
  six explicit native fixture skips and zero failures.
- **32 ASan/UBSan CTest entries:** all passed.
- **824 native database/network checks per configuration:** Debug, ASan/UBSan and
  installed Release, using isolated PostgreSQL and source-derived synthetic peers.
- Six installed daemon health/shutdown checks and the container DNS connection.

See the [transfer contract and test reports](_rewrite/docs/modernization/evidence/native-primary-transfer-contract.json)
and [installed image evidence](_rewrite/docs/modernization/evidence/native-primary-transfer-container-verification.json).
The tested local image is `localhost/fourstory:postgresql-native-primary`; it is
not a published registry image. The original client executable has not been run.

The subsequent branch consolidation also passes **200 Debug CTest entries**
(186 passes, eight internal skips and six explicit skips), plus **87 Control
checks across three ASan/UBSan suites**. See the [merge verification report](_rewrite/docs/modernization/evidence/main-consolidation.json)
for the integrated branch tips, container smoke results and remaining Control
persistence limits.

The preceding increment persists **skill cooldowns for fresh characters that never
transfer between Maps**. Migration 021 adds atomic core/skill receipts, exact retry
checks and crash recovery. Debug, ASan/UBSan and installed Release runs each pass 181 Map,
29 pool/TLS and 191 outer TCP checks, plus the existing World/two-Map matrix.
The previous installed image fails the new online-countdown assertion. See the
[skill checkpoint contract](_rewrite/docs/modernization/evidence/skill-checkpoints-contract.json)
for source evidence, deployment requirements and limitations. The full Debug suite
remains at 200 entries (186 passes, 14 skips); all 32 sanitizer entries pass.
That increment was verified in `localhost/fourstory:postgresql-skill-checkpoints`;
its six services pass health/DNS/SIGTERM checks. See [container evidence](_rewrite/docs/modernization/evidence/skill-checkpoints-container-verification.json).

The preceding resource increment enforces **learned-skill ownership and rank-based HP/MP
costs** in native Map casts. Definitions come from the pinned backup catalogs;
checks and resource deduction are atomic. Success packets carry the learned rank,
and forged or incomplete requests cannot charge a cast. Source skill 134 at rank 2
costs exactly 88 MP in the encrypted PostgreSQL test, including durable save.
See the [skill resource contract](_rewrite/docs/modernization/evidence/skill-costs-contract.json).
No schema migration or packet change is required. Verification passes 200 Debug
CTest entries (186 actual passes, 14 skips), all 33 ASan/UBSan entries, and 880
native database/network checks per Debug, sanitizer and installed Release run.
That increment used `localhost/fourstory:postgresql-skill-costs`;
all six daemons pass health/DNS/SIGTERM checks. See
[container evidence](_rewrite/docs/modernization/evidence/skill-costs-container-verification.json).
Active effects and combat damage remain unfinished.

The preceding increment generates **native cooldowns on normal skill use** from
learned rank, source attack delay, powered weapon slots and passive/item speed
rates. Same-kind timers extend atomically without shortening a longer timer.
Actual encrypted tests cover immediate-repeat rejection, expiration and durable
save. Debug, ASan/UBSan and installed Release each pass **906 database/network
checks**; 200 Debug CTest entries have 186 passes and 14 skips, and all 33 sanitizer
entries pass. The preceding image fails the new repeated-cast assertion.

That increment used `localhost/fourstory:postgresql-skill-timing`.
Its six daemons pass health/DNS/SIGTERM smoke. See the
[timing contract](_rewrite/docs/modernization/evidence/skill-timing-contract.json) and
[container evidence](_rewrite/docs/modernization/evidence/skill-timing-container-verification.json).
Buff-dependent timing, effect expiry, cancellation and the original client
executable remain pending; the loop extension is described below. Speed-dependent casts on buff-bearing transferred
characters close before mutation; no buff speed is guessed.

The preceding increment samples **current skill cooldowns during admission**.
CHARINFO subtracts time since character loading, and CONREADY samples again for
the initial PostgreSQL checkpoint. A short cooldown can expire during admission;
neither message restarts it or changes learned ranks. A missing runtime tracker or
an active timer for an unlearned skill refuse admission. Packet layouts stay
unchanged. See the [admission timer contract](_rewrite/docs/modernization/evidence/admission-timers-contract.json).

Verification: **186 Debug passes** (200 entries, 14 skips), **33 ASan/UBSan passes**
and **910 native database/network checks per Debug, sanitizer and installed Release
run**. The previous image fails the delayed-readiness regression. That increment used
`localhost/fourstory:postgresql-admission-timers`; all six
installed daemons pass health/DNS/SIGTERM checks. See
[container evidence](_rewrite/docs/modernization/evidence/admission-timers-container-verification.json).

The preceding increment implements **native repeated skill casting** with the original
`CS_LOOPSKILL_REQ` / `CS_LOOPSKILL_ACK` layouts. Loops use source `dwLoopDelay`,
check existing cooldowns before HP/MP costs, retain learned ranks and share the
same live timer with ordinary casts. They do not rearm other same-kind skills.
Item-free weapon checks honor the original mask and durability; missing active
prerequisites return the original result code. Consumable and active-effect
branches remain explicitly unsupported. See the
[loop contract](_rewrite/docs/modernization/evidence/skill-loop-contract.json).

Verification passes **186 Debug tests** (200 entries, 14 fixture skips), all
**33 ASan/UBSan tests** and **929 native database/network checks** in each Debug,
sanitizer and installed Release run. The previous image fails the loop-ACK
regression. That increment used `localhost/fourstory:postgresql-skill-loop`;
all six installed services pass health/DNS/SIGTERM smoke. See
[container evidence](_rewrite/docs/modernization/evidence/skill-loop-container-verification.json).

The preceding implementation adds **ordinary native cast requirements**: source map
restriction, the distinct previous-active-effect requirement and non-consuming
weapon eligibility. Rejection codes and ordering follow the original server.
An unsuitable weapon on normal use retains the source own/shared-kind cooldowns
without charging HP/MP; the loop branch keeps its separate behavior. Unsupported
consumable or buff-dependent work closes before mutation. See the
[cast requirement contract](_rewrite/docs/modernization/evidence/skill-gates-contract.json).

This increment passes **186 Debug tests** (200 entries, 14 fixture skips), all
**33 ASan/UBSan suites** and **933 native database/network checks** in each Debug,
sanitizer and installed Release run. The previous installed image fails the map
restriction regression. That increment used `localhost/fourstory:postgresql-skill-gates`; all six services pass health/DNS/SIGTERM smoke. See
[container evidence](_rewrite/docs/modernization/evidence/skill-gates-container-verification.json).

An earlier increment adds **durable single-reagent consumption** for ordinary
and repeated casts on fresh primary characters. Migration **022** commits the item
count or deletion, HP/MP, timers and a receipt in one PostgreSQL transaction before
the original inventory and success packets. Checkpoint coordination prevents an
older save from undoing a cast; uncertain outcomes retain the reservation for
recovery. Depleted stacks reject further casts without another charge.

Verification: **186 Debug passes** (200 entries, 14 fixture skips), **33 ASan/UBSan
passes** and **973 native database/network checks per Debug, sanitizer and installed
Release run**. That increment used `localhost/fourstory:postgresql-skill-reagents`;
six services pass health/DNS/SIGTERM checks. See the
[reagent contract](_rewrite/docs/modernization/evidence/skill-reagents-contract.json) and
[container evidence](_rewrite/docs/modernization/evidence/skill-reagents-container-verification.json).
Migration 022 and updated Map grants introduced that storage contract.
The original client executable has not been run.

An earlier increment extends **durable reagent consumption to transferred
characters**. Migration **023** records hashes of the complete state before and
after a cast.
The current primary server validates the exact item and authority epoch, then
commits the inventory change, character core, cooldowns and audit receipt together.
The updated graph survives transfer back, relogin and process recovery, including
last-item deletion. Full unsigned item IDs and original item fields are preserved.

After transfer, the complete checkpoint is authoritative: this path leaves stale
normalized item/skill rows untouched and never uses them to restore consumed items.
Apply migrations through **023** before deploying these binaries; existing Map
runtime grants suffice. See the
[graph reagent contract](_rewrite/docs/modernization/evidence/graph-reagents-contract.json).
Equipped reagents, premium-item overrides, inventory movement and full combat remain pending.
Verification uses encrypted test peers derived from the original source; the
original client executable has not been run.

Verification passes **186 Debug tests** (200 entries, 14 fixture skips), all
**33 ASan/UBSan suites** and **1,046 native database/network checks** in each Debug,
sanitizer and installed Release run. All six container services pass health,
DNS and SIGTERM checks. That increment used
`localhost/fourstory:postgresql-graph-reagents`. See
[container evidence](_rewrite/docs/modernization/evidence/graph-reagents-container-verification.json).

The previous increment adds **source multi-attack target expansion** to native
ordinary and loop casts. Pinned `TSKILLDATA` and the learned rank determine the
hit budget; random per-target duplication and first-target padding reproduce the
original target order. All valid ranks of the three recovered templates (324,
412, 1407) require at most seven hits. Larger derived budgets above 16 fail closed.

Expanded ammunition uses the existing atomic per-bag stack transaction. The
locked database validator recomputes its full hit budget from authoritative learned
state; a transferred graph remains authoritative over stale normalized skill rows.
Resources and cooldowns apply once per cast, while ammunition is charged per
expanded hit. Migration **026** records `hit_mode=expanded` alongside the grouped
item receipts; existing receipts retain `direct`.

Verification passes **186 Debug tests** (200 entries, 14 fixture skips), all
**33 ASan/UBSan suites** and **1,589 native database/network checks** in each
Debug, sanitizer and installed Release run. Twelve migration-upgrade checks
preserve existing receipts; the previous image fails the new expansion regression.
All six container services pass health, DNS and SIGTERM checks. The local image is
`localhost/fourstory:postgresql-multi-attack` (then also `:main`), running as
UID/GID 10001:10001. The installed test uses Release daemons with verified Debug
backend/pool integration executables. See
[container evidence](_rewrite/docs/modernization/evidence/multi-attack-container-verification.json).

See the [multi-attack contract](_rewrite/docs/modernization/evidence/multi-attack-contract.json).
Complete combat damage/target/AOI rules, active effects, equipment mutation and
original-client executable acceptance remain pending. The random outcome range is
preserved; the original Windows process seed sequence is not reproduced.

The preceding increment adds **native inventory moves**: whole stacks can move to
an empty carried-bag slot, and different item templates can swap positions within
or between bags. IDs, counts, raw attributes and client descriptors are conserved.
The source five-byte request and DEL/ADD or ordered UPDATE responses are retained;
MOVEITEM success follows confirmed commit. Bag capacities come from the backup
(default bag template 3 has 16 slots; template 4 has four).

Migration **027** makes the existing unique item-slot constraint deferrable inside
a swap and adds grouped movement receipts. Fresh rows or the complete transferred
checkpoint commit atomically with core and timers. Recovered graphs stay authoritative
over stale normalized item positions. Apply migration 027 and the updated Map grants.

Verification passes **186 Debug tests** (200 entries, 14 fixture skips), all
**33 ASan/UBSan suites** and **1,730 native database/network checks** in each
Debug, sanitizer and installed Release run. Thirteen migration-upgrade checks
verify preservation, atomic swaps and receipt constraints; the previous image
fails the new inventory packet regression. All six container services pass
health, DNS and SIGTERM checks. The local image is
`localhost/fourstory:postgresql-inventory-moves` (then also `:main`), running as
UID/GID 10001:10001. The installed test uses Release daemons with verified Debug
backend/pool integration executables. See
[container evidence](_rewrite/docs/modernization/evidence/inventory-moves-container-verification.json).

[Inventory move contract](_rewrite/docs/modernization/evidence/inventory-moves-contract.json)
records the preceding scope and tests. The stack increment below adds split/merge
and same-template unequal swaps. Item dropping, timed bags, secured inventories
and equipment changes remain unsupported.
Original-client executable acceptance and complete gameplay remain unfinished.

The current increment adds **native stack splitting and merging** in the existing
MOVEITEM handler. Raw CTItem equality decides merge versus full-stack swap, even
for the same template. Splits atomically allocate a new ID from the same world
counter as character creation; merges retain destination identity and clamp to
pinned stack capacity. A full destination preserves the original successful no-op
packet sequence. Original UPDATE/DEL/ADD ordering follows confirmed commit.

Migration **028** adds paired stack receipts without rewriting earlier migrations
or historical data. Fresh rows or complete transfer graphs commit with the allocator,
core, timers and receipts. Actual encrypted tests split/move on the second Map and
merge after returning to the first. Concurrent split, late rollback, disconnect
and recovery tests preserve quantities and IDs.

Verification passes **186 Debug tests** (200 entries: eight internal legacy skips
and six explicit native fixture skips), **34 ASan/UBSan suites**, and **1,861
native database/network checks** separately in Debug, sanitizer and installed
Release runs. Seventeen migration-upgrade checks preserve existing rows/receipts
and verify stack constraints. The prior image fails the new split request after
a real primary transfer. All six services pass health/DNS/SIGTERM smoke.
The local image is `localhost/fourstory:postgresql-inventory-stacks` (also `:main`),
UID/GID 10001:10001. Installed verification uses Release daemons with verified
Debug backend/pool integration executables.
See the [stack contract](_rewrite/docs/modernization/evidence/inventory-stacks-contract.json)
and [installed image evidence](_rewrite/docs/modernization/evidence/inventory-stacks-container-verification.json).

**No original client binary build is currently certified.** The owner has only
uncompiled client source, without game data. Protocol source version `0x2918`
does not identify an executable release. The [client acceptance gate](_rewrite/docs/modernization/client-acceptance.md)
records the missing executable/data/environment and all blocked scenarios. Complete
gameplay, remaining service persistence and tools remain unfinished; no overall
completion percentage or full compatibility claim is made.

## Database authority

`TGAME_RAGEZONE.bak` and `TGLOBAL_RAGEZONE.bak` are the immutable historical source
of truth. Keep their hashes and recovered values unchanged. When server contracts
need adaptation, change the derived PostgreSQL schema through a new migration.

- `legacy_game`, `legacy_global` and `legacy_game_tgame` preserve recovered data.
- `content` and the compatibility views expose explicit, versioned projections.
- `app_global` and `app_world` own mutable application and operational state.
- Applied migrations **001–028 are immutable**; the next schema change starts at 029.
- Backups, credentials and private extraction output stay outside Git. Historical
  accounts, player records and missing combat values are never invented.

See [database setup and contracts](database/README.md),
[forensic reconstruction](_rewrite/docs/database-reconstruction/README.md) and
[preservation checks](_rewrite/docs/modernization/evidence/preservation.json).
SQL Server is unnecessary for the verified native runtime paths. ODBC comparison
paths and unported services remain in the repository and runtime image.

## Linux quick start

Install Git LFS when working with the original client/library assets:

```sh
git lfs install --local
git lfs pull
```

Run the six-service development topology with Docker Compose:

```sh
docker compose up --build -d
docker compose ps
docker compose logs -f
docker compose down
```

The committed Compose configuration uses in-memory services without a database.
It is a process/transport smoke environment; native PostgreSQL requires the
migrations, activated catalogs, dedicated database roles and private configuration
in [deploy/README.md](deploy/README.md). Keep deployment secrets in ignored
configuration files or private environment files.

Build and verify a runtime image with rootless Podman:

```sh
podman build --target runtime --build-arg BUILD_JOBS=2 \
  -t localhost/fourstory:linux .
python3 tools/container_smoke.py --engine podman \
  --image localhost/fourstory:linux
```

Each container runs one daemon as UID/GID `10001:10001`. The binaries are installed
under `/opt/fourstory/bin/`: `tloginsvr_asio`, `tmapsvr_asio`, `tworldsvr_asio`,
`tcontrolsvr_asio`, `tpatchsvr_asio` and `tlogsvr_asio`.

## Build and test

For a native Ubuntu 24.04 development environment:

```sh
sudo apt-get install build-essential cmake ninja-build pkg-config \
  libboost-system-dev libssl-dev openssl libspdlog-dev \
  libtomlplusplus-dev libsoci-dev unixodbc-dev libpq-dev
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
```

`linux-release` builds the servers without test targets. `linux-asan` builds and
runs the selected sanitizer suites:

```sh
cmake --preset linux-asan
cmake --build --preset linux-asan
ctest --preset linux-asan
```

Native integration tests need an owned disposable PostgreSQL lab. The
[deployment guide](deploy/README.md) documents `run_native_verification.py`, pinned
reference manifests and role grants. Tests needing absent fixtures are skipped;
a passing default CTest run does not imply that those database checks executed.

The [Linux CI workflow](.github/workflows/linux-container.yml) builds the container,
runs tests and verifies synthetic PostgreSQL Login/pool flows. Local evidence does
not establish the outcome of a GitHub Actions run.

The Windows CMake/vcpkg build path remains available:

```powershell
$env:VCPKG_ROOT = "C:\vcpkg"
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release
```

The latest native PostgreSQL runtime is verified on Linux; equivalent Windows
verification is pending. The stack uses Boost.Asio coroutines, SOCI/libpq,
OpenSSL, spdlog and toml++. Linux images use Ubuntu packages. The vcpkg baseline,
container base digest and package versions still need release-level pinning.

## Repository and documentation

| Location | Purpose |
|---|---|
| `Server/T*Svr/`, `Client/` | Original server/client behavior and protocol references |
| `Server/T*SvrAsio/` | Modern server implementations and component tests |
| `Lib/Own/FourStoryCommon/` | Shared database, cluster, logging and operations infrastructure |
| `Lib/Own/TNetLib/`, `Lib/Own/TProtocol/` | Transport, packet codec and protocol definitions |
| `database/postgresql/` | Numbered migrations and database compatibility contracts |
| `deploy/`, `Dockerfile`, `compose.yaml` | Linux deployment, sample configuration and role grants |
| `tools/database/` | Extraction, migration and isolated integration verification |
| `_rewrite/docs/modernization/` | Current status, protocol traceability, evidence and continuation |
| `_rewrite/docs/database-reconstruction/` | Backup analysis, provenance and unresolved historical contracts |

Component references: [Login](Server/TLoginSvrAsio/README.md),
[Map](Server/TMapSvrAsio/README.md), [World](Server/TWorldSvrAsio/README.md),
[Control](Server/TControlSvrAsio/README.md), [Patch](Server/TPatchSvrAsio/README.md),
[Log](Server/TLogSvrAsio/README.md) and [shared infrastructure](Lib/Own/FourStoryCommon/README.md).
The [older patch catalog](_rewrite/docs/PATCH_README.md) and
[legacy-to-modern changelog](_rewrite/docs/CHANGELOG_LEGACY_TO_MODERN.md) retain
historical context; current acceptance is tracked in the modernization documents.

Next priorities are active buff/timer semantics, skill cancellation and consumable inventory,
complete entity visibility, transactional item/economy
operations, native social persistence and execution of the supported original
client. Full feature parity is not yet established.

## License

See the original notices in `Server/TLoginSvr/` and the other legacy subtrees.
No separate project-wide license grant is established by this README.
