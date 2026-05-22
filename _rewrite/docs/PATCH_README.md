# Patch catalog — Modernized cluster vs. legacy "Sources 5.0 (Araz)"

This document is the **patch-by-patch index** of what the
`Server/T*SvrAsio/` daemons + `Lib/Own/FourStoryCommon/` change
relative to the as-shipped commercial server distribution archived
as "Sources 5.0 (Araz)" (see
[`CLIENT_BUILD_NOTES.md`](CLIENT_BUILD_NOTES.md) for the path on
disk). For the narrative behavioral diff and the technology-stack
table, see
[`CHANGELOG_LEGACY_TO_MODERN.md`](CHANGELOG_LEGACY_TO_MODERN.md).

The Araz files are kept in-tree under `Server/T*Svr/` (and
`Server/TMapSvrAsio/legacy_src/` for the gameplay reference) **as
unmodified read-only references**. No patch in this catalog touches
those files — every fix lives in the corresponding `*Asio` directory
or the shared `FourStoryCommon` library.

## Legend

* **Severity:** 🔴 wire breaker / security · 🟠 functional bug ·
  🟡 portability / robustness · ⚪ cleanup
* **Status:** ✅ landed · 🟡 partial · ⏸ deferred · ❌ open
* Each row links to the canonical analysis doc where the deep dive
  lives, when one exists.

---

## 0. Cross-cutting

| ID | Severity | Patch | Status |
|---|---|---|---|
| X-1 | 🟡 | Win32 IOCP / `WSARecv` / `CreateIoCompletionPort` → **Boost.Asio coroutines** (epoll on Linux, IOCP on Windows). Same code, two platforms. | ✅ |
| X-2 | 🟡 | ATL `CString` (1 112 occurrences) → `std::string` / `std::string_view`. Public-API surface in `tnetlib` migrated; per-server interiors migrated as each daemon is ported. | ✅ for ported servers |
| X-3 | 🟡 | Win32 CryptoAPI (`CryptAcquireContext`, `CALG_RC4`) → **OpenSSL EVP**. RC4 KATs (RFC 6229) under `tnetlib_crypto` tests. | ✅ |
| X-4 | 🟡 | `CRegKey` (Windows Registry) → **toml++** typed config file. | ✅ |
| X-5 | 🟡 | `LogEvent` / `ATLTRACE` / `printf` / `OutputDebugString` → **spdlog** structured channel + dedicated audit channel. | ✅ |
| X-6 | 🟡 | ODBC via `CSqlDatabase` → **SOCI 4.x + ODBC backend** with a real connection pool (`fourstory::db::SessionPool`). | ✅ |
| X-7 | 🟡 | `CAtlServiceModuleT` Service framework → plain `main()` + `asio::signal_set` (SIGINT/SIGTERM) + legacy `SM_QUITSERVICE_REQ` still honored. | ✅ |
| X-8 | ⚪ | VS 2017 `.vcxproj` per-server → root **CMake 3.20+** + **vcpkg manifest mode** (`vcpkg.json`). | ✅ |
| X-9 | 🔴 | `Lib/Own/TNetLib/Packet.cpp::operator>>(CString&)` pre-auth heap-corruption (signed `int nLength` cast to `DWORD` wraps; `memcpy(buff, src, (size_t)-1)`). Defense in depth in three layers — see `MODERNIZATION_PLAN.md` §"TNetLib security audit". | ✅ |
| X-10 | 🔴 | Duplicate `TNetLib` (v140 in `Server/TNetLib/` + v141 in `Lib/Own/TNetLib/`) consolidated onto canonical path. Eliminates accidental ABI skew. | ✅ (commit `dd18d8d`) |
| X-11 | 🟡 | Vendored OpenSSL 0.9.8l (15-year-old dead reference) → deleted (`7b4b79e`); vcpkg pulls modern 3.x with `legacy` provider for RC4. | ✅ |
| X-12 | ⚪ | 7 `.cpp` files using `#include "StdAfx.h"` (uppercase) vs filesystem `stdafx.h` (lowercase) — worked on Windows by case-insensitive FS, broke on Linux. Normalized in `7ae24ce`. | ✅ |
| X-13 | 🟠 | 8 functional bugs in `TNetLib` (CS leak on `Open` failure, UB destructor on default-ctor `Close`, uninit fields in `SqlBase::Init`, signed `nCol` underflow in `IsNull`, uninit `BindDesc` ctor, `BYTE i` loop wrap in `Encrypt/DecryptHeader`, `Write` cap leaves invalid sentinel, `CopyData` 16-bit wrap). | ✅ |
| X-14 | 🟡 | New shared static lib **`fourstory_common`** consolidates SOCI pool, `schema_validator::CheckColumns`, `IAuditLogger`, `ISmtpClient`, `AdminShell`, `HealthEndpoint`, `LoginRateLimiter`, `RegistryRefresher`. Eliminates per-server copy-paste. | ✅ |
| X-15 | 🟡 | Legacy 0 tests → **CTest** with per-server in-process suites against `Fake*` services. SOCI suites skip when `*_TEST_MSSQL_CONN` env vars are unset, so CI without a DB passes. | ✅ |
| X-16 | ⚪ | `Rijndael.cpp/.h` (1 572 LOC custom AES impl) — zero references repo-wide → deleted. Removed cache-timing on S-box lookups and a potential key-schedule underflow incidentally. | ✅ |
| X-17 | ⚪ | `CryptDeriveKey(..., CRYPT_EXPORTABLE, ...)` exporting key material — flag removed (unused in our code path anyway). | ✅ |

