# 11 — Executed verification and limitations

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
