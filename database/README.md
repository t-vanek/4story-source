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

All subsequently applied migrations through **034** are also immutable. Current
native Map deployment requires migrations through 034 and the existing
[`map-runtime-grants.sql`](../deploy/sql/map-runtime-grants.sql). `map_sessions`
retains one mutable primary; `map_replicas` adds operational grants and secondary
connection phases, bound to the exact primary/target process identities and
catalog releases. Secondary teardown never releases the primary account or writes
its character graph. These rows are new runtime state, not reconstructed historical
data. Migration 020 adds an exact transfer journal, authority epochs and full
transfer-state checkpoints. Movement-triggered primary handoff, source/target
replacement and graph-backed relogin are verified against native PostgreSQL.
See the [primary transfer contract](../_rewrite/docs/modernization/evidence/native-primary-transfer-contract.json)
for tests and remaining gameplay requirements. Add schema changes as 035+.

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
receipt insertion. Transferred graphs are added by 023 below; equipped reagents,
ammunition and cash exceptions remain outside this scope. See the
[reagent contract](../_rewrite/docs/modernization/evidence/skill-reagents-contract.json).

Migration 023 extends the same ledger with `state_contract` and before/after full
graph hashes. Contract-2 consumption validates the current ready primary, authority
epoch, catalog receipt and exact original encoded item. It commits the updated
complete graph, core, cooldowns and audit receipt together. Current core and sampled
cooldowns may advance; learned IDs/ranks and unrelated graph sections must match.
Original uint64 item identities use their signed bigint bit pattern in the ledger.

The complete checkpoint remains authoritative after transfer and on graph-backed
relogin. This branch does not read or rewrite stale `TITEMTABLE`/`TSKILLTABLE` rows;
last-item deletion cannot be undone by reloading those rows. Migration 023 retains
existing receipts as contract 3 and needs no additional Map grants. Apply it before
deploying the new binaries, including for fresh-primary consumption. See the
[graph reagent contract](../_rewrite/docs/modernization/evidence/graph-reagents-contract.json).

Migration 024 adds `consumption_kind` to the existing item ledger, retaining old
receipts as `reagent` and marking new arrow/bolt transactions as `ammunition`.
Migration 025 extends this ledger for multi-unit and multi-stack debits.
`cast_id` groups all stack receipts of one transaction; existing records retain
separate cast identities. `hit_count` is 1–16 for non-expanded ammunition targets,
while reagent receipts still debit exactly one unit. All stack changes, core,
timers and the recovery checkpoint commit atomically. A second-stack conflict or
second-receipt failure rolls back the whole operation. The existing receipt
sequence allocates cast IDs; rollback gaps are expected and casts are not retried
after unknown outcomes.

Fresh transactions lock the selecting inventory and revalidate the loaded weapon
fingerprint, then recompute source per-bag/slot selection with BYTE arithmetic.
Complete graph transactions derive selection from their authoritative checkpoint.
No extra role privileges are needed. Migration 026 below extends this contract
to source-generated targets. See the
[batch contract](../_rewrite/docs/modernization/evidence/ammo-batch-contract.json).

Migration 026 adds `hit_mode` (`direct`/`expanded`) without changing historical
receipts or the atomic stack transaction. Expanded ammunition is validated against
pinned MTYPE_EFC rows and authoritative learned rank; all final hits must be paid.
Fresh transactions lock the learned row, while transferred characters use their
complete checkpoint and reject changed projection ranks. Existing Map grants
cover that column. Inventory migration 027 below extends the runtime schema. See the [multi-attack contract](../_rewrite/docs/modernization/evidence/multi-attack-contract.json).

Migration 027 adopts the existing `item_slot` unique index as a deferrable,
initially immediate constraint. Only the move transaction defers it while swapping
two positions, then forces validation before recording success. Existing item
values and identity/slot uniqueness are preserved. `inventory_movements` groups
one or two complete-stack relocations with exact item and graph fingerprints.
Fresh moves update only `dwStorageID` and `bItemID`; graph moves keep normalized
rows untouched. Reapply the updated Map grants for these two columns and the new
ledger/sequence. That increment made 027 immutable. See the
[inventory move contract](../_rewrite/docs/modernization/evidence/inventory-moves-contract.json).

Migration 028 adds `inventory_stack_changes` without modifying 027 receipts.
Each split/merge records source and destination counts/hashes under one operation.
Splits use the same `worlds.item_high_water` transaction as Login starter creation;
rollback restores the allocator along with items and recovery state. Graph splits
reserve a global ID without materializing stale normalized child rows. Apply the
updated Map grants for item INSERT, stack receipt INSERT and narrow high-water
UPDATE. Native audit uses 029, statistics 030, actor compatibility 031 and
equipment 032, maintained postures 033 and client cancellation 034. All are immutable; the next schema change is 035. See the
[stack contract](../_rewrite/docs/modernization/evidence/inventory-stacks-contract.json).

The pinned item chart contains 65 arrow-using and 77 bolt-using weapon templates,
all requiring one unit. Premium templates 25020, 25021 and 25022 referenced by the
Windows code are absent; no historical rows are fabricated. See the
[ammunition contract](../_rewrite/docs/modernization/evidence/ammunition-contract.json).

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

## Native audit contract (migration 029)