---

## 1. `TLoginSvr` → `TLoginSvrAsio`

Legacy `Server/TLoginSvr/` is **37 files / 9 191 LOC** of
ATL+IOCP+ODBC+Windows-only code. The modern server at
`Server/TLoginSvrAsio/` ships 15/15 `CS_*` + 5/5 `CT_*` handlers.
Per-row deep dives live in
[`LOGIN_SERVER_COMPARISON.md`](LOGIN_SERVER_COMPARISON.md); this is
the patch index.

| ID | Severity | Symptom on the wire / behavior | Cause | Fix | Status |
|---|---|---|---|---|---|
| L-1 | 🔴 | Garbled equipment rendering across the entire char list | `CHARLIST` item layout sent 10 bytes; client (`TNetHandler.cpp:425-440`) expects 12 with `wCustomTex` between `wColor` and `bRegGuild` | Added `EquipItem::custom_tex` + wire encoding + `TITEMTABLE.wCustomTex` schema column | ✅ |
| L-2 | 🔴 | Lobby never decorated worlds with per-user char counts | `GROUPLIST` 5th byte sent `flags` (`TGROUP.bUseRate`) instead of `bCount` (per-user char count) | `GroupInfo::has_char` query (`COUNT(DISTINCT TALLCHARTABLE.dwCharID)` capped to 255); cap-override branch in `ResolveStatus` | ✅ |
| L-3 | 🔴 | RC4 keystream desync with shipped client — every byte garbage | Default secret hashed as 31 bytes; legacy hashes 31 + trailing NUL (= 32 bytes) | `config.cpp::kDefaultLegacySecretLen` uses `sizeof()` unchanged; regression test `test_rc4_secret_length` | ✅ |
| L-4 | 🟠 | JP/TW logins silently drop trailing bytes; high `site_code` values truncated | `TNetSender.cpp:46` emits `DWORD dwSiteCode` when `MODIFY_DIRECTLOGIN` is set; legacy server reads only the low byte | `handlers.cpp::ParseLoginReq` reads 4 bytes when nation is JP/TW; `AuthRequest::site_code` carries the full DWORD | ✅ |
| L-5 | 🟠 | Non-ASCII names in JP/KR/TW/RU/DE always returned `CR_PROTECTED` | `IsValidCharName` ASCII-only | `services/charname_validator.cpp` per-locale byte filters (Latin-1, Shift-JIS, Big5, EUC-KR, CP1251) | ✅ |
| L-6 | 🟠 | `bInPcBang` / `dwPremium` always 0 in `CS_LOGIN_ACK` regardless of DB state | `AuthResult` fields hard-coded | `SociAuthService::Authenticate` queries `TPCBANG` + `TUSERPREMIUM` (non-expired tier) | ✅ |
| L-7 | 🟠 | 2FA codes never reached users | `SpdlogSmtpClient` log-only default | `AsioSmtpClient` (Boost.Asio plain SMTP, EHLO/HELO + AUTH LOGIN) wired when `[smtp] host` is set | ✅ |
| L-8 | 🔴 | Any peer could fire `CT_EVENTUPDATE_REQ` and poison the event registry | Dispatcher didn't gate CT_* on peer IP | `LoginServer::Dispatch` checks `sess->RemoteIPv4() == m_control_server_ip`; binary refuses to start in production mode without it pinned (or `control_server_gate_open = true` opt-in for dev) | ✅ |
| L-9 | 🟠 | Lobby never highlighted the user's last-played slot | `LoginAck.dwCharID` always 0 | `AuthResult::last_char_id` populated from `TUSERINFOTABLE.dwLastCharID` (schema-optional; falls back to 0) | ✅ |
| L-10 | 🟠 | `LR_NEEDAGREEMENT` users could pull `GROUPLIST_ACK` / `CHANNELLIST_ACK` before completing EULA | Two handlers missed the `IsAgreed` gate | `handlers.cpp::OnGroupListReq` + `OnChannelListReq` now `Close()` on miss | ✅ |
| L-11 | 🟠 | Login storms after a crash — every account stuck on `LR_DUPLICATE` | Stale `TCURRENTUSER` rows from the previous PID's live sessions | `SociSessionTerminator::ClearStaleSessions` called from `main.cpp` after schema validation; bulk sweep on `SIGTERM` | ✅ |
| L-12 | 🟠 | `EventEntry` stored opaque bytes; downstream consumers couldn't introspect | Parser flat-stored the body | `services/event_registry.h` — typed fields + `parsed` flag, opaque fallback when wire shape doesn't match | ✅ |
| L-13 | ⚪ | No way to fingerprint client builds in audits | `dlCheck` parsed but not logged | `spdlog::debug("CS_LOGIN_REQ user=X client_hash=0x…")` when checksum tail present | ✅ |
| L-14 | 🔴 | Plaintext / weak hash in `TACCOUNT_PW.szPasswd` | Legacy stored plaintext-ish passwords | **BCrypt-only** (cost 10) via vendored `libbcrypt`. One-shot offline `tloginsvr_bcrypt_migrate` rehashes legacy rows; rejects already-bcrypt rows idempotently. | ✅ |
| L-15 | 🟡 | Schema drift between DB and SOCI bind silently returned bad rows | Legacy ran on whatever schema happened to be deployed | `db/schema_validator.cpp` confirms **50 TGLOBAL + 23 TGAME columns** at boot; fails fast with `SchemaError` listing missing entries | ✅ |
| L-16 | 🟡 | Pre-auth flood could exhaust memory | No connection cap, no pre-auth idle timeout | `[server] max_connections` (default 4096); `HandleConnection` watchdog (60 s default) drops sockets that don't complete the LOGIN exchange | ✅ |
| L-17 | 🟡 | No rate limiting on auth attempts per peer IP | Legacy didn't ship one | `LoginRateLimiter` token bucket (5 attempts / 10 s refill, GC after 600 s idle) | ✅ |
| L-18 | ⏸ | `HwidManagerSvr` (HWID anticheat) | Not wired in legacy build anyway (`m_hExecFile == INVALID_HANDLE_VALUE`) | Out of scope by design | ⏸ |

