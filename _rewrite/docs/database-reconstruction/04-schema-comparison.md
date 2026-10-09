# 04 — Backup versus fragments

Comparison unit: one restored database object and one export family. Results for
`schema` and `schema.old-dump-2019` are identical because the families are byte
duplicates. Do **not** add their counts to measure independent coverage.

| Object/projection | Classification | Count per export family |
|---|---|---:|
| FK_SET | COMPATIBLE | 2 |
| INDEX_SET | COMPATIBLE | 194 |
| MODULE | COMPATIBLE | 9 |
| MODULE | IDENTICAL | 341 |
| MODULE | ONLY_IN_BACKUP | 3 |
| MODULE | ONLY_IN_FRAGMENTS | 3 |
| TABLE_COLUMNS | IDENTICAL | 268 |

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
