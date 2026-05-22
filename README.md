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

## Overall progress

Cluster-wide rewrite status as of 2026-05-22:

```
Edge servers      ████████████████████  100%   (Login + Patch + Log + Control)
TMapSvr           █░░░░░░░░░░░░░░░░░░░    6%   (19 / ~300 handlers scaffolded)
TWorldSvr         ██░░░░░░░░░░░░░░░░░░   19%   (W3a-4d — CoOffloadIf wired + guild-level cache, 14 handlers)
─────────────────────────────────────────
Cluster total     ███░░░░░░░░░░░░░░░░░  ~18%   (LOC-weighted, see below)
```

| Component | Legacy LOC | Modern LOC | Wire handlers | DB schema | Status |
|---|---:|---:|---|---|---|
| **TLoginSvrAsio** | 9 191 | 15 815 | 15/15 CS + 5/5 CT | ✅ validator | **✅ Production complete** |
| **TPatchSvrAsio** | 3 824 | 2 813 | 9/9 CT | ✅ validator | **✅ Production complete** |
| **TLogSvrAsio** | 3 908 | 2 664 | UDP `_UDPPACKET` | ✅ validator | **✅ Production complete** |
| **TControlSvrAsio** | 7 290 | 19 599 | 63/65 CT + TLS peer auth | ✅ validator | **✅ F1–F5 complete + round-2 audit** |
| **TMapSvrAsio** | 112 842 | 7 458 | 14 CS + 5 CT (scaffold) | ✅ 8 validators | 🟡 **Scaffold only — no gameplay logic** |
| **TWorldSvrAsio** | 38 851 | ~7 800 | 14/287 + DB I/O off io_context + guild-level cache | 🟡 W3a-1+W3a-4d (TGUILD* + TGUILDCHART) | 🟡 **W3a-4d — CoOffloadIf + guild-level cache** |
| `Lib/Own/FourStoryCommon` | — | (shared) | — | — | ✅ SOCI + audit + smtp + ops |

LOC weighting: `(24 213 complete + ~6 700 scaffolded) / 175 906 legacy ≈ 17 %`.
By cluster-edge functionality, the four daemons that gate access to the
world (auth, patching, audit, ops) are **100 %** complete — what's left
is the gameplay surface (Map + World), which is ~83 % of the legacy
LOC and where the architectural risk lives.

