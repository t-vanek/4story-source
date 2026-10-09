#!/usr/bin/env python3
"""Render evidence-indexed investigation reports; never reads private data rows."""
from collections import Counter,defaultdict
import hashlib
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'_rewrite/docs/database-reconstruction'
E=OUT/'evidence'


def read(name):return json.loads((E/name).read_text())


def write(name,content):
    (OUT/name).write_text(content.strip()+'\n')


def main():
    artifacts=read('artifact-inventory.json')['artifacts']; models=read('backup-schema.json')
    profiles=read('data-profiles.json');compare=read('schema-comparison.json');source=read('source-object-mapping.json')
    backups=[a for a in artifacts if a['path'] in ('TGAME_RAGEZONE.bak','TGLOBAL_RAGEZONE.bak')]
    source_artifacts=[a for a in artifacts if not a['path'].startswith('database/')]
    backup_table='\n'.join(f"| `{a['path']}` | {a['size_bytes']:,} | `{a['sha256']}` |" for a in backups)
    version_matrix=[]
    for a in artifacts:
        path=a['path']
        if path in ('TGAME_RAGEZONE.bak','TGLOBAL_RAGEZONE.bak') or path.startswith('_rewrite/docs/schema'):
            generation='2019-01-27 RAGEZONE backup lineage'; confidence='HIGH'
            evidence='RESTORE metadata and object comparison; schema/schema.old-dump-2019 byte equality'
            compatible='Partially aligned legacy C++ source; current portable C++ services require extensions and behavior fixes'
            conflicts='Exact game release UNKNOWN; external database references; mount procedure export renames; global/game duplicated catalogs differ'
        elif path.startswith('database/'):
            generation='PROPOSED reconstruction/migration foundation';confidence='HIGH'
            evidence='Generated from backup-schema.json and tested in isolated databases'
            compatible='Migration utilities; server adapters not implemented'
            conflicts='Snapshot provenance columns and canonical names are not existing server contracts'
        elif path.startswith('Server/') and path.endswith('.sql'):
            generation='Portable server development fixtures/extensions';confidence='HIGH'
            evidence=path+'; inspect CREATE/ALTER/DROP declarations in fragment-objects.json'
            compatible='Individual development features; not a complete production schema'
            conflicts='Some scripts contain destructive DROP/CASCADE; PostgreSQL runtime backend remains disabled'
        else:
            generation='UNKNOWN / not a database schema artifact';confidence='UNKNOWN'
            evidence='File inventory; documentation/configuration/data rather than authoritative DB metadata'
            compatible='UNKNOWN';conflicts='No independent database/game version evidence'
        version_matrix.append({'path':path,'sha256':a['sha256'],'database_generation':generation,
            'game_version':'UNKNOWN','compatible_server_implementation':compatible,'evidence':evidence,
            'conflicting_evidence':conflicts,'confidence':confidence})
    (E/'version-compatibility-matrix.json').write_text(json.dumps(version_matrix,indent=2)+'\n')
    write('README.md','''# 4Story database reconstruction — evidence and migration foundation

Investigation date: **2026-10-08**. Scope: this checkout, its two root backups,
legacy C++ source and the current C++20/Asio/SOCI rewrite. This report supersedes
the .NET assumptions and unsupported version/type claims in older notes without
modifying those historical artifacts.

Both SQL Server backups were restored and checked. Typed snapshot structures for
all 268 tables were created in PostgreSQL; 106,692 actual reference rows from 15
tables were imported with value reconciliation and original text-byte preservation.
The proposed canonical item catalog contains 7,706 verified rows. **These results
do not establish that a complete game server can run against PostgreSQL.**

| Report | Purpose |
|---|---|
| [01 — Inventory](01-artifact-inventory.md) | Scope, formats and checksums |
| [02 — Versions](02-version-analysis.md) | Engine, schema, game and protocol distinctions |
| [03 — Backups](03-backup-analysis.md) | Actual restore and integrity results |
| [04 — Comparison](04-schema-comparison.md) | Object-level equality and differences |
| [05 — Source mapping](05-source-code-mapping.md) | Dependencies, contracts and procedure replacements |
| [06 — Legacy model](06-reconstructed-legacy-schema.md) | All entities, provenance and completeness |
| [07 — PostgreSQL architecture](07-postgresql-architecture.md) | Ownership, types, constraints and workloads |
| [08 — Migration](08-migration-strategy.md) | Reproduction, validation and recovery |
| [09 — Gaps](09-gaps-and-risks.md) | Concrete unresolved compatibility issues |
| [10 — Roadmap](10-implementation-roadmap.md) | P0–P3 implementation order |
| [11 — Verification](11-verification-report.md) | Executed work, tests and limits |

Machine-readable evidence is in [evidence/](evidence/). Executable artifacts are
in [`database/postgresql/`](../../../database/postgresql/) and
[`tools/database/`](../../../tools/database/). Original SQL fragments and backups
remain unchanged. Extracted data and credentials are outside Git.

Follow-up policy and catalog contracts are documented in
[`database/README.md`](../../../database/README.md). Original backup evidence
remains authoritative; current server contracts are adapted in derived PostgreSQL
layers using additional numbered migrations.
''')
    write('01-artifact-inventory.md',f'''# 01 — Artifact inventory

The recursive inventory contains **{len(source_artifacts)} pre-existing artifact
files** plus {len(artifacts)-len(source_artifacts)} generated SQL/mapping artifacts
at the recorded scan. Every entry records relative path, byte size, detected format,
SHA-256 and suspected purpose in [artifact-inventory.json](evidence/artifact-inventory.json).
The generated report directory excludes itself to avoid recursive checksums.
Git internals, IDE caches, dependency/build output and generated build directories
are excluded and their file counts are recorded. Discovery used ignored and hidden
repository files too; it was not limited to tracked files or `_rewrite/docs`.

| Original backup | File bytes | SHA-256 |
|---|---:|---|
{backup_table}

`4story.bak` was **not found in this repository**. The available originals are the
two files above. Their `TAPE` signature and successful SQL Server RESTORE commands
confirm Microsoft Tape Format SQL Server database backups. `file` describes the
generic NT backup container; it does not establish game version.

The original artifact set includes **657 SQL files**, **13 CSV/TSV files**, 29
Markdown files, 18 TOML files and other configuration/build manifests. Two further
`.bak` files under `Client/TClient/` are C++ source-edit backups, not database backups.
No `.bacpac`, `.dacpac`, `.mdf`, `.ldf` or database archive container was discovered
in the repository. Physical MDF/LDF names reported by FILELISTONLY are names inside
the SQL backups, not additional repository files.

| Family | Evidence and purpose | Inspection |
|---|---|---|
| `_rewrite/docs/schema/` | 6 UTF-8 BOM tab-separated column/index/FK exports; views, triggers and 323 procedure files | Parsed; matched against restored metadata |
| `_rewrite/docs/schema.old-dump-2019/` | Same export layout | Every corresponding file byte-identical to `schema/` |
| `Server/TLoginSvrAsio/schema/*.sql` | PostgreSQL/MSSQL dev fixtures, 2FA extension, synthetic dev account | Indexed; existing scripts not executed |
| `Server/TPatchSvrAsio/schema/patch-tables.sql` | Patch metadata fixture | Indexed; not a full schema export |
| `Server/TLogSvrAsio/schema/*.sql` | Audit table and ITEMLOGTL migration | Indexed; production lineage not inferred |
| `_rewrite/docs/SCHEMA.md`, `SQL_AUDIT.md`, `extract-schema.ps1` | Secondary historical claims and extraction method | Read; claims rechecked, extractor not executed |
| `Server/*Asio/*.toml`, `deploy/config.example/*.toml` | Database selection and service configuration | Keys inventoried, values redacted |
| `vcpkg.json`, CMake files, `Lib/Own/FourStoryCommon/fourstory/db/orm/` | Current C++/SOCI architecture | Backend and ORM construction inspected |

The CSV suffix is misleading: schema files are **TSV**, parsed with the actual
delimiter and BOM. They omit schema name, check constraints, index direction,
included columns, collation, identity seeds/counters and some trust flags. They
are partial metadata exports, not executable complete schema definitions.

[fragment-objects.json](evidence/fragment-objects.json) indexes declarations,
references and lexical transaction/write flags without publishing SQL literals.
[configuration-contracts.json](evidence/configuration-contracts.json) lists TOML
keys and whitelisted backend names only. No DSN, password, login, IP or data row is
published by these inventories. Backup originals are now ignored explicitly in
`.gitignore`; that does not remove or change them.
''')
    write('02-version-analysis.md','''# 02 — Version and compatibility analysis

| Concept | Established value | Confidence and evidence |
|---|---|---|
| Backup writer engine | SQL Server **11.0.2100** (2012 generation) | HIGH: both RESTORE HEADERONLY outputs |
| Original database format/compatibility | DatabaseVersion **706**, compatibility **90** | HIGH: backup headers; independent of game version |
| Inspection engine | SQL Server **16.0.4265.3**, Developer edition | HIGH: SERVERPROPERTY; SQL Server 2022 instance |
| Restored compatibility | **100**, collation `Latin1_General_CI_AS_KS` | HIGH: restored sys.databases |
| PostgreSQL verification | PostgreSQL 18 image; actual version in verification evidence | HIGH: database query and image digest |
| Schema lineage | RAGEZONE snapshots backed up **2019-01-27** | HIGH: headers plus normalized metadata/module comparison |
| Schema revision/build number | **UNKNOWN** | No authoritative migration ledger or release ID in artifacts |
| Game server/client release | **UNKNOWN** | Repository labels mention 5.0, insufficient to date the database or client build |
| Source protocol constant | `TVERSION = (WORD)0x2918` | HIGH: `Lib/Own/TProtocol/include/ProtocolBase.h:4`; not a game release number |
| Live content/patch version | **UNKNOWN** | Global TVERSION has zero rows; no authoritative client assets/patch catalog pairing |

The full **per-artifact matrix**, including suspected generation, compatible
implementation, evidence, conflicts and HIGH/MEDIUM/LOW/UNKNOWN classification, is
[version-compatibility-matrix.json](evidence/version-compatibility-matrix.json).
All exact game-version fields remain UNKNOWN. HIGH database-lineage confidence
does not imply HIGH game-version confidence.

Important historical anchors: global `TCheckPasswd` was created/modified in May
2015; global `TLogin` modified 2015-05-26; game `TCreateChar` modified 2015-12-30;
`TGetMountSaddle` modified 2018-08-17. These timestamps are catalog metadata,
not proof of release order, authenticity of clocks, or a complete migration chain.
Physical column-ID gaps also prove dropped-column history, without identifying
the dropped definitions or a game revision.

`git log` associates the checked-in schema directories with initial commit
`c77f790` (2026-05-18); current server HEAD is `47762c5` (2026-07-07). The duplicate
directory name `old-dump-2019` does **not** identify an older schema than `schema`:
their files are byte-identical. Fragments therefore share the same observed
backup lineage, with export-side procedure renames described in report 04.

The portable rewrite contains later requirements (2FA, peer authentication,
registry/metrics, modern audit, Bow procedure names). These are source revisions,
not evidence that another complete game/database generation was supplied.
The old `SCHEMA.md` claim of original compatibility 100 actually describes the
post-restore value; its C#/EF Core architecture and general type assumptions do
not describe this checkout's C++ rewrite. `SQL_AUDIT.md`'s claim that the modern
stack invokes no stored procedures is obsolete for current Control/World/Patch.

There is also independent evidence of **different catalog structures and values
inside the two co-dated databases**: global/game TITEMCHART have 44/52 columns and
2,519/7,706 rows; only 2,517 item keys overlap, and 37 shared numeric fields contain
mismatches on at least one overlapping key. Monster catalogs have 1,811/3,534 rows,
1,216 overlapping keys and differences in 27 shared numeric fields. These may
reflect different database roles, stale caches or content revisions; no exact
game release can be inferred. See cross-database-catalog-comparison.json. The
target importer deliberately selects the TGAME catalog and never unions both.
''')
    write('03-backup-analysis.md','''# 03 — Backup investigation and recovery

Both root `.bak` files were copied into a new rootless Podman SQL Server container
on a private network. SQL Server had no published host port. No existing database,
service account or running game server was used. Originals were never mounted
writable. Restore destinations were fresh `forensic_TGLOBAL_RAGEZONE` and
`forensic_TGAME_RAGEZONE`, with new Linux MDF/LDF paths and **no WITH REPLACE**.

| Header field | TGLOBAL_RAGEZONE | TGAME_RAGEZONE |
|---|---|---|
| Backup start/finish | 2019-01-27 16:01:55 | 2019-01-27 16:01:33 |
| Backup sets | 1 full database set, position 1 | 1 full database set, position 1 |
| Writer | 11.0.2100, `GAMER-PC\\OLDSCHOOL` | Same |
| Original compatibility | 90 | 90 |
| Database collation | Latin1_General_CI_AS_KS | Same |
| Recovery | SIMPLE | SIMPLE |
| Compression | No | No |
| Backup checksums | Absent | Absent |
| Encryption/TDE requirements found | None in exposed metadata | None in exposed metadata |
| Logical data/log files | TGLOBAL_Data / TGLOBAL_Log | TGAME_Data / TGAME_Log |
| Data file bytes | 449,708,032 | 1,372,192,768 |
| Log file bytes | 796,917,760 | 4,325,376 |

FILELISTONLY reports original files below a Windows
`C:\\Program Files\\Microsoft SQL Server\\MSSQL11.OLDSCHOOL\\MSSQL\\DATA` path.
Full paths, LSNs, GUIDs, metadata flags and sizes are preserved in
[backup-metadata.json](evidence/backup-metadata.json). Full backups have no required
differential/log base for this completed restore. The two backups were taken 22
seconds apart: coordinated consistency across both databases is **not proven**.
Original timestamp timezone semantics are not established by the file mtime or
the newer header fields exposed when reading an old backup.

**Executed successfully:** HEADERONLY, FILELISTONLY, VERIFYONLY, RESTORE DATABASE,
online-state queries, and DBCC CHECKDB WITH NO_INFOMSGS, ALL_ERRORMSGS on each
restored database. CHECKDB returned exit 0 and no diagnostic output. VERIFYONLY
is a readability/completeness check, not a substitute for data integrity. Since
backup checksums are absent, externally calculated SHA-256 and CHECKDB are the
recorded verification evidence; no nonexistent backup checksum is claimed.

These specific SQL Server 2012 backups restored directly on SQL Server 2022 and
their compatibility rose from 90 to 100. No intermediate restore was needed in
this experiment. This observed result is more precise than assuming arbitrary
historical backups work. Microsoft's [2022 upgrade documentation](https://learn.microsoft.com/en-us/sql/database-engine/install-windows/supported-version-and-edition-upgrades-2022?view=sql-server-ver17)
discusses backup migration separately from in-place upgrade; its general compatibility
qualification does not establish the original level of these files. Use the
recorded successful restore for these exact hashes, and re-inspect other inputs.

| Recovered metadata | Global | Game | Total |
|---|---:|---:|---:|
| Tables | 68 | 200 | 268 |
| Columns | 635 | 1,770 | 2,405 |
| Indexes / PK indexes | 78 / 41 | 216 / 128 | 294 / 169 |
| Foreign keys | 5 | 0 | 5 |
| Default expressions | 82 | 254 | 336 |
| Identity columns | 17 | 11 | 28 |
| Stored procedures | 60 | 263 | 323 |
| Views | 0 | 27 | 27 |
| Triggers | 3 | 0 | 3 |
| User functions / sequences / CHECK constraints | 0 | 0 | 0 |

Game tables occupy `dbo` and **`tgame`** (`TLASTMONTHPOINTTABLE`), a distinction
omitted by the old CSV exports. All 353 module definitions were readable; no
encrypted/unavailable module was observed. Full definitions were retained only
in private inspection output; public evidence contains definition hashes,
parameter contracts and dependencies, avoiding embedded literals or credentials.

Data was counted with COUNT_BIG for **all 268 tables**. Global has 31 nonempty
tables / 720,809 total rows; game has 106 / 249,175. Global `TACCOUNT` has 1 row,
`TACCOUNT_PW` 0, `TCURRENTUSER` 0, `TLOG` 699,982. Game characters, guilds and item
instances have **0 rows**. This is useful content/history material, not a recovered
populated player world. `TCASHITEMBUYTABLE` partition metadata claimed 251 rows,
but COUNT_BIG found 0: reconciliation uses exact counts, never that stale estimate.

Restored metadata includes unresolved cross-database and linked-server references
(`TGLOBAL_GSP`, `TGAME_GSP`, `TGAME`, `TGLOBAL`, `TQUEST_GSP`, `fourstory_ob`, master).
Restoring a database does not repair those dependencies or establish that its
stored procedures are operational. SQL login/master credentials and external
billing services were not supplied or reconstructed.
''')
    comparison_table='\n'.join(f'| {kind} | {status} | {count} |' for (kind,status),count in sorted(Counter((r['kind'],r['status']) for r in compare if r['fragment']=='schema').items()))
    write('04-schema-comparison.md',f'''# 04 — Backup versus fragments

Comparison unit: one restored database object and one export family. Results for
`schema` and `schema.old-dump-2019` are identical because the families are byte
duplicates. Do **not** add their counts to measure independent coverage.

| Object/projection | Classification | Count per export family |
|---|---|---:|
{comparison_table}

Detailed per-object records are in [schema-comparison.json](evidence/schema-comparison.json).
All **268 tables / 2,405 columns** agree on the CSV projection: physical column ID,
name, type, byte width, precision, scale, nullability, identity flag, PK membership
and default definition. That projection lacks schema/collation/computed/identity
seed information; it is not proof that the CSV reconstructs every database setting.

The **194 index sets across indexed tables** agree on exported key order, SQL
index type, unique flag and PK flag. There are 294 actual indexes. Names are ignored
to avoid system-generated-name noise; CSV omissions prevent a full equality claim
for includes, direction, filters, disabled state or constraint identity. These
sets and the two database-level FK projections are classified COMPATIBLE.
The restored FK `FK_TIPADDR_TMACHINE` is enabled but untrusted, information absent
from the fragments. Zero game FKs is observed; a claimed historical performance
motivation is not proven.

Of 353 modules in each family, **350 names match**: 341 normalize identically;
9 differ only in CRLF/LF within literal content and are COMPATIBLE, not silently
identical. Comments, keyword case, quoting of identifiers, whitespace outside
literals and standalone GO separators are normalized. Literal case and internal
spacing remain significant. Normalization is lexical and cannot prove general
business equivalence.

Three pairs remain distinct objects by name:

| Backup object | Export declaration (despite filename) | Result |
|---|---|---|
| TGetMountSaddle | TGetMountSaddle_copy | Signature/body match after declaration removal |
| TSetMountSaddle | TSetMountSaddle_copy | Same |
| TDelMountSaddle | TDelMountSaddle_copy | Same |

Each original is ONLY_IN_BACKUP and each `_copy` declaration ONLY_IN_FRAGMENTS.
[procedure-alias-comparison.json](evidence/procedure-alias-comparison.json) verifies
these matches. Do not execute files on the assumption that filenames name their
objects or automatically alias them in production.

There is **no demonstrated schema upgrade/downgrade** between the two export
directories and these backups. Combining duplicate exports adds no data. Modern
server SQL files are development fixtures/extensions with different subsets and
contracts, not a later complete database dump. They contain 43 lexical table
declarations and one development procedure declaration, with conditional/repeated
definitions; these counts are not counts of distinct production objects.

Views/procedures are logically ordered after referenced tables and functions;
FK/index installation follows table creation and data validation. Cross-database
views/wrappers cannot be deployed from filename ordering. Dependency metadata is
in backup-schema.json; it exposes 123 global and 849 game dependency rows,
including unresolved external references. The implemented snapshot layer depends
only on import control metadata and numeric table keys, avoiding such wrappers.

Reference-data *values* are not present in the metadata exports, so their equality
with backup values is UNVERIFIED. Real reference data was extracted directly from
the backup copies; its import comparison is documented in report 11.

Cross-database content comparison is separately recorded in
[cross-database-catalog-comparison.json](evidence/cross-database-catalog-comparison.json).
Global/game same-named TITEMCHART and TMONSTERCHART are **not interchangeable**:
they differ in column sets, key membership and common-field values. This establishes
a concrete content conflict for a naive union, while its historical cause remains
UNKNOWN. The verified canonical item projection takes the game catalog only.
''')
    active=[r for r in source if r['implementation']=='portable' and r['scope']=='source']
    missing=defaultdict(list)
    for r in source:
        if r['scope']=='source' and r['implementation'] in ('portable','shared') and not r['backup_matches'] and not r['object'].startswith('INFORMATION_SCHEMA.'):
            missing[r['object'].split('.')[-1]].append(r)
    missing_table='\n'.join(f"| `{name}` | `{rows[0]['path']}:{rows[0]['line']}` | {'Partial dev DDL/name match' if rows[0]['available_artifacts'] else 'No supplied definition'} |" for name,rows in sorted(missing.items()))
    write('05-source-code-mapping.md',f'''# 05 — Source dependencies and procedure responsibilities

The actual architecture is **C++20 + Boost.Asio + SOCI**, with a shared custom ORM
(`EntityMapping`, `Repository`, `DbContext`, `SpCall`). No .NET persistence layer
is introduced. `vcpkg.json` enables SOCI ODBC; shared
`Lib/Own/FourStoryCommon/src/db/session_pool.cpp:18` throws for PostgreSQL.
Dialect branches and PostgreSQL dev DDL therefore do not prove an enabled PG server.

[source-object-mapping.json](evidence/source-object-mapping.json) records component
path/line → object → SQL operation/ORM usage/validator → available artifact and
restored metadata candidates. It includes legacy sources, tests, current portable
services and the copied `TMapSvrAsio/legacy_src` reference separately; that directory
is not compiled (CMakeLists.txt:41). Adjacent/raw C++ SQL strings are decoded;
EntityMapping types are joined to `.Set<Type>()` callers, not merely literal names.

Current portable, non-test source has **{len(active)} lexical edges**, **160 distinct
normalized names**, of which 158 are application object names and 143 have a
restored name match. These are explainable *name availability* counts, not runtime
behavior/column coverage. Name matches across global/game DBs are ambiguous until
the pool/owner is resolved. Shared peer authentication adds its own missing objects.
The source currently calls **37 distinct legacy procedure names**; older
SQL_AUDIT.md's no-SP conclusion does not apply to current code.

| Domain / owning component | Observed objects and operations | Concrete source |
|---|---|---|
| Login/auth | TACCOUNT_PW, TCURRENTUSER, TUSERINFOTABLE, bans, secure code; credential read/session insert/update | TLoginSvrAsio/services/soci_auth_service.cpp |
| Character lifecycle | TCHARTABLE, TALLCHARTABLE, TITEMTABLE, starter/reference tables; multi-step create/delete | TLoginSvrAsio/services/soci_char_service.cpp |
| Character/session loading | TCURRENTUSER, TCHARTABLE; read/targeted save, ORM mapping | TMapSvrAsio/db/queries.h; services/soci_player_service.cpp |
| Inventory/items | TINVENTABLE, TITEMTABLE, owner/storage fields; bag rows and instance persistence | TMapSvr/DBAccess.h; TMapSvrAsio/services/soci_inventory_service.cpp |
| Skills/quests | TSKILLTABLE, TQUESTTABLE, TQUESTTERMTABLE plus charts; learned/progress rows and boot catalogs | TMapSvrAsio/db/queries.h; services/soci_quest_service.cpp |
| Monsters/NPCs/maps | TNPCCHART, TMONSTERCHART, TMONSPAWNCHART, TMAPMONCHART, TMONATTRCHART, TMONITEMCHART | TMapSvrAsio/db/queries.h; services/spawn_manager.cpp |
| Guild/social | TGUILDTABLE, TGUILDMEMBERTABLE, article/cabinet/tactics tables, TFRIENDTABLE; ORM loads and parameterized writes | TWorldSvrAsio/services/guild_entities.h; soci_guild_repository.cpp; friend_entities.h |
| Mail/auction | TPOSTTABLE/TPOSTITEMTABLE/TAUCTIONTABLE families, item ownership/currency writes and procedures | TMapSvr/DBAccess.h; schema/procs/TGAME_RAGEZONE/TSavePost.sql, TAuctionBid.sql |
| Events/world state | Bow/tournament/rank/occupation charts and procedure calls | TWorldSvrAsio/services/soci_bow_repository.cpp; soci_tournament_repository.cpp; soci_month_rank_repository.cpp |
| Patch/control/audit | TVERSION/TPREVERSION, peer registry/metrics/auth, operator audit and TLOG_AUDIT | TPatchSvrAsio/services/patch_repository.cpp; TControlSvrAsio/services/soci_*.cpp; TLogSvrAsio/services/log_sink.cpp |

Static catalogs are boot-loaded and cached in chart services. Monster instances,
AI/combat ticks, player registries and peer heartbeat timers are runtime state
(`domain/monster.h`, `services/spawn_manager.cpp`, `services/char_state_store.h`,
shared `cluster/peer_client.cpp`). Persist progression, economy, ownership and
durable world outcomes; do not turn per-tick position/HP updates into SQL writes.
TCURRENTUSER is operational session state and must be rebuilt/invalidate sessions
at a database cutover, not imported as active historical sessions.

Missing restored definitions, with exact evidence locations:

| Object | First observed source site | Artifact coverage |
|---|---|---|
{missing_table}

Several missing objects are intentional modern extensions (2FA, audit, peers),
others are missing callable procedures (patch metadata/Bow). Archived legacy-only
requirements are additionally enumerated in the JSON; they do not establish
current portable runtime requirements. Optional catch-and-continue paths do not
make those features functionally covered.

**Contract conflicts requiring review:**

- Guild validator asks for `TGUILDARTICLETABLE.dwIndex/szBody/timeWrite`, while the
  backup and actual AddArticle SQL use `dwID/szArticle/dwTime` (soci_guild_repository.cpp:310).
  Its cabinet `wItemKind` and tactics `bRole` requests are also absent. These are
  warn-only validator drift, not evidence to invent replacement legacy columns.
- Modern inventory reads smalldatetime `dEndTime` into int64 and inserts NULL for
  a permanent row (`soci_inventory_service.cpp:34,64`); original TINVENTABLE declares
  it NOT NULL. Time conversion and bag/instance semantics need explicit contracts.
- Legacy monster stat lookup uses `(wMonAttr,bLevel)` (`TMapSvr/TMap.cpp:629`), while
  portable spawn_manager.cpp:65 uses `(monster ID,bLevel)`. On supplied data these
  produce 39 versus 3,468 missing template combinations; this is a demonstrated
  behavior/data incompatibility, not a PG type conversion problem.
- Character item ID allocation still emits `SELECT TOP 1` before falling back to
  synthetic IDs (`soci_char_service.cpp:807`). PG must use a reviewed allocator
  rather than entering that fallback on dialect failure.

Every original procedure is covered by
[backup-procedure-modernization.json](evidence/backup-procedure-modernization.json):
323 objects with parameters, callers, dependencies, lexical writes/transaction
flags, proposed responsibility and PG migration status. A separate fragment matrix
retains each export file. Literal values and full private definitions are omitted.

| Procedure/family | Observed behavior / callers | Proposed replacement; transaction contract |
|---|---|---|
| TLogin, TCheckPasswd, TLogout | Auth/session, returns/OUT fields, optional external billing; modern auth/session services inline parts | Application auth + one persistence transaction for account/session claim; preserve protocol error codes and duplicate-login invariants |
| TOPLogin, TUserProtectedAdd | Operator auth and ban update; Control SpCall callers | Application policy + parameterized persistence; audited operation, PG permissions |
| TCreateChar/TDeleteChar | Global/game wrappers and multi-table mutations; current SociCharService | Domain orchestrator in one PG DB across auth/game schemas; slot/name uniqueness and rollback, explicit ID RETURNING |
| TGenerateDBItemID | World-partitioned 2^56 ID ranges; counter UPDATE/SELECT under transaction | Persistence allocator/sequence with world-range mapping and high-water verification; no synthetic collision fallback |
| TSaveItemDataStart/TSaveItem/TSaveItemDataEnd | Temp inventory/item writes followed by transactional replacement | Application save orchestration + persistence transaction; preserve snapshot atomicity and ownership, eliminate shared temp data when proven safe |
| TSavePost/TAuctionBid/TAuctionBuyDirect | Money checks, bidder/item/mail mutations; several originals lack explicit transaction | Domain trade/mail service with item/auction/account locks and atomic currency/ownership transfer; behavior tests before improving transaction boundaries |
| TGuildEstablish and guild writes | Duplicate checks, guild/member state, transaction/return codes | Guild aggregate application layer + constraints and transaction; lock stable IDs in consistent order |
| Tournament/Bow/rank/event procedures | Current World EXEC callers; scheduling, persistence, awards | Application scheduler/domain plus persistence transactions; idempotent reward keys; do not blindly translate to PG functions |
| OPTool*, tests, batches, mass clear | Administrative/destructive maintenance and helper operations | Explicit admin/background worker; separate privilege and controlled policy, never automatic migration startup |
| TALLCHARTABLE_PW triggers | Change feed into TALLCHARTABLE_TRIGGER; scalar SELECT over INSERTED/DELETED | Reviewed set-based outbox/transactional change feed; scalar historical code is unsafe for multirow batches |

No legacy business procedure was converted to a PG function or claimed verified.
Constraints own relational invariants; application services own behavior; the
persistence layer owns SQL/transaction boundaries; scheduling belongs in existing
workers. Small PG functions remain an option only for a proven atomic operation
that cannot be expressed clearly with RETURNING/constraints/parameterized SQL.

The index is **lexical, not a compiler or runtime trace**. Preprocessor branches,
runtime-generated identifiers, aliases and concatenated dynamic SQL require review.
[dynamic-sql-review-sites.json](evidence/dynamic-sql-review-sites.json) lists those
generator sites. `SpCall::BuildSql` is T-SQL-specific and its current debug/error
logs include rendered argument literals; redact that before credential/admin cutover.
''')
    entities=[]
    for db,m in models.items():
        groups=defaultdict(list)
        for c in m['columns']:groups[(c['schema_name'],c['table_name'])].append(c)
        counts={(r['schema_name'],r['table_name']):r['exact_count'] for r in profiles[db]['exact_row_counts']}
        for (schema,table),cols in sorted(groups.items()):
            pk=next((i for i in m['indexes'] if i['table_name']==table and i['schema_name']==schema and i['is_primary_key']),None)
            keys=','.join(c['column_name'] for c in pk['column_details']) if pk else 'none'
            entities.append(f'| `{db}.{schema}.{table}` | {len(cols)} | `{keys}` | {counts[(schema,table)]:,} | CONFIRMED |')
    completeness={'schema':{'tables_confirmed':268,'columns_confirmed':2405,'readable_modules':353,'scope':'relative to the two supplied database snapshots only'},
        'relationships':{'declared_FKs':5,'untrusted_FKs':1,'game_FKs':0,'scope':'inferred gameplay relations are separately tested, not claimed complete'},
        'static_reference':{'known_CHART_tables':93,'nonempty_CHART_tables':81,'imported_game_CHART_tables':14,'additional_imported_tables':['TSKILLDATA'],'rows_imported_and_value_verified':106692},
        'persistence':{'source_portable_object_names_excluding_system_catalog':158,'backup_name_matches':143,'populated_characters':0,'populated_item_instances':0,'populated_guilds':0},
        'operational':{'backup_restore_and_checkdb':'passed','game_server_on_PostgreSQL':'not executed; backend disabled and T-SQL dependencies unresolved'}}
    (E/'completeness.json').write_text(json.dumps(completeness,indent=2)+'\n')
    write('06-reconstructed-legacy-schema.md','''# 06 — Logical legacy reconstruction and provenance

**CONFIRMED** means restored original metadata or an original definition.
**INFERRED** means a relationship/behavior derived from source or data checks.
**PROPOSED** means a modern structure with no claim of legacy existence.
Table names alone never promote an inferred relationship to CONFIRMED.

Every known table's columns, schema, nullability, type widths/precision, defaults,
collation, PK membership and identity state are in
[backup-schema.json](evidence/backup-schema.json). Keys/index includes/direction,
FK flags and module hashes/parameters/dependencies are recorded there too.
Each database links to the immutable backup SHA-256 in backup-metadata and artifact
inventory. All 268 table definitions were generated in
[`database/legacy-reconstructed/`](../../../database/legacy-reconstructed/) and
executed on fresh SQL Server databases; their logical projections match all 2,405
columns after redundant default parentheses and historical column-ID gaps are
normalized. Those files reconstruct **tables/PKs**, not secondary indexes, FKs,
views, procedures, permissions or historical identity high-water counters.
They must not be described as a complete runnable legacy deployment.

Confirmed deployed global FK graph (TIPADDR edge is untrusted):

```mermaid
erDiagram
    TGROUP ||--o{ TCHANNEL : "CONFIRMED bGroupID"
    TGROUP ||--o{ TSERVER : "CONFIRMED bGroupID"
    TMACHINE ||--o{ TSERVER : "CONFIRMED bMachineID"
    TMACHINE ||--o{ TIPADDR : "CONFIRMED definition; untrusted"
    TMACHINE ||--o{ TNETWORK : "CONFIRMED bMachineID"
```

Gameplay relationships inferred from named SQL parameters/joins and source:

```mermaid
erDiagram
    TACCOUNT_PW ||--o{ TCHARTABLE : "INFERRED dwUserID; cross-DB"
    TCHARTABLE ||--o{ TINVENTABLE : "INFERRED bag metadata"
    TCHARTABLE ||--o{ TSKILLTABLE : "INFERRED learned skills"
    TCHARTABLE ||--o{ TQUESTTABLE : "INFERRED progress"
    TQUESTTABLE ||--o{ TQUESTTERMTABLE : "INFERRED character+quest"
    TGUILDTABLE ||--o{ TGUILDMEMBERTABLE : "INFERRED dwGuildID"
    TCHARTABLE ||--o{ TGUILDMEMBERTABLE : "INFERRED dwCharID"
    TITEMCHART ||--o{ TITEMTABLE : "INFERRED wItemID"
```

TITEMTABLE ownership is polymorphic (`bOwnerType,dwOwnerID`), and storage is a
separate tuple (`bStorageType,dwStorageID,bItemID`). A single unconditional character
FK would be wrong for guild/mail/other ownership. TINVENTABLE and item instances
must remain separate until the exact container/slot model is settled. `dlID` in
TITEMTABLE is **not identity**: the allocator is TDBITEMINDEXTABLE/TGenerateDBItemID.
Several tables including TSKILLTABLE have no declared PK; row duplicates must be
profiled before introducing a candidate unique key.

Reference checks are in [relationship-checks.json](evidence/relationship-checks.json).
Quest terms→quests has 0 unmatched rows; rewards→quests has 62; spawn mappings→
monster/spawn have 2/6; monster loot→monster has 2; skill data→skill has 0.
These are raw comparisons, not asserted mandatory FKs: sentinels, removed content
and fallback behavior must be reviewed. A naive same-ID monster-attribute join
produces 7,170 unmatched rows and is deliberately labelled UNVERIFIED. The legacy
source proves attribute-template lookup through wMonAttr instead; that contract
has 39 missing (attribute,level) combinations versus 3,468 for the portable lookup.

Completeness has separate, explainable dimensions
([completeness.json](evidence/completeness.json)):

| Dimension | Confirmed numerator/denominator and limit |
|---|---|
| Supplied schema | 268/268 tables, 2,405 columns, 353/353 readable modules; complete metadata within the two supplied backups |
| Relationships | All 5 declared FKs captured, 1 untrusted; 0 declared game FKs. Inferred graph incomplete and cannot be scored against an unknown ground truth |
| Static content | 93 known CHART tables, 81 nonempty across both DBs. Imported 14 of 81 game CHART tables plus TSKILLDATA; 106,692 selected rows value-verified. Other content remains in originals |
| Source persistence availability | 143 of 158 portable application object names have backup matches; exact behavior/signatures/ownership are not implied |
| Historical players | 0 character, item-instance and guild rows; one global account and no TACCOUNT_PW rows. Player progress cannot be fabricated from audit counts |
| Operational compatibility | Restores/CHECKDB passed; PostgreSQL game startup and legacy cross-DB procedure execution unverified/incomplete |

No single completion percentage is assigned. Backup schema completeness does not
prove game-content completeness, release matching, player recovery or current
server compatibility. Data gaps include external databases/services, modern
extensions, original client/patch pairing and the missing source relations above.

Full known-entity inventory follows. Counts are exact COUNT_BIG; CONFIRMED refers
to original structure/data availability, not a modern business interpretation.

| Original entity | Columns | Declared PK | Exact rows | Provenance |
|---|---:|---|---:|---|
'''+ '\n'.join(entities))
    write('07-postgresql-architecture.md','''# 07 — PostgreSQL target architecture

The target keeps three distinct layers in one logical PostgreSQL database:

```mermaid
flowchart LR
    B[Immutable SQL Server backups] --> L[legacy_global / legacy_game / legacy_game_tgame snapshots]
    M[reconstruction runs, hashes, byte provenance] --- L
    L --> V[Explicit transformations and semantic validation]
    V --> C[Canonical auth / game / social / content / ops / audit domains]
    C --> S[Existing C++ SOCI repositories and application services]
```

**Implemented:** reconstruction control plane, all 268 typed snapshot tables,
exact text-byte sidecars, content.releases and content.item_templates. Only the
item projection is implemented in the canonical layer. Auth/game/social/ops/audit
below are **PROPOSED specifications**, not already-created functional schemas.
Snapshot tables retain original names and values; `_import_run_id` and
`_source_row_number` preserve provenance and duplicate rows in keyless tables.
Original PKs become per-run unique constraints. Original FKs, business defaults,
identities and non-PK unique/index semantics are preserved as evidence, not blindly
applied to incomplete staging subsets. Stage columns map one-for-one via mapping.json.

| Proposed domain | Ownership and invariant | Source evidence / access pattern |
|---|---|---|
| auth.accounts / credentials / sessions | Login owns account policy; stable protocol user ID; credential scheme explicit; at most one active session claim per account | soci_auth_service, TCURRENTUSER; unique claim + transactional insert; rebuild transient sessions at cutover |
| game.characters / progress / positions | Login owns create/delete, Map owns saves; stable char ID; one active char per account slot and approved name equivalence; save version for optimistic concurrency | soci_char_service, soci_player_service, TCHARTABLE; character-scoped transactions |
| game.item_instances / locations / inventories | Stable dlID, one owner/location, transactional transfers; container slot uniqueness after sentinel rules; retain six legacy attribute/time slots initially | TITEMTABLE, TINVENTABLE, TSaveItem*; owner/storage-filtered access |
| game.skills / quest_progress / quest_terms | Character FK; candidate char+skill/quest/term keys gated on duplicates and term-type semantics | queries.h, TSKILLTABLE/TQUEST*; character-scoped batch load/save |
| social.guilds / members / friends / mail | Guild/member/character FKs, one applicable membership, directional friend edge; mail currency/item ownership atomic | guild_entities, friend_entities, TPOST*/TAUCTION*; ID/guild/recipient filters |
| content catalogs | Immutable release IDs, approved active release per realm, boot cache; no per-hit SQL lookup | Soci*Chart and spawn_manager; content.releases/item_templates are implemented subset |
| ops / audit | Registry, auth and history isolated from gameplay privileges; retention based on measured volume | peer registry/metrics, TLOG_AUDIT, TOP_AUDIT_LOG; append batches and time-range reads |

Use one PG database with schemas so former global/game writes can share one
SOCI transaction. Splitting databases before a demonstrated requirement would
recreate distributed transaction problems. Keep migration/DDL credentials separate
from runtime roles; application roles must not update historical snapshots.
The verified disposable database used an administrative role only. Production
GRANT/role provisioning is a pending deployment contract, not tested here.

| Observed source type/semantics | Snapshot mapping | Canonical policy |
|---|---|---|
| tinyint (BYTE) | smallint, CHECK 0..255 | Boolean only for a proven 0/1 field; otherwise byte enum/flag |
| smallint | signed smallint | WORD fields widen to integer 0..65535 when protocol evidence proves reinterpretation; handle -1 sentinels explicitly |
| int | signed integer | DWORD fields may widen to bigint 0..4294967295; signed source bits are not automatically a negative balance/error |
| bigint | bigint | Preserve dlID and world allocation ranges; do not replace wire IDs with UUIDs or invent unsigned overflow rules |
| real / float(p) | real / real if p<=24 else double precision | Preserve numeric representation; currency units/decimal scaling require domain proof |
| varchar/char/text | decoded text + exact original source bytes | Preserve source collation/code page metadata; approve normalization/case/trailing-space equivalence before unique indexes |
| nvarchar/nchar | Unicode text + exact UTF-16 source bytes | UTF-8 PG text; validate surrogates/length; do not assume names are ASCII |
| smalldatetime/datetime | timestamp without time zone | Keep original precision/naive time until source timezone is known; new ops timestamps timestamptz/UTC |
| image/binary/varbinary | bytea | Preserve raw bytes; versioned codecs before interpreting serialized fields |
| numeric/decimal | numeric(p,s) generator support | No such type in these backups; not claimed tested with real source rows |

Both database collations have code page **1252**. That does not prove historical
strings were correctly encoded; Korean-origin code/content cannot establish CP949
for every varchar field. Exact source-byte preservation plus Unicode value hashes
allow later diagnosis without destructive mojibake repair. A PG `text` column's
byte length and collation do not reproduce SQL Server varchar limits or
case/accent/trailing-space uniqueness automatically. No citext/ICU locale is
selected without a corpus comparison and login/name protocol policy.

Real exceptions: TSKILLCHART.wMapID=-1; dwDuration contains signed values whose
uint32 reinterpretation must follow source DWORD use. NPC fPosZ has a value about
-9.4e26: preserve it in reconstruction and flag/quarantine for publication rather
than silently clamping. All signed columns are preserved unchanged in staging.
The implemented item projection alone reinterprets wItemID as uint16 (legacy
DBAccess.h WORD and portable Narrow16), verified including -1→65535 synthetically.

Identity values are imported explicitly. Future canonical generators must resume
above both the source identity high-water counter and actual max imported ID,
respecting step/world ranges, rather than merely restarting at 1 or using max of
remaining rows after deletions. TITEMTABLE uses a separate world counter, not
identity. PG identity/sequence allocation is not gapless or rollback-able
([official sequence documentation](https://www.postgresql.org/docs/18/functions-sequence.html)).
Numeric protocol IDs remain stable; UUID is not needed here. JSONB is used only
for the small import manifest/control metadata, not flattened character/item rows.
Legacy attribute slots remain columns until their domain and query patterns justify
a child table; separate release/item keys in the canonical projection support an
immutable boot catalog without scanning all snapshots.

Authentication compatibility: `Client/TClient/TClientWnd.cpp` and modern
`soci_auth_service.cpp:49` document SHA1-hex on the wire and BCrypt over that value.
Existing modern code rejects non-BCrypt rows. **Hashing a stored SHA1-hex with
BCrypt can wrap the existing wire secret; it does not recover the plaintext or
create a plaintext-based modern hash.** Never apply that to arbitrary MD5/plaintext
rows without the client contract. Current backup TACCOUNT_PW has 0 rows, so no
populated auth migration has been verified. Retain a credential-scheme field in
the proposed model; preserve recognized compatible hashes, reset unknown schemes,
or rehash on successful verified login using the actual submitted secret. A future
plaintext/Argon2 path needs a client/auth protocol change. Existing bcrypt_migrate
was inspected, not executed; its generic wrapping cannot certify unknown inputs.

Expected MMORPG workload strategy (architectural expectations, **no benchmarks**):

- Cache immutable charts at boot/release change. Keep AI/combat ticks and monster
  instances in memory; persist only durable outcomes at bounded save boundaries.
- Add owner/storage indexes for items, character indexes for skills/quests,
  account+slot and approved active-name uniqueness for characters, guild ID for
  membership, and recipient/status/date for mail after real EXPLAIN/workload checks.
- Lock stable item/character/auction IDs in a consistent order during transfers;
  include currency and ownership in one transaction. Retry detected deadlock or
  serialization failures with bounded policy and idempotent operation keys.
  [PostgreSQL row locks](https://www.postgresql.org/docs/18/explicit-locking.html)
  provide the mechanism, not the application invariant by themselves.
- Replace shared item counter hot spots with a reviewed range-aware allocator.
  Retain save versions to reject stale concurrent character writes. Avoid N+1
  guild/member loads; current ORM already performs batched rowset reads.
- Partition audit history only after retention/volume measurements justify it;
  699,982 historical TLOG rows establish data volume, not throughput or a mandatory
  partitioning scheme. Never claim scaling figures from this forensic test.
''')
    write('08-migration-strategy.md','''# 08 — Reproduction, migration and verification pipeline

Execution order is controlled by checksummed numbered migrations, **not historical
fragment filenames**. Never execute the old development fixtures on an existing
database: they contain DROP/CASCADE and incompatible subsets. Use a new disposable
instance for investigation; use reviewed production roles/backup policy for a
later deployment. Current game code remains unchanged.

```mermaid
flowchart TD
    B[Original hashes and backups] --> R[HEADER / FILELIST / VERIFY / isolated restore / CHECKDB]
    R --> M[Metadata and schema comparison]
    R --> X[Allowlisted reference rows + exact text bytes, private files]
    M --> D[001 control → 002 typed snapshots → 003 item catalog → 004 text provenance]
    X --> P[Manifest/file/hash/null/range/PK preflight]
    D --> P
    P --> T[Per-table transaction + run lock]
    T --> H[Row count + multiset value hash + byte hash]
    H --> J[Committed checkpoint]
    J --> C[Canonical item projection + six-field comparison]
    C --> V[Verified import; semantic content approval still separate]
```

Reproduction from repository root (Python 3, rootless Podman, Linux containers;
SQL Server 2022 Developer and PostgreSQL 18 images). Dependencies are limited to
the standalone migration tool; the server remains C++/SOCI.

```sh
DB_WORK=/tmp/4story-database-lab
python3 -m venv /tmp/4story-database-tools
/tmp/4story-database-tools/bin/pip install -r tools/database/requirements.txt
python3 tools/database/disposable_environment.py start --work "$DB_WORK"
SQL_CONTAINER="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["mssql_container"])' "$DB_WORK/state.json")"
python3 tools/database/inspect_backup.py --container "$SQL_CONTAINER" \
  --private-output "$DB_WORK/inspection" --restore-disposable
python3 tools/database/forensics.py --private-backup-metadata "$DB_WORK/inspection"
python3 tools/database/profile_data.py --container "$SQL_CONTAINER" \
  --private-metadata "$DB_WORK/inspection" \
  --output _rewrite/docs/database-reconstruction/evidence/data-profiles.json
python3 tools/database/generate_migrations.py
python3 tools/database/extract_reference.py --container "$SQL_CONTAINER" \
  --output "$DB_WORK/reference-snapshot"
python3 tools/database/disposable_environment.py run-pg --work "$DB_WORK" -- \
  /tmp/4story-database-tools/bin/python3 tools/database/migrate.py schema
python3 tools/database/disposable_environment.py run-pg --work "$DB_WORK" -- \
  /tmp/4story-database-tools/bin/python3 tools/database/migrate.py import-reference \
  --manifest "$DB_WORK/reference-snapshot/manifest.json"
python3 tools/database/disposable_environment.py stop --work "$DB_WORK"
```

Choose a fresh nonexisting work directory. start waits for database readiness,
creates only labelled UUID-named containers/network, uses random passwords in
mode-0600 env files and publishes PG only on a random localhost port. run-pg
injects PG* variables without printing them. stop verifies ownership labels and
removes only those resources; private evidence files remain for the operator.
No host SQL service or game startup is involved. The initial manual inspection
environment was likewise isolated, with fixed forensic container names; later
the reusable labelled helper was independently verified.

inspect_backup refuses nonempty instances except the two fixed forensic databases
under explicit --resume-disposable. It never replaces them. Metadata/full original
modules remain private in a mode-0700 directory; the public forensic generator
publishes schema, hashes, dependencies and aggregate counts only.

generate_migrations derives 001–003 and mapping.json from public metadata; 004 is
a reviewed hand-written migration. Regenerate/review **before first application**.
After application, create a new numbered migration rather than editing old SQL.
The runner uses a migration advisory lock, transactional DDL and a checksum ledger;
an altered previously applied file is rejected. `schema` is safe to repeat.

extract_reference verifies the container backup hashes against the pinned source
inventory. It exports only **15 explicit gameplay catalog tables**; source rows,
raw text bytes and manifests stay outside the repository with restrictive modes.
It rejects an existing snapshot manifest and writes new snapshots, never overwrites
originals. `float/real` use SQL Server CONVERT style 3 on the restored modern engine
to retain distinct numeric values ([Microsoft conversion reference](https://learn.microsoft.com/en-us/sql/t-sql/functions/cast-and-convert-transact-sql?view=sql-server-ver17)).
Snapshots were read from isolated restored copies with no game or concurrent writer.
Live-source extraction would additionally need a coordinated snapshot/isolation
contract; this utility does not claim CDC/live-online migration support.

The importer validates metadata fingerprint, table allowlist, path containment,
file hashes, column sets, row counts, nullability, signed ranges and duplicate PKs.
Byte sidecars must exactly cover textual columns and row positions. Each table is
loaded in one transaction; hashes/counts and its checkpoint commit together. Data
is not overwritten across imports: a manifest identifies a run, and provenance
keys separate different snapshots. Original keyless duplicates are retained.
Per-table load order currently follows deterministic metadata order: the selected
snapshot tables have no mandatory cross-table FK; canonical release is created
before item rows. For future dependent canonical domains, load auth→characters→
containers/items→skills/quests→guild/social→mail/auction and validate each FK edge.

Re-running a manifest re-reads and checks checkpointed target rows/bytes, skipping
only verified matches. Drift is rejected, not repaired silently. A failure rolls
back the current table and records only table/SQLSTATE/error code in a separate
transaction; previous checkpoints remain recoverable. Source preflight failures
can report a row ordinal without data values. Driver-level batch failures may not
identify the failing row; row payloads/credentials are never logged. The current
reader buffers a catalog table in memory; chunked checkpoints/streaming are a P2
task for very large or sensitive datasets, not claimed implemented.

Row verification uses an order-independent multiset hash (duplicates and NULLs
preserved); real/double use IEEE float bits, timestamps retain microsecond values,
and binary/text data has independent byte preservation. JSONB journals contain
manifest/provenance, not source rows. The canonical item projection compares all
6 projected values including item identity, plus row count; all other source columns remain in
the typed snapshot. No missing rows, currencies, accounts or client data are seeded.

Before any player migration: establish credential/client contracts; select an
approved timezone/collation policy; classify sentinels; reconcile account/global
character indexes; resolve item/world ID allocation; scan duplicates/orphans;
implement explicit persistence adapters and test transactions/concurrency. Preserve
source keys and restart generators above historical high water. Do not import
TCURRENTUSER as active sessions. Import audit/private data only through a separately
approved restricted pipeline; this reference importer intentionally rejects it.
''')
    write('09-gaps-and-risks.md','''# 09 — Exact gaps, conflicts and required evidence

| Gap / observed conflict | Evidence | Required resolution |
|---|---|---|
| No 4story.bak and no authoritative game/client/content pairing | Repository-wide inventory; TVERSION=0x2918 source; global TVERSION empty | Supply original distribution/build/patch manifest and client assets to identify exact supported release |
| No recovered player world/auth corpus | TACCOUNT=1, TACCOUNT_PW=0, TCHARTABLE/TITEMTABLE/TGUILDTABLE=0; exact counts | A populated, authorized account/game backup pair and migration-safe fixture corpus; do not invent progress |
| Missing external DBs/services | sys.sql_expression_dependencies points to TGLOBAL_GSP/TGAME_GSP/TQUEST_GSP/TGLOBAL/TGAME/fourstory_ob and linked servers | Original schemas/data or a reviewed replacement contract; remapping only names does not recreate billing/content behavior |
| Global/game catalog conflict | Item catalogs have 44/52 columns, 2,519/7,706 rows, mismatches across 37 shared numeric fields; monster catalogs also differ | Select domain owner/source release explicitly; investigate stale-cache vs revision provenance, never merge solely by same table name |
| Current server cannot select PG backend | SessionPool throws; vcpkg SOCI only ODBC | Opt-in Linux SOCI PostgreSQL wiring, backend integration test, then repository adapters |
| MSSQL-specific calls/dialects remain | SpCall DECLARE/EXEC/OUT; World 37 unique SP call names; TOP/identity/date expressions | Preserve protocol return/output contracts in application/persistence transactions; no blind PG function conversion |
| Monster attribute key differs | Legacy TMap.cpp:629 wMonAttr versus spawn_manager.cpp:65 monster ID; 39 vs 3,468 misses | PostgreSQL migration 006 now derives the current monster-ID contract through backup wMonAttr/bLevel: 3,495 matches, 39 retained gaps. C++ backend/gameplay integration and gap classification remain pending |
| Required patch/Bow procs absent | TUpdatePreVersion/TBetaToVersion/TDeletePreVersion and TAddBOWPlayer/TClearBOWPlayers/TDeleteSingleBOWPlayer source callers | Obtain authoritative definitions or reconstruct behavior with call/parameter tests before enabling features |
| Modern extension schemas incomplete | Peer/auth/metrics/operator audit absent in backup; 2FA/audit dev scripts partial | Versioned production migrations for exact current requirements; independent permissions and data ownership |
| Validator/schema drift | Guild dwIndex/szBody/timeWrite vs actual dwID/szArticle/dwTime; wItemKind/bRole absent | Adapt derived PostgreSQL contracts after proving semantics; preserve backup columns and avoid inventing wItemKind/bRole values solely to satisfy validators |
| Inventory/time mismatch | smalldatetime NOT NULL dEndTime versus int64 read/NULL insert | Establish bag vs item semantics, expiration sentinel and time conversion; behavioral fixtures |
| Real reference anomalies | 62 reward→quest, 2 spawn→monster, 6 spawn→spawn mismatches; extreme NPC fPosZ | Determine deliberate retired/sentinel content vs errors; quarantine publication, retain original evidence |
| Unsigned/sentinel interpretation | TSKILLCHART negative dwDuration and wMapID=-1; legacy DWORD/WORD source | Per-field bit/sentinel mapping with protocol tests, not blanket widening or rejecting negatives |
| Collation/code page uncertainty | Latin1_General_CI_AS_KS, CP1252; Korean-origin/Unicode material; raw bytes preserved | Compare actual normalized login/name corpus, source-byte provenance and client encoding; choose PG policy based on behavior |
| Historical date timezone unknown | SQL datetime/smalldatetime naive, modern std::mktime/int64 paths | Original server timezone/runtime convention and expiry semantics; no automatic Europe/Prague/UTC reassignment |
| Missing game FK semantics | Zero declared game FKs, one untrusted global FK; sparse player data | Infer/test each relationship and polymorphic owner case, validate populated data before constraints |
| Item ID allocator semantics | TGenerateDBItemID world ranges 2^56; TITEMTABLE not identity; modern TOP/fallback | Preserve world/ID high water and atomic allocation; prove no duplicate/overlap on transfer/import |
| Authentication hashes not interchangeable | BCrypt over client SHA1 wire secret; generic offline wrapper; no populated TACCOUNT_PW corpus | Scheme classification, approved wrapper only for matching wire secret, reset/rehash policy and synthetic auth tests |
| Existing SQL logger exposes rendered values | fourstory/db/orm/sp_call.cpp debug/error query logging | Redact parameters and backend exception details before sensitive application migration |

These are concrete findings; they do not justify assuming every static unmatched
key is an error. The 7,170-row naive monster-attribute comparison is explicitly
not a proven FK. Existing `SCHEMA.md` claims game FK absence is intentional for
performance and labels a game version; neither inference is proven by metadata.

The supplied sources/definitions are sufficient for additional behavior work,
but schema metadata alone cannot establish exact wire versions, unknown external
service behavior, lost player rows or historical encoding/timezone decisions.
None of the blockers stopped backup recovery, metadata reconstruction, static
data migration or isolated verification. The canonical player/domain schemas and
full stored-procedure behavior suite remain proposed work, not hidden placeholders
or fabricated working data.
''')
    write('10-implementation-roadmap.md','''# 10 — Prioritized implementation roadmap

| Priority | Concrete step | Acceptance evidence |
|---|---|---|
| **P0** | Establish authoritative client/content/server pairing and freeze source hashes | Explicit version/protocol matrix; signed-off UNKNOWNs or supplied release manifest |
| **P0** | Database compatibility view implemented through legacy wMonAttr; review remaining 39 gaps and bad NPC coordinate, then connect the C++ PG backend | Migration 006: 3,495 real matching monster stat rows; C++ spawn/stat/combat integration still required |
| **P0** | Extend derived PostgreSQL contracts for guild validator drift, inventory expiry/container semantics and missing required calls, retaining backup authority | Each derived transform/extension has a reviewed behavior fixture and owner; optional features explicitly disabled if unavailable |
| **P0** | Define auth wire/hash, source collation/timezone and item/world ID policies; redact rendered SQL logging | Synthetic auth/ID/time/encoding cases, duplicate-login/slot/item invariants, privacy audit; no unknown account data conversion |
| **P1** | Enable SOCI PostgreSQL on Linux through an explicit build option and link/backend test; keep Windows ODBC supported | Fresh Linux container SessionPool PG acquire/query/transaction test; unchanged ODBC checks |
| **P1** | Implement first complete persistent vertical: account/session + character create/load/save/delete and reviewed item ID allocator | Canonical DDL + explicit legacy transforms + real PG repository tests including rollback/concurrent slot/name/session conflicts |
| **P1** | Extend canonical catalogs from immutable snapshots; configure an approved active content release and production roles | Content semantic checks, name/ID mapping, checksum/byte verification and cache boot without old cross-DB wrappers |
| **P2** | Port inventory/item transfer, quests/skills, guild/social/mail/auction transactions | Idempotent operations, owner/location constraints and failure/concurrency tests; populated migration corpus needed |
| **P2** | Replace event/rank/Bow/tournament/admin procedure responsibilities using existing workers/services | Preserve return/output contracts; award idempotency, schedule recovery and no hidden SQL Server fallback |
| **P2** | Add streaming/chunk checkpoints and restricted player/audit extraction | Repeatable large imports with interrupted recovery, exact byte/value/row reconciliation and no private row output |
| **P3** | Optimize indexes, persistence batching, retention and partitioning from measured workloads | EXPLAIN plans, contention/concurrency tests and recorded benchmarks; no fabricated throughput claims |
| **P3** | Containerized staging rehearsal, client smoke flow and controlled deployment rollback plan | Full login→load→save→transfer→logout behavior on pinned client/catalog release, coordinated backups and recovery drill |

Recommended next implementation unit: **P0 source/schema compatibility contract
and monster-stat key correction**, then P1 opt-in Linux PostgreSQL SessionPool and
the account/character vertical. Merely switching backend or applying a dev schema
would conceal the observed content/behavior conflicts. Do not migrate production
players until their corpus and auth/encoding/time/ID policies exist.

This task delivered the isolated forensic/import foundation. The current server
still uses MS SQL/ODBC; no wholesale repository rewrite, new application ORM or
unrelated server changes were introduced.
''')
    write('11-verification-report.md','''# 11 — Executed verification and limitations

Date: 2026-10-08. Evidence applies to the exact original hashes in report 01 and
the generated artifacts in this checkout. Rootless Podman was used because the
host Docker socket was not accessible. Container image identities/digests are in
[environment.json](evidence/environment.json); no credentials are included.

| Action | Actual result / evidence |
|---|---|
| Recursive repository inventory | 731 pre-existing artifact files; generated SQL/mapping artifacts additionally recorded; SHA-256/format/purpose per file |
| Backup HEADERONLY/FILELISTONLY/VERIFYONLY | Both completed; metadata in backup-metadata.json |
| Isolated restore | Both databases restored on SQL Server 16.0.4265.3; ONLINE, compatibility 100 |
| DBCC CHECKDB | Both exit 0, no diagnostic output; no backup checksum falsely claimed |
| Restored catalog extraction | 268 tables, 2,405 columns, 294 indexes, 5 FKs, 353 readable modules; full schema/parameters/dependencies in backup-schema.json |
| Exact COUNT_BIG | All 268 tables; one partition estimate mismatch corrected by exact count |
| Backup/export comparisons | All 268 column projections equal; 350 module name matches, 3 rename pairs; identical export directories |
| Generated MSSQL table DDL | Executed in two fresh reconstructed_* DBs; all 2,405 logical column definitions match under documented normalization |
| PostgreSQL schema migrations | 001–004 applied successfully; 268 typed historical snapshot tables plus control/text provenance and canonical content |
| Actual source reference import | **106,692 rows / 15 tables**, per-table value hashes match; textual source-byte hashes independently match |
| Canonical item projection | **7,706 rows**, item identity and all 5 projected attributes (6 values) compare exactly |
| Repeat actual import | Same run/manifest reused; all 15 target checkpoints rechecked and skipped without duplicate rows |
| Synthetic tests | **14/14 passed**: lexical normalization, literals, multiset/float/time/byte hashes; PG checksums, Unicode/unsigned WORD, byte preservation, tamper/duplicate/range/null rejection, rollback/resume, checkpoint drift and database CHECK |
| Reusable disposable helper | Fresh labelled UUID containers, readiness, all four migrations and owned-resource cleanup passed |
| Original preservation | Read-only source access, copies for restore; hash and Git-diff check recorded in preservation evidence |

Machine-readable results: [pg-import-with-bytes-verification.json](evidence/pg-import-with-bytes-verification.json),
[pg-repeat-import-verification.json](evidence/pg-repeat-import-verification.json),
[mssql-reconstruction-verification.json](evidence/mssql-reconstruction-verification.json),
[test-results.json](evidence/test-results.json). The first import preceded text-byte
sidecars; the final byte-preserving run and replay are the primary acceptance evidence.
Different snapshot manifests create separate provenance runs rather than overwriting
one another; counts above describe the accepted snapshot, not the sum of rehearsals.

Table DDL normalization deliberately retains logical order rather than recreating
physical column-ID holes left by historical DROP COLUMN, and ignores redundant
outer parentheses introduced by SQL Server for constant defaults. 7 global and
3 game physical column IDs differ in the fresh reconstruction. This is recorded,
not hidden as byte-for-byte engine storage identity. Secondary indexes, FKs,
permissions and module behavior were not recreated by those table-only scripts.

Development failures corrected during this investigation: sqlcmd disallows -h
with -y0, emits no JSON row for empty FOR JSON results, and very long stdin SQL
lines need line breaks. The initial helper smoke ran a migration before PG was
ready; start now explicitly waits for both databases and its fresh-environment
smoke passed. These were tooling/reproduction failures, not damaged backup files.

Not executed or established: game server startup against PostgreSQL; Windows
build; full auth/player import; old development SQL scripts; legacy business
procedures or external billing calls; all semantic catalog constraints; client
release compatibility; production deployment; live-source CDC or performance
benchmarks. Existing C++ game unit tests from the earlier container task are not
counted as database migration tests here. No schema metadata/value comparison is
substituted for a full end-to-end gameplay test.

Verification commands:

```sh
python3 -m unittest discover -s tools/database/tests -p test_forensics.py -v
# PG integration requires psycopg and a NEW empty disposable database named
# fourstory_synthetic_* with PG* environment supplied privately:
FOURSTORY_DB_TESTS=1 python3 -m unittest discover -s tools/database/tests -v
```

Integration tests refuse a database outside that explicit synthetic naming
contract. The original real-data copies were isolated, and the synthetic tests
used a separate fresh database. Private snapshot/module files are outside Git;
no account row, credential, token or private audit payload was exported or committed.
Temporary database containers/networks were removed after collecting evidence;
original backups, original fragments and private evidence files were retained.
''')
    procedures=read('backup-procedure-modernization.json')
    table=['# Procedure modernization index','',
           'All 323 original procedures. CONFIRMED names/contracts/dependencies; replacement responsibilities are PROPOSED. No PG business port is claimed. Full machine-readable matrix: [backup-procedure-modernization.json](evidence/backup-procedure-modernization.json).','',
           '| Original | Caller evidence (first non-test) | Explicit BEGIN TRAN | Lexical writes | Proposed responsibility | PG status |',
           '|---|---|---|---|---|---|']
    for proc in procedures:
        callers=[c for c in proc['callers'] if c['scope']=='source']
        caller='`'+callers[0]['path']+':'+str(callers[0]['line'])+'`' if callers else 'No indexed direct caller; may be indirect/admin'
        writes=', '.join('`'+t+'`' for t in proc['lexical_write_targets']) or 'No lexical target found'
        table.append(f"| `{proc['database']}.{proc['procedure']}` | {caller} | {proc['explicit_begin_transaction']} | {writes} | {proc['replacement']} | Not ported |")
    write('procedure-modernization.md','\n'.join(table))
    print('Wrote 11 required reports, navigation, entity/procedure indexes and version/completeness evidence.')


if __name__=='__main__':main()
