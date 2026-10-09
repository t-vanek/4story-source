# 4Story database reconstruction — evidence and migration foundation

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

Follow-up: the selected target is PostgreSQL, with immutable `.bak` authority
and database-side compatibility migrations. See the
[database policy and implemented catalog contracts](../../../database/README.md)
and [executed compatibility verification](evidence/postgresql-compatibility-verification.json).
