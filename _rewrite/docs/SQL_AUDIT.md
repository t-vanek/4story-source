# SQL audit — modern login stack

**Scope:** `Server/TLoginSvrAsio` + `Server/TPatchSvrAsio` + `Server/TLogSvrAsio`.
**Date:** 2026-05-19.
**Method:** static cross-reference of schema DDL files vs all SQL string
literals in modern service code (`services/soci_*.cpp`,
`db/schema_validator.cpp`, `services/patch_repository.cpp`,
`services/log_sink.cpp`).

## TL;DR

- **Stored procedures: none.** Modern stack does **not** call any
  legacy SP. All SQL is inline parameterized queries through SOCI. The
  SP folder in `_rewrite/docs/schema/procs/` is reference-only for the
  Authenticate / CharCreate / CharDelete / SessionTerminate port logic.
- **Tables: 30 distinct tables referenced**, 23 covered by some dev
  fixture, **7 referenced only against the legacy production schema**
  (untested in dev).
- **5 actionable issues** below, ranging from trivial cleanup to one
  real footgun (`mssql-dev.sql` is missing tables the schema validator
  insists on — anyone seeding MSSQL from `mssql-dev.sql` alone gets
  a startup crash).

## Cross-reference matrix

Legend:
- **PG** — `Server/TLoginSvrAsio/schema/postgres-dev.sql`
- **MS** — `Server/TLoginSvrAsio/schema/mssql-dev.sql`
- **2FA** — `Server/TLoginSvrAsio/schema/2fa-tables.sql`
- **AUD** — `Server/TLogSvrAsio/schema/tlog-audit.sql`
- **CSV** — documented in `_rewrite/docs/schema/T*_RAGEZONE.tables.csv` (legacy prod)
- **VAL** — checked by `db/schema_validator.cpp` at startup
- **—** — referenced in code but no DDL in this repo

| Table | PG | MS | 2FA | AUD | CSV | VAL | Read by | Written by |
|---|---|---|---|---|---|---|---|---|
| `TACCOUNT` | ✓ | — | — | — | ✓ | — | (none — legacy) | (none) |
| `TACCOUNT_PW` | ✓ | ✓ | — | — | ✓ | ✓ | soci_auth_service | soci_auth_service |
| `TCURRENTUSER` | ✓ | ✓ | — | — | ✓ | ✓ | soci_auth, soci_map, soci_session | soci_auth, soci_session |
| `TLOG` | ✓ | ✓ | — | — | ✓ | ✓ | — | soci_auth, soci_session |
| `TUSERINFOTABLE` | ✓ | **✗** | — | — | ✓ | ✓ | soci_auth, soci_session | soci_auth, soci_session |
| `TUSERPROTECTED` | ✓ | ✓ | — | — | ✓ | ✓ | soci_auth | — |
| `IPBLACKLIST_game` | ✓ | ✓ | — | — | ✓ | ✓ | (handlers via legacy) | — |
| `TGROUP` | ✓ | **✗** | — | — | ✓ | ✓ | soci_map | — |
| `TCHANNEL` | ✓ | **✗** | — | — | ✓ | ✓ | soci_map | — |
| `TSERVER` | ✓ | **✗** | — | — | ✓ | ✓ | soci_map | — |
| `TIPADDR` | ✓ | **✗** | — | — | ✓ | ✓ | soci_map | — |
| `TVETERANCHART` | ✓ | **✗** | — | — | ✓ | ✓ | soci_char | — |
| `TPCBANG` | ✓ | ✓ | — | — | ✓ | — | soci_auth (optional) | — |
| `TUSERPREMIUM` | ✓ | ✓ | — | — | ✓ | — | soci_auth (optional) | — |
| `TCHARTABLE` | ✓ | **✗** | — | — | ✓ | ✓ | soci_char, soci_map | soci_char |
| `TALLCHARTABLE` | ✓ | **✗** | — | — | ✓ | ✓ | soci_char, soci_map | soci_char |
| `TKEEPINGNAME` | ✓ | **✗** | — | — | ✓ | ✓ | soci_char | — |
| `TRESERVEDNAME` | ✓ | **✗** | — | — | ✓ | ✓ | soci_char | — |
| `TGUILDMEMBERTABLE` | ✓ | **✗** | — | — | ✓ | ✓ | soci_char | — |
| `TGUILDTABLE` | ✓ | **✗** | — | — | ✓ | ✓ | soci_char | — |
| `TITEMTABLE` | ✓ | **✗** | — | — | ✓ | ✓ | soci_char | soci_char |
| `TBRPLAYERTABLE` | ✓ | **✗** | — | — | ✓ | — | soci_char, soci_map | — |
| `TBOWPLAYERTABLE` | ✓ | **✗** | — | — | ✓ | — | soci_map | — |
| `TUSEREMAIL` | — | — | ✓ | — | — | ✓ | soci_auth | — |
| `TUSERTRUSTEDIP` | — | — | ✓ | — | — | ✓ | soci_auth | soci_auth |
| `TSECURECODE` | **—** | **—** | — | — | ✓ | — | soci_auth | soci_auth |
| `TTESTLOGINUSER` | **—** | **—** | — | — | ✓ | — | soci_auth | — |
| `TSVRCHART` (VIEW) | **—** | **—** | — | — | views.sql | — | soci_map | — |
| `TCHANNELCHART` | **—** | **—** | — | — | ✓ | — | soci_map | — |
| `TSPAWNPOSCHART` | **—** | **—** | — | — | ✓ | — | soci_map | — |
| `TVERSION` | **—** | **—** | — | — | ✓ | — | patch_repository | — |
| `TPREVERSION` | **—** | **—** | — | — | ✓ | — | patch_repository | — |
| `TINTERFACECHART` | **—** | **—** | — | — | **—** | — | patch_repository (optional) | — |
| `TLOG_AUDIT` | — | — | — | ✓ | — | — | — | log_sink |

## Stored procedures referenced in comments (not invoked)

Modern code **inlines** all SP logic. The following legacy SP names
appear only in source-code comments as porting reference; none are
actually called via `CALL`/`EXEC`:

