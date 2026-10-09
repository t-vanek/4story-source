# 03 — Backup investigation and recovery

Both root `.bak` files were copied into a new rootless Podman SQL Server container
on a private network. SQL Server had no published host port. No existing database,
service account or running game server was used. Originals were never mounted
writable. Restore destinations were fresh `forensic_TGLOBAL_RAGEZONE` and
`forensic_TGAME_RAGEZONE`, with new Linux MDF/LDF paths and **no WITH REPLACE**.

| Header field | TGLOBAL_RAGEZONE | TGAME_RAGEZONE |
|---|---|---|
| Backup start/finish | 2019-01-27 16:01:55 | 2019-01-27 16:01:33 |
| Backup sets | 1 full database set, position 1 | 1 full database set, position 1 |
| Writer | 11.0.2100, `GAMER-PC\OLDSCHOOL` | Same |
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
`C:\Program Files\Microsoft SQL Server\MSSQL11.OLDSCHOOL\MSSQL\DATA` path.
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
