# Login Server: Legacy vs. New — Current Comparison

> **Status: refreshed 2026-05-20.** Reflects the present state of both
> trees on `master`. The original 2026-05-18 pre-Phase-B snapshot is
> kept verbatim as **Appendix A** at the bottom for institutional
> memory — it explains *why* a given interface was shaped the way it
> was; this top half explains *what's there now*.
>
> Authoritative per-server detail still lives in
> [`Server/TLoginSvrAsio/README.md`](../../Server/TLoginSvrAsio/README.md).

Compare `Server/TLoginSvr/` (Win32 / ATL / IOCP / ODBC, ~9 200 LOC,
**0 tests**) with `Server/TLoginSvrAsio/` (C++20 / Boost.Asio
coroutines / SOCI / spdlog / TOML / OpenSSL / libbcrypt,
~15 100 LOC, **17 ctest suites**).

## TL;DR

The modern `TLoginSvrAsio` is materially better across every axis a
team would evaluate a login server on: security, performance,
portability, observability, testability, maintainability. The legacy
`TLoginSvr` is functional but carries critical security holes
(plaintext passwords in stored procedures, password leaked to
`ATLTRACE`, no rate limit, no pre-auth flood gate, no graceful
shutdown sweep) and Windows-only infrastructure that's expensive to
operate in modern environments. **All 15 wire handlers and all 5
control handlers are now production-real in the modern server**; the
gap analysis below is for ops, not for feature parity.

If you're picking which binary to ship: `TLoginSvrAsio`. The legacy
tree should be retained only as a wire-protocol reference + smoke-
test peer during cutover.

## At-a-glance matrix

