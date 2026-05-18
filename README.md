# 4Story Server Cluster — Modernization

A C++20 / Boost.Asio reimplementation of the 4Story MMORPG server cluster.
The legacy Win32/ATL/IOCP sources from `Server/T*Svr/` stay in the tree
untouched and remain authoritative for shipped behavior; the modernized
binaries under `Server/T*SvrAsio/` run alongside them and are wire-format
byte-for-byte compatible with the original game client.

## Status

| Component | Legacy path | Modernized path | State |
|---|---|---|---|
| Login server | `Server/TLoginSvr` | `Server/TLoginSvrAsio` | ✅ production complete (auth, lobby, char flow, 2FA, ops hardening) |
| Patch metadata server | `Server/TPatchSvr` | `Server/TPatchSvrAsio` | ✅ all 9 `CT_*` handlers ported |
| Audit log collector | `Server/TLogSvr` | `Server/TLogSvrAsio` | ✅ UDP `_UDPPACKET` ingest + SOCI INSERT into `TLOG_AUDIT` |
| Shared infrastructure | (was duplicated per server) | `Lib/Own/FourStoryCommon` | ✅ extracted: SOCI pool, audit, SMTP, admin shell, health, rate limit |
| Wire codec | `Lib/Own/TNetLib` (legacy) | `Lib/Own/TNetLib` (modernized) | ✅ RC4 + XOR + framing, RFC 6229 verified |
| Anticheat (`HwidManagerSvr`) | `Server/TLoginSvr/HwidManagerSvr.cpp` | — | 🚫 out of scope by design |
| Map / world server (`TWorldSvr`) | `Server/TWorldSvr` | — | ⏸ deferred; legacy stays canonical for now |
| Control server (`TControlSvr`) | `Server/TControlSvr` | — | ⏸ deferred |

The cluster runs end-to-end against the restored `TGLOBAL_RAGEZONE` +
`TGAME_RAGEZONE` MSSQL databases. A shipped legacy 4Story client connects
to the modernized login server and walks the full LOGIN → CHARLIST →
CREATECHAR → START round-trip; the patch server serves real
`TVERSION`/`TPREVERSION` rows; the log collector receives the live
`_UDPPACKET` audit stream the login server emits.

## Stack

* **C++20** with Boost.Asio coroutines (`co_await`, `async_*`)
* **SOCI 4.x** with the ODBC backend → MS SQL Server (postgres branches kept but disabled)
* **OpenSSL** EVP for RC4; **libbcrypt** (vendored at `Lib/3rdParty/bcrypt/`) for password hashing
* **spdlog** for structured logging + the audit channel
* **toml++** for configuration
* **vcpkg** (manifest mode — `vcpkg.json`) for dependency management
* **CTest** with in-process integration tests against test fakes; SOCI suites skip cleanly without a DB

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
│       ├── TNetLib/                # wire codec + AsioSession (Phase 1 modernized)
│       ├── TProtocol/              # wire structs / MessageId enum (shared)
│       └── FourStoryCommon/        # shared infra: SOCI pool, audit, smtp, ops
├── Server/
│   ├── TLoginSvr/                  # legacy login (unmodified)
│   ├── TLoginSvrAsio/              # modernized login — production-ready
│   ├── TPatchSvr/                  # legacy patch (unmodified)
│   ├── TPatchSvrAsio/              # modernized patch
│   ├── TLogSvr/                    # legacy audit log collector (unmodified)
│   ├── TLogSvrAsio/                # modernized log collector
│   ├── TWorldSvr/                  # legacy world (not yet ported)
│   └── TControlSvr/                # legacy control (not yet ported)
├── _rewrite/docs/                  # plan + analysis documents
│   ├── MODERNIZATION_PLAN.md
│   ├── LOGIN_SERVER_COMPARISON.md
│   ├── PROTOCOL.md, SCHEMA.md
│   └── GAP_ANALYSIS.md
└── tools/                          # dev scripts
```

Each modernized component has its own README with the full handler
mapping, configuration schema, and bring-up notes:

* [`Server/TLoginSvrAsio/README.md`](Server/TLoginSvrAsio/README.md)
* [`Server/TPatchSvrAsio/README.md`](Server/TPatchSvrAsio/README.md)
* [`Server/TLogSvrAsio/README.md`](Server/TLogSvrAsio/README.md)
* [`Lib/Own/FourStoryCommon/README.md`](Lib/Own/FourStoryCommon/README.md)

## Build (Windows / MSVC + vcpkg)

```powershell
$env:VCPKG_ROOT = "C:\vcpkg"
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
      -DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake
