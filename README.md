# 4Story Emulator Server

An incremental **C++20** reimplementation of the 4Story server cluster, targeting
**Linux containers and native PostgreSQL**. The compatibility goal is the original
client with unchanged packet layouts. The project is **in development**: selected
native database and protocol flows are verified, while complete gameplay and an
actual original-client session remain unfinished.

Original Windows sources live in `Server/T*Svr/`; the running modern implementations
live in `Server/T*SvrAsio/`. See the [implementation status](_rewrite/docs/modernization/README.md),
[capability matrix](_rewrite/docs/modernization/capability-matrix.md) and
[next implementation tasks](_rewrite/docs/modernization/next-steps.md).

## Verified functionality

Updated 2026-10-09. These scopes describe executed tests; historical handler counts
and completeness percentages are not evidence of native PostgreSQL feature parity.

| Area | Verified scope | Remaining work |
|---|---|---|
| Linux containers | Six installed daemons, health endpoints, Map → World DNS connection and graceful SIGTERM shutdown | Full persistent deployment acceptance and pinned build dependencies |
| Login | Native PostgreSQL authentication/session transactions, process ownership, duplicate protection, encrypted synthetic TCP and bounded email confirmation | Historical credential import, additional client profiles and original executable acceptance |
| Characters | Native creation/list/deletion, starter inventory, atomic item IDs, fresh world admission, client hydration, core checkpoints, logout and reconnect | Complete item/economy mutations and ancillary character state |
| Map content | Pinned source catalogs, real monster attributes, routing and actor catalogs loaded by the actual daemon | Complete spawn, movement validation, entity visibility and gameplay parity |
| Primary Map handoff | Movement-triggered transfer between two Maps, one database writer, exact state transfer, skill timers, return trips and recovery after process replacement | Full effect/quest/companion simulation, multiple-neighbor changes and special transfer branches |
| World, Control, Patch and Log | Modern daemons and handler/transport tests; World handoff coordination | Remaining native repositories, social persistence and tool acceptance |

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

## Database authority

`TGAME_RAGEZONE.bak` and `TGLOBAL_RAGEZONE.bak` are the immutable historical source
of truth. Keep their hashes and recovered values unchanged. When server contracts
need adaptation, change the derived PostgreSQL schema through a new migration.

- `legacy_game`, `legacy_global` and `legacy_game_tgame` preserve recovered data.
- `content` and the compatibility views expose explicit, versioned projections.
- `app_global` and `app_world` own mutable application and operational state.
- Applied migrations **001–020 are immutable**; the next schema change starts at 021.
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

Next priorities are runtime timer persistence for characters that never transfer,
complete entity visibility and active gameplay state, transactional item/economy
operations, native social persistence and execution of the supported original
client. Full feature parity is not yet established.

## License

See the original notices in `Server/TLoginSvr/` and the other legacy subtrees.
No separate project-wide license grant is established by this README.
