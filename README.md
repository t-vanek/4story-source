# 4Story Emulator Server

A modern, open re-implementation of the **4Story** MMORPG server cluster.
The project hosts a private-server emulator that speaks the original game
client's wire protocol byte-for-byte, so a shipped legacy 4Story client
can connect to it without any binary patching and walk the full
LOGIN → CHARLIST → CREATECHAR → START round-trip.

The original Win32 / ATL / IOCP server sources are kept in the tree
under `Server/T*Svr/` as the authoritative reference for shipped
behavior. The emulator binaries live next to them under
`Server/T*SvrAsio/` and replace the legacy daemons one component at a
time while staying wire-compatible.

The emulator targets preservation, study, and private hosting of a game
that is no longer commercially operated — the goal is a server cluster
that boots on commodity hardware, builds cleanly on modern toolchains,
and can be reasoned about without the original Win32-only build farm.

## Why re-write the emulator

The shipped server was tightly coupled to a 2000s-era Windows stack:
ATL/COM, Win32 IOCP completion ports, MFC dialog ops UIs, per-server
duplicated SQL access layers, and a hand-rolled threading model with
implicit lock ordering. That code still runs, but it is hard to host
outside its original environment, hard to instrument, and hard to extend.

Re-writing the emulator on a modern foundation buys us:

* **Portability** — the cluster builds on both MSVC 2022 and Linux
  (GCC/Clang) from the same CMake. No more "Windows-only build server."
* **Readability** — Boost.Asio coroutines (`co_await`) replace IOCP
  callback chains; the handler flow reads top-to-bottom instead of
  being scattered across `OnRead` / `OnWrite` / completion routines.
* **Memory safety** — RAII sessions, `std::span`/`std::string_view`
  framing, and bounded buffers replace raw `char*` arithmetic. The
  legacy `CS_LOGIN_REQ` trailing XOR/add checksum is now actually
  enforced server-side.
* **Shared infrastructure** — SOCI connection pooling, structured
  audit logging, SMTP, the admin shell, health probes, and rate
  limiting are extracted once into `Lib/Own/FourStoryCommon` instead
  of being copy-pasted per server.
* **Testability** — handler dispatch, the wire codec, and per-service
  business logic run in-process against `Fake*` services under CTest.
  The SOCI integration suites skip cleanly when no DB is configured,
  so CI passes without a database.
* **Operability** — TOML config (`toml++`) replaces ad-hoc INI parsing;
  `spdlog` gives structured logs and a dedicated audit channel; the
  schema validator fails fast on DB drift instead of crashing mid-session.
* **No anti-cheat lock-in** — HShield / XTrap / NPGame / `HwidManagerSvr`
  are intentionally out of scope. The emulator does not call home to
  any third-party anti-cheat service.
* **Wire compatibility, not behavior drift** — every ACK structure is
  reproduced byte-for-byte against the legacy `CSSender.cpp` /
  `Sender.cpp` / `LogPacket.h` references, so the original client
  binary stays the source of truth for what the server must emit.

## Technology stack

* **C++20** with Boost.Asio stackless coroutines (`co_await`, `async_*`)
* **SOCI 4.x** with the ODBC backend → MS SQL Server
  (PostgreSQL branches are kept but disabled)
* **OpenSSL** EVP for RC4; **libbcrypt** (vendored at
  `Lib/3rdParty/bcrypt/`) for password hashing
* **spdlog** for structured logging and the audit channel
* **toml++** for configuration
* **vcpkg** manifest mode (`vcpkg.json`) for dependency pinning
* **CMake 3.20+** as the single build system across MSVC and Linux
* **CTest** with in-process integration tests against test fakes; SOCI
  suites skip cleanly when no DB is configured

Builds with MSVC 2022 + vcpkg on Windows; the same CMake also builds on
Linux against distro packages (`libsoci-dev`, `unixodbc-dev`,
`libspdlog-dev`, `libtomlplusplus-dev`, `libssl-dev`, `libboost-all-dev`).

## Repository layout

```
4Story_5.0_Source/
├── CMakeLists.txt                  # root — picks up vcpkg + adds subprojects
├── vcpkg.json                      # manifest mode dependency pin
├── Client/                         # legacy TClient sources (unmodified)
├── Lib/
│   ├── 3rdParty/bcrypt/            # vendored libbcrypt (no working vcpkg port)
│   └── Own/
│       ├── TNetLib/                # wire codec + AsioSession (modernized)
│       ├── TProtocol/              # wire structs / MessageId enum (shared)
│       └── FourStoryCommon/        # shared infra: SOCI pool, audit, smtp, ops
├── Server/
│   ├── TLoginSvr/                  # legacy login (reference, unmodified)
│   ├── TLoginSvrAsio/              # emulator login server
│   ├── TPatchSvr/                  # legacy patch (reference, unmodified)
│   ├── TPatchSvrAsio/              # emulator patch metadata server
│   ├── TLogSvr/                    # legacy audit log collector (unmodified)
│   ├── TLogSvrAsio/                # emulator audit UDP collector
│   ├── TWorldSvr/                  # legacy world server (reference)
│   └── TControlSvr/                # legacy control server (reference)
├── _rewrite/docs/                  # plan + analysis documents
└── tools/                          # dev scripts
```

Each emulator component has its own README with the full handler
mapping, configuration schema, and bring-up notes:

