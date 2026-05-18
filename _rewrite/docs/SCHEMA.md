# 4Story 5.0 MSSQL Schema — Reverse-Engineering Notes

Status: **Phase 0 complete** (2026-05-17). Source: real MSSQL backups restored on local SQL Server 2022.

This document supersedes the inferred schema notes in [PROTOCOL.md §4e](PROTOCOL.md#4e-database-schema-reconstructed-from-dbaccessh-files) with hard data extracted from the actual `.bak` files.

## Source

- `TGLOBAL_RAGEZONE.bak` (34.9 MB) → restored as DB **TGLOBAL_RAGEZONE** — login/master DB
- `TGAME_RAGEZONE.bak` (32.4 MB) → restored as DB **TGAME_RAGEZONE** — game/world DB
- Origin: SQL Server 2012 (MSSQL11 instance `OLDSCHOOL`), backed up 2019-01-27
- Auto-upgraded by SQL Server 2022 during restore (compat level remains 100)

Schema extraction artifacts (regenerable via `extract-schema.ps1`):
- `schema/TGLOBAL_RAGEZONE.tables.csv` — column definitions (635 cols across 68 tables)
- `schema/TGLOBAL_RAGEZONE.indexes.csv` — 78 indexes
- `schema/TGLOBAL_RAGEZONE.fks.csv` — 5 foreign keys
- `schema/TGLOBAL_RAGEZONE.triggers.sql` — 3 triggers
- `schema/TGAME_RAGEZONE.tables.csv` — 1770 cols across 200 tables
- `schema/TGAME_RAGEZONE.indexes.csv` — 216 indexes
- `schema/TGAME_RAGEZONE.fks.csv` — **0 foreign keys** (intentional — perf pattern)
- `schema/TGAME_RAGEZONE.views.sql` — 27 views
- `schema/procs/TGLOBAL_RAGEZONE/*.sql` — 60 stored procedures
- `schema/procs/TGAME_RAGEZONE/*.sql` — 263 stored procedures

## Aggregate stats

| | Tables | Columns | Indexes | FKs | Triggers | Views | Procs |
|---|--:|--:|--:|--:|--:|--:|--:|
| TGLOBAL | 68 | 635 | 78 | 5 | 3 | 0 | 60 |
| TGAME | 200 | 1770 | 216 | 0 | 0 | 27 | 263 |
| **Total** | **268** | **2405** | **294** | **5** | **3** | **27** | **323** |

## Type system used

Real MSSQL types observed in the schema:

| MSSQL type | Usage | PostgreSQL mapping | C# type |
|------------|-------|--------------------|---------|
| `tinyint` | bytes (BYTE), enums, flags | `smallint` (PG has no unsigned 8-bit) | `byte` |
| `smallint` | WORDs, levels, map IDs | `smallint` | `short` |
| `int` | DWORDs, IDs, counters, gold | `integer` | `int` |
| `bigint` | item instance IDs (`dlID`), 64-bit IDs | `bigint` | `long` |
| `real` | float positions (fPosX/Y/Z) | `real` | `float` |
| `varchar(N)` | ASCII/CP1252 text | `text` or `varchar(N)` | `string` |
| `nvarchar(N)` | unicode text (rare) | `text` | `string` |
| `smalldatetime` | timestamps (logout, create) | `timestamp without time zone` | `DateTime` |
| `datetime` | precise timestamps | `timestamp without time zone` | `DateTime` |
| `image` | rarely used legacy BLOB | `bytea` | `byte[]` |

## Naming conventions (confirmed from real data)

- Tables: `T*TABLE` (dynamic data) vs `T*CHART` (static reference data) vs `TVIEW_*` (views)
- Columns use **Hungarian notation**:
  - `dw` → `int` (DWORD)
  - `dl` → `bigint` (DWORD long? — used for item instance IDs)
  - `b` → `tinyint` (BYTE)
  - `w` → `smallint` (WORD)
  - `f` → `real` (float)
  - `sz` → `varchar` (zero-terminated string in C)
  - `d` → `smalldatetime` (date)
  - `n` → `int` (signed number, less common)
- Primary keys are typically `dwID`, `dw{Entity}ID`, or `dl{Entity}ID`; mostly identity (auto-increment).

## Key tables (sampled)

### `TCHARTABLE` (47 columns) — character master record
PK: `dwCharID` (identity int). Columns include:
- Identity: `dwCharID`, `dwUserID`, `bSlot`, `szNAME(50)`
- Cosmetics: `bClass, bRace, bCountry, bRealSex, bSex, bHair, bFace, bBody, bPants, bHand, bFoot, bHelmetHide`
- Stats: `bLevel, dwEXP, dwHP, dwMP, wSkillPoint, bStatLevel, bStatPoint, dwStatExp`
- Currency: `dwGold, dwSilver, dwCooper`
- Position: `dwRegion, wMapID, wSpawnID, wLastSpawnID, fPosX, fPosY, fPosZ, wDIR`
- Guild: `bGuildLeave, dwGuildLeaveTime`
- Misc: `wTemptedMon, bAftermath, dwRankPoint, bDelete, dCreateDate, dDeleteDate, dwLastDestination, bOriCountry, dLogoutDate, bStartAct`

### `TITEMTABLE` (35+ columns) — item instances
PK: `dlID` (identity bigint). Columns include:
- Identity: `dlID, bStorageType, dwStorageID, bOwnerType, dwOwnerID`
- Item template: `bItemID, wItemID, bLevel, bCount, bGLevel, bGradeEffect`
- Durability: `dwDuraMax, dwDuraCur`
- Refinement/socket: `bRefineCur, bGem`
- Magic slots: `bMagic1..bMagic6` + `wValue1..wValue6` + `dwTime1..dwTime6`
- Time/wrap: `dEndTime, wMoggItemID`

(Full schema is in `schema/TGAME_RAGEZONE.tables.csv`.)

## Anomalies / cruft to ignore during migration

- `dtproperties` — legacy SQL Server diagram metadata (delete on migration)
- `OTESTER`, `STATISTIC*` — dev/test tables, drop in rewrite
- `charkilling_log` — lowercase name, likely ad-hoc audit log added later — keep but rename to `TKillingLog`
- `IPBLACKLIST_game` — referenced in `TLogin` SP for IP banning

## Stored procedure surface (323 procs)

The original architecture **does substantial business logic in stored procedures** — auth, character creation, item operations, guild management, auction, etc. For the .NET rewrite, **all of this moves to C# code**:

- `FourStory.Login` services replace `TLogin`, `TLogout`, `TCheckPasswd`, `TCheckHWID`, `TGetBanReason`, ...
- `FourStory.World` / `.Map` services replace gameplay procs
- EF Core LINQ queries replace the data-access portion of procs
- Wolverine commands/handlers replace the control-flow portion

### Notable SPs already inspected

**`TLogin`** (`schema/procs/TGLOBAL_RAGEZONE/TLogin.sql`):
- Input: `@szUserID, @szPasswd, @szLoginIP, @bIPCheck`
- Output: `@dwKEY, @dwCharID, @dwUserID, @szIPAddr, @wPort, @bCreateCnt, @bInPcBang, @dwPremiumID`
- Return codes: `0=success, 1=no user, 2=invalid passwd, 3=duplicate`
- Contains commented-out federated login callout: `EXEC [192.168.1.9,6121].fourstory_ob.memlogin.csp_gamelogincheck` — the original deployment had a separate billing/login backend.
- Uses `IPBLACKLIST_game` table for IP bans.

## Foreign keys (or lack thereof)

- **TGAME has zero FKs** — referential integrity is enforced entirely in application code. This is a common MMO pattern: FKs add lock contention and validation overhead. For the rewrite we should:
  - Define FKs in EF Core via navigation properties (`HasOne/WithMany`)
  - Choose at migration time whether to materialize them as real DB FKs (safer, slightly slower) or virtual-only (matches legacy behavior). **Recommended: real FKs** — modern Postgres handles them efficiently and they catch bugs.
- **TGLOBAL has 5 FKs** — minimal, mostly on account/group tables. Will reproduce in EF Core.

## Migration approach for Phase 1 (preview)

1. **Don't transcribe schema by hand** — use `dotnet ef dbcontext scaffold` against the restored MSSQL DBs to generate initial entity classes, then refine:
   ```pwsh
   dotnet ef dbcontext scaffold "Server=localhost;Database=TGAME_RAGEZONE;Integrated Security=True;TrustServerCertificate=True" `
     Microsoft.EntityFrameworkCore.SqlServer `
     --output-dir Entities/Game `
     --context-dir Data `
     --context GameDbContext `
     --use-database-names `
     --no-onconfiguring
   ```
2. **Rename to .NET conventions** in a second pass (e.g., `dwCharID` → `CharId`, with `[Column("dwCharID")]` attribute to preserve DB column name).
3. **Switch provider to Npgsql** for PostgreSQL once entities are stable:
   - Change `Microsoft.EntityFrameworkCore.SqlServer` to `Npgsql.EntityFrameworkCore.PostgreSQL`
   - Apply type conversions noted above (especially `tinyint` → `smallint` since PG has no 1-byte unsigned).
4. **Add unique constraints** that should exist but weren't FKs (e.g., `(dwUserID, bSlot)` on TCHARTABLE — only 1 char per slot).
5. **Data migration**: separate script using EF Core + bulk insert. NOT part of code-first migrations.

## Open items

- **String encoding**: are `varchar` columns CP1252, CP949, or system-default? Need to query a real char name and inspect bytes. Test:
  ```sql
  SELECT CAST(szNAME AS VARBINARY(50)), szNAME FROM TGAME_RAGEZONE.dbo.TCHARTABLE WHERE bDelete = 0;
  ```
- **Triggers** in TGLOBAL — 3 of them, inspect `schema/TGLOBAL_RAGEZONE.triggers.sql` to determine if they encode business rules we need to port.

---

*Last updated: 2026-05-17. Phase 0 complete with real DB data.*
