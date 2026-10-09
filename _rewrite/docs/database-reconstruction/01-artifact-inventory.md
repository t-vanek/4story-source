# 01 — Artifact inventory

The recursive inventory contains **731 pre-existing artifact
files** plus 7 generated SQL/mapping artifacts
at the recorded scan. Every entry records relative path, byte size, detected format,
SHA-256 and suspected purpose in [artifact-inventory.json](evidence/artifact-inventory.json).
The generated report directory excludes itself to avoid recursive checksums.
Git internals, IDE caches, dependency/build output and generated build directories
are excluded and their file counts are recorded. Discovery used ignored and hidden
repository files too; it was not limited to tracked files or `_rewrite/docs`.

| Original backup | File bytes | SHA-256 |
|---|---:|---|
| `TGAME_RAGEZONE.bak` | 34,009,088 | `2cff1635d523322d3de8ddafaeacc3335581dc338dca901239ac80288f51d259` |
| `TGLOBAL_RAGEZONE.bak` | 36,560,896 | `fca545bcb90bcc72aa4f3514a6177c2b88d2b0bf71bf847b0128f090a20877fb` |

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
