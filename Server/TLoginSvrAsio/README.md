# TLoginSvrAsio — modernized 4Story login server

Production-grade reimplementation of `Server/TLoginSvr/` on a portable
C++20 stack: Boost.Asio coroutines, SOCI database access, OpenSSL
crypto, libbcrypt, spdlog, toml++. Runs as a separate binary; the
legacy `TLoginSvr.exe` is untouched and can be A/B-compared during the
cutover. Wire-format byte-for-byte compatible with the shipped legacy
client (RC4 + XOR codec, RFC 6229 verified).

## Status — production complete (minus anticheat)

Every legacy `CTLoginSvrModule` handler is ported, every operational
piece (audit log, schema validator, rate limit, admin shell, 2FA) is
wired, and the binary runs end-to-end against the restored MSSQL
databases (`TGLOBAL_RAGEZONE` + `TGAME_RAGEZONE`). Shared infrastructure
(SOCI pool, audit, SMTP, admin shell, health, rate limit, registry
refresher) was lifted into the [`fourstory_common`](../../Lib/Own/FourStoryCommon/README.md)
static library and is now also consumed by `TPatchSvrAsio` and
`TLogSvrAsio`.

| Area | State |
|---|---|
| Wire codec (RC4 + XOR + framing) | ✅ RFC 6229 + symmetry tests |
| Asio reactor + session lifecycle | ✅ coroutines, RAII, pre-auth idle watchdog |
| Two-pool DB layer (TGLOBAL + TGAME) | ✅ SOCI ODBC against MSSQL |
| Schema validator on startup | ✅ 40 TGLOBAL + 23 TGAME columns checked, fail-fast |
| Auth flow (LOGIN, AGREEMENT) | ✅ TLogin SP parity — IP banlist, TUSERPROTECTED, bLocked-kick, TUSERINFOTABLE agreement, BCrypt + transparent upgrade |
| Char flow (LIST, CREATE, DELETE) | ✅ TCHARTABLE + TITEMTABLE equipped + TGUILDTABLE fame, guild block + level-5 split, password verify, BR/BOW notify |
| Map locator (START + lobby lists) | ✅ TFindServerID port (TSVRCHART + TCHANNELCHART + TSPAWNPOSCHART fallback), BR/BOW shard override, live TCURRENTUSER counts |
| Session cleanup (TLOGOUT) | ✅ Disconnect/ClientRequest/MapHandoff modes |
| Control protocol (CT_*) | ✅ SERVICEMONITOR/SERVICEDATACLEAR/CTRLSVR/EVENTUPDATE/EVENTMSG + event registry |
| Debug handlers (TESTLOGIN/TESTVERSION) | ✅ gated `test_handlers_enabled` |
| Graceful shutdown (SM_QUITSERVICE_REQ) | ✅ wire-protocol triggers `io.stop()` |
| Operational hardening | ✅ rate limit, pre-auth timeout, audit log, admin shell |
| HWID anticheat | 🚫 out of scope by design |

## Ported wire handlers (15/15 — 100%)

| Handler | Status | Backed by |
|---|---|---|
| `CS_LOGIN_REQ` | ✅ real | `IAuthService::Authenticate` — BCrypt + transparent upgrade, IP banlist, TUSERPROTECTED, llChecksum |
| `CS_AGREEMENT_REQ` | ✅ real | `IAuthService::SetAgreement` (TUSERINFOTABLE upsert) + per-session gate flip |
| `CS_GROUPLIST_REQ` | ✅ real | `IMapServerLocator::ListGroups` — live TCURRENTUSER count |
| `CS_CHANNELLIST_REQ` | ✅ real | `IMapServerLocator::ListChannels` |
| `CS_CHARLIST_REQ` | ✅ real | `ICharService::List` — items + guild fame; trailing `CS_BOWPLAYERNOTIFY_ACK` |
| `CS_CREATECHAR_REQ` | ✅ real | `ICharService::Create` — TCHARTABLE + TALLCHARTABLE + starter items |
| `CS_DELCHAR_REQ` | ✅ real | `ICharService::Delete` + `IAuthService::VerifyPassword` |
| `CS_START_REQ` | ✅ real | `IMapServerLocator::Lookup` — TFindServerID port, BR/BOW shard |
| `CS_VETERAN_REQ` | ✅ real | `ICharService::GetVeteranLevels` (cached TVETERANCHART, 30s refresh) |
| `CS_TERMINATE_REQ` | ✅ real | legacy magic check + `ISessionTerminator` cleanup |
| `CS_HOTSEND_REQ` | ✅ silently dropped | Legacy client's exec-file integrity heartbeat; validation is anti-cheat tooling, intentionally out of scope |
| `CS_SECURITYCONFIRM_ACK` | ✅ real | TSECURECODE compare + `ISmtpClient` for the issuance side |
| `CS_TESTLOGIN_REQ` | ✅ real | TTESTLOGINUSER pick; gated `test_handlers_enabled = false` |
| `CS_TESTVERSION_REQ` | ✅ real | returns server's protocol version |
| `SM_QUITSERVICE_REQ` | ✅ real | graceful `io.stop()` via `on_quit_request` hook |