---

## 2. `TPatchSvr` → `TPatchSvrAsio`

Legacy `Server/TPatchSvr/` is **30 files / 3 824 LOC**. The modern
server ships 9/9 `CT_*` handlers. Round-by-round deep dive in
[`TPATCH_AUDIT.md`](TPATCH_AUDIT.md).

| ID | Severity | Symptom | Cause | Fix | Status |
|---|---|---|---|---|---|
| P-1 | 🟠 | Client wastes a download round-trip on a file it already has after every poll | `ListPatchesSince` SQL used `>= :v`; legacy `CTBLVersion.sql` uses `> :v` | Strict greater-than in both `ListPatchesSince` and `ListPrePatchesSince` | ✅ |
| P-2 | 🟠 | Same off-by-one in pre-version listing | `ListPrePatchesSince` `>=` vs legacy `>` | (same patch as P-1) | ✅ |
| P-3 | 🔴 | `CT_CHANGEIF_REQ` silently returned 0 files in every deploy | Modern queried `TINTERFACECHART`; the deployed schema has `TUSER_INTERFACE` (3 columns), and `dwSize` is `float` not `int`. The error was swallowed in debug logs. | Switched to `TUSER_INTERFACE` (`bOption`, `szName`, `dwSize`); synthesize `dwVersion` via `(SELECT MAX(dwVersion) FROM TVERSION)` sub-query exactly like legacy; bind `dwSize` as `double` and narrow C++-side | ✅ |
| P-4 | 🔴 | `CT_NEWPATCH_ACK.dwMinBetaVer` wrong on every deploy with `TPREVERSION.dwBetaVer < 2` | Modern picked `SELECT TOP 1 dwBetaVer FROM TPREVERSION ORDER BY dwBetaVer`; legacy calls `{? = CALL TMinBetaVer}` which returns a **hard-coded operator cutoff** (currently `2`) | `EXEC TMinBetaVer @dwMinVer = @v OUTPUT; SELECT @v` wrapper; SOCI binds the OUT param via a single-column rowset | ✅ |
| P-5 | 🟠 | `OnCT_PREPATCHCOMPLETE_REQ` was a no-op on RageZone DB (`TPreCompleteAdd` SP not deployed) | Modern called the missing SP; legacy required the SP | `PatchRepository::MarkPreVersionComplete` inlines the documented promote-pre-version sequence (MSSQL: single `MERGE INTO TVERSION USING TPREVERSION`; PG/SQLite: portable two-statement `UPDATE … FROM` + `INSERT … WHERE NOT EXISTS`), all in one `soci::transaction`. Body also shipped as `dbo.TPreCompleteAdd` in `schema/patch-tables.sql` so legacy `TPatchSvr.exe` binaries that *do* expect the SP keep working. | ✅ |
| P-6 | 🟠 | Stale-client connections sitting on the socket forever | Modern didn't port the legacy 60-second `m_dwTick` purge from `OnCT_SERVICEMONITOR_ACK` | `PatchSession` records `connected_at`; `PatchServer` keeps a `weak_ptr` registry and runs `SweepStaleClients(60 s)` on every monitor heartbeat + as a periodic safety-net timer. Regression coverage in `tests/test_stale_sweep.cpp` (3 predicate branches, real loopback sockets). | ✅ |
| P-7 | 🟡 | Boot continued past a missing column → confusing runtime errors | No schema validator | `tpatchsvr::db::ValidateGlobalSchema` runs between pool construction and listener open. Fails fast on missing `TVERSION` / `TPREVERSION` columns; warns on missing `TUSER_INTERFACE`. | ✅ |
| P-8 | ⏸ | Direct FTP server (legacy `TPatchSvr` shipped one) | Anti-pattern in a daemon | Out of scope — operators run a real FTP daemon; we return the URL in `CT_PATCH_ACK` | ⏸ |

