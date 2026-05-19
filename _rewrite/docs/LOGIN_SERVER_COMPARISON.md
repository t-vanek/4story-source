# Login Server: Legacy vs. New — Pre-Phase-B Analysis

> **Status: historical snapshot from 2026-05-18.** This file captures
> the gap analysis taken right before Phase B (DB backends) started.
> Phases B, C, D and a seven-round wire-compatibility audit have
> since landed all the items called out below. For the current
> state and the audit log, see
> [`Server/TLoginSvrAsio/README.md`](../../Server/TLoginSvrAsio/README.md)
> — its "Wire-compatibility fixes" table indexes the 14 follow-up
> patches (`37044bf..HEAD` on `claude/compare-login-servers-zeUDp`).
>
> Keeping this file around as institutional memory: the original
> "TL;DR" snapshot below is useful for reviewers tracing why a given
> interface was shaped the way it is.

Compare `Server/TLoginSvr/` (Win32/ATL/IOCP/ODBC, ~11 444 LOC) with
`Server/TLoginSvrAsio/` (C++20/Asio/spdlog/TOML, ~1 500 LOC after
Phase A).

## TL;DR

Phase A scaffolding is complete: 12/12 handlers wired into the
dispatcher, 5 of them with real (non-stub) logic, the wire codec is
byte-for-byte legacy-compatible (RC4 + XOR), and the full operations
surface is up (config, health, logging, signals). What's missing is
the part that owns persistent state: **all five `*Service`
interfaces have only in-memory implementations**, and **7 of the 12
handlers are stubs because they have nothing to talk to**. Phase B
ports the DB layer; once it lands, the same handlers + tests start
serving real production traffic against the legacy MSSQL schema.

## Handler coverage matrix

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

## Per-handler business-logic gaps

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

## Legacy features missing in new

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

## New features not in legacy

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

## Phase B recommended sequence

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

## Risks for Phase B

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

## Open questions for production rollout

These aren't blockers for Phase B; they're decisions someone needs to
make before flipping the cutover switch.

1. **A/B deployment** — run tloginsvr_asio alongside legacy on different ports + load-balance fraction of traffic through; or full cutover after Phase B?
2. **Schema migration plan** — keep using legacy MSSQL schema forever (Phase B target) or migrate to PG sometime later?
3. **Sharding** — single-process login per region or multiple behind a load balancer? (Affects IConnectionRegistry impl choice.)
4. **Anti-cheat hooks** — HShield, XTrap, NPGame are Windows-only proprietary blobs the legacy server links. Modern server doesn't link them. Production deploys on Linux need those features re-evaluated.
5. **TLogSvr UDP audit** — preserve the legacy UDP audit protocol for back-compat with existing log-analysis pipelines, or replace with structured-log shipping (spdlog→Seq)?
6. **Captured-packet test corpus** — do we have any real legacy-client packet captures we can replay for fidelity testing?

## What's actually working today

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