### Control protocol (CT_*, 5/5 — server-to-server)

| Handler | Status |
|---|---|
| `CT_SERVICEMONITOR_ACK` | ✅ replies `CT_SERVICEMONITOR_REQ` with live counts from `IConnectionRegistry::Count()` |
| `CT_SERVICEDATACLEAR_ACK` | ✅ no-op (registry is canonical, no derived map to rebuild) |
| `CT_CTRLSVR_REQ` | ✅ heartbeat |
| `CT_EVENTUPDATE_REQ` | ✅ upsert/remove in `IEventRegistry` (matches legacy `m_mapEVENT` semantics) |
| `CT_EVENTMSG_REQ` | ✅ logs broadcast |

## Service interfaces

Each service has a production implementation backed by SOCI / spdlog /
local memory, and a test-only `Fake*` for the ctest suite and dev mode
without a DB.

| Interface | Production impl | Test fake | Notes |
|---|---|---|---|
| `IAuthService` | `SociAuthService` | `FakeAuthService` | BCrypt + transparent upgrade, TUSERINFOTABLE agreement, TSECURECODE 2FA |
| `ICharService` | `SociCharService` | `FakeCharService` | TGLOBAL + TGAME split; items, fame, BR shard |
| `IMapServerLocator` | `SociMapServerLocator` | `FakeMapServerLocator` | TFindServerID port + BR/BOW |
| `ISessionTerminator` | `SociSessionTerminator` | `FakeSessionTerminator` | DELETE TCURRENTUSER / UPDATE TLOG.timeLOGOUT |
| `IConnectionRegistry` | `LocalConnectionRegistry` | — (in-process is canonical) | duplicate-kick + agreement gate state |
| `IAuditLogger` | `SpdlogAuditLogger` (+ `UdpAuditLogger` decorator) | — | structured stderr or legacy `_UDPPACKET` UDP shim to TLogSvr |
| `IEventRegistry` | `LocalEventRegistry` | — | GM-broadcast events from CT_EVENTUPDATE_REQ |
| `ISmtpClient` | `SpdlogSmtpClient` (log-only) | — | replace with real SMTP impl for production 2FA mail |
| `LoginRateLimiter` | concrete class | — | token bucket per peer IP |

**Production = SOCI/Local/Spdlog impls.** `Fake*` are only wired when
`[database]` is empty in TOML (smoke tests / no-DB dev). `main.cpp`
explicitly constructs production variants when a connection string is
configured.

Shared plumbing (`SpdlogAuditLogger`, `UdpAuditLogger`,
`SpdlogSmtpClient`, `LoginRateLimiter`, `AdminShell`, `HealthEndpoint`,
`RegistryRefresher`) lives in
[`fourstory_common`](../../Lib/Own/FourStoryCommon/README.md) — login-
specific interfaces (`IAuthService`, `ICharService`,
`IConnectionRegistry`, `IMapServerLocator`, `ISessionTerminator`) stay
in this server's `services/`.

## Database schema

Real legacy split — the new server respects it:

* **TGLOBAL** (`tloginsvr.toml [database]`) — accounts, sessions,
  server registry, cross-world char index. Tables: `TACCOUNT_PW`,
  `TUSERINFOTABLE`, `TUSERPROTECTED`, `TCURRENTUSER`, `TLOG`,
  `IPBLACKLIST_game`, `TSERVER`, `TIPADDR`, `TGROUP`, `TCHANNEL`,
  `TALLCHARTABLE`, `TVETERANCHART`, `TRESERVEDNAME`, `TKEEPINGNAME`,
  `TTESTLOGINUSER`, `TSECURECODE`.

* **TGAME** (`tloginsvr.toml [database.world]`) — per-world chars,
  items, guilds, shard tables. Tables: `TCHARTABLE`, `TITEMTABLE`,
  `TGUILDMEMBERTABLE`, `TGUILDTABLE`, `TBRPLAYERTABLE`, `TSVRCHART`,
  `TCHANNELCHART`, `TSPAWNPOSCHART`.