---

## 3. `TLogSvr` → `TLogSvrAsio`

Legacy `Server/TLogSvr/` is **17 files / 3 908 LOC** including an MFC
dialog GUI. The modern server is a headless UDP collector for the
legacy `_UDPPACKET` audit frame.

| ID | Severity | Patch | Status |
|---|---|---|---|
| LG-1 | 🟠 | Bounded DB-outage retry buffer — mirrors legacy `m_listReadCompleted` requeue + `WorkTickProc(30 s)` reconnect. Default cap **1000 records** (matches legacy `MAX_IO_CONTEXT`), FIFO order preserved by `PushFront` on retry failure. Drops at cap counted in `DroppedQueueFull`. | ✅ |
| LG-2 | 🟡 | Boot-time schema validator on every `LT_*` column the SOCI sink binds, with a `[A-Za-z_][A-Za-z0-9_]{0,127}` identifier guard on the configured `target_table` so a malformed TOML can't smuggle SQL through the `INFORMATION_SCHEMA` probe. | ✅ |
| LG-3 | 🟡 | Dialect-aware blob bind: MSSQL `CONVERT(VARBINARY)`, PG `CAST(... AS bytea)`, SQLite pass-through. `i_null` indicator for empty payloads. | ✅ |
| LG-4 | 🟡 | Drop counters surfaced via spdlog at shutdown — `received` / `drops_bad_format` / `inserted` / `enqueued` / `drained` / `dropped_queue_full` / `queue_depth_at_shutdown`. Destructor warns if depth at shutdown is non-zero. | ✅ |
| LG-5 | ⚪ | MFC dialog GUI + `-I`/`-U` service install — dropped. systemd / docker / Windows-service wrapper owns lifecycle. | ✅ |
| LG-6 | ⚪ | `TMiniDump` (Win32 minidump on unhandled exception) — dropped. OS-level core dumps cover the same need. | ✅ |
| LG-7 | ⚪ | Date-partitioned `ITEMLOGTLyyyymmdd` tables — dropped. Single `TLOG_AUDIT` with `LT_LOGDATE` indexed; SQL Server's own partitioning is the supported path. The validator deliberately refuses to start against the legacy date-partitioned shape so operators know a migration is needed. | ✅ |
| LG-8 | ⚪ | `LP_CHAT` packet branch — wired to `Packet_Nothing` in legacy (never functional). Modern drops the same packets, counted as `drops_bad_format`. | ✅ |