- `TLogin`, `TLogout`, `TOPLogin`, `TTestLogin`
- `TFindServerID`, `TGetServerID`, `TGetServerInfo`
- `TGetNation`, `TCheckIP`, `TCheckPasswd`, `TAgreement`
- `TCheckMapChar`, `TCreateChar`, `TDeleteChar`, `TRoute`
- `TMinBetaVer`, `CSPPreComplete`, `CSPClearLoginUser`, `CSPLogout`
- `TSaveSecureCode`

Definitions for all of these (except CSP*) are dumped in
`_rewrite/docs/schema/procs/`. The CSP* family belongs to the
TControlSvr admin path and is not relevant to the modern stack.

## Findings

### F1. `mssql-dev.sql` is incomplete vs schema validator (HIGH)

**File:** `Server/TLoginSvrAsio/schema/mssql-dev.sql`.

`mssql-dev.sql` defines only 7 tables (IPBLACKLIST_game, TACCOUNT_PW,
TUSERPROTECTED, TCURRENTUSER, TLOG, TPCBANG, TUSERPREMIUM). The
header comment says *"minimum tables to exercise SociAuthService"* —
but `ValidateGlobalSchema` (`db/schema_validator.cpp:13-70`) requires:

```
TUSERINFOTABLE, TSERVER, TIPADDR, TGROUP, TCHANNEL,
TVETERANCHART, TALLCHARTABLE, TRESERVEDNAME, TKEEPINGNAME,
TUSEREMAIL, TUSERTRUSTEDIP
```

Cascade effects:
- Booting modern login against a fresh MSSQL seeded only from
  `mssql-dev.sql + 2fa-tables.sql` fails at startup with `SchemaError`.
- `dev-account.sql:46` INSERTs into `TUSERINFOTABLE`, which doesn't
  exist in `mssql-dev.sql`. Running the seeder against an
  mssql-dev-only DB blows up before login is even tested.