* [`Server/TLoginSvrAsio/README.md`](Server/TLoginSvrAsio/README.md)
* [`Server/TPatchSvrAsio/README.md`](Server/TPatchSvrAsio/README.md)
* [`Server/TLogSvrAsio/README.md`](Server/TLogSvrAsio/README.md)
* [`Lib/Own/FourStoryCommon/README.md`](Lib/Own/FourStoryCommon/README.md)

## Build

### Windows (MSVC 2022 + vcpkg)

```powershell
$env:VCPKG_ROOT = "C:\vcpkg"
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
      -DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake
cmake --build build --config Release
```

First configure pulls and builds Boost, OpenSSL, SOCI[odbc], spdlog,
and toml++ (≈30 min). Subsequent configures are incremental.

Targets produced under `build/bin/Release/`:

* `tloginsvr_asio.exe` — login + lobby + char flow
* `tpatchsvr_asio.exe` — patch metadata
* `tlogsvr_asio.exe`   — audit UDP collector

### Linux (GCC/Clang + distro packages)

```sh
sudo apt install cmake g++ libboost-all-dev libssl-dev \
                 libsoci-dev unixodbc-dev \
                 libspdlog-dev libtomlplusplus-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## Testing

```sh
ctest --test-dir build -C Release --output-on-failure
```

In-process tests (handler dispatch, wire codec, per-service business
logic against `Fake*` services) run without a DB. The SOCI integration
suites under `Server/TLoginSvrAsio/tests/test_soci_*` skip automatically
when `TLOGINSVR_TEST_MSSQL_CONN` is unset, so CI without a DB still
passes. Set the env var to a connection string to run them:

```sh
export TLOGINSVR_TEST_MSSQL_CONN="DSN=4story;UID=sa;PWD=…"
ctest --test-dir build -C Release --output-on-failure
```

## Documentation index

* [`_rewrite/docs/MODERNIZATION_PLAN.md`](_rewrite/docs/MODERNIZATION_PLAN.md)
  — cluster-wide phased roadmap
* [`_rewrite/docs/LOGIN_SERVER_COMPARISON.md`](_rewrite/docs/LOGIN_SERVER_COMPARISON.md)
  — handler-by-handler legacy vs emulator parity audit
* [`_rewrite/docs/PROTOCOL.md`](_rewrite/docs/PROTOCOL.md) — wire codec
  reference (header layout, RC4 keying, checksum algorithms)
* [`_rewrite/docs/SCHEMA.md`](_rewrite/docs/SCHEMA.md) — DB column
  catalog the emulator services read/write
* [`_rewrite/docs/GAP_ANALYSIS.md`](_rewrite/docs/GAP_ANALYSIS.md) —
  what's intentionally not emulated (and why)
* [`_rewrite/docs/CONTROL_SERVER_PORT_PLAN.md`](_rewrite/docs/CONTROL_SERVER_PORT_PLAN.md)
  — design notes for the upcoming control-server port
* [`_rewrite/docs/CHANGELOG_LEGACY_TO_MODERN.md`](_rewrite/docs/CHANGELOG_LEGACY_TO_MODERN.md)
  — behavioral diff between the shipped server and the emulator
* [`_rewrite/docs/CLIENT_BUILD_NOTES.md`](_rewrite/docs/CLIENT_BUILD_NOTES.md)
  — notes on rebuilding the legacy client from source

## Roadmap

Near-term (cluster edge):

* **Control server (`TControlSvr` → `TControlSvrAsio`)** — port the
  inter-server routing/orchestration daemon onto the shared
  `FourStoryCommon` infra. Design captured in
  `_rewrite/docs/CONTROL_SERVER_PORT_PLAN.md`.
* **Operator tooling** — round out the admin shell (account lookup,
  ban/unban, session kick) and expose a minimal HTTP health/metrics
  endpoint so the cluster is observable without RDP.

Mid-term (gameplay surface):

* **World server (`TWorldSvr` → `TWorldSvrAsio`)** — the big one. Port
  zone hosting, mob AI, party/guild, trade, and inventory persistence.
  Until this lands, the legacy `TWorldSvr` binary remains canonical
  for in-game behavior.
* **Map data pipeline** — reproducible extraction of map / NPC / drop
  tables from the shipped data files, so world content can be
  regenerated rather than restored from a binary backup.

Longer-term (preservation):

* **Linux production deployment** — the code already builds on Linux;
  the goal is a fully Linux-hosted cluster (systemd units, container
  images, no Wine for the auxiliary tools).
* **Schema migration story** — formalize the additive-only migration
  flow so community deployments can upgrade between emulator versions
  without hand-editing tables.
* **Reference dataset** — publish a minimal, lawful starter DB seed so
  new operators don't need access to an original `.bak` to stand the
  cluster up.

Explicitly **out of scope**:

* HShield / XTrap / NPGame / `HwidManagerSvr` anti-cheat. The emulator
  no-ops `CS_HOTSEND_REQ` so the legacy client's post-CHANNELLIST
  heartbeat does not crash the session, and nothing more.
* Japan channeling (`m_bNation == NATION_JAPAN`). No JP deploy target;
  the branch is dead code on every other build.

## License

See `Server/TLoginSvr/` and other legacy sub-trees for original notices.
Emulator code carries no separate license header; the project is for
private-server preservation work.