---

## 4. `TControlSvr` → `TControlSvrAsio`

Legacy `Server/TControlSvr/` is **23 files / 7 290 LOC** of ATL +
IOCP + PDH + Win32-SCM code. The modern server ships **63/65 `CT_*`
handlers** (the two intentional skips are documented in
`CONSOLIDATION.md` §6: `CT_SERVICEUPLOAD*` UNC file-share path).
F1–F5 + round-2 audit details in
[`CONTROL_SERVER_PORT_PLAN.md`](CONTROL_SERVER_PORT_PLAN.md).

| ID | Severity | Patch | Status |
|---|---|---|---|
| C-1 | 🔴 | `CHATBANLIST_ACK` / `EVENTLIST_ACK` / `CASHITEMLIST_ACK` / `PREVERSIONTABLE_ACK` row count was `DWORD`; legacy uses `WORD` | `senders.cpp` writes `uint16` | ✅ |
| C-2 | 🔴 | `CHATBANLIST_ACK` row order wrong | Reordered to legacy: id / target / created / minutes / reason / op | ✅ |
| C-3 | 🔴 | `CT_EVENTUPDATE_REQ` shipped only `kind + value` (truncated) | Appends full `EventInfo` via `event_codec::Write` | ✅ |
| C-4 | 🟠 | 5 missing operator-side handlers (`CT_ITEMFIND_REQ` / `CT_ITEMSTATE_REQ` / `CT_MONACTION_REQ` / `CT_SERVICEDATACLEAR_REQ` / `CT_PLATFORM_REQ`) | Wired in `handlers_extra.cpp` | ✅ |
| C-5 | 🟠 | Peer→control `CT_SERVICECHANGE_REQ` missing | Wired in `RunPeerLoop` | ✅ |
| C-6 | 🟠 | 9 peer→operator ACK route-backs missing (ITEMFIND/STATE / MONSPAWNFIND / EVENTQUARTER* / TOURNAMENT / RPSGAME / CMGIFT*) | `OnPeerAckRouteBack` + 2 specialized strip-paths | ✅ |
| C-7 | 🟠 | Post-dial event push (`SendEventToNewConnect`) was dropped after dial | Restored in `OnNewConnectReq` for Login/Map/World peers | ✅ |
| C-8 | 🟠 | No rate limit on `CT_OPLOGIN_REQ` / `CT_STLOGIN_REQ` — credential-stuffing exposure | `LoginRateLimiter` (token bucket). Tripped peers get the same generic ack as wrong password — attackers can't distinguish rate-limit from invalid creds. Tunable via `[login_rate]` (`burst=0` disables). | ✅ |
| C-9 | 🟠 | SOCI service inventory was load-once (legacy behavior) | `RegistryRefresher` re-reads `TMACHINE/TGROUP/TSVRTYPE/TSERVER/TIPADDR` every `[inventory] refresh_seconds`; `PeerRegistry.Rebind` picks up adds + drops. `0` keeps legacy load-once. | ✅ |
| C-10 | 🟠 | `CT_SERVICEUPLOAD*` (UNC file-share path) dropped silently | Returns `bRet=2` instead, so GUI shows an error tile. The full UNC path itself stays out (anti-pattern). | ✅ stub |
| C-11 | 🟡 | Sync SOCI calls on the io_context blocked the reactor under DB latency | `fourstory::db::CoOffload` helper: wraps a sync SOCI call in `co_await asio::post(pool, …)` + resumes on the original executor; exceptions propagate via `void(exception_ptr, R)`. Wired on hot operator paths (`CT_OPLOGIN_REQ` / `CT_STLOGIN_REQ` / `CT_USERPROTECTED_REQ`); other SOCI sites opt in by wrapping the call. | 🟡 partial (opt-in per call site) |
| C-12 | 🔴 | Inter-server peer link was plain TCP — credentials and admin commands in clear text on the wire | **Mutual TLS on the peer listener** (`8afce14`), **client-side TLS on the dialer** (`fe95e01` + re-roll `8b5f16d`), RFC 5280 SAN matching (`330c2b5` + re-roll), RFC 6125 wildcard SAN (`8af9278` + re-roll), post-handshake CN validation on `CT_PEER_REGISTER_REQ` (`9804a48`), **hybrid 3-byte TLS-signature detection** so legacy clients still get the plain channel (`149d0b6` + fix `5964762`). | ✅ |
| C-13 | 🟠 | No way for operators to discover peers without preconfiguring them | `CT_PEER_DISCOVER` peer-service-discovery mode (`18f7f2a`) | ✅ |
| C-14 | 🟡 | PDH platform counters (`CT_PLATFORM_REQ` peer-side data) Windows-only | Wire-preserved; peers ship zeros on non-Windows. Operators observe machine health via `/metrics`. | 🟡 by design |
| C-15 | ⏸ | End-to-end legacy `TController.exe` smoke test against a real `TGLOBAL_RAGEZONE` | Open — stand the modernized daemon up and walk the GUI through login → service list → event manage | ⏸ |