Schemas:
* `schema/mssql-dev.sql` — minimal dev fixture for the auth flow
* `schema/postgres-dev.sql` — PG-dialect equivalent (PG backend
  currently not in vcpkg feature set; code branches retained)
* `schema/dev-account.sql` — seeds `dev` / `dev123` (BCrypt hash,
  agreement set) against a real `TGLOBAL_RAGEZONE`

The startup `schema_validator` (`db/schema_validator.cpp`) confirms 40
TGLOBAL + 23 TGAME columns before the listener opens. Fail-fast on
schema drift.

## Configuration

Single TOML file. Empty `[database]` keeps the binary in dev / no-DB
mode. Setting `[database]` + `[database.world]` flips into production
mode (SOCI services, schema validator, audit log).

```toml
[server]
port = 4816                          # legacy TSERVER row for bType=TLOGIN_GSP
# accepted_versions = [0x2918]       # client wVersion whitelist; default = legacy single value
# test_handlers_enabled = false      # CS_TESTLOGIN_REQ / CS_TESTVERSION_REQ gate

[crypto]
disable_rc4 = false                  # set true for server-server test peers
# rc4_secret_hex = "…"               # override default legacy wire secret

[log]
level = "info"                       # trace|debug|info|warn|error|critical|off

[health]
port = 8815                          # /healthz HTTP endpoint; 0 disables

[admin]
bind = "127.0.0.1"
port = 0                             # admin TCP shell; 0 disables. Never expose to the open internet.

[audit.udp]
# host = "192.168.1.5"               # legacy TLogSvr collector — sends wire-faithful _UDPPACKET
# port = 2000

[database]                           # TGLOBAL — accounts, sessions, server registry
backend = "odbc"
connection_string = "DRIVER={ODBC Driver 17 for SQL Server};SERVER=localhost;DATABASE=TGLOBAL_RAGEZONE;Trusted_Connection=yes;TrustServerCertificate=yes"
pool_size = 8

[database.world]                     # TGAME — per-world chars/items/guilds
backend = "odbc"
connection_string = "DRIVER={ODBC Driver 17 for SQL Server};SERVER=localhost;DATABASE=TGAME_RAGEZONE;Trusted_Connection=yes;TrustServerCertificate=yes"
pool_size = 8
```

Full annotated schema with all keys: `tloginsvr.example.toml`.

## Operational features

| Feature | Where |
|---|---|
| **Pre-auth idle timeout** | `LoginServer::HandleConnection` watchdog (60s default, configurable) |
| **Rate limiting per peer IP** | `LoginRateLimiter` token bucket, 5 attempts / 10s refill default |
| **Schema validator on startup** | `db/schema_validator.cpp` — fail-fast on missing tables/columns |
| **Structured audit log** | `SpdlogAuditLogger` — emits `event=login outcome=… uid=… ip=… key=…` |
| **Legacy UDP audit shim** | `UdpAuditLogger` (decorator) — wire-faithful `_UDPPACKET` to TLogSvr |
| **Admin TCP shell** | `AdminShell` — `status`, `kick`, `ban-ip`, `log-level`, `quit` (localhost bind only) |
| **Periodic cache refresh** | `RegistryRefresher` — 30s tick reloads `TVETERANCHART` |
| **Health endpoint** | `/healthz` HTTP JSON (uptime + status) for k8s probes |
| **Graceful shutdown** | SIGINT/SIGTERM + `SM_QUITSERVICE_REQ` both trigger `io.stop()` |
| **BCrypt transparent upgrade** | Legacy plaintext rows in `TACCOUNT_PW.szPasswd` get rewritten on successful login |

## Build

CMake + vcpkg + MSVC. First configure pulls + builds Boost.Asio, OpenSSL,
SOCI[odbc], spdlog, toml++ (~30 min). BCrypt is vendored at
`Lib/3rdParty/bcrypt/` (no working vcpkg port).

```powershell
$env:VCPKG_ROOT = "C:\vcpkg"
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
      -DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake
cmake --build build --target tloginsvr_asio --config Release
build\bin\Release\tloginsvr_asio.exe --config tloginsvr.toml
```

Linux build also works (distro packages cover everything):

```sh
apt install libsoci-dev libpq-dev unixodbc-dev libspdlog-dev \
            libtomlplusplus-dev libssl-dev libboost-all-dev
cmake -B build -S . && cmake --build build --target tloginsvr_asio
```

## Tests

```
ctest --output-on-failure
```