Per-server detail (handler tables, schema, configuration, tests) lives
in each component's README; an Araz-source-to-modern patch catalog
lives in [`_rewrite/docs/PATCH_README.md`](_rewrite/docs/PATCH_README.md).

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
* **TLS peer fabric** — TControlSvrAsio's inter-server peer protocol
  speaks mutual TLS with RFC 5280 SAN matching and RFC 6125 wildcard
  rules; legacy clients still get the plain-text channel (hybrid
  3-byte TLS-handshake detection).
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
* **OpenSSL** EVP for RC4 + peer-link TLS; **libbcrypt** (vendored at
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
│   ├── TLoginSvrAsio/              # ✅ emulator login server
│   ├── TPatchSvr/                  # legacy patch (reference, unmodified)
│   ├── TPatchSvrAsio/              # ✅ emulator patch metadata server
│   ├── TLogSvr/                    # legacy audit log collector (unmodified)
│   ├── TLogSvrAsio/                # ✅ emulator audit UDP collector
│   ├── TControlSvr/                # legacy control server (reference, unmodified)
│   ├── TControlSvrAsio/            # ✅ emulator control / orchestration server
│   ├── TMapSvr/                    # legacy gameplay engine (reference, unmodified)
│   ├── TMapSvrAsio/                # 🟡 emulator map server — scaffold only
│   ├── TWorldSvr/                  # legacy cluster coordinator (reference)
│   ├── TWorldSvrAsio/              # 🟡 emulator cluster coordinator — W1 scaffold
│   ├── TBRSvr/  TBoWSvr/           # legacy empty shells (BR/BoW compile flags)
│   └── Tools/                      # legacy ops tools (unmodified)
├── _rewrite/docs/                  # plan + analysis + patch catalog
└── tools/                          # dev scripts
```

Each emulator component has its own README with the full handler
mapping, configuration schema, and bring-up notes:

* [`Server/TLoginSvrAsio/README.md`](Server/TLoginSvrAsio/README.md) — ✅ complete
* [`Server/TPatchSvrAsio/README.md`](Server/TPatchSvrAsio/README.md) — ✅ complete
* [`Server/TLogSvrAsio/README.md`](Server/TLogSvrAsio/README.md) — ✅ complete
* [`Server/TControlSvrAsio/README.md`](Server/TControlSvrAsio/README.md) — ✅ complete
* [`Server/TMapSvrAsio/README.md`](Server/TMapSvrAsio/README.md) — 🟡 scaffold (see also `ARCHITECTURE.md` / `CONSOLIDATION.md`)
* [`Server/TWorldSvrAsio/README.md`](Server/TWorldSvrAsio/README.md) — 🟡 W1 scaffold (transport + dispatch stub)
* [`Lib/Own/FourStoryCommon/README.md`](Lib/Own/FourStoryCommon/README.md) — ✅ shared infrastructure

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

* `tloginsvr_asio.exe`   — login + lobby + char flow
* `tpatchsvr_asio.exe`   — patch metadata
* `tlogsvr_asio.exe`     — audit UDP collector
* `tcontrolsvr_asio.exe` — control / orchestration daemon
* `tmapsvr_asio.exe`     — map server (scaffold; not production-ready)

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
suites under `Server/T*Asio/tests/test_soci_*` skip automatically
when the corresponding `*_TEST_MSSQL_CONN` env var is unset, so CI
without a DB still passes. Set the env var to a connection string to
run them:

```sh
export TLOGINSVR_TEST_MSSQL_CONN="DSN=4story;UID=sa;PWD=…"
ctest --test-dir build -C Release --output-on-failure
```

## Documentation index

* [`_rewrite/docs/MODERNIZATION_PLAN.md`](_rewrite/docs/MODERNIZATION_PLAN.md)
  — cluster-wide phased roadmap
* [`_rewrite/docs/PATCH_README.md`](_rewrite/docs/PATCH_README.md)
  — **patch catalog** vs the legacy "Sources 5.0 (Araz)" distribution
* [`_rewrite/docs/CHANGELOG_LEGACY_TO_MODERN.md`](_rewrite/docs/CHANGELOG_LEGACY_TO_MODERN.md)
  — narrative behavioral diff between the shipped server and the emulator
* [`_rewrite/docs/LOGIN_SERVER_COMPARISON.md`](_rewrite/docs/LOGIN_SERVER_COMPARISON.md)
  — handler-by-handler legacy vs emulator parity audit
* [`_rewrite/docs/TPATCH_AUDIT.md`](_rewrite/docs/TPATCH_AUDIT.md)
  — TPatchSvr byte-level parity audit (P-1…P-6 closed)
* [`_rewrite/docs/PROTOCOL.md`](_rewrite/docs/PROTOCOL.md) — wire codec
  reference (header layout, RC4 keying, checksum algorithms)
* [`_rewrite/docs/SCHEMA.md`](_rewrite/docs/SCHEMA.md) — DB column
  catalog the emulator services read/write
* [`_rewrite/docs/SQL_AUDIT.md`](_rewrite/docs/SQL_AUDIT.md) — SQL
  injection / schema-drift audit across all SOCI call sites
* [`_rewrite/docs/GAP_ANALYSIS.md`](_rewrite/docs/GAP_ANALYSIS.md) —
  what's intentionally not emulated (and why)
* [`_rewrite/docs/CONTROL_SERVER_PORT_PLAN.md`](_rewrite/docs/CONTROL_SERVER_PORT_PLAN.md)
  — design notes for the TControlSvrAsio port (F1–F5 + round 2)
* [`_rewrite/docs/PEER_PROTOCOL_PLAN.md`](_rewrite/docs/PEER_PROTOCOL_PLAN.md)
  — three-phase plan for peer-link TLS, access/refresh tokens, and
  gRPC migration
* [`_rewrite/docs/CLIENT_BUILD_NOTES.md`](_rewrite/docs/CLIENT_BUILD_NOTES.md)
  — notes on rebuilding the legacy client from source

## Roadmap

### Completed (cluster edge)

* **TLoginSvrAsio** — every legacy `CTLoginSvrModule` handler ported.
  BCrypt-only auth, TUSERPROTECTED IP banlist, 2FA via TSECURECODE,
  schema validator, rate limit, audit log (both spdlog + legacy
  `_UDPPACKET` to TLogSvr), live TCURRENTUSER counts. Production
  cutover ready against `TGLOBAL_RAGEZONE` + `TGAME_RAGEZONE`.
* **TPatchSvrAsio** — all 9 `CT_*` handlers + boot-time schema
  validator, periodic stale-client sweep, pre-version promotion
  inline (works without legacy `TPreCompleteAdd` SP). Six audit
  items P-1…P-6 closed.
* **TLogSvrAsio** — UDP `_UDPPACKET` collector with `TLOG_AUDIT`
  sink, bounded retry queue mirroring legacy `m_listReadCompleted`,
  schema validator.
* **TControlSvrAsio** — F1–F5 complete, 63/65 `CT_*` handlers wired,
  Round-2 audit fixes applied (wire parity, event push, peer-ack
  route-backs). Peer fabric speaks mutual TLS with RFC 5280 SAN
  matching and RFC 6125 wildcard rules; hybrid first-byte detection
  lets legacy clients still use the plain channel.
* **FourStoryCommon** — SOCI pool, schema-validator framework,
  audit/SMTP/rate-limit/admin-shell/health-endpoint plumbing pulled
  into a single static lib so the four Asio daemons no longer
  copy-paste this code.

### In progress (cluster core)

* **TMapSvrAsio** — Layered scaffold (transport → dispatch → handlers
  → services → persistence) is built and ships 14 `CS_*` + 5 `CT_*`
  handlers wired through the dispatcher. Eight `Validate*Schema`
  validators gate boot against `TCHARTABLE` / `TINVENTABLE` /
  `TNPCCHART` / `TSKILLTABLE` / `TQUESTTABLE` / `TQUESTTERMTABLE` /
  `TMONSTERCHART` / `TMONSPAWNCHART` / `TCOMPANIONTABLE`. **Game
  logic — damage formulas, AI ticks, quest VM, drop tables — is
  NOT implemented.** The 297 legacy `OnCS_*` and 300+ `DM_/MW_/SS_`
  handlers are catalogued in `CONSOLIDATION.md`; the priority signal
  is in PR #25.

### Open (cluster edge wrap-up)

* **End-to-end legacy `TController.exe` smoke test** — stand the
  modernized control daemon up against a copy of `TGLOBAL_RAGEZONE`
  and walk the GUI through login → service list → event manage to
  confirm wire parity in a real bring-up. The `IServiceController`
  interface is wired with both a disabled-by-default fallback and a
  Windows SCM impl (Linux build links the SCM impl as a no-op stub).
* **Operator tooling** — round out the admin shell (account lookup,
  ban/unban, session kick) and expose a minimal HTTP health/metrics
  endpoint so the cluster is observable without RDP.

### Mid-term (gameplay surface)

* **TMapSvrAsio gameplay layer** — port damage / AI / quest VM out of
  `Server/TMapSvr/`. This is the big one (~113 kLOC legacy) and the
  bulk of the remaining cluster work. The scaffolding is in place;
  the rules layer is what needs design (Lua-via-sol2 vs data-driven
  YAML interpreter for quests, register-based dispatch for the 20k-LOC
  `SSHandler.cpp` switch, etc.).
* **TWorldSvr modernization** — cluster coordinator. Port char
  persistence, party/guild, trade, and inventory persistence. Until
  this lands, the legacy `TWorldSvr` binary remains canonical.
* **Map data pipeline** — reproducible extraction of map / NPC / drop
  tables from the shipped data files, so world content can be
  regenerated rather than restored from a binary backup.

### Longer-term (preservation)

* **Linux production deployment** — the code already builds on Linux;
  the goal is a fully Linux-hosted cluster (systemd units, container
  images, no Wine for the auxiliary tools).
* **Schema migration story** — formalize the additive-only migration
  flow so community deployments can upgrade between emulator versions
  without hand-editing tables.
* **Reference dataset** — publish a minimal, lawful starter DB seed so
  new operators don't need access to an original `.bak` to stand the
  cluster up.

### Explicitly out of scope

* HShield / XTrap / NPGame / `HwidManagerSvr` anti-cheat. The emulator
  no-ops `CS_HOTSEND_REQ` so the legacy client's post-CHANNELLIST
  heartbeat does not crash the session, and nothing more.
* Japan channeling (`m_bNation == NATION_JAPAN`) as a *deployment*
  target — the wire parser does read the trailing `DWORD dwSiteCode`
  the JP/TW client sends, so the protocol path stays compatible.

## License

See `Server/TLoginSvr/` and other legacy sub-trees for original notices.
Emulator code carries no separate license header; the project is for
private-server preservation work.
