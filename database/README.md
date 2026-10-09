# Backup authority and PostgreSQL compatibility

The user's selected target is **PostgreSQL**. The two original `.bak` files are
the authoritative historical database source. Adapt the derived PostgreSQL
database to exercised server contracts using numbered migrations and explicit
transforms. Keep backup contents and the faithful historical snapshot layer intact.

| Layer | Authority and allowed changes |
|---|---|
| `TGAME_RAGEZONE.bak`, `TGLOBAL_RAGEZONE.bak` | Immutable originals, pinned SHA256 fingerprints. Never overwrite them or replace their contents with assumptions from code or exported SQL. |
| `legacy_game`, `legacy_global`, `legacy_game_tgame` | Faithful imported source values and types with import provenance. Preserve signed bits, missing rows, duplicates and original text bytes. These are not application write targets. |
| `content` | Explicit derived canonical projections, each linked to an import release. |
| `game_compat` | Derived, read-only current C++ chart contracts. Database views adapt names and proven relationships while retaining source values. |
| `app_global`, `app_world` | Mutable application and operational state, added through separate migrations with documented ownership and defaults. |

A source/code disagreement is recorded with its caller, the backup definition,
the proposed database transform and a behavior test. Code describes the requested
application interface; the backup establishes original data and historical meaning.
When that meaning is unknown, keep a visible unresolved contract. Do not fabricate
player rows, role codes, timestamps, credentials or combat statistics to satisfy a
validator. Original SQL fragments, old dev fixtures and repository labels cannot
override the backup evidence. Global and game catalogs remain distinct; the
implemented gameplay views deliberately publish **TGAME**.

Migration files `001`–`006` are immutable. `005` creates explicit catalog
selection/history and `006` exposes the 15 reference tables with both quoted
legacy identifiers and case-folded aliases for unquoted queries. The views exclude
import bookkeeping columns. Their join to the selected release makes them read-only.

All subsequently applied migrations through **022** are also immutable. Current
native Map startup requires 022 and the updated
[`map-runtime-grants.sql`](../deploy/sql/map-runtime-grants.sql). `map_sessions`
retains one mutable primary; `map_replicas` adds operational grants and secondary
connection phases, bound to the exact primary/target process identities and
catalog releases. Secondary teardown never releases the primary account or writes
its character graph. These rows are new runtime state, not reconstructed historical
data. Migration 020 adds an exact transfer journal, authority epochs and full
transfer-state checkpoints. Movement-triggered primary handoff, source/target
replacement and graph-backed relogin are verified against native PostgreSQL.
See the [primary transfer contract](../_rewrite/docs/modernization/evidence/native-primary-transfer-contract.json)
for tests and remaining gameplay requirements. Add schema changes as 023+.

Migration 021 adds fresh-primary skill checkpoints (contract 3). Initial readiness,
periodic checkpoints and final logout atomically store remaining skill durations
with the character core and a receipt. Recovery and transfer validate the receipt
against both durable core and skills. Only `dwRemainTick` receives a new Map write
grant; learning, deleting skills and changing ranks remain separate work. Contract
2 continues to own complete state after a primary transfer. Apply 021 and the
updated Map grants before deploying these binaries; old binaries cannot write
contract-3 receipts. Historical data and migrations 001–020 are unchanged. See the
[skill checkpoint contract](../_rewrite/docs/modernization/evidence/skill-checkpoints-contract.json).

Migration 022 adds immediate single-reagent consumption for fresh primary PCs.
The exact item row fingerprint fences a decrement or last-item deletion. Inventory,
core HP/MP, learned timers and a consumption receipt commit before client success.
The runtime coordinates these writes with periodic checkpoints; an uncertain
transaction closes the client and blocks stale final saves until process recovery.
Map grants add only item-count updates, item deletion, fingerprint execution and
receipt insertion. Transferred graphs, equipped reagents, ammunition and cash
exceptions remain outside this scope. See the
[reagent contract](../_rewrite/docs/modernization/evidence/skill-reagents-contract.json).

`game_compat."TMONATTRCHART"` maps the current monster-ID lookup to the original
`TMONSTERCHART.wMonAttr` attribute family at the template's `bLevel`. It exposes
the monster ID as `wID` and original combat values as the remaining fields.
Original `legacy_game."TMONATTRCHART".wID` values are unchanged. This contract
covers the current spawn call; alternate/summoned attributes and dynamic levels
need separate evidence and migrations if introduced. Missing combinations are
listed in `runtime_control.missing_monster_attributes`, never filled with made-up
values. The pinned data contains **3,495 matches and 39 gaps** among 3,534 monsters.
The current static spawn path now refuses missing attributes instead of inventing
HP. Original selection/regen parity remains tracked in the
[behavior review](../_rewrite/docs/modernization/protocol-compatibility.md).

After applying the checksummed schema and importing a complete reference manifest,
activate that exact manifest using the same PG environment as the importer:

```sh
python3 tools/database/migrate.py schema
python3 tools/database/migrate.py import-reference --manifest /private/snapshot/manifest.json
python3 tools/database/activate_catalog.py --manifest /private/snapshot/manifest.json
```