10 ctest targets — wire codec, handler dispatch, per-service business
logic against the test fakes. SOCI integration suites skip automatically
when `TLOGINSVR_TEST_MSSQL_CONN` env var is unset (so CI without a DB
passes). With env set:

```sh
export TLOGINSVR_TEST_MSSQL_CONN="DSN=TLOGINSVR_MSSQL;DATABASE=tloginsvr_dev;UID=sa;PWD=…"
ctest -R tloginsvr_asio_soci --output-on-failure
```

## Bringing up against the real legacy DB

```powershell
# 1. Restore the .bak files
sqlcmd -S localhost -E -Q "RESTORE DATABASE TGLOBAL_RAGEZONE FROM DISK='…\TGLOBAL_RAGEZONE.bak'"
sqlcmd -S localhost -E -Q "RESTORE DATABASE TGAME_RAGEZONE  FROM DISK='…\TGAME_RAGEZONE.bak'"

# 2. Seed a known-password dev account
sqlcmd -S localhost -E -d TGLOBAL_RAGEZONE -i schema\dev-account.sql

# 3. Launch
build\bin\Release\tloginsvr_asio.exe --config tloginsvr.toml
```

Expected startup log:

```
schema_validator (global) OK (40 columns checked)
schema_validator (world)  OK (23 columns checked)
char_service: refreshed N veteran-chart row(s)
registry_refresher: tick every 30s (1 hook(s))
services: SOCI (odbc+odbc) — auth + char + map + terminator
login server listening on 0.0.0.0:4816 (RC4: enabled)
health endpoint listening on 0.0.0.0:8815
```

Then point a legacy client at `localhost:4816` and log in as
`dev` / `dev123`.

## What's intentionally NOT ported

| Legacy piece | Why skipped |
|---|---|
| `HwidManagerSvr` (HWID anticheat) | Out of scope by user request; not wired into auth flow in legacy build anyway |
| `CSPLoginJP` (Japan channeling) | Only fires when `m_bNation == NATION_JAPAN`; no JP deploy target |
| `m_qCheckPoint` HotSend queue | Trigger path commented out in legacy build (`m_hExecFile == INVALID_HANDLE_VALUE`) |
| `CDebugSocket` outbound client | Replaced by inbound `AdminShell` |
| `CSmtp` / `jwsmtp` direct linkage | Replaced by `ISmtpClient` interface — operators plug in their preferred transport |
| `base64.cpp` / `md5.cpp` | Replaced by OpenSSL EVP + libbcrypt |
| Win32 IOCP | Replaced by Boost.Asio coroutines |
| Win32 Registry config | Replaced by TOML |

## Architecture

```
Boost.Asio io_context
├── AsioListener (port 4816, RC4 enabled)
│   └── AsioSession (per peer)
│       └── LoginServer::Dispatch — 20 case statements →
│           handlers::On* coroutines
├── HealthEndpoint (port 8815) — /healthz
├── AdminShell (127.0.0.1:N) — line-based admin commands
├── RegistryRefresher — 30s veteran-chart reload
└── signal_set (SIGINT/SIGTERM) → io.stop()

Services (production wiring in main.cpp):
├── SessionPool×2 (TGLOBAL + TGAME) — SOCI ODBC connections
├── SociAuthService       ← TGLOBAL
├── SociCharService       ← TGLOBAL + TGAME
├── SociMapServerLocator  ← TGLOBAL + TGAME
├── SociSessionTerminator ← TGLOBAL
├── LocalConnectionRegistry (in-process — matches legacy m_mapTUSER)
├── LocalEventRegistry (in-process — matches legacy m_mapEVENT)
├── LoginRateLimiter
├── SpdlogAuditLogger [+ UdpAuditLogger decorator if [audit.udp] set]
└── SpdlogSmtpClient (default; swap for real SMTP in prod)
```

## Roadmap

| Phase | Scope | Status |
|---|---|---|
| **A** | Wire codec + handler scaffolding | ✅ |
| **B** | SOCI services + real DB | ✅ |
| **C** | Production hardening (audit, rate limit, schema validator, admin shell, 2FA, per-char routing, …) | ✅ |
| **D.1** | Sibling-server modernization — patch + log + shared lib | ✅ `TPatchSvrAsio` + `TLogSvrAsio` + `fourstory_common` |
| **D.2** | Sibling-server modernization — control | ⏸ legacy `TControlSvr` retained for now |
| **E** | World/Map modernization | only if a concrete driver (cross-platform, security, vendor pressure) shows up |

See `_rewrite/docs/MODERNIZATION_PLAN.md` for the cluster-wide roadmap.