---

## 5. `TMapSvr` → `TMapSvrAsio`

Legacy `Server/TMapSvr/` is **136 files / 112 843 LOC** — the bulk
of the cluster's gameplay logic (quest engine × 24 files, mob AI ×
14 files, two ~15 kLOC monolithic switch handlers). The modern
server is **scaffold only** at
[`Server/TMapSvrAsio/README.md`](../../Server/TMapSvrAsio/README.md);
the porting recipe is in
[`Server/TMapSvrAsio/CONSOLIDATION.md`](../../Server/TMapSvrAsio/CONSOLIDATION.md).

| ID | Severity | Patch | Status |
|---|---|---|---|
| M-1 | 🟡 | Layered architecture (transport → dispatch → handlers → services → persistence) replaces the legacy monolithic `SSHandler.cpp` + per-feature `.cpp` sprawl | ✅ scaffold |
| M-2 | 🟡 | 8 schema validators at boot (`TCHARTABLE`, `TINVENTABLE`, `TNPCCHART`, `TSKILLTABLE`, `TQUESTTABLE` + `TQUESTTERMTABLE`, `TMONSTERCHART` + `TMONSPAWNCHART`, `TCOMPANIONTABLE`) | ✅ |
| M-3 | 🟡 | 9 SOCI services covering the per-char + chart loaders that the gameplay layer will consume | ✅ |
| M-4 | 🟡 | Wire dispatch with per-message counter / latency / audit instrumentation | ✅ |
| M-5 | 🟡 | Pre-auth idle watchdog + per-session rate limit + admin shell + Prometheus metrics endpoint | ✅ |
| M-6 | ❌ | **Combat / damage formula** | ❌ design pending |
| M-7 | ❌ | **Mob AI tick + spawn manager** | ❌ design pending |
| M-8 | ❌ | **Quest VM** — Lua-via-sol2 vs data-driven YAML interpreter (T7 in TMapSvrAsio roadmap) | ❌ design pending |
| M-9 | ❌ | **Drop table / loot generator** | ❌ |
| M-10 | ❌ | **Bulk handler port** — 280 of ~297 `CS_*` and ~299 of ~300 `DM_/MW_/SS_` still on the legacy side | ❌ recipe documented |