cmake --build build --config Release
```

First configure pulls + builds Boost, OpenSSL, SOCI[odbc], spdlog,
toml++ (≈30 min). Subsequent configures are incremental.

Targets produced under `build/bin/Release/`:

* `tloginsvr_asio.exe` — login + lobby + char flow
* `tpatchsvr_asio.exe` — patch metadata
* `tlogsvr_asio.exe` — audit UDP collector

## Tests

```sh
ctest --test-dir build -C Release --output-on-failure
```

In-process tests (handler dispatch, wire codec, per-service business
logic against `Fake*` services) run without a DB. The SOCI integration
suites under `Server/TLoginSvrAsio/tests/test_soci_*` skip automatically
when `TLOGINSVR_TEST_MSSQL_CONN` is unset, so CI without a DB still
passes. Set the env var to a connection string to run them.

## Bringing the cluster up against the real legacy DB

```powershell
# 1. Restore the .bak files (one-shot)
sqlcmd -S localhost -E -Q "RESTORE DATABASE TGLOBAL_RAGEZONE FROM DISK='…\TGLOBAL_RAGEZONE.bak'"
sqlcmd -S localhost -E -Q "RESTORE DATABASE TGAME_RAGEZONE  FROM DISK='…\TGAME_RAGEZONE.bak'"

# 2. Seed a dev account + apply the 2FA tables
sqlcmd -S localhost -E -d TGLOBAL_RAGEZONE -i Server\TLoginSvrAsio\schema\dev-account.sql
sqlcmd -S localhost -E -d TGLOBAL_RAGEZONE -i Server\TLoginSvrAsio\schema\2fa-tables.sql

# 3. Apply the TLogSvr audit schema (one table)
sqlcmd -S localhost -E -d TGLOBAL_RAGEZONE -i Server\TLogSvrAsio\schema\tlog-audit.sql

# 4. Launch the cluster (three terminals)
build\bin\Release\tloginsvr_asio.exe --config Server\TLoginSvrAsio\tloginsvr.toml
build\bin\Release\tpatchsvr_asio.exe --config Server\TPatchSvrAsio\tpatchsvr.toml
build\bin\Release\tlogsvr_asio.exe   --config Server\TLogSvrAsio\tlogsvr.toml
```

Point a legacy client at `localhost:4816` and log in as `dev` / `dev123`.

## Compatibility

* **Wire protocol**: byte-for-byte 1:1 with the legacy server. Every
  ACK structure (`CS_LOGIN_ACK`, `CS_CHARLIST_ACK`, `CS_GROUPLIST_ACK`,
  `CS_START_ACK`, `CT_PATCH_ACK`, `_UDPPACKET`/`_LOG_DATA_`) is verified
  against `Server/TLoginSvr/CSSender.cpp`, `Server/TPatchSvr/Sender.cpp`,
  and `Server/TLogSvr/LogPacket.h`. The legacy `CS_LOGIN_REQ` trailing
  XOR/add checksum (CSHandler.cpp:185-202) is enforced on the server
  side; tests that previously sent zero-checksum dummy bodies were
  fixed.
* **Database schema**: unchanged. The legacy `TGLOBAL` + `TGAME` schemas
  are read as-is. The startup `schema_validator` checks 40 TGLOBAL + 23
  TGAME column names and fails fast on drift. Only additive migrations
  (`Server/TLoginSvrAsio/schema/2fa-tables.sql`,
  `Server/TLogSvrAsio/schema/tlog-audit.sql`) are needed; no destructive
  changes.
* **Anti-cheat (HShield / XTrap / NPGame / HwidManagerSvr)**: removed
  from scope by design. `CS_HOTSEND_REQ` (the legacy exec-file integrity
  heartbeat) is a silent no-op so the legacy client's post-CHANNELLIST
  ping doesn't crash the session.
* **Japan channeling (`m_bNation == NATION_JAPAN`)**: skipped — no JP
  deploy target. The branch is dead code on every other build.

## Documentation index

* [`_rewrite/docs/MODERNIZATION_PLAN.md`](_rewrite/docs/MODERNIZATION_PLAN.md)
  — cluster-wide phased roadmap
* [`_rewrite/docs/LOGIN_SERVER_COMPARISON.md`](_rewrite/docs/LOGIN_SERVER_COMPARISON.md)
  — handler-by-handler legacy vs modernized parity audit
* [`_rewrite/docs/PROTOCOL.md`](_rewrite/docs/PROTOCOL.md) — wire codec
  reference (header layout, RC4 keying, checksum algorithms)
* [`_rewrite/docs/SCHEMA.md`](_rewrite/docs/SCHEMA.md) — DB column
  catalog the modernized services read/write
* [`_rewrite/docs/GAP_ANALYSIS.md`](_rewrite/docs/GAP_ANALYSIS.md) —
  what's intentionally not ported (and why)

## License

See `Server/TLoginSvr/` and other legacy sub-trees for original notices.
Modernized code carries no separate license header; the project is for
private-server preservation work.