- `ValidateWorldSchema` requires `TCHARTABLE`, `TITEMTABLE`,
  `TGUILDMEMBERTABLE`, `TGUILDTABLE` — none in `mssql-dev.sql`. If
  the global+world pool point at the same fixture (the typical "single
  DB for dev" setup) the world validator also fails.

**Fix:** mirror the full PG dev fixture to MSSQL. Either expand
`mssql-dev.sql` to include all 18 tables the validator checks, or
split into `mssql-dev-global.sql` + `mssql-dev-world.sql` matching
the validator's two-pool split.

### F2. Tables referenced in code but absent from all dev fixtures (MEDIUM)

These exist only in the legacy production CSV dump and are not
created by any DDL the modern stack ships:

| Table | Used in | Code wraps in try/catch? |
|---|---|---|
| `TSECURECODE` | `soci_auth_service.cpp:645,669,676,709-721` | yes (silent debug log) |
| `TTESTLOGINUSER` | `soci_auth_service.cpp:898,931` | yes (warns then no-op) |
| `TSPAWNPOSCHART` | `soci_map_server_locator.cpp:148` | yes (returns nullopt) |
| `TCHANNELCHART` | `soci_map_server_locator.cpp:119` | yes (via FindServerForChar) |
| `TSVRCHART` (VIEW) | `soci_map_server_locator.cpp:118` | yes (via FindServerForChar) |
| `TVERSION` | `patch_repository.cpp:24,120` | yes (error log) |
| `TPREVERSION` | `patch_repository.cpp:58,120` | yes (error log) |
| `TINTERFACECHART` | `patch_repository.cpp:89` | yes (debug log) |

Code defensively swallows the resulting `relation does not exist`,
so these don't crash a dev run — but it also means CI never
exercises the secure-code, test-login, per-char routing, or patch
paths against a real schema. Bugs in the SQL strings only surface
once someone points the modern stack at the legacy prod DB.

**Fix:** add DDL for these to `postgres-dev.sql` (or a sibling
fixture file) and mirror to MSSQL. Even stub tables with a single
row are enough to cover the happy paths. `TINTERFACECHART` is
likely region-specific and may not exist in legacy CSV — verify
before shipping a DDL guess.

### F3. Schema validator inconsistency: optional vs required (MEDIUM)

`schema_validator.cpp` is binary fail-fast — every column in its
list must exist or the server refuses to boot. But the SOCI services
themselves treat **most** of these tables as optional (try/catch
+ debug log). This creates two bugs:

- Tables that *are* optional in code but *required* in the validator:
  the validator over-promises. e.g. `TVETERANCHART` is best-effort
  in `soci_char_service.cpp:78-99` (empty rowset is fine), but the
  validator demands `bID` + `bLevel` columns.
- Tables that *are* required in code but *not* in the validator:
  `TSECURECODE`, `TTESTLOGINUSER`, `TBRPLAYERTABLE`, `TBOWPLAYERTABLE`,
  `TSPAWNPOSCHART`, `TCHANNELCHART`, `TSVRCHART`. If any of these is
  malformed, the failure surfaces deep in handler code at runtime.

**Fix:** decide which tables are "must exist on boot" vs "feature-
flagged at runtime", and align validator + code. Mark optional-feature
tables with a soft validator pass (log warning, don't throw).

### F4. `TACCOUNT` defined but never read (LOW)

**File:** `Server/TLoginSvrAsio/schema/postgres-dev.sql:20-29`.

`TACCOUNT` was the original (plaintext-password) account table.
Modern code reads/writes `TACCOUNT_PW` exclusively. `TACCOUNT` is
not in the validator's column list, not in `mssql-dev.sql`, and no
code path touches it.

**Fix:** delete the `TACCOUNT` block from `postgres-dev.sql`. Keep
the legacy CSV dump for historical reference but drop the dev fixture
overhead. (If kept for forward-compat with some legacy tool, add a
one-line comment explaining why.)

### F5. No schema validation in patch / log servers (LOW)

Only `TLoginSvrAsio` runs `ValidateGlobalSchema/WorldSchema` at
boot (`main.cpp:209,223`). `TPatchSvrAsio` and `TLogSvrAsio` go
straight to handling traffic; a missing `TVERSION` or misnamed
`TLOG_AUDIT` table only shows up on the first client request.

**Fix:** add a thin validator for each — `TPatchSvrAsio` checks
`TVERSION + TPREVERSION` (treat `TINTERFACECHART` as optional);
`TLogSvrAsio` checks the configured audit table name resolves.

### F6. `TLOG_AUDIT.LT_LOG` binding fragility (TRIVIAL)

**File:** `Server/TLogSvrAsio/services/log_sink.cpp:41`.

The INSERT uses `CONVERT(VARBINARY(512), :blob)` — MSSQL-specific
syntax. PG would need `:blob::bytea`. The TPatchSvr / login services
abstract over MSSQL vs PG via a `is_mssql` dialect check; `log_sink`
doesn't. Not currently a bug because `TLOG_AUDIT` is MSSQL-only by
design, but if anyone tries to run the log server against PG (e.g.
for dev) it explodes. Worth either adding the dialect branch or
documenting the MSSQL-only constraint in the header.

## Issues NOT found

For completeness, items checked and clean:

- **SQL injection** — every parameter is bound via `soci::use`; no
  string concatenation of user input into queries. The one
  interpolated identifier (`log_sink.cpp:29`, `m_table`) is a TOML
  config constant, not user input.
- **Cross-DB consistency** — TGLOBAL/TGAME pool split is honored
  (services that touch both, like `soci_char_service::DeleteChar`,
  open separate transactions on each pool — see lines 537-595).
- **MSSQL/PG dialect branches** — all places that need different
  syntax (TOP vs LIMIT, OUTPUT vs RETURNING, DATEADD vs interval
  arithmetic) check `m_pool.GetBackend()` and emit the right SQL.
- **Hard-coded credentials in DDL** — `dev-account.sql` ships a
  BCrypt-hashed dev password (`dev123`) — labelled as dev-only;
  not a finding. No other DDL contains creds.

## Recommended fix order

1. **F1** — incomplete `mssql-dev.sql` is the highest-impact
   actionable item; one PR to expand it to validator-parity.
2. **F2** — add the missing TGLOBAL/TGAME stubs so dev DB exercises
   the secure-code, test-login, and per-char routing paths.
3. **F3** — align validator strictness with code's actual
   tolerance. Smallest-LOC fix is to demote validator entries for
   optional tables (TVETERANCHART, TPCBANG, TUSERPREMIUM, TBRPLAYERTABLE,
   TBOWPLAYERTABLE) to warning-only.
4. **F5** — validators for patch + log servers; ~30 LOC each.
5. **F4** — drop `TACCOUNT` from PG dev DDL.
6. **F6** — dialect-aware `TLOG_AUDIT` binding or a header note.

---

# Part 2 — Legacy `TLoginSvr` vs modern `TLoginSvrAsio` DB coverage

**Question answered here:** does the modern login server cover every
DB operation the legacy login server performed?

**TL;DR — yes, with five intentional design diffs and three real
gaps.** All client-visible flows (LOGIN, CHARLIST, CREATECHAR,
DELCHAR, START, AGREEMENT, VETERAN, SECURITYCONFIRM) are covered.
Three lower-priority gaps documented below as **G1–G3**.

## Legacy DB-operation catalogue

Inventoried from `Server/TLoginSvr/DBAccess.h` (28 query classes) +
all `DEFINE_QUERY(...)` callsites in
`Server/TLoginSvr/{TLoginSvr.cpp,CSHandler.cpp,SSHandler.cpp}`.

### Bootstrap

| Legacy callsite | Operation | Modern equivalent | Status |
|---|---|---|---|
| `TLoginSvr.cpp:174` `CSPClearLoginUser` (`TClearLoginCurrentUser` SP) | wipe TCURRENTUSER on startup | `SociSessionTerminator::ClearStaleSessions` (`soci_session_terminator.cpp:112-117`) | ✓ covered |
| `TLoginSvr.cpp:355` `CSPLoadService` (`TLoadService` SP) | look up TControlSvr IP/port from DB | TOML `[control_server]` block in `tloginsvr.example.toml` | ✓ moved to config |
| `TLoginSvr.cpp:365` `CSPGetNation` (`TGetNation` SP) | read server nation from DB | TOML `nation = "..."` | ✓ moved to config |
| `TLoginSvr.cpp:373` `CTBLUserCount` | preload per-group user counts into RAM cache | none — modern queries live counts on demand in `GetGroupList` | ✓ design diff (cache eliminated) |
| `TLoginSvr.cpp:382` `CTBLVeteranChart` | preload veteran level table | `SociCharService::SociCharService` ctor (`soci_char_service.cpp:78-99`) | ✓ covered (with 30s refresh per README) |
| `TLoginSvr.cpp:434` `CTBLGroup` | load per-world DSN/credentials list | TOML `[[database.world]]` array | ✓ moved to config |

### Per-handler DB calls

| Wire message | Legacy SP/query | Modern equivalent | Status |
|---|---|---|---|
| `CS_LOGIN_REQ` | `CSPCheckIP` (`TCheckIP` SP) — IP banlist | `SociAuthService::Authenticate` step 1 (`soci_auth_service.cpp:109-114`) — inline `SELECT FROM IPBLACKLIST_game` | ✓ covered |
| `CS_LOGIN_REQ` | `CSPLogin` / `CSPLoginJP` (`TLogin` SP) — auth + session insert + premium/PC-Bang lookup | `SociAuthService::Authenticate` (entire method, with BCrypt transparent upgrade — see R6 + R12 in `Server/TLoginSvrAsio/README.md`) | ✓ covered + improved |
| `CS_TESTLOGIN_REQ` | `CSPTestLogin` (`TTestLogin` SP) | `SociAuthService::AuthenticateTest` (`soci_auth_service.cpp:898-963`) | ✓ covered |
| `CS_GROUPLIST_REQ` | `CTBLGroupList` (TGROUP + TCURRENTUSER + TALLCHARTABLE 3-way join) | `SociMapServerLocator::GetGroupList` (`soci_map_server_locator.cpp:372-460`) | ✓ covered (same 3-way join, inline) |
| `CS_CHANNELLIST_REQ` | `CTBLChannel` (TCHANNEL + TCURRENTUSER count) | `SociMapServerLocator::GetChannelList` (`soci_map_server_locator.cpp:462+`) | ✓ covered |
| `CS_CHARLIST_REQ` | `CTBLChar` (top 6 chars) | `SociCharService::GetCharList` SELECT TCHARTABLE | ✓ covered |
| `CS_CHARLIST_REQ` | `CTBLItem` (per-char equipped items, 4-column key) | `SociCharService::GetCharList` per-char inline query (line 187, 222) | ✓ covered (w/ R4 wCustomTex fix) |
| `CS_CHARLIST_REQ` | `CSPGetGuildInfo` (`TGetGuildInfo` SP) — guild fame/name | `SociCharService::GetCharList` inline TGUILDMEMBERTABLE → TGUILDTABLE join (line 261, 274) | ✓ covered |
| `CS_CHARLIST_REQ` | `CSPFindBOWPlayer` / `CSPFindBRPlayer` (cosmetic shard tag on charlist) | **not called from CharList in modern** — only from MapServerLocator | **G1 — minor gap** |
| `CS_VETERAN_REQ` | `CSPVeteran` (`TCheckVeteran` SP) — per-user veteran tier check | dead in legacy too (commented out, `CSHandler.cpp:1488-1499`); modern matches by sending bOption=3 from cached chart | ✓ covered (both sides static) |
| `CS_CREATECHAR_REQ` | `CSPVeteran` pre-check on `bLevelOption` | dead in legacy too (commented out, `CSHandler.cpp:1069-1091`) | ✓ covered |
| `CS_CREATECHAR_REQ` | `CSPCreateChar` (`TCreateChar` SP) — atomic char insert | `SociCharService::CreateChar` (`soci_char_service.cpp:399-490`) | ✓ covered |
| `CS_DELCHAR_REQ` | `CSPCheckPasswd` (`TCheckPasswd` SP) | `SociAuthService::CheckPassword` (`soci_auth_service.cpp:960-975`) — uses BCrypt verify on `TACCOUNT_PW.szPasswd` | ✓ covered (BCrypt-aware) |
| `CS_DELCHAR_REQ` | `CSPDeleteChar` (`TDeleteChar` SP) | `SociCharService::DeleteChar` (`soci_char_service.cpp:530-597`) — guild-leave check + level-aware soft/hard delete + TALLCHARTABLE soft-delete | ✓ covered |
| `CS_START_REQ` | `CSPFindServerID` (`TFindServerID` SP) — per-char zone routing | `SociMapServerLocator::FindServerForChar` (`soci_map_server_locator.cpp:80-159`) — same TSVRCHART⋈TCHANNELCHART⋈TSPAWNPOSCHART chain | ✓ covered |
| `CS_START_REQ` | `CSPFindBOWPlayer` / `CSPFindBRPlayer` (PvP shard membership) | `SociMapServerLocator::IsInShard` (line 197, 201) — direct SELECT against TBRPLAYERTABLE / TBOWPLAYERTABLE | ✓ covered |
| `CS_START_REQ` | `CSPRoute` (`TRoute` SP) — resolve final IP:port | `SociMapServerLocator::Lookup` inline TSERVER ⋈ TIPADDR (line 247-280) | ✓ covered |
| `CS_AGREEMENT_REQ` | `CSPAgreement` (`TAgreement` SP) | `SociAuthService::Agreement` (`soci_auth_service.cpp:577-616`) — handles INSERT-if-missing | ✓ covered |
| `CS_SECURITYCONFIRM_ACK` | `CSPAddNewMACAddress` (`TAddMacAddress` SP) | `SociAuthService::ConfirmSecurityCode` + `AddTrustedIP` (line 645-810) — stores **IP** in TUSERTRUSTEDIP, not MAC | **G2 — design diff** |
| Disconnect / logout | `CSPLogout` (`TLogout` SP) | `SociSessionTerminator::Terminate` (`soci_session_terminator.cpp:37-90`) | **G3 — partial coverage** (see below) |

### Defined-but-never-invoked legacy SPs (intentional)

These query classes exist in `DBAccess.h` but no callsite invokes
them. Modern omits them deliberately:

| Legacy class | SP | Why modern skips |
|---|---|---|
| `CSPCheckForbiddenHWID` | `TCheckHWID` | HWID anti-cheat marked out-of-scope (master README "Caveats") |
| `CSPCheckHwid` | `TCheckHwid` | conditional `#ifdef USE_HWID_AUTH` — disabled in legacy build |
| `CSPLogLoginAttempt` | `TLogLogin` | same `USE_HWID_AUTH` gate |
| `CSPGetClientSha` | `TGetClientSha` | exec-file integrity check; legacy build also dead |
| `CSPGetBanReason` | `TGetBanReason` | declared, never called by legacy. **Could** enrich modern ban response with reason text — currently modern sends only the numeric code. Minor UX gap if reused, see G2 notes. |

## Gaps in modern coverage

### G1. Modern CharList does not consult BR/BOW shard tables (LOW)

**Legacy:** `CSHandler.cpp:619-633, 802-816` — when building the
char list, legacy queries `CSPFindBOWPlayer` then `CSPFindBRPlayer`
on each group's DB. The returned `dwCharID` flags which character
sits on the BR/BOW shard. The result feeds into the per-char active
flag on the CHARLIST_ACK packet (and influences which char the
"Quick Start" button targets).

**Modern:** `soci_char_service.cpp::GetCharList` doesn't touch
either table. BR/BOW shard awareness only kicks in later, in
`SociMapServerLocator::IsInShard` at `CS_START_REQ` time.

**Impact:** clients on BR/BOW shards see the same char list as
PvE chars; the route happens correctly when they hit Start. Cosmetic
indicator missing (if the client renders one — needs verification
against the shipped client UI).

**Fix:** add a `world_pool.IsInShard` lookup in
`SociCharService::GetCharList` and tag the returned `CharSlot`
entries. ~20 LOC.

### G2. SECURITYCONFIRM stores IP, not MAC (DESIGN DECISION — note only)

**Legacy:** `CSHandler.cpp:1522` — after the security code is
confirmed, legacy stores the **MAC address** the client reported
during login (`pUser->m_strMacAddress`) via `TAddMacAddress` SP.

**Modern:** `SociAuthService::AddTrustedIP` stores the **client IP**
(`pUser->client_ip`) in `TUSERTRUSTEDIP`. MAC isn't captured at all.

**Why this is OK:** the legacy MAC was self-reported by the client
and trivially spoofable (no kernel-mode anti-cheat enforces it).
Modern IP-pinning is at least as strong against the threat model
that motivates 2FA (new-location detection). Plus IPv4 is
end-to-end observable by the server, whereas the legacy MAC was
just whatever the client typed.

**Action:** none — but worth documenting in the modern README so
operators don't expect MAC-based correlation in audit queries.

### G3. `SociSessionTerminator` skips TALLCHARTABLE play-time accumulation (LOW — design diff)

**Legacy `TLogout` SP** (`_rewrite/docs/schema/procs/TGLOBAL_RAGEZONE/TLogout.sql`):

```sql
-- Always:
UPDATE TLOG SET dwCharID, bGroupID, bChannel, timeLOGOUT WHERE dwKEY = ...

-- If char was logged in (@dwCurCharID <> 0 AND @dwCharID = @dwCurCharID):
UPDATE TALLCHARTABLE SET bLevel, dwExp, dLoginDate, dLogoutDate,
                         dwPlayTime = dwPlayTime + DATEDIFF(second, ...)
WHERE dwCharID = ... AND bWorldID = ...

DELETE FROM TCURRENTUSER WHERE dwUserID = ...
```

**Modern `Terminate`** (`soci_session_terminator.cpp:37-90`):

```sql
-- If char_id != 0:
UPDATE TLOG SET timeLOGOUT, dwCharID
-- Else:
UPDATE TLOG SET timeLOGOUT

-- If char_id != 0:
UPDATE TUSERINFOTABLE SET dwLastCharID  -- NEW, R12 fix

-- Unless MapHandoff:
DELETE FROM TCURRENTUSER WHERE dwUserID
```

**Differences:**

1. **Modern doesn't stamp `TLOG.bGroupID` / `TLOG.bChannel` at
   logout time.** Legacy reads these from TCURRENTUSER and writes
   them onto the matching TLOG row. Modern leaves them at their
   defaults (0 if not set at login). Impact: BI/audit queries that
   filter TLOG by group or channel see no data on modern.
2. **Modern doesn't update `TALLCHARTABLE`** (bLevel, dwExp,
   dLoginDate, dLogoutDate, dwPlayTime). In practice the login
   server's `CSPLogout` callsite always passes `dwCharID=0,
   bLevel=0, dwExp=0` (see `TLoginSvr.cpp:683-685, 1064-1067`),
   which means the `IF @dwCurCharID <> 0 AND @dwCharID = @dwCurCharID`
   guard is **never satisfied from the login server path**. So this
   branch is dead in legacy too — the world server's own logout
   path does the TALLCHARTABLE updates. Modern is correct here.
3. **Modern adds `TUSERINFOTABLE.dwLastCharID` stamping** (R12
   fix). Legacy reads `dwLastCharID` from a different mechanism
   (probably stamped by the world server on map exit).

**Action:** add `bGroupID` and `bChannel` to the modern logout
TLOG UPDATE, sourced from the same TCURRENTUSER row the DELETE
keys off. ~5 LOC. Difference #2 is not a real gap. Difference #3
is a deliberate modern addition.

## Summary

| Category | Count | Notes |
|---|---|---|
| Legacy DB operations | 28 query classes | 24 invoked, 4 dead |
| Modern covered fully | 21 of 24 | All client-visible flows |
| Modern covered with design diff | 5 (TLoadService→TOML, TGetNation→TOML, CTBLGroup→TOML, CTBLUserCount→eliminated, AddMacAddress→AddTrustedIP) | All intentional, documented |
| Modern gaps | 3 (G1 CharList BR/BOW tag, G2 MAC→IP note, G3 TLOG group/channel stamp) | All LOW priority |

**Bottom line:** modern stack is functionally complete for the
login-server scope. Only G1 and G3 are actionable fixes (~25 LOC
combined); G2 is a documentation note.

---

# Part 3 — Deep-dive SP-by-SP comparison

Part 2 mapped wire handlers to SOCI services at the table-touch
level. Part 3 reads the **actual SP bodies** in
`_rewrite/docs/schema/procs/` and compares them line-by-line to
modern implementations. This surfaces gaps that table-level audit
missed — branches, side-effects, sub-SP EXECs, secondary tables.

## Method

For each legacy SP invoked from `Server/TLoginSvr/`, read the SP
definition, list every:
- SELECT (read), with WHERE clauses + JOINs
- INSERT/UPDATE/DELETE (write), including secondary tables
- RETURN branch + condition
- EXEC sub-SP

Then map to the modern equivalent and flag every operation modern
**doesn't perform**.

## Findings — additional gaps beyond G1–G3

### G4. `TIPAUTHORITY` IP banlist never consulted by modern (HIGH)

**Legacy chain:** `CSPCheckIP` → `TCheckIP` SP →
`SELECT TOP 1 bAuthority FROM TIPAUTHORITY WHERE @szIPAddress LIKE(szIP)`.
Returns 6 if matched. That return code is stored in `bIPCheck` and
passed into the next call (`TLogin` SP, `@bIPCheck` param). TLogin
then has `IF (@bIPCheck = 6) RETURN 6` (TLogin.sql:105-108) which
early-aborts with `LR_NOMATCHED`.

**Modern:** `SociAuthService::Authenticate` only checks
`IPBLACKLIST_game` (different table). `TIPAUTHORITY` is never read.

**Impact:** any IPs banned in the old way (via TIPAUTHORITY with
SQL LIKE patterns — e.g. `192.168.%`) are not enforced. The
`IPBLACKLIST_game` table uses exact match only.

**Fix:** add a `TIPAUTHORITY` check parallel to `IPBLACKLIST_game`
in `Authenticate` Step 1, with LIKE-pattern matching. ~10 LOC.
Document the two-table design in `auth_service.h`.

### G5. `TFindServerID` side-effect SPs not replicated (LOW–MEDIUM, 4 sub-SPs)

Legacy `TFindServerID` (`_rewrite/docs/schema/procs/TGAME_RAGEZONE/TFindServerID.sql:84-97`)
executes **four sub-SPs** as side effects on every `CS_START_REQ`:

| Sub-SP | Purpose | Modern coverage |
|---|---|---|
| `EXEC TUpdateActiveChar @dwCharID` | stamps "active char" marker on the user | **missing** |
| `EXEC TGLOBAL_GSP.DBO.TUpdateEnterLuckyDate @dwUserID` | daily login bonus stamp | **missing** |
| `EXEC TScammingPost @dwCharID` | scam-report post-system trigger | **missing** |
| `EXEC TChangedPetSystemToMountSystem @dwUserID` | one-time pet→mount migration | **missing** |

Plus 8 more `EXEC` lines commented out (event posts: Christmas,
NewYear, Valentine, Whiteday, Seventhday, Harvest, Halloween,
Olympic — all disabled in current SP).

**Impact:** every login that hits `CS_START_REQ` legacy stamps four
side-effect rows; modern skips them. Daily-login bonus tracking
breaks first; the others have lower impact.

**Fix:** decide per-SP whether to port the logic into modern
session terminator / route handler, or whether legacy SP behavior
was a hack we don't want. Document the decision.

### G6. Char position auto-correction on fallback missing (MEDIUM)

**Legacy `TFindServerID` (line 75-82):** when the char's current
`(wMapID, wUnitID)` has no server registered, fallback queries
`TSPAWNPOSCHART` for the char's `wSpawnID`, recomputes the unit,
and on success **updates the char's position in the DB**:

```sql
UPDATE TCHARTABLE SET wMapID = @wMapID,
                       fPosX = @fPosX,
                       fPosZ = @fPosZ,
                       fPosY = 0
WHERE dwCharID = @dwCharID
```

**Modern `SociMapServerLocator::FindServerForChar`
(`soci_map_server_locator.cpp:80-159`):** does the same TSPAWNPOSCHART
fallback **but does not UPDATE TCHARTABLE**. The char's stale
position stays in the DB, so the next CS_START re-hits the
fallback path indefinitely (and the world server may also reject
the spawn coordinates).

**Fix:** after the fallback succeeds in `lookup_at(spawn_map, ...)`,
`UPDATE TCHARTABLE` with the new map+pos. ~5 LOC at line 158.

### G7. `USERIPLOG` audit table never written (LOW)

**Legacy `TLogin` SP, line 160:**
```sql
INSERT INTO USERIPLOG (IP, USERNAME, Date_time) VALUES (@szLoginIP, @szUserID, GETDATE())
```

Inserted on every successful login. Modern doesn't write this table
(modern uses `TLOG` for the same purpose, but `TLOG` is keyed by
`dwUserID` not username, and only carries timestamps not the
inbound IP).

Note: modern does record the IP — `TCURRENTUSER.szLoginIP` is set
at login. So the per-login IP→user mapping is recoverable from
`TLOG.dwUserID` ⋈ `TCURRENTUSER.szLoginIP`, **but only while the
session is alive**. Once the user disconnects and TCURRENTUSER is
deleted, the IP is gone — `TLOG.szLoginIP` doesn't exist.

**Impact:** post-mortem "what IP did user X log in from yesterday"
queries fail on modern.

**Fix:** either add `szLoginIP` column to TLOG (modern + DDL
change), or add a parallel `USERIPLOG` write. Schema change is
cleaner. ~10 LOC + DDL.

### G8. `ClearStaleSessions` is too aggressive (HIGH)

**Legacy `TClearLoginCurrentUser`:**
```sql
DELETE TCURRENTUSER WHERE dwCharID = 0
```

Only deletes sessions that **never made it past character select**
(dwCharID = 0). Active in-game sessions (dwCharID != 0, handed off
to a map server) are preserved.

**Modern `SociSessionTerminator::ClearStaleSessions`
(`soci_session_terminator.cpp:115`):**
```sql
DELETE FROM "TCURRENTUSER"
```

No WHERE clause — wipes **every** session including ones currently
in-game.

**Impact:** if the modern login server restarts during normal
operation, every in-game user's global session row vanishes. Map
servers still hold their entries (and the actual game state is
unaffected), but:
- Cross-instance duplicate-kick (G_DUPLICATE check on next login)
  uses TCURRENTUSER. After wipe, a logged-in user can log in
  again from a second client without being kicked.
- Audit consistency breaks — `TCURRENTUSER` vs map-server in-memory
  state diverge until users log out.

**Fix:** add `WHERE "dwCharID" = 0` to match legacy semantics.
One-line change at `soci_session_terminator.cpp:115`.

### G9. `CreateChar` is grossly under-implemented (HIGH)

Legacy `TGAME_RAGEZONE.TCreateChar` SP
(`_rewrite/docs/schema/procs/TGAME_RAGEZONE/TCreateChar.sql`) is
**315 lines** and performs:

**Reads:**
- `TCLASSCHART` — class base HP/MP (modern hardcodes)
- `TRACECHART` — race base HP/MP (modern hardcodes)
- `TCHARTABLE` name dup, `TCHARTABLE` slot dup (modern ✓)
- `TCHARTABLE` oriCountry lookup
- `TNPCCHART` name dup check (modern **missing**)
- `TMONSTERCHART` name dup check (modern **missing**)
- `TGLOBAL_GSP.dbo.TVETERANCHART` veteran level (modern cached chart ✓)
- `TLEVELCHART` veteran exp lookup (modern hardcodes? need to verify)

**Writes:**
- `TCHARTABLE` INSERT ✓ (with 31 columns; modern inserts ~17)
- `TINVENTABLE` × 2 (inventory slots 254, 255 with default items 3, 2) — **modern missing**
- `TTITLETABLE` INSERT — **modern missing**
- `TCABINETTABLE` INSERT — **modern missing**
- `TSKILLTABLE` INSERT SELECT (from TSTARTSKILL by class) — **modern missing**
- `THOTKEYTABLE` INSERT SELECT (from TSTARTHOTKEY by class) — **modern missing**
- `TGLOBAL.TALLCHARTABLE` ✓ (modern INSERTs explicitly)
- Starter inventory: cursor through `TSTARTITEMCHART WHERE country, class` + `EXEC TPutItemInInven` for each row — **modern uses hardcoded `StarterSet(req.char_class)` instead** (different scheme; class-only, no country variant)
- Starter recall pet via `TSTARTRECALL` + `EXEC TCreateRecallMon` — **modern missing**
- Mount: `DELETE TPETTABLE WHERE wPetID=2` + `INSERT TPETTABLE` — **modern missing**
- Welcome mail via `EXEC TSavePost` — **modern missing**

**Impact:** chars created by modern login server are missing
inventory slots, skills, hotkeys, mounts, titles, cabinets. If the
world server tolerates missing rows (auto-creates on first access),
behavior is silent-degraded; if it fails, char creation is broken
end-to-end.

**Fix:** this is a multi-day port. Priority order:
1. `TSKILLTABLE` from `TSTARTSKILL` (chars need their starter
   skills or they can't fight).
2. `THOTKEYTABLE` from `TSTARTHOTKEY` (chars need a usable UI).
3. `TINVENTABLE` × 2 (inventory slot tracking).
4. Cursor through `TSTARTITEMCHART` (country-specific starter
   items, instead of class-only hardcoded list).
5. Mount/recall/welcome mail (cosmetic, can defer).

Each port needs DDL for the chart tables (`TSTARTSKILL`,
`TSTARTHOTKEY`, `TSTARTITEMCHART`, `TSTARTRECALL`) plus the
business-logic equivalent in `SociCharService::CreateChar`.

### G10. `DeleteChar` is grossly under-implemented (HIGH)

Legacy `TGAME_RAGEZONE.TDeleteChar` SP
(`_rewrite/docs/schema/procs/TGAME_RAGEZONE/TDeleteChar.sql`) on
**hard delete** (level ≤ 5) DELETEs from **~25 tables**:

`TPOSTTABLE, TFRIENDTABLE, TFRIENDGROUPTABLE, TPROTECTEDTABLE,
TSKILLTABLE, TITEMTABLE, TINVENTABLE, TCABINETTABLE,
TQUESTTERMTABLE, TQUESTTABLE, TRECALLMONTABLE,
TRECALLMAINTAINTABLE, TSKILLMAINTAINTABLE, THOTKEYTABLE,
TITEMUSEDTABLE, TEXPITEMTABLE, TSOULMATETABLE,
TCASTLEAPPLICANTTABLE, TDUELCHARTABLE, TDUELSCORETABLE,
TPVPOINTTABLE, TPVPRECENTTABLE, TPVPRECORDTABLE, TCHARTABLE,
TTEMP* × 6 (TEMPINVENTABLE, TEMPITEMTABLE, ...)`.

Plus DELETE FROM `TGLOBAL.TALLCHARTABLE`.

**Modern `SociCharService::DeleteChar`
(`soci_char_service.cpp:530-597`):** DELETEs from only 2 tables on
hard delete: `TCHARTABLE` and `TITEMTABLE` (filtered by
`dwOwnerID + bOwnerType=0`). Plus soft-deletes TALLCHARTABLE.

**Impact:** modern leaks ~23 tables' worth of orphan rows per
char delete. Over time the DB accumulates orphan friend lists,
hotkeys, skills, quest progress, PvP records, soulmate bonds, etc.

**Fix:** port the full cleanup list. Conservatively, modern can
use the existing legacy SP directly (CALL TDeleteChar) since
modern allegedly hits real MSSQL via SOCI's ODBC backend — but
this contradicts the "no SPs" design choice. Inline port is
~50 LOC. Schema validator should grow to check these tables
exist on the world pool.

### G11. `TAgreement` welcome bonus missing (LOW)

**Legacy `TAgreement` (TAgreement.sql:11-40):** within 7 days of
`releaseDate` (read from `releaseDate` table — yes, that's
literally the table name), the SP:
- `UPDATE TUSERINFOTABLE SET dCabinetUse = DATEADD(d, 360, NOW)`
- `INSERT TCASHITEMCABINETTABLE` — free welcome item (wItemID
  18428, 30-day expiry)

Also, legacy increments `bAgreement = bAgreement + 1` (counter),
while modern sets `bAgreement = 1` (flag).

**Modern `SociAuthService::Agreement`:** sets `bAgreement = 1`
only. Welcome bonus + cabinet extension skipped.

**Impact:** new players don't receive the launch-week welcome
item.

**Fix:** decide if this is in-scope. If the game is a fresh
deploy without releaseDate logic, the entire branch can be
dropped. If launch promotions are wanted, port the inserts.
~30 LOC + DDL for `releaseDate` and `TCASHITEMCABINETTABLE`.

### G12. `TRoute` round-robin load balancing missing (MEDIUM)

**Legacy `TRoute` (TRoute.sql:48-71):**
- COUNT active IPs in `TIPADDR` for the resolved `bMachineID`
- Read `TMACHINE.bRouteID`
- `@bRouteID = (@bRouteID % @bCount) + 1`
- Cursor FETCH ABSOLUTE that index from TIPADDR to pick the IP
- `UPDATE TMACHINE SET bRouteID = @bRouteID` for next time

This is round-robin LB across multiple IPs of the same physical
machine.

**Modern `SociMapServerLocator::Lookup`
(`soci_map_server_locator.cpp:247-280`):**
```sql
SELECT TOP 1 ... FROM TSERVER s JOIN TIPADDR i ...
ORDER BY s.bServerID
```

Always returns the first IP. No rotation across multiple active
IPs.

**Impact:** map servers with multiple IPs (multi-NIC, multi-VLAN,
geo-routed) lose the load distribution. All clients get the same
IP back regardless.

**Fix:** add a `TMACHINE` round-robin counter (or use a modern
counter in shared state) and pick the n-th row. ~20 LOC.

### G13. `TKEEPINGNAME` exact match vs LIKE (LOW)

**Legacy `TGLOBAL.TCreateChar.sql:33`:**
```sql
IF EXISTS(SELECT TOP 1 * FROM TKEEPINGNAME WHERE @szName like(szName))
```

Note: `@szName LIKE szName` — the **stored** name is the pattern.
This lets ops insert patterns like `Admin%` into TKEEPINGNAME to
block all names starting with `Admin`.

**Modern `SociCharService::CreateChar`
(`soci_char_service.cpp:325`):**
```sql
SELECT COUNT(*) FROM "TKEEPINGNAME" WHERE "szName" = :n
```

Exact match only.

**Impact:** wildcard-banned name patterns silently allowed by
modern.

**Fix:** change `=` to `LIKE` with the parameter as the haystack
(SQL Server: `WHERE :n LIKE "szName"`). ~1 LOC. Add test that
inserts `'Admin%'` and tries to create `'AdminBob'`.

### G14. `TLOG.bGroupID / bChannel` never stamped on session (LOW — extends G3)

`TLog.dwKEY` is inserted at login time by modern (already
documented in G3 for logout). But neither at login (`Authenticate`
step 6, line 428) nor at logout (`Terminate`, line 45) does
modern stamp `bGroupID` or `bChannel` onto the TLOG row. Legacy
TLogin SP also leaves them 0 at login, but legacy TLogout SP
copies them from TCURRENTUSER to TLOG at logout time.

Already flagged in G3 from the logout side; flagging here for
completeness — the login-time INSERT is fine because the group
isn't known yet.

### G15. `TGetNation` source table `TLIMITEDLEVELCHART` undocumented (TRIVIAL)

Legacy `TGetNation` reads `bNation` from `TLIMITEDLEVELCHART`
(`TGetNation.sql:8`). This table isn't in dev fixtures, schema
validator, or modern code — modern uses TOML config `nation =`
instead, which is a design diff (G already documented in Part 2).
No additional gap, but worth noting `TLIMITEDLEVELCHART` is
referenced exactly once in the legacy SP corpus.

### G16. `TestLogin` design diff (DESIGN — note)

Legacy `TTestLogin` picks **any unused account** with at least
one char (`TACCOUNT_PW WHERE dwUserID NOT IN TCURRENTUSER AND
EXISTS char`) and returns `szUserID + szPasswd` to the caller as
OUT params. The client then displays/uses these creds.

Modern `AuthenticateTest` reads from `TTESTLOGINUSER` — a
hand-curated pool of QA-only accounts. No password returned to
client.

**Why modern is safer:** legacy could leak real-user passwords
to anyone hitting `CS_TESTLOGIN_REQ`. Modern can only leak
explicitly-flagged test accounts.

**Action:** none — design improvement. Document in
`auth_service.h`.

### G17. `TLogin` bIPCheck=6 early exit dead branch removed (TRIVIAL — note)

`TLogin.sql:105-108` has `IF (@bIPCheck = 6) RETURN 6`. This is
the link between `TCheckIP` (G4) and the auth flow. Modern
eliminates this branch by also not calling `TCheckIP` (G4).
Fixing G4 obviates this note.

## Updated coverage summary

| Category | Count |
|---|---|
| Legacy DB operations | 28 query classes (24 invoked + 4 dead) |
| Modern covered fully | 17 of 24 |
| Modern covered with design diff | 5 (Load/Nation/Group→TOML, UserCount cache eliminated, AddMac→AddTrustedIP) |
| **Modern partial / gapped** | **11 (G1–G2 covered earlier; G3 logout TLOG; G4–G13 newly found)** |
| Modern intentionally skipped | 4 dead SPs (HWID, ClientSha, BanReason, etc.) |

## Severity-ranked actionable list

| Sev | Gap | Effort | Reference |
|---|---|---|---|
| HIGH | G8 — `ClearStaleSessions` wipes active sessions | 1 LOC | `soci_session_terminator.cpp:115` |
| HIGH | G9 — `CreateChar` missing ~8 tables of side data | days, port + DDL | `soci_char_service.cpp:419-490` |
| HIGH | G10 — `DeleteChar` leaks ~23 tables on hard delete | ~50 LOC | `soci_char_service.cpp:530-597` |
| HIGH | G4 — `TIPAUTHORITY` IP banlist never checked | ~10 LOC | `soci_auth_service.cpp` Step 1 |
| MED | G6 — char position not auto-corrected on fallback | ~5 LOC | `soci_map_server_locator.cpp:158` |
| MED | G12 — `TRoute` round-robin LB missing | ~20 LOC | `soci_map_server_locator.cpp:247` |
| MED | G3 — `TLOG.bGroupID/bChannel` not stamped on logout | ~5 LOC | `soci_session_terminator.cpp:45` |
| LOW | G5 — 4 sub-SPs from `TFindServerID` skipped | per-SP decision | `soci_map_server_locator.cpp` |
| LOW | G7 — `USERIPLOG` audit never written | ~10 LOC + DDL | `soci_auth_service.cpp` Step 6 |
| LOW | G11 — `TAgreement` welcome bonus skipped | ~30 LOC + DDL | `soci_auth_service.cpp:577` |
| LOW | G13 — `TKEEPINGNAME` exact match vs LIKE | 1 LOC | `soci_char_service.cpp:325` |
| LOW | G1 — CharList BR/BOW tag missing | ~20 LOC | `soci_char_service.cpp` GetCharList |
| NOTE | G2 — SECURITYCONFIRM IP vs MAC | docs | README |
| NOTE | G14 — TLOG.bGroupID at login (intentional) | none | — |
| NOTE | G15 — TLIMITEDLEVELCHART moved to TOML | docs | — |
| NOTE | G16 — TestLogin design diff | docs | `auth_service.h` |
| NOTE | G17 — bIPCheck=6 dead branch (subsumed by G4) | none | — |

**Total actionable gaps:** 4 HIGH + 3 MED + 5 LOW + 1 LOW (G1) = **13**.

## Recommended next steps

1. **Triage G8 first** — single-line fix, prevents real-world
   data corruption on login server restart. PR today.
2. **G4 next** — ~10 LOC, restores IP banlist parity.
3. **G3 + G6 + G13** — tiny fixes, group into one round of
   cleanup PRs (~15 LOC total).
4. **G10 (DeleteChar)** — schedule for a dedicated PR with the
   full table list reviewed against current schema. Add tests
   that verify every cleanup table is hit.
5. **G9 (CreateChar)** — biggest unknown. Decide first:
   does the world server tolerate missing TINVENTABLE /
   TSKILLTABLE / etc., or does it crash on first access? Needs
   E2E test against a real map server to answer. If world server
   tolerates: lower priority (clients still get usable chars
   eventually). If not: HIGH blocker for production.
6. **G12 + G7 + G5 + G11 + G1** — pick by what ops actually
   need.

This finishes the **complete** legacy-vs-modern DB-coverage audit.
The headline change from Part 2: modern is **not** functionally
complete — 4 HIGH-severity gaps surface only when reading the
actual SP bodies, not visible from the table-touch matrix.