**Do not deploy the scaffold against players.** Stand it up only in
dev to exercise the transport + dispatch.

---

## 6. `TWorldSvr`

Legacy `Server/TWorldSvr/` is **30 files / 38 851 LOC** (cluster
coordinator, char persistence, guild, war). **No modernized version
exists yet** — port is gated on the TMapSvrAsio gameplay-layer
design decisions because the World↔Map wire contract is the bigger
constraint.

| ID | Severity | Patch | Status |
|---|---|---|---|
| W-1 | 🟡 | Async DB + per-shard write queue with connection pool (legacy `m_hDB` is a single DB thread serving all TMapSvr instances) | ❌ planned (W2) |
| W-2 | 🟡 | Global maps (`m_mapTCHAR`, `m_mapTGuild`) under one lock — partition (per-guild grain, per-char actor model) | ❌ planned (W2 char / W3a guild) |
| W-3 | ✅ | TWorldSvrAsio binary exists — W1 ships the scaffold (TOML config, accept loop, packet framing, dispatch stub). See [`Server/TWorldSvrAsio/README.md`](../../Server/TWorldSvrAsio/README.md) for the W2..W7 phasing. | ✅ |

---

## 7. Empty shells (TBR / TBoW)

| ID | Severity | Patch | Status |
|---|---|---|---|
| E-1 | ⚪ | `Server/TBRSvr/` (92 LOC, 3 files) and `Server/TBoWSvr/` (92 LOC, 3 files) are **empty shells** — Battle Royale + Bow-of-War features compile into TMapSvr via `#ifdef BR_COMPILE_MODE` / `#ifdef BOW_COMPILE_MODE`. The Phase 6 cleanup will convert these to a runtime feature flag and delete the empty shells. | ⏸ Phase 6 |

---

## 8. Tooling / dev experience

| ID | Severity | Patch | Status |
|---|---|---|---|
| T-1 | 🟡 | Single-source CMake (root `CMakeLists.txt` + per-server subdirs) instead of `.sln` + per-server `.vcxproj`. CI runs on every push (Windows + Linux matrix planned, Linux landed). | ✅ |
| T-2 | 🟡 | `vcpkg.json` manifest mode pins Boost, OpenSSL, SOCI[odbc], spdlog, toml++ to known-good versions; first configure takes ~30 min, subsequent are incremental. | ✅ |
| T-3 | 🟡 | `tools/` dev scripts (build helpers, packet ID extraction, schema dump) | ✅ |
| T-4 | 🟡 | UTF-8 source files (legacy was CP949 / ISO-8859 in places — broke grep, broke clang-tidy) | ✅ post-`75b29ea` |
| T-5 | 🟡 | `_rewrite/docs/packet-ids.csv` — 1 542-row catalog of every wire ID across CS / SS / CT / MW / DM families | ✅ |
| T-6 | 🟡 | `_rewrite/docs/schema/` + `extract-schema.ps1` — reproducible SQL schema extraction from a restored `.bak` | ✅ |
| T-7 | 🟡 | `tloginsvr_bcrypt_migrate` — one-shot offline migration tool for `TACCOUNT_PW` legacy hashes. Idempotent. | ✅ |

---

## 9. How to read this catalog

* Each row points at exactly one fix. If the fix sits across multiple
  commits, the canonical analysis doc (`*_AUDIT.md`,
  `LOGIN_SERVER_COMPARISON.md`, `CONTROL_SERVER_PORT_PLAN.md`)
  enumerates them. Use `git log --grep '<keyword>'` to walk the
  commit chain.
* "Status" reflects whether the **modern Asio path** ships the fix.
  Legacy `Server/T*Svr/` is read-only reference — none of these
  patches touch it.
* "Severity" is the impact on a real deploy:
  * 🔴 wire breaker or security exposure — client/server desync,
    pre-auth memory corruption, plaintext credentials on the wire.
  * 🟠 functional bug — wrong-but-stable behavior the client
    silently tolerates.
  * 🟡 portability / robustness — Linux build, schema validator,
    rate limit.
  * ⚪ cleanup — removed dead code, swapped framework, no behavior
    change on a working deploy.