Use the isolated environment helper described in the
[reproduction guide](../_rewrite/docs/database-reconstruction/08-migration-strategy.md)
to inject credentials without logging them. Activation requires both pinned
backup fingerprints, all 15 tables, a verified import, original text-byte sidecars
and fresh row/value/byte reconciliation. It executes the 14 current map chart
queries, validates monster mapping cardinality, and commits the selected run and
history together. A failed check rolls back selection. Repeating the same manifest
does not create another activation record. No automatic "latest import" selection
occurs. Re-activate a previous verified manifest to roll back selection.

Restart servers to select a new catalog; coordinated live reload is not implemented.
The native Map bootstrap loads all caches in one repeatable-read, read-only
transaction and pins the configured manifest. Migration `007` exposes publication
metadata to that restricted reader. The native PostgreSQL driver is enabled on
Linux, but mutable T-SQL repositories still require explicit replacements.

For the native application catalog reader, use a non-owner role with only:

```sql
GRANT USAGE ON SCHEMA game_compat TO fourstory_catalog_reader;
GRANT SELECT ON ALL TABLES IN SCHEMA game_compat TO fourstory_catalog_reader;
ALTER ROLE fourstory_catalog_reader SET search_path = pg_catalog, game_compat;
```

Create that role separately according to deployment credential policy. Do not
grant it access to historical snapshots, reconstruction, migration control or
runtime selection, and do not make it an owner or a member of an administrative
role. The migrations do not create cluster-wide users or passwords. Database
owners/administrators can still change data; application isolation relies on these
permissions and verification detects any source drift before publication.

Remaining contracts include inventory expiry/timezone and bag/item semantics,
guild validator-only fields, auth/session/character writes, modern operational
extensions, item ID allocation and stored-procedure replacements. Track them in
[compatibility-contracts.json](postgresql/compatibility-contracts.json) and the
[forensic gap report](../_rewrite/docs/database-reconstruction/09-gaps-and-risks.md).
Use new numbered migrations; never edit already applied SQL to conceal a mismatch.

The isolated PostgreSQL verification imported and rechecked **106,692 original
rows**, exercised all 14 map chart SELECTs, and passed **21 tests** covering
source preservation, aliases, release selection/rollback, failure recovery,
checksum/target drift, missing source stats and restricted reader permissions.
See [verification evidence](../_rewrite/docs/database-reconstruction/evidence/postgresql-compatibility-verification.json).
The subsequent [native C++ verification](../_rewrite/docs/modernization/evidence/native-postgresql.json)
loads these caches in the actual Map daemon and verifies health and SIGTERM without
SQL Server. Full authenticated gameplay and mutable PostgreSQL persistence remain pending.


## Native map configuration

Use `deploy/tmapsvr-postgresql.example.toml`. Its `[content]` section resolves
`FOURSTORY_CONTENT_CONNECTION` from the environment; other TOML values are not
implicitly expanded. Supply native libpq options, for example
`host=db.example.net dbname=fourstory user=fourstory_catalog_reader sslrootcert=/run/secrets/catalog-ca.crt`.
Provide the password through the private environment value or a protected libpq
password file. Keep secrets outside version control and command-line arguments.

The default is `sslmode=verify-full` with a five-second connect timeout. Private
CAs require a readable certificate mount matching the DB hostname. Deliberate
`disable`, `require` or `verify-ca` overrides are accepted; `allow` and `prefer`
are rejected to avoid an implicit plaintext downgrade. `connect_timeout` must
be 1–300 seconds. A failed connection emits a fixed diagnostic without the DSN.

The `[content]` connection is independent of the still-legacy mutable `[database]`
connection. With only `[content]`, the map loads authentic catalogs and spawns,
but cannot authenticate or load/save players. All content queries finish before
network listeners open; failure to match the manifest aborts startup.

## Native mutable Login schema

`008-native-login.sql` creates empty `app_global` tables for the existing Login
service. It imports no historical accounts/passwords/player rows. Runtime role
setup is in [login-runtime-grants.sql](../deploy/sql/login-runtime-grants.sql);
use [the configuration example](../deploy/tloginsvr-postgresql.example.toml).
New event times are `timestamptz`; original dates remain untouched. New account
names use an ICU case-insensitive/accent-sensitive policy with trailing-space
normalization. Its exact SQL Server/code-page equivalence must be established
before importing historical users. Credential provisioning must bcrypt the
supported client's actual wire credential; never infer a populated password corpus
from the empty source `TACCOUNT_PW`.

Migration `009-login-runtime-owner.sql` enforces one active Login per application
DB before startup cleans pre-handoff sessions. The dedicated advisory-lock
connection and per-transaction owner token fence old writers after replacement;
owner loss shuts down Login with exit status 1. A replacement waits for in-flight
writers before recovering lobby sessions. Use direct/session-pooled PostgreSQL
connections; transaction pooling cannot hold this process lock. Full character
persistence, remote Map duplicate cleanup and multi-active Login remain pending.
Migrations 001–009 are applied and immutable; schema corrections need 010 or later.