`029-native-audit.sql` creates the empty `app_audit` schema, contract version 1 and
`TLOG_AUDIT`. It does not alter restored snapshots, player data or earlier migrations.
The supplied backups do not include the external legacy daily audit tables; source
`LogPacket.h` and `CUdpSocket::LogDBSave` define the original audit fields. Identity
and receipt timestamp are explicitly modern metadata. No retention deletion or
historical event import is seeded.

`lt_clientip` and `lt_key1`–`lt_key7` hold bounded raw bytea values, preserving the
source CHAR bytes without guessing a client code page. `lt_log` stores the nonempty
payload unchanged, with NULL for an empty payload. Native writer and reader both
qualify `app_audit` independently of search_path. Apply the dedicated append-only
[grants](../deploy/sql/log-runtime-grants.sql) and use the
[native example](../deploy/tlogsvr-postgresql.example.toml). Deployment and the
upgrade/fault runner are described in [deploy/README.md](../deploy/README.md#native-postgresql-audit-ingest-and-queries).


Migration **030** extends `actor_compat` with item attributes and grade tables and
a completeness view. Publish a newly verified four-table actor manifest (9,773
pinned backup rows), reapply Map grants and use its manifest hash in native Map
configuration. Old imported releases and migration receipts remain unchanged;
old two-table releases cannot satisfy the new stat runtime contract. Existing
player graph/catalog bindings are not rewritten. Migration 031 below supports the
specific verified additive actor upgrade before switching a populated world. See
[Map startup](../deploy/README.md#native-map-character-statistics) and
[catalog upgrade evidence](../_rewrite/docs/modernization/evidence/character-statistics-catalog-upgrade.json).
Migration **031** adds a schema-owner-only directional compatibility certificate
and activation-linked owner retirement audit. `activate_actor_catalog.py
--manifest <four-table> --previous-manifest <two-table>` certifies both unchanged
shared charts only after verifying complete imports, original bytes, metadata and
pinned backup identities. All Map owners must be offline; detached workers lose
write authority through atomic token rotation. Player checkpoints and transfer
journals keep their original bytes/hashes/identities. Map receives SELECT only on
the current-target view, never certificate-write permission. Recovery checks remain
strict for character/routing catalogs and unsupported graph sections. See the
[offline procedure](../deploy/README.md#offline-actor-catalog-transition).
At that increment, migrations 001–031 became immutable; migration 032 follows below.


### Native equipment receipts

Migration **032** adds `equipment_operations` and `equipment_item_changes` to
`app_world`. The header records the exact five-byte original request, ownership
identity/epoch, storage contract, graph hashes, core fingerprint and final diff
size. Child rows distinguish created/deleted identities from existing items with
nullable before/after hashes and preserve the source parent of each allocation.
Zero-diff source-equivalent equipment requests are valid operations. These are
new operational records, not an import of historical item logs.

The existing Map transaction locks ownership/checkpoints, verifies the complete
fresh inventory or authoritative graph, rechecks learned ranks and pinned bag
rules, then persists displacement/split/swap, identity allocation, rederived
HP/MP clamping, sampled skill timers and receipts atomically. Graph operations
retain stale normalized rows unchanged. No automatic retry follows an unknown
commit. Equipment eligibility uses the original `bLevel` chart column and raw
full-width item extensions. Inactive account pets are preserved separately from
active-effect dependencies. Apply the current Map grants after migration.
See [contract and limits](../_rewrite/docs/modernization/evidence/equipment-contract.json).
Backups and migrations **001–034** are immutable; next migration **035**.

### Native maintained postures

Migration **033** adds ordered `map_maintained_effects` and fresh recovery contract
4 (`core_state` + `skill_state` + `maintain_state`). Empty maintained collections
keep contract 3. Contract 2 continues to hold the complete authoritative transfer
graph; its normalized children can remain stale and are never overwritten by a
graph-backed effect change. The original eight maintained fields are preserved;
the first integrated runtime scope is permanent warrior postures 131/132.

Equipment locks and validates the previous state, derives posture creation and
equipment cancellation on the server, then commits items, posture, core, timers
and receipt together. New equipment receipts record both effect collections;
their existing `state_contract` remains **storage** 2 (graph) / 3 (normalized),
distinct from checkpoint recovery version 4. Historical receipts remain unchanged.
Core-only saves compare the native collection and cannot silently replace it.

Reapply `deploy/sql/map-runtime-grants.sql`: the dedicated Map role gets only
SELECT/INSERT/DELETE on native maintained rows and execution of their comparison
function. No historical table grants or automatic unknown-commit retries are added.
Stop existing Maps before schema/application upgrade; use all six binaries from
the same tested image. See [posture evidence](../_rewrite/docs/modernization/evidence/postures-contract.json).

### Native client effect cancellation

Migration **034** adds `maintained_effect_operations`. Exact 19-byte source
requests, ordered effect collections and owner/connection/authority/graph/core
receipts are append-only. Own-PC permanent postures cancel atomically with native
core/cooldown recovery; fresh last-effect removal restores contract 3 and graph
state retains contract 2. Replicas never write. No-match requests preserve original
ACK semantics and record `removed=0`; an uncertain write is never automatically
retried. Prior applied migrations and backups remain immutable.

See [effect cancellation evidence](../_rewrite/docs/modernization/evidence/effect-end-contract.json)
and [local verification](../deploy/README.md#native-effect-cancellation).
`verify_posture_upgrade.py --through-cancellation` verifies additive 032→033→034
preservation; actual owner, gameplay and wire tests are separate. Full generic
effects, native combat and actual-client acceptance remain open.