| Dimension | Legacy `TLoginSvr` | Modern `TLoginSvrAsio` |
|---|---|---|
| Build target | Windows only (ATL Service, COM `LIBID`, Win32 Registry) | Linux + Windows (CMake + vcpkg + distro packages) |
| I/O reactor | Win32 IOCP, fixed `MAX_THREAD` workers | Boost.Asio coroutines, scales with cores |
| Per-connection lifetime | `new CTUser` + raw pointers, manual `delete` in `OnCloseSession` | `std::shared_ptr<AsioSession>` + RAII (`ConnectionCounterGuard`) |
| Locking | Single coarse `CRITICAL_SECTION m_csLI` over all session maps | Per-service `std::mutex`, no global hotspot |
| DB access | Single `CSqlDatabase m_db` serialized via critical section | `SessionPool×2` (TGLOBAL + TGAME), `pool_size = 8`, `AcquireTimeout` |
| Schema split | One DB session for everything | TGLOBAL (accounts) + TGAME (chars/items/guilds) pools; startup validator checks 40 + 23 columns and fails fast |
| Password storage | **Plaintext SHA1-hex in TACCOUNT_PW.szPasswd** | **BCrypt** (`$2a$`/`$2b$`/`$2y$`); pre-bcrypt rows rejected; offline `tloginsvr_bcrypt_migrate` tool |
| Password leakage | `ATLTRACE("UserID:%s, Passwd=%s")` — plaintext into trace log (`CSHandler.cpp:441`) | spdlog audit emits `event=login outcome=… uid=… ip=… key=…` — never password |
| String safety | `lstrcpy(query->m_szPasswd, ...)` into fixed `TCHAR[MAX_NAME+1]` (`CSHandler.cpp:239, 323`) | `std::string` / `std::vector<std::byte>`, length checks in `handlers.cpp::ParseLoginReq`, malformed packet → `Close()` |
| Rate limiting | None | `LoginRateLimiter` token bucket (5 attempts / 10 s default, per IP) |
| Pre-auth flood | None — peer can hold thousands of half-open sockets | `max_connections` cap + per-session `pre_auth_timeout` watchdog (60 s default) |
| Control-protocol gating | IP check in `Accept()`, no fail-safe | `control_server_ip` required at boot; `main.cpp:113-123` refuses to start without it (unless `control_server_gate_open = true` opt-in); CT_* drops from wrong peer in `Dispatch` |
| IP banlist | `IPBLACKLIST_game` exact match (one SP) | Same + `TIPAUTHORITY` LIKE-pattern subnet bans (`soci_auth_service.cpp:115-152`) |
| 2FA | Dead code (`CS_SECURITYCONFIRM_ACK` path commented out) | Real path — `TSECURECODE` store + case-insensitive compare + try-counter, `AsioSmtpClient` (EHLO/HELO + AUTH LOGIN) emails the code when `[smtp] host` set |
| Wire crypto | RC4 + XOR — but **shipped with a keystream-desync bug** (default secret hashed at 31 bytes vs client's 32) | Same wire codec, bug fixed, regression test `test_rc4_secret_length`, RFC 6229 verified |
| Wire bugs (other) | 14 known issues — JP site-code truncation, garbled equipment list, missing char-count in GROUPLIST, EULA gate bypass, etc. | All 14 fixed; round-by-round audit table in `Server/TLoginSvrAsio/README.md` ("Wire-compatibility fixes") |
| Exception safety | Unhandled throw inside a worker takes down the IOCP thread | Per-dispatch + per-coroutine try/catch (`login_server.cpp:139-148, 424-445`); one bad query just closes its connection |
| Caching | TVETERANCHART re-read every CS_VETERAN_REQ | `RegistryRefresher` reloads chart on a 30 s tick |
| Graceful shutdown | None — crash leaves stale `TCURRENTUSER` rows; on next boot every account hits `LR_DUPLICATE` | SIGINT/SIGTERM + `SM_QUITSERVICE_REQ` both → snapshot registry, terminate live sessions, `io.stop()` |
| Stale-session sweep at boot | None | `SociSessionTerminator::ClearStaleSessions` runs after schema validation |
| Observability | `ATLTRACE`, `LogEvent`, dberror.log file | spdlog (level-configurable), structured audit logger, `/healthz` HTTP endpoint, admin TCP shell (`status / kick / ban-ip / log-level / quit`, localhost-bind), optional UDP shim wire-faithful to legacy TLogSvr |
| Configuration | Hard-coded constants + Win32 Registry | TOML (`tloginsvr.toml`) — version-controllable, code-reviewable |
| Modularity | God class `CTLoginSvrModule` (~9 200 LOC) | Interface-driven (`IAuthService`, `ICharService`, `IConnectionRegistry`, `IMapServerLocator`, `ISessionTerminator`, `IEventRegistry`, `ISmtpClient`, `IAuditLogger`) — production SOCI impl + test fakes per interface |
| Tests | 0 | 17 ctest suites — wire codec, handler dispatch, per-service business logic, SOCI integration (skips if no DB env) |
| Anti-cheat (HWID) | `HwidManagerSvr` (Win-only, not wired into auth in shipped build anyway) | Out of scope by design |

## Handler coverage matrix (current)

All 20 messages (15 CS_/SM_ + 5 CT_) are **real, not stubs** in the
modern server. The original Phase-A matrix is preserved in Appendix A.

| ID | Legacy backing | Modern backing |
|---|---|---|
| CS_LOGIN_REQ | `CSPLogin` / `CSPLoginJP` SP | `SociAuthService::Authenticate` — BCrypt-only, IP banlist (2 tables), `TUSERPROTECTED`, `bLocked`-kick, `TUSERINFOTABLE` agreement, `llChecksum`, JP/TW DWORD `dwSiteCode` |
| CS_AGREEMENT_REQ | `CSPAgreement` SP | `SociAuthService::SetAgreement` + per-session gate flip |
| CS_GROUPLIST_REQ | `CTBLGroupList` query | `SociMapServerLocator::ListGroups` — live `TCURRENTUSER` counts, per-user `bCount` |
| CS_CHANNELLIST_REQ | `CTBLChannel` query | `SociMapServerLocator::ListChannels` |
| CS_CHARLIST_REQ | `TCHARTABLE` + `TITEMTABLE` + guild | `SociCharService::List` — items + guild fame; trailing `CS_BOWPLAYERNOTIFY_ACK`; correct 12-byte equipment layout including `wCustomTex` |
| CS_CREATECHAR_REQ | `TCreateChar` SP | `SociCharService::Create` — `TCHARTABLE` + `TALLCHARTABLE` + starter items |
| CS_DELCHAR_REQ | `CSPCheckPasswd` + `CSPDeleteChar` | `SociCharService::Delete` + `SociAuthService::VerifyPassword` (BCrypt); guild block + level-5 soft/hard split |
| CS_START_REQ | `CSPFindServerID` + `CSPRoute` (+BR override) | `SociMapServerLocator::Lookup` — TFindServerID port, BR/BOW shard, fallback to TSPAWNPOSCHART |
| CS_VETERAN_REQ | `TVETERANCHART` re-read | `SociCharService::GetVeteranLevels` (cached, 30 s refresh) |
| CS_TERMINATE_REQ | `TLogoutAll` on magic key | Legacy magic check + `ISessionTerminator` cleanup |
| CS_HOTSEND_REQ | exec-file heartbeat (anticheat) | Silently dropped — intentionally out of scope |
| CS_SECURITYCONFIRM_ACK | Dead code | Real — `TSECURECODE` compare + `ISmtpClient` for issuance |
| CS_TESTLOGIN_REQ | `TTESTLOGINUSER` SP | Real, gated `test_handlers_enabled = false` by default |
| CS_TESTVERSION_REQ | Returns version | Real, same gate |
| SM_QUITSERVICE_REQ | Service-manager shutdown | Real — wire-protocol → `io.stop()` via `on_quit_request` hook |
| CT_SERVICEMONITOR_ACK | Returns session counts | Real — `IConnectionRegistry::Count()` |
| CT_SERVICEDATACLEAR_ACK | Rebuild `m_mapACTIVEUSER` | No-op (registry is canonical, no derived map to rebuild) |
| CT_CTRLSVR_REQ | No-op heartbeat | Real — heartbeat |
| CT_EVENTUPDATE_REQ | GM event sync | `IEventRegistry` upsert/remove (matches legacy `m_mapEVENT` semantics) |
| CT_EVENTMSG_REQ | Event broadcast | Logs broadcast |

Summary: **20/20 messages real**, **0 stubs**, **0 intentional gaps
beyond HwidManagerSvr / HotSend anticheat tooling**.

## Security comparison — line-level citations

| Finding | Legacy location | Modern remediation |
|---|---|---|
| Plaintext password passed to SP | `Server/TLoginSvr/CSHandler.cpp:239, 323`, `Server/TLoginSvr/DBAccess.h:29` (`PARAM_ENTRY_STR(SQL_PARAM_INPUT, m_szPasswd)`) | `Server/TLoginSvrAsio/services/soci_auth_service.cpp::CheckPassword` — BCrypt only, no plaintext branch |
| Password leaked into trace log | `Server/TLoginSvr/CSHandler.cpp:441` (`ATLTRACE(...Passwd=%s..., pUser->m_strPasswd)`) | Audit log emits `uid` and `outcome`, never the password (`SpdlogAuditLogger`) |
| Unbounded `lstrcpy` into fixed buffers | `Server/TLoginSvr/CSHandler.cpp:212, 238-240, 322-324, 1096, 1244, 1524` | `handlers.cpp::ParseLoginReq` and friends bound every read at `MAX_NAME`, malformed body → session close |
| Single global lock | `Server/TLoginSvr/TLoginSvrModule.h:33` (`CRITICAL_SECTION m_csLI`), wrapped 14× across `CSHandler.cpp` | `LocalConnectionRegistry::m_mtx`, `LocalEventRegistry::m_mtx`, `LoginRateLimiter` — discrete mutexes per service |
| No rate limit | Not present | `fourstory::ops::LoginRateLimiter` |
| No pre-auth timeout | Not present | `login_server.cpp:178-204` — `steady_timer` watchdog |
| No max-connections cap | Not present | `login_server.cpp:91-106` — `m_active_connections` atomic + `max_connections` config |
| Stale session pile-up after crash | Not present | `SociSessionTerminator::ClearStaleSessions` on boot + `graceful_shutdown` sweep on SIGINT/SIGTERM |
| CT_* gate trust-on-first-IP | `Server/TLoginSvr/TLoginSvr.cpp::Accept` checks `m_addrCtrl` but no fail-safe | `main.cpp:113-123` refuses to start when gate isn't pinned; `Dispatch` drops CT_* from non-control peers and logs |
| Stored procedures dropped (less attack surface in DB itself) | 19 SPs, schema-bound | Inline parameterized SOCI queries — easier to audit, harder for an attacker who pivots through SQL to find ambient permissions |

## Performance comparison

| Aspect | Legacy | Modern |
|---|---|---|
| Reactor scaling | Fixed `MAX_THREAD` IOCP workers, hand-managed | Asio M:N scheduler, scales with `std::thread::hardware_concurrency()` |
| Lock contention under high QPS | Single `m_csLI` mutex over `m_mapTSESSION`, `m_mapTUSER`, `m_mapACTIVEUSER`, `m_mapEVENT` simultaneously | Per-service mutexes; auth → connection registry → audit log → rate limiter don't contend |
| DB serialization | One `CSqlDatabase` instance, effectively serialized | `SessionPool` (default 8 connections × 2 schemas = 16 concurrent queries); `AcquireTimeout` gracefully sheds load instead of stalling |
| Allocations per packet | `new CPacket()` per group/channel/char ack (`CSHandler.cpp:497, 558, 718, 904`) — risk of leak on early-return | `std::vector<std::byte>` packet body, copied once into the dispatch frame |
| Hot data caching | Re-query TVETERANCHART per request | `RegistryRefresher` caches chart with 30 s tick |
| Cold-start latency | Re-reads everything, no schema check, no banner | Schema validator + stale-session sweep + listener open — fail fast on schema drift |
| Throughput ceiling | Bottlenecked by `m_csLI` + single DB session | Bottlenecked by DB pool size (tunable) and downstream MSSQL itself |

## Code-quality comparison

| Aspect | Legacy | Modern |
|---|---|---|
| Encapsulation | God class `CTLoginSvrModule` aggregating threading, sockets, DB, all handlers, configuration | DI via interfaces; each `*Service` is independently testable + swappable |
| Test coverage | 0 tests | 17 ctest suites (handlers, wire codec, services, end-to-end with SOCI) |
| Static analysis surface | C-style strings + raw pointers + Win32 handles | RAII, `std::shared_ptr`, `std::vector`, `std::optional`, `std::span` — analyzable by clang-tidy / cppcheck out of the box |
| Logging | `ATLTRACE`, ad-hoc `LogEvent`, dberror file | spdlog with leveled output, structured fields, optional UDP wire-faithful sink |
| Configuration | Win32 Registry + hard-coded paths (`C:\4s\dberror.log`) | TOML, env-var overrides, annotated example |
| Portability | Windows only | Linux + Windows, CI buildable on both |
| Build system | MSVC vcxproj | CMake + vcpkg / distro packages |
| Documentation | Code comments in mixed Korean/English, mostly stale | Per-file header comment block, README with operational runbook, 14-row "institutional memory" audit table |

## When the legacy server still has value

Not "never". A few cases:

1. **Wire-protocol reference** — `CSHandler.cpp` is the canonical
   spec for what bytes the shipped client expects. Modern server
   was written against it; if there's ever a parser discrepancy, the
   legacy parser is the tiebreaker.
2. **A/B smoke test** — `tloginsvr.exe` and `tloginsvr_asio` can run
   side-by-side on different ports; route a fraction of traffic
   through the modern binary during cutover.
3. **Forensic reproduction** — if a wire bug is reported against an
   old client patch level, the legacy server is the bug-compatible
   peer that lets you reproduce.

Beyond that, there's no reason to deploy `TLoginSvr.exe` to a
production-facing port.

## Recommended next steps

1. **Production cutover** — schedule a maintenance window, run
   `tloginsvr_bcrypt_migrate --apply` against `TGLOBAL_RAGEZONE`,
   stop the legacy service, start `tloginsvr_asio` against the same
   port. Pin `[server] control_server_ip` to TControlSvr's address.
2. **Anti-cheat decision** — HShield / XTrap / NPGame are Windows-
   only proprietary blobs the legacy server links. If you go
   Linux-native for the login server (recommended), document the
   anti-cheat regression for the gameplay-server team; their
   shipped Map/World binaries still need the cheat hooks.
3. **Sibling servers** — `TPatchSvrAsio` and `TLogSvrAsio` are
   already in `Server/`; their cutover follows the same pattern.
   `TControlSvr` modernization is pending (Phase D.2 — see roadmap
   in `Server/TLoginSvrAsio/README.md`).
4. **Retire legacy tree** — once cutover is stable for one release
   cycle, move `Server/TLoginSvr/` to an `_archive/` directory so
   the source tree no longer presents it as a deployable target.

---

# Appendix A — Pre-Phase-B snapshot (2026-05-18)

> The text below was the original contents of this file before Phase
> B (DB backends) started. It is preserved verbatim so reviewers
> tracing why a given interface was shaped the way it is have the
> historical "we knew it was a stub" record. Everything called out
> as a gap / TODO / "Phase B" has since landed — see the top half of
> this document and `Server/TLoginSvrAsio/README.md` for the current
> state.

Compare `Server/TLoginSvr/` (Win32/ATL/IOCP/ODBC, ~11 444 LOC) with
`Server/TLoginSvrAsio/` (C++20/Asio/spdlog/TOML, ~1 500 LOC after
Phase A).

## TL;DR (historical)

Phase A scaffolding is complete: 12/12 handlers wired into the
dispatcher, 5 of them with real (non-stub) logic, the wire codec is
byte-for-byte legacy-compatible (RC4 + XOR), and the full operations
surface is up (config, health, logging, signals). What's missing is
the part that owns persistent state: **all five `*Service`
interfaces have only in-memory implementations**, and **7 of the 12
handlers are stubs because they have nothing to talk to**. Phase B
ports the DB layer; once it lands, the same handlers + tests start
serving real production traffic against the legacy MSSQL schema.

## Handler coverage matrix (historical Phase-A view)

| ID | Legacy handler | New status | Real impl path |
|---|---|---|---|
| CS_LOGIN_REQ | Full DB auth via CSPLogin / CSPLoginJP | **Real** (in-memory backend) | IAuthService.Authenticate |
| CS_GROUPLIST_REQ | Iterates TGROUP via CTBLGroupList | Stub (empty list) | needs IMapServerLocator extension OR new IWorldDirectory |
| CS_CHANNELLIST_REQ | Iterates TCHANNEL via CTBLChannel | Stub (empty list) | same as GROUPLIST |
| CS_CHARLIST_REQ | TCHARTABLE + TITEMTABLE + guild join | **Real** (items hardcoded empty) | ICharService.List |
| CS_CREATECHAR_REQ | TCreateChar SP + starter inventory | **Real** (no starter items) | ICharService.Create |
| CS_DELCHAR_REQ | password check + guild block + level-5 soft-delete | **Real** (password ignored, no guild check) | ICharService.Delete + IAuthService.VerifyPassword |
| CS_START_REQ | TSERVER+TIPADDR JOIN + BR override | **Real** (no BR override) | IMapServerLocator.Lookup |
| CS_AGREEMENT_REQ | Upserts TUSERINFOTABLE.bAgreement | Stub (log only) | needs IAuthService extension OR new IAgreementService |
| CS_HOTSEND_REQ | exec-file integrity check | Stub (log only) — feature off by design | none planned |
| CS_VETERAN_REQ | Reads TVETERANCHART thresholds | Stub (bOption=0, no bonus) | needs IAuthService extension OR new IVeteranService |
| CS_TERMINATE_REQ | TLogoutAll on magic-key match | Partial (just Close()) | ISessionTerminator already wired on close |
| CS_SECURITYCONFIRM_ACK | LR_SECURITY 2FA flow — dead code in legacy | Stub (always CODE_CORRECT) | none planned (parity with legacy) |
| CS_TESTLOGIN_REQ | Debug-only | NOT IMPLEMENTED (intentional skip) | none planned |
| CS_TESTVERSION_REQ | Debug-only | NOT IMPLEMENTED (intentional skip) | none planned |
| CT_SERVICEMONITOR_ACK | Returns session counts to control server | NOT IMPLEMENTED | Phase D — control protocol |
| CT_SERVICEDATACLEAR_ACK | Rebuilds m_mapACTIVEUSER from m_mapTUSER | NOT IMPLEMENTED | Phase D |
| CT_CTRLSVR_REQ | No-op heartbeat | NOT IMPLEMENTED | Phase D |
| CT_EVENTUPDATE_REQ | GM event sync (in-game events) | NOT IMPLEMENTED | Phase D |
| CT_EVENTMSG_REQ | Event broadcast — no-op in legacy | NOT IMPLEMENTED | Phase D |
| SM_QUITSERVICE_REQ | Service manager shutdown signal | NOT IMPLEMENTED — replaced by SIGINT/SIGTERM | done differently |

Summary: **5/14 functional CS_ handlers have real logic**, **7/14 are stubs** (have real wire format but no backing logic), **2/14 intentionally skipped** (debug-only). All **6 CT_/SM_ handlers** are deferred to a later "control protocol" phase that needs the GM tooling work too.

## Per-handler business-logic gaps (historical)

For each Phase-A handler that's classified as "Real", what specific
DB-driven behavior the in-memory impl doesn't reproduce.

### CS_LOGIN_REQ
Legacy does:
1. Wire-version check
2. Checksum validation against client-side computation
3. IP block via `CSPCheckIP` (table TIP_BLOCK)
4. Auth via `CSPLogin` (or `CSPLoginJP` for nation==JP) → reads TACCOUNT + TACCOUNT_PW, derives KEY
5. On LR_DUPLICATE: kicks previous session
6. Stamps `pUser->m_dwID/m_bLogout/m_bAgreement/m_bCreateCnt/m_dwAcceptTick/m_dlCheckKey`
7. Inserts into `m_mapTUSER` + `m_mapACTIVEUSER`
8. UdpSocket::LogLogin to TLogSvr

New (real path via IAuthService + IConnectionRegistry):
1. Wire version check ✅
2. **Checksum validation missing** — TODO
3. **IP block missing** — InMemoryAuthService has IpBan, no `client_ip` plumbed in
4. Auth ✅ (plain-text comparison in InMemory; Phase B → BCrypt)
5. Duplicate-kick ✅ ("newest wins" — divergence from legacy "both die")
6. Per-session state — stored in ConnectionEntry (user_id + session_key + handoff flag)
7. Registry tracks session ✅
8. **UDP audit log missing** — Phase D (replace with spdlog→Seq/Loki sink)

### CS_CHARLIST_REQ
Legacy joins TCHARTABLE + TITEMTABLE (equip slot) + guild for fame.

New ICharService.List returns characters but **items array is hardcoded empty** (Phase A doc'd limitation) and **no guild fame** (no guild service yet). The wire format slot for items is correct (`bEquipItemCount = 0`); the client renders chars without equipped gear thumbnails until items land.

### CS_CREATECHAR_REQ
Legacy `TCreateChar` SP also:
- Inserts starter inventory items into TITEMTABLE
- Records into TALLCHARTABLE (cross-world char index)
- Decrements `m_mapCurrentUser[groupID]` capacity counter (only when first char in group)
- Veteran-bonus level override from `m_vVETERAN` table

New InMemoryCharService:
- Validates name charset + length ✅
- Checks unique name + slot ✅
- **No starter inventory** — TODO
- **No TALLCHARTABLE mirror** — InMemory has single map
- **No group capacity tracking** — needs CTBLGroupList equivalent
- **No veteran-bonus path** — Phase A returns level=1

### CS_DELCHAR_REQ
Legacy:
1. `CSPCheckPasswd` — verify account password before delete
2. `CSPDeleteChar`:
   - Block delete if char is in a guild
   - Soft-delete (bDelete=1) if level > 5; hard delete otherwise

New:
1. **Password ignored** — InMemory always succeeds; Phase B needs `IAuthService.VerifyPassword(user_id, password)` method
2. Delete just removes — **no guild check**, **no level-5 split** — both belong in SociCharService

### CS_START_REQ
Legacy:
1. `CSPFindServerID` — find map server hosting the char
2. `CSPFindBOWPlayer` / `CSPFindBRPlayer` — PvP shard override (BR shard ID = 50)
3. `CSPRoute` — resolve (group, serverID, type=4) → IP:port

New IMapServerLocator.Lookup:
1. Single map by group_id → endpoint ✅
2. **No BR override** — needs internal JOIN with TBRPLAYERTABLE inside SociMapServerLocator
3. **channel ignored** — needs per-channel sharding eventually

## Stored procedure & table inventory (for Phase B backlog)

19 unique SPs across 14 tables. Phase B implementations don't have
to literally call these SPs (we can rewrite the queries inline in
C++) but the *behavior* of each is the spec for what to reproduce.

### Tables touched

| Table | Purpose | Read by | Written by |
|---|---|---|---|
| TACCOUNT | account master | CSPLogin, CSPCheckPasswd, CSPAgreement | CSPAgreement |
| TACCOUNT_PW | password storage | CSPLogin, CSPCheckPasswd | (BCrypt upgrade Phase C) |
| TCHARTABLE | per-world char records | CTBLChar, CSPFind*, CSPCreateChar, CSPDeleteChar | CSPCreateChar, CSPDeleteChar |
| TALLCHARTABLE | cross-world char index | CTBLGroupList | CSPCreateChar, CSPDeleteChar |
| TITEMTABLE | inventory | CTBLItem | CSPCreateChar (starter items) |
| TGROUP | world group metadata | CTBLGroupList | (admin only) |
| TCHANNEL | channel metadata | CTBLChannel | (admin only) |
| TCURRENTUSER | live session directory | CTBLGroupList, CTBLChannel | login (insert), logout (delete), START_REQ (handoff) |
| TLOG | audit trail | — | login (insert), logout (timestamp update) |
| TIP_BLOCK | IP banlist | CSPCheckIP | (admin only) |
| TSERVER | map server registry | CSPRoute | (admin only) |
| TIPADDR | machine→IP map | CSPRoute | (admin only) |
| TBRPLAYERTABLE | BR-eligible chars | CSPFindBRPlayer | (gameplay event) |
| TVETERANCHART | veteran bonus thresholds | CSPVeteran (commented out) | (admin only) |
| TUSERINFOTABLE | per-account agreement flag | CSPAgreement | CSPAgreement |
| (guild tables) | guild membership + fame | CSPGetGuildInfo | (gameplay) |
| (MAC_ADDRESS) | 2FA device registry | — | CSPAddNewMACAddress (dead code) |

### SPs grouped by Phase-B service

**SociAuthService** (covers CS_LOGIN_REQ + CS_AGREEMENT_REQ + CS_DELCHAR_REQ password check):
- `CSPCheckIP` → INSERT-OR-SELECT against TIP_BLOCK
- `CSPLogin` → JOIN TACCOUNT + TACCOUNT_PW + TUSERINFOTABLE; insert TCURRENTUSER on success
- `CSPLoginJP` → Japan variant (skip if nation != JP)
- `CSPCheckPasswd` → SELECT TACCOUNT_PW WHERE dwUserID=?; BCrypt verify
- `CSPAgreement` → UPSERT TUSERINFOTABLE.bAgreement

**SociCharService** (covers CS_CHARLIST + CS_CREATECHAR + CS_DELCHAR):
- `CTBLChar` → SELECT TOP 6 from TCHARTABLE
- `CTBLItem` → SELECT TITEMTABLE WHERE bStorageType=Inven AND dwStorageID=EQUIP
- `CSPGetGuildInfo` → JOIN guild tables for fame+color
- `CSPCreateChar` → INSERT TCHARTABLE + TALLCHARTABLE + starter TITEMTABLE rows + `m_mapCurrentUser` increment
- `CSPDeleteChar` → check guild → UPDATE TCHARTABLE.bDelete=1 (level>5) or DELETE row + capacity decrement

**SociMapServerLocator** (covers CS_START_REQ + CS_GROUPLIST_REQ + CS_CHANNELLIST_REQ):
- `CSPFindServerID` → SELECT TSERVER WHERE dwCharID=?
- `CSPFindBOWPlayer` → SELECT FROM TBOWPLAYER WHERE dwCharID=?
- `CSPFindBRPlayer` → SELECT FROM TBRPLAYERTABLE WHERE dwCharID=?
- `CSPRoute` → JOIN TSERVER + TIPADDR for active map server
- `CTBLGroupList` → enumerate TGROUP with live user counts via TCURRENTUSER
- `CTBLChannel` → enumerate TCHANNEL for a group

**SociSessionTerminator** (covers close path):
- `TCURRENTUSER` DELETE on Disconnect/ClientRequest
- `TCURRENTUSER` no-op on MapHandoff (Map server validates the key)
- `TLOG` INSERT login timestamp; UPDATE timeLOGOUT on close

**SociVeteranService** (new, covers CS_VETERAN_REQ):
- Read TVETERANCHART thresholds; return bOption=3 with values

## Legacy features missing in new (historical)

| Feature | Legacy code | Phase to add | Notes |
|---|---|---|---|
| UDP audit log → TLogSvr | UdpSocket::LogLogin / LogCharCreate / LogCharDelete / LogGameStart | Phase D | Replace with spdlog→Seq/Loki/Splunk sink — modern equivalent |
| Exec-file integrity check | CheckFile + LogExecCheck | Out of scope | Feature disabled in shipped legacy build per COMPLETENESS_ANALYSIS.md §10 |
| Japan-channeling auth | `CSPLoginJP` + `bChanneling` byte | Optional Phase B | Only fires when nation == JP; skip unless deploying to JP |
| Test login / test version | CS_TESTLOGIN_REQ, CS_TESTVERSION_REQ | Out of scope | Debug-only; modern equivalent is integration tests |
| Service manager shutdown | SM_QUITSERVICE_REQ | Done differently | SIGINT/SIGTERM via boost::asio::signal_set already wired |
| Control server protocol | 5 CT_* handlers | Phase D | Needs GM tooling work too — own milestone |
| LR_SECURITY 2FA flow | CS_SECURITYCONFIRM_ACK | Out of scope | Dead code in shipped legacy (COMPLETENESS_ANALYSIS.md §1 path commented out) |
| m_dwAcceptTick timeout monitoring | CT_SERVICEMONITOR_ACK side | Phase D | Detect stuck-pre-auth sessions; replace with idle-timeout in AsioSession |
| Hardcoded path `C:\4s\dberror.log` | TLoginSvr.cpp config | Fixed in legacy + new | New uses TNETLIB_DB_ERROR_LOG env var (committed `48f73d4`) |
| Windows Registry config | CTLoginSvrModule::LoadConfig | Done differently | New uses tloginsvr.toml |

## New features not in legacy (historical)

| Feature | Why it's a win | Tracked since |
|---|---|---|
| Cross-platform build (Linux + Windows) | Operability — Docker/k8s deploys, Linux servers cheaper than Windows | Phase 1 |
| spdlog structured logs | Greppable, machine-readable, sink-pluggable (Seq, Loki, ELK) | Phase A.1 |
| /healthz HTTP endpoint | k8s liveness/readiness probes, load balancer health checks | Phase A.3 |
| TOML config (vs Registry) | Version-controllable, code-reviewable, testable | Phase A.2 |
| Service interface pattern | Phase B can swap backends without touching handlers; unit-testable | Phase A.1 |
| 10 ctest targets / 155+ KATs | Regression coverage the legacy server never had | All phases |
| Pre-auth RCE fixed in operator>>(CString&) | One critical CVE-class bug closed in TNetLib | security audit |
| OpenSSL EVP (vs ATL/Win32 CryptoAPI) | Modern crypto provider, FIPS-able, audited | Phase 1 step C |
| BCrypt-ready password interface | Legacy uses plaintext or weak hash; new ready for proper hashing | Phase A.1 (impl Phase B) |
| Duplicate-kick "newest wins" | UX improvement — user can recover from stuck old session | Phase A.4 |
| TerminationReason enum (MapHandoff preserves TCURRENTUSER) | Cleaner contract than legacy m_bLogout bool gymnastics | Phase A.7 |
| Rate-limit hook (in design) | Throttle pre-auth login attempts — legacy has nothing | Phase C |

## Phase B recommended sequence (historical — now landed)

Order matters because services have dependencies — Char needs Auth's
VerifyPassword, MapLocator needs to know what's a valid group, etc.

1. **B.0 — SOCI infrastructure** (1 day)
   - vcpkg dep already declared
   - Connection pool wrapper (max N concurrent sessions, recycle on error)
   - Async dispatch helper: `asio::post(thread_pool, [&] { soci::statement … })` then `co_await` the future on the calling coroutine
   - Schema validator: on boot, verify expected tables + columns exist; fail fast if not

2. **B.1 — SociAuthService** (2-3 days)
   - `Authenticate` → CSPCheckIP + CSPLogin
   - `VerifyPassword` (new method) → CSPCheckPasswd
   - `AcceptAgreement` (new method) → CSPAgreement
   - BCrypt password verify + transparent upgrade (`needs_rehash` flag for legacy plaintext rows)
   - Cross-cuts: ip_check stage needs `client_ip` plumbed from accept socket — TODO in AsioSession to pass remote_endpoint() through

3. **B.2 — SociCharService** (2-3 days)
   - `List` → CTBLChar + CTBLItem JOIN + guild JOIN
   - `Create` → TCreateChar replica (TCHARTABLE + TALLCHARTABLE + starter items)
   - `Delete` → TDeleteChar replica (guild block + level-5 soft delete)
   - Composes with IAuthService for delete-password check

4. **B.3 — SociMapServerLocator** (1-2 days)
   - `Lookup` → CSPFindServerID + CSPRoute internally; BR override via TBRPLAYERTABLE
   - New methods: `ListGroups()`, `ListChannels(group_id)` for CS_GROUPLIST/CHANNELLIST handlers

5. **B.4 — SociSessionTerminator** (1 day)
   - Direct DELETE TCURRENTUSER on Disconnect/ClientRequest
   - INSERT TLOG on login (need a hook in OnLoginReq), UPDATE timeLOGOUT on Terminate
   - MapHandoff = no-op (matches legacy expectation)

6. **B.5 — Phase-A stub handlers gain real impls** (1 day)
   - OnGroupListReq → SociMapServerLocator.ListGroups
   - OnChannelListReq → SociMapServerLocator.ListChannels
   - OnAgreementReq → SociAuthService.AcceptAgreement + per-session flag
   - OnVeteranReq → new SociVeteranService.GetThresholds

7. **B.6 — Cross-cutting** (1-2 days)
   - Plumb `remote_endpoint().address().to_string()` from accept socket into AuthRequest.client_ip
   - Add ICharService.SetStarterInventory / refactor Create for items
   - Schema validator wired into main startup
   - Production config: actual MSSQL conn string in tloginsvr.toml secret-management story

Total: ~12 days of focused work for one developer. Half is service
impls; quarter is plumbing; quarter is the gap-fill (stubs → real).

## Risks for Phase B (historical)

| Risk | Severity | Mitigation |
|---|---|---|
| Synchronous IAuthService called from Asio coroutine blocks the io_context | High | Either: (a) use asio::post(thread_pool) inside Soci impls + co_await; (b) keep services sync + dedicated worker pool with bounded queue. Decision in B.0. |
| Wire format byte-level diff vs real legacy client untested | High | Capture a packet trace from the legacy client during a real login; replay against tloginsvr_asio in a test; assert byte-by-byte ack match. |
| TCURRENTUSER race conditions with multi-process / sharded login | Medium | In-process IConnectionRegistry is correct; sharded deploy needs Redis-backed impl. Document for now. |
| MapHandoff TCURRENTUSER row leaks if Map reconnect never happens | Medium | Add a periodic sweeper job (delete rows where dEnterDate < now() - 5min AND no Map activity). Phase B.4. |
| BCrypt latency on legacy-plaintext upgrade path | Low | Upgrade only fires on a successful plaintext-match login (rare in practice). Bounded. |
| Schema drift between dev MSSQL and prod | Medium | Schema validator in B.0 + CI integration test running against a fixture DB. |
| Connection pool exhaustion under login storm | Medium | Pool size tunable in config; rejection path emits LR_INTERNAL with 503-equivalent in spdlog. |
| Wire-format checksum validation (CS_LOGIN_REQ trailing INT64 llChecksum) currently ignored | Low | Phase B can either implement the legacy checksum algo (documented in tnetlib::packet_codec wire notes) or treat its absence as a known limitation. Defensive only — clients aren't normally hostile. |

## Open questions for production rollout (historical)

These aren't blockers for Phase B; they're decisions someone needs to
make before flipping the cutover switch.

1. **A/B deployment** — run tloginsvr_asio alongside legacy on different ports + load-balance fraction of traffic through; or full cutover after Phase B?
2. **Schema migration plan** — keep using legacy MSSQL schema forever (Phase B target) or migrate to PG sometime later?
3. **Sharding** — single-process login per region or multiple behind a load balancer? (Affects IConnectionRegistry impl choice.)
4. **Anti-cheat hooks** — HShield, XTrap, NPGame are Windows-only proprietary blobs the legacy server links. Modern server doesn't link them. Production deploys on Linux need those features re-evaluated.
5. **TLogSvr UDP audit** — preserve the legacy UDP audit protocol for back-compat with existing log-analysis pipelines, or replace with structured-log shipping (spdlog→Seq)?
6. **Captured-packet test corpus** — do we have any real legacy-client packet captures we can replay for fidelity testing?

## What's actually working today (historical — Phase A)

Smoke-testable in this commit:

```bash
$ ./build/bin/tloginsvr_asio --config tloginsvr.toml
[info] login server listening on 0.0.0.0:4815 (RC4: enabled)
[info] health endpoint listening on 0.0.0.0:8815

# Real legacy client (in theory — wire-format compatible) can connect
# and walk through the LOGIN → CHARLIST → CREATECHAR → START flow
# end-to-end. It'll authenticate against any in-memory seeded user
# (which is nobody by default — Phase B wires the real DB).
```

The 10 ctest targets verify the wire codec + handler dispatch + per-service business logic with in-memory backends. None of them touch a real DB. Once Phase B lands, each in-memory backend has a Soci sibling and the same tests run against a containerized MSSQL fixture in CI.
