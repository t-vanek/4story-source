# Linux containers

The portable Asio cluster is the Linux deployment path. The original
Win32 servers and client remain reference sources. Each container runs
one daemon as UID/GID 10001, logs to stdout, and receives SIGTERM directly.

## Current native PostgreSQL verification contract

The current native Login/Map path requires migrations **001–034**, current
`sql/login-runtime-grants.sql` and `sql/map-runtime-grants.sql`, plus explicitly
activated character/routing/four-table actor catalogs. The Map role now has bounded gameplay
writes for consumption and inventory moves/splits/merges, including item INSERT
and the shared world ID counter. Earlier sections below describe their original
increment and do not override this requirement. Applied migrations are immutable;
the next schema change starts at 035. Existing player graph catalog bindings need
an explicit compatibility migration before switching a populated world; see
[native character statistics](#native-map-character-statistics).

Reproduce local native integration against the existing private, backup-derived
manifests with an isolated lab (paths below are placeholders for those manifests):

```sh
python3 tools/database/disposable_environment.py start --postgresql-only --work /tmp/fourstory-native-lab
python3 tools/database/run_native_verification.py --work /tmp/fourstory-native-lab \
  --map-runtime-only --build-dir build/linux-debug \
  --snapshot /private/characters/manifest.json \
  --routing-snapshot /private/routing/manifest.json \
  --actor-snapshot /private/actor/manifest.json \
  --report /tmp/fourstory-native-verification.json
python3 tools/database/disposable_environment.py stop --work /tmp/fourstory-native-lab
```

Use the documented Python dependencies/virtual environment and build-deps runtime
for this harness. Its default runtime image is the local build-deps image. For
installed Release verification, add `--image localhost/fourstory:postgresql-inventory-stacks
--runtime-bin-dir /opt/fourstory/bin`. Backend/pool test executables still come from
the verified `--build-dir`. See [current results](../_rewrite/docs/modernization/README.md).
This starts and tests native processes with synthetic owned accounts; it does not
claim a complete playable deployment. The owner has only uncompiled client source
and no matching client data. [Original-client acceptance](../_rewrite/docs/modernization/client-acceptance.md)
remains blocked. Keep all builds, commits and containers local; no GitHub push or
external deployment is authorized before complete gameplay.

## Run the development cluster

From the repository root:

```sh
docker compose up --build -d
docker compose ps
docker compose logs -f
docker compose down
```

This uses the committed `deploy/config.example/` configurations with
in-memory services and no database. It verifies the process topology,
health endpoints and the Map → World connection. It is a transport smoke
environment; gameplay and persistence are still incomplete. Map RC4 is
disabled in this environment.
The Login control-packet gate is pinned to container loopback for smoke
mode; configure the actual Control server IPv4 address for a real cluster.

Login (4816), Patch (3715) and Map (5815) are published on host loopback.
World, Control and the UDP log collector are reachable on the private
Compose network. Health endpoints stay inside the network. The health
response proves that a daemon is running; it does not prove that a client
can complete login or that all dependencies are available.

Inside a container use Compose DNS names (`world`, `log`, `control`, etc.).
An IP advertised to the game client must be reachable **from the client**;
a container DNS name is not a client-facing address. For remote clients,
configure host port bindings and the advertised addresses together.

## Connect an existing SQL Server

```sh
cp -r deploy/config.example deploy/config
FOURSTORY_CONFIG_DIR=./deploy/config docker compose up -d
```

Edit the copies before starting with a database. They are Git-ignored and
mounted read-only. Use the full component examples in `Server/T*SvrAsio/`
to configure the required global/world databases, server inventory,
security and client crypto. TOML strings do not expand environment variables.
The image contains Microsoft ODBC Driver 18, installed using the
[Microsoft Linux installation instructions](https://learn.microsoft.com/en-us/sql/connect/odbc/linux-mac/installing-the-microsoft-odbc-driver-for-sql-server).
Building the image accepts that driver's EULA.

For example, in the appropriate `[database]` section:

```toml
backend = "odbc"
connection_string = "DRIVER={ODBC Driver 18 for SQL Server};SERVER=sql.example.net,1433;DATABASE=TGLOBAL_RAGEZONE;UID=fourstory;PWD=replace-me;Encrypt=yes;TrustServerCertificate=no"
pool_size = 4
worker_threads = 2
```

SQL password authentication works from Linux; the old Windows
`Trusted_Connection=yes` examples require a separate Kerberos setup.
Provide the SQL Server CA certificate to the container if it uses a
private certificate authority. Driver 17 connection strings must be
updated to Driver 18. The server's existing schema validators still
require the original databases, stored procedures and project migrations.
This deployment does not provision or restore a database.

PostgreSQL is the selected target. Shared `SessionPool` now compiles the native
SOCI/libpq backend by default on Linux. The Map daemon can load its actual static
catalogs through a separate `[content]` connection; see
[tmapsvr-postgresql.example.toml](tmapsvr-postgresql.example.toml) and
[database configuration](../database/README.md). Mount the CA certificate and
supply `FOURSTORY_CONTENT_CONNECTION` through a private environment file.
The runtime image includes the SOCI PostgreSQL library.

Native Login authentication/session transactions are implemented; complete
character/item/world repositories still require porting;
changing their `backend` setting does not translate T-SQL or schema validators.
See [current implementation status](../_rewrite/docs/modernization/README.md).
The ODBC package remains for existing comparison paths while those migrations
are implemented. Native catalog startup and its tests use no SQL Server service.

## Build and verify

```sh
docker build -t fourstory:linux .
python3 tools/container_smoke.py --image fourstory:linux
docker build --target test --build-arg TEST_JOBS=2 -t fourstory:tests .
```

The smoke script checks all six health endpoints, container DNS for the
Map → World connection, the ODBC driver, OpenSSL RC4 availability, and
clean exit after SIGTERM. It removes the containers/network it creates.
The `test` image target runs existing CTest suites in Debug, with assertions
enabled. DB integration checks without a configured database skip their
DB portion; this does not validate SQL Server connectivity.

Docker-compatible builds also work with rootless Podman:

```sh
podman build -t localhost/fourstory:linux .
python3 tools/container_smoke.py --engine podman --image localhost/fourstory:linux
podman build --target test --build-arg TEST_JOBS=2 -t localhost/fourstory:tests .
```

Use `podman compose` if a Compose provider is installed. The `:z` volume
option permits shared configuration mounts on SELinux hosts.

For a native Ubuntu 24.04 build:

```sh
sudo apt-get install build-essential cmake ninja-build pkg-config \
  libboost-system-dev libssl-dev openssl libspdlog-dev \
  libtomlplusplus-dev libsoci-dev unixodbc-dev libpq-dev
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
```

`linux-release` builds only the servers and bcrypt migration tool.
`BUILD_TESTING=OFF` omits all test targets. Legacy `tnetlib` defaults off
on Linux and on for Windows; `tnetlib_portable` is used by every modern
server. `cmake --install` installs the six daemons and migration tool.
Image build concurrency defaults to two workers; override `BUILD_JOBS`
to suit available memory. Images use Ubuntu packages rather than vcpkg.
The `Linux container` GitHub Actions workflow runs the container test
target and smoke checks on changes to the portable server sources.
The base tag and package repositories receive updates; pin a base digest
and package versions when preparing a reproducible release.


The `native-postgresql` CI job builds the shared driver test and runs it against
an isolated PostgreSQL instance with TLS, least-privilege synthetic fixtures,
rollback/concurrent-write and disconnect checks. It does not need backup files.
Real-catalog verification is a separate local run using the pinned private extract.
On Windows, PostgreSQL remains opt-in: select the vcpkg `postgresql` feature and
`FOURSTORY_ENABLE_POSTGRESQL=ON`. That Windows combination has not been tested here.

Native Login now has a separate [example](tloginsvr-postgresql.example.toml) and
[application role grants](sql/login-runtime-grants.sql). Apply migrations through 010,
configure a dedicated TLS PostgreSQL role with `app_global,pg_catalog` search path,
and supply `FOURSTORY_LOGIN_CONNECTION` without writing credentials to the TOML.
One active Login process per application DB is enforced with a dedicated
PostgreSQL advisory-lock connection and transaction ownership tokens. A second
process refuses startup before cleanup; after connection loss, a replacement
claims ownership and fences old writes. Budget one extra connection beyond the
auth pool. The lock requires a direct or session-pooled connection, not transaction
pooling. Owner loss exits with status 1 so the container supervisor can restart it. No accounts or
credentials are imported automatically; character/world persistence remains in
progress. The `--login-only` verification mode exercises synthetic real-PG login,
encrypted protocol, rollback, concurrency, owner loss/replacement, pipelined
requests, disconnect and shutdown during in-flight authentication. CI also runs
this mode; a local pass does not claim a remote Actions run.

Login without database configuration requires explicit
`[development] allow_no_database=true`; the checked-in smoke config sets it.
Production PostgreSQL examples do not. Historical data and migrations 001–010
remain immutable; later adaptations need new migration numbers.

### Login and transport sanitizers

```sh
cmake --preset linux-asan
cmake --build --preset linux-asan
ctest --preset linux-asan
python3 tools/database/run_native_verification.py --work /tmp/fourstory-native-lab --login-only --build-dir build/linux-asan
```

Use the build-deps image or the same Ubuntu development packages and an owned
PostgreSQL-only lab. The preset instruments Login, its shared libraries and the
selected transport/shutdown/native-DB tests with ASan and UBSan; it is not a full
six-daemon sanitizer run. `vptr` checks are excluded because the packaged SOCI
library does not export the required RTTI symbols. Leak detection and all other
selected checks halt on error. CI uses both Debug and this sanitizer configuration.


### Source-client email verification

Migration 010 adds operational connection challenges and per-account issuance
budgets. Enable only for deliberately provisioned `TUSEREMAIL` accounts and a
configured trusted local/private SMTP relay. No historical account/email import
is implied. The non-direct source `0x2918` profile confirms a code, receives only
`SECURITYRESULT`, then sends LOGIN again with the form password. Only a verified
private connection grant selects the SHA1 adapter for that retry; ordinary LOGIN
still requires the bcrypt-covered wire credential. Direct-login/JP/TW and other
profiles are refused for this feature until their behavior is independently proven.

Policy: six ASCII alphanumeric characters, five attempts, five-minute lifetime,
three issued challenges per account per five minutes (including cancelled mail),
and a thirty-second verified retry grant. Confirmation creates no session and
adds no trusted IP. LOGIN rechecks credentials, bans, agreement and duplicate state.
The SMTP transaction runs on a worker with a fifteen-second deadline and retains
coalesced multiline replies. Its plaintext hop requires a trusted relay; provider
TLS belongs on that relay. No email code, body or recipient is logged by SMTP.

`--login-only` now starts an owned Node-based SMTP sink on the disposable network,
using `docker.io/library/node:24-alpine`; it sends no external email. It verifies
source packet sequence, challenge isolation, limits, expiry, failure cleanup and
responsive health during a deliberately silent relay. The sink and captured
messages are removed in the harness cleanup. Default login throttling stays on:
after its burst, it replenishes one attempt per ten seconds, so this test takes
several minutes. Do not interpret the synthetic peer as an original-client run.

## Native PostgreSQL character lobby

Apply migrations 001–018 and both `sql/login-runtime-grants.sql` and
`sql/character-runtime-grants.sql` for the dedicated Login role. Extract the
immutable reference tables with `extract_reference.py --profile characters`,
import with `migrate.py`, and explicitly activate with
`activate_character_catalog.py --manifest /private/reference/manifest.json`.
The existing Map catalog selection remains independent. Configure the returned
manifest under `[characters].manifest_sha256` in the Login TOML.

Provision normal `app_global.TGROUP` rows and `app_world.worlds` as the schema
owner. Match each client group to its reviewed source `item_world` and high-water;
these are distinct identifiers. The recovered source has item world 0 and
high-water 735812, which migration 013 enforces as the minimum for that lineage.
Other item worlds need operator-reviewed provenance; signed-positive worlds
128–255 are not supported yet. No historical accounts, reservations or populated
player records are seeded by these migrations. Credentials remain in the existing
secret environment configuration, and the daemon uses native libpq/SOCI with TLS.

Verification: `run_native_verification.py --characters-only --work /private/lab
--snapshot /private/reference/manifest.json` runs real PG and encrypted Login TCP
checks with clearly synthetic accounts. It verifies process-restart lobby
persistence. The native Map section below covers fresh loading/core save/reconnect;
full world gameplay and original-client execution remain unfinished acceptance gates.

## Native PostgreSQL Login routing

Apply all numbered migrations through 018 and reapply the Login grants (SELECT
on pending handoffs and Map claims is required even for auth-only operation). Then
apply `sql/routing-runtime-grants.sql`
after the Login and character grants. Extract with `extract_reference.py --profile
routing` from the owned restored backup, import the private manifest with the
existing migration tool, and publish with `activate_routing_catalog.py --manifest
/private/routing/manifest.json`. The four physical tables are TMAPCHART,
TUNITCHART, TCHANNELCHART and TSPAWNPOSCHART; TSVRCHART is their original joined
view, not an invented historical table. Activation validates every value and
byte checkpoint and refuses ambiguous cell ownership. Source gaps remain intact.

Configure the returned hash as `[routing].manifest_sha256`. Provision
`app_world.routing_worlds(group_id, source_group)` explicitly; client world IDs
and backed-up chart group IDs need not be equal. Reviewed operational endpoints
still come from app_global TSERVER/TMACHINE/TIPADDR, with active logical channels
in TCHANNEL. These rows describe the deployment; historical server addresses are
not imported. A missing manifest disables native START; an incorrect configured
manifest refuses startup. No first-server fallback exists in the native path.

Run `run_native_verification.py --routing-only --work /private/lab --snapshot
/private/characters/manifest.json --routing-snapshot /private/routing/manifest.json`
for the actual native C++ transaction and encrypted Login TCP tests. Add
`--build-dir build/linux-asan` for the instrumented binaries. The native Map section
below consumes the 60-second pending reservation into a fenced active claim.
Source-client execution and full gameplay persistence remain unfinished.
Do not use the existing legacy Map repositories against app_world.

## Native PostgreSQL Map fresh-character runtime

Apply migrations through **019** and reapply Login and Map grants (Login now reads
`app_world.map_sessions` even in auth-only deployments). Provision a separate Map
LOGIN role and apply `sql/map-runtime-grants.sql`; it cannot rewrite inventory,
read password hashes or access historical schemas. Supply its libpq connection
through `[database].connection_string_env` with TLS verification as for Login.

Extract the actor profile using `extract_reference.py --profile actor`, import its
private four-table manifest, and publish explicitly with `activate_actor_catalog.py`.
The Map `[native_map]` section requires `character_manifest_sha256`,
`routing_manifest_sha256` and `actor_manifest_sha256`. Configure `[cluster]`
`group_id`/`server_id` consistently with the provisioned world and endpoints, enable
DB workers, and configure the real `[world]` peer. Only normal PvE mode is currently
accepted. `[content]` remains an independent read-only catalog connection.
Start from [tmapsvr-native.example.toml](tmapsvr-native.example.toml), replacing
the explicitly pinned hashes and operational endpoints with the reviewed deployment.

Use `run_native_verification.py --map-runtime-only --work /private/lab --snapshot
/private/characters/manifest.json --routing-snapshot /private/routing/manifest.json
--actor-snapshot /private/actor/manifest.json` for native transactions and the actual
four-daemon TCP flow (Login, World and two Maps); add `--build-dir build/linux-asan` for sanitizers.
To verify installed Release daemons, add
`--image localhost/fourstory:postgresql-native-primary --runtime-bin-dir /opt/fourstory/bin`.
The integration executables still come from `--build-dir`; `--runtime-bin-dir`
selects the installed Login/World/Map binaries and requires `--map-runtime-only`.
SQL Server is used only to extract authoritative reference snapshots, never by
these daemons.

Fresh characters with guild membership or active recalls are refused pending
those implementations. Fresh neighboring-Map admission uses migration 019
one-use grants tied to the primary and target process/connection identities.
Only the primary loads the supported character graph and persists core state; the replica consumes
the original World summary. Dynamic border routing and primary transfer remain
unimplemented. After owner loss, ready
sessions with a matching contract-1 checkpoint recover to their last committed
core, releasing the prior login. Changes after that checkpoint can be lost on a
crash. Old/no-receipt, already orphaned or drifted claims remain blocked. Pre-ready
claims are recoverable.
Do not clear these reservations manually as a normal restart procedure. The
installed `localhost/fourstory:postgresql-world-handoff` image includes this increment
and passes native four-daemon verification and all six daemon smoke checks as
UID/GID 10001:10001; see
[Release evidence](../_rewrite/docs/modernization/evidence/world-handoff-container-verification.json).
The older `postgresql-characters` image does not include this increment.


Migration **018** and the current Map grants are required for native checkpoints.
The default `[native_map].checkpoint_interval_ms=1800000` follows the original
30-minute save tick; configure 100..1800000 ms for the deployment's recovery target.
This is periodic core persistence, not a durable inventory/economy journal. The
worker sweep queues one checkpoint at a time, and shutdown waits for pending
checkpoint/final-save completions. `app_world.map_checkpoints` retains one latest
receipt per character, with revision, last committed time, outcome and observed
recovery time. Do not invent/backfill receipts for older claims. An orphan requires
an explicit reviewed recovery decision; it must not be blindly deleted. Database
logs and process exit status distinguish recovery from an ordinary successful save.


Native Map accepts clients only after its configured World confirms registration.
It closes client sockets on detected World loss, saves/releases them using the
existing native adapter, then reconnects with fresh registration. The five-second
registration deadline rejects silent or incompatible peers. `/healthz` remains a
process liveness check; it does not certify World readiness. Failed saves retain
account reservations and are logged as failures; inspect these before recovery.
This lifecycle requires no migration beyond 018. TCP blackhole detection and a
full World persistence backend remain outstanding.


World now bounds each peer's outstanding sends to 256 frames / 4 MiB (headers
included); excess backlog closes that connection. Its expected-secondary admission
uses the source success flow, with account/peer checks and preservation of a valid
primary on secondary rejection. The installed `postgresql-world-handoff` image
contains these changes. The native runtime verification adds independently encoded
synthetic World peers, including exact rank/tournament registration replays. These
are World protocol checks. Successful native replicas are separately exercised
with a deliberately synthetic same-channel cell partition; the harness restores
the original compatibility view and leaves historical rows untouched. Primary
transfer and complete distributed gameplay remain pending.

This image also prevents an ungranted second Map claim from closing the first
Map's valid character. Map distinguishes unannounced/announced/retired identities,
handles World retirement without close-all echo, and still saves a ready native
primary before release. World close-all requires an accepted connection on the
registered Map peer. The four-daemon verification exercises rejection on an actual
second Map, alongside successful granted replica admission and both disconnect
directions. It does not establish primary transfer or original-client execution.
See the [retirement contract](../_rewrite/docs/modernization/evidence/map-retirement-contract.json).

Native primary CHARINFO now precedes final CONNECT, matching the source client
map-loading order. CONREADY never duplicates native CHARINFO. See the
[replica contract](../_rewrite/docs/modernization/evidence/map-replica-contract.json)
for the reproduced ordering regression, source trace and current limits.


The current image also checks World primary handoff responses against exact
registered source/target connections and release/load/confirmation phases.
Incomplete handoffs close after a total five-second coordination deadline. This
is a prerequisite for native primary transfer; it does not enable PostgreSQL
ownership transfer or a complete Map transient-state codec. Client opcodes and
source transfer bytes are unchanged. See the
[handoff contract](../_rewrite/docs/modernization/evidence/world-handoff-contract.json).

Plain TCP sessions serialize socket/channel work on a per-session strand and
cancel blocked senders on shutdown. Native Login/Map liveness checks use the
session's atomic state. This does not change the optional TLS transport or make
all shared game state safe for multiple reactor threads. The current sanitizer
preset covers 29 entries; a separate native PostgreSQL run verifies actual daemons.


The subsequent installed `localhost/fourstory:postgresql-main-transfer` image
preserves raw native item fields and restores loaded skill cooldowns into live Map
admission/use/teardown. It also includes the complete source transfer codec, whose
all-section fixture is independent of the C++ layout. This image does not enable
native primary ownership transfer: embedded ENTERSVR remains refused until the
fenced PostgreSQL transaction and full live-state restoration are implemented.
Migrations 001–019 are unchanged. See the
[state contract](../_rewrite/docs/modernization/evidence/main-transfer-state-contract.json)
and [installed verification](../_rewrite/docs/modernization/evidence/main-transfer-container-verification.json).


Image verified for this earlier increment: `localhost/fourstory:postgresql-native-primary`.
Apply migration 020 and the current `deploy/sql/map-runtime-grants.sql` to the
operational PostgreSQL database before using its native Map path. Do not edit
applied 001–019 or the authoritative backups. The new image integrates native
primary handoff, authority fencing, complete graph checkpoints and source/target
failure recovery using the original client protocol. Its exact tested scope and
limits are in the [native primary contract](../_rewrite/docs/modernization/evidence/native-primary-transfer-contract.json)
and [installed verification](../_rewrite/docs/modernization/evidence/native-primary-transfer-container-verification.json).

Native verification pins the requested image tag to one immutable image ID.
Each disposable lab has one shared PostgreSQL TLS identity, so the harness now
rejects simultaneous runs against the same work directory before modifying TLS.
Use separate owned labs for parallel runs. The tested original routing catalog
separates channels; two-Map boundary tests explicitly override and restore a
compatibility view in a disposable database. They do not change historical rows
or establish real-client/gameplay compatibility.

## Native PostgreSQL audit ingest and queries

Apply numbered migrations **001–029** through the checksummed runner as the schema
owner. Provision a dedicated `LOGIN NOSUPERUSER NOCREATEDB NOCREATEROLE NOINHERIT`
role with CONNECT on the chosen database, then run:

```bash
psql -v ON_ERROR_STOP=1 -v log_role=fourstory_log -f deploy/sql/log-runtime-grants.sql
```

Connection credentials belong in the existing libpq environment/secret setup,
not shell history or the repository. The script grants only audit SELECT, INSERT
on original event columns, schema USAGE and sequence USAGE. No UPDATE, DELETE,
TRUNCATE or DDL rights are granted. `lt_id` and `received_at` are allocated by the
server. Native code always addresses `app_audit`, independent of search_path.

Use [tlogsvr-postgresql.example.toml](tlogsvr-postgresql.example.toml), supply the
`FOURSTORY_LOG_DSN` secret and trusted CA to the local container, and start:

```bash
tlogsvr_asio --config deploy/tlogsvr-postgresql.example.toml
```

Set `sslmode=verify-full` in the DSN; this is also SessionPool's default. Adjust the
UDP peer allowlist for the actual local cluster. A missing DSN variable, conflicting
connection sources, wrong native schema, missing write/sequence grants or failed
native startup read causes failure before the UDP listener starts. The existing
stdout mode remains an explicitly unconfigured development path.

The original CHAR fields are raw bytea, preserving all bytes without code-page
guessing. Native audit queries return those original bytes; startup prints their
hex representation. LP_LOG storage/read is verified, while LP_CHAT, retention,
durable spool/reconciliation, automatic closed-pool replacement and complete
original tool/client acceptance remain unfinished. Unknown outcomes are counted
separately and never replayed. RAM queues are not durable across process failure.

### Reproduce native audit verification

After building `linux-debug` or `linux-asan` using the documented build-deps image,
activate the Python environment containing psycopg and run:

```bash
python3 tools/database/disposable_environment.py start \
  --work /tmp/fourstory-log-check --postgresql-only
python3 tools/database/verify_log_outcomes.py \
  --work /tmp/fourstory-log-check --build-dir build/linux-debug \
  --report /tmp/fourstory-log-check-result.json
python3 tools/database/disposable_environment.py stop \
  --work /tmp/fourstory-log-check
```

This runner now uses the actual 029 migration and runtime grants. It creates only
owned test data/roles, checks upgrade from 028 and preserved receipts, tests native
queries/privileges, injects lost COMMIT replies, exercises actual UDP binary/raw
text persistence, and tests health during blocked retry plus graceful/SIGKILL
process replacement. It removes its database, role and temporary connection files;
the owned lab's PostgreSQL environment file remains private until cleanup. Fault
injection deliberately disables TLS on loopback; separate pool tests cover TLS.

For installed Release add `--image localhost/fourstory:postgresql-native-audit
--runtime-bin-dir /opt/fourstory/bin`. The backend test still comes from
`--build-dir`; the UDP tests use the installed Release daemon as UID/GID 10001.
The runner maps the private configuration owner to that UID without loosening its
file permissions. This image stays local; no publishing or deployment is required.


### Native Map character statistics

Use the local image `localhost/fourstory:postgresql-character-statistics`, or build
with `podman build --target runtime --build-arg BUILD_JOBS=2 -t localhost/fourstory:postgresql-character-statistics .`.
Follow the native Login/Map provisioning steps above, then:

1. Apply numbered migrations **001–030** through `tools/database/migrate.py apply`.
2. Extract a fresh actor profile from the owned restored backup container using
   `extract_reference.py --container <owned-mssql> --profile actor --output /private/actor-v2`.
   It now includes item magic, skill points, item attributes and item grades.
3. Import its private manifest with `migrate.py import-reference --manifest
   /private/actor-v2/manifest.json`, then publish with
   `activate_actor_catalog.py --manifest /private/actor-v2/manifest.json`.
4. Reapply `sql/map-runtime-grants.sql` as the schema owner so the dedicated Map
   role can read the new actor views. Supply the resulting actor manifest hash in
   `[native_map]` (the verified backup export is
   `bc17bc975c398ae2c05c2cc92e029df807be3c1fdc94b9cc43fe3659a280b878`).
5. Start the existing Login/World/Map daemons with the previously documented native
   runtime roles/DSN environment variables and matching endpoints. An incomplete
   old actor release fails native Map startup instead of producing guessed stats.

Run `run_native_verification.py --map-runtime-only` as above using the new actor
manifest. The suite now includes `verify_character_statistics_wire.py` and retains
inventory, skill, ownership, DB failure and process-recovery scenarios. Use
`verify_actor_upgrade.py --work <owned-lab> --previous-manifest <old-two-table-json>
--manifest <new-four-table-json> --report <report.json>` for repeatable catalog
upgrade/preservation checks.

For existing two-table player graph/checkpoint bindings, use the offline procedure
below. Other catalog changes remain blocked until separately proven. Actual client
acceptance remains blocked because no original executable/assets are available.


### Offline actor-catalog transition

Previous increment image: `localhost/fourstory:postgresql-actor-transition` (build with
`podman build --target runtime --build-arg BUILD_JOBS=2 -t localhost/fourstory:postgresql-actor-transition .`).
The supported transition is exactly the old two-table actor release to the
four-table statistics release, with identical original item-magic/skill-point rows.
This procedure adds no external deployment or publication.

1. Stop the local Map processes and complete their shutdown. For a previously
   crashed process, retain all claims, checkpoints and prepared transfer receipts;
   normal new-process recovery will inspect them. Keep client admission stopped
   until both catalog publication and Map recovery have completed.
2. With the schema-owner connection, apply migrations through **031**. Preserve all
   original `.bak` files, imports and existing migration receipts. Import the complete
   four-table manifest using `migrate.py import-reference`; keep the original
   two-table manifest and its verified import available.
3. Run `activate_actor_catalog.py --manifest /private/actor-v2/manifest.json
   --previous-manifest /private/actor-v1/manifest.json --report /private/actor-upgrade.json`.
   The tool verifies both imports and original bytes, certifies identical shared
   charts, switches the selector and retires inactive owner tokens atomically.
   Live owners, active worker transactions, changed shared data or uncertified graph
   receipts prevent publication. Resolve the reported cause before another attempt.
4. Reapply `deploy/sql/map-runtime-grants.sql` as the schema owner for the dedicated
   Map role. It needs SELECT on the new `actor_compat` view, no access to certificate
   or owner-retirement audit writes. Set every Map's `actor_manifest_sha256` to the
   published four-table hash. Character and routing hashes stay independently pinned.
5. Start the current local Map/World/Login processes. Replacement owners verify and
   recover old claims; fresh ready checkpoints use the new actor hash. Publication
   itself never edits player receipt bodies, hashes, keys or original owner tokens.

If the COMMIT response is lost, inspect `actor_catalog`, `actor_catalog_activations`,
`actor_graph_compatibility` and `actor_owner_retirements` on a new operator connection
before deciding the outcome. The publisher never automatically retries. An exact
verified replay is a no-op; this does not make unknown writes safe to retry blindly.

Reproduce migration/proof checks with `verify_actor_upgrade.py` as above. For real
old-binary graph recovery, add `--actor-upgrade-from /private/actor-v1/manifest.json
--previous-image <local-pre-statistics-image-id>` to the native Map verification
command. This runs three separate lifecycles: logout, target SIGKILL and interrupted
source prepare. Use `--build-dir build/linux-asan` for the sanitizers, or
`--image localhost/fourstory:postgresql-actor-transition --runtime-bin-dir
/opt/fourstory/bin` for installed Release daemons (backend integration binary remains
Debug). Run the general native matrix separately without the upgrade flags.
See [transition evidence](../_rewrite/docs/modernization/evidence/actor-transition-contract.json).


## Native equipment transactions

Image verified for the equipment increment: `localhost/fourstory:postgresql-equipment`. Its reproduction:

```sh
podman build --layers --target runtime --build-arg BUILD_JOBS=2 -t localhost/fourstory:postgresql-equipment .
```

Apply migrations **001–032** with the schema owner and reapply
`deploy/sql/map-runtime-grants.sql` for the configured Map role. Migration 032
adds two empty runtime receipt tables; it does not rewrite historical data,
existing items, actor releases or recovery packets. Map receives INSERT and
SELECT on those tables, with no UPDATE/DELETE grant. Do not alter migrations
001–032 after application. Next schema change: 033.

Use the existing native Login/World/Map configuration, owner/session fencing and
verified character/routing/four-table actor manifests described above. An existing
old two-table world must first follow the certified offline transition; merely
switching a hash does not authorize old-graph recovery. Start with the local
Compose/Podman procedure above; no external deployment/publication is authorized.

Reproduce the native equipment and full Map regression against a disposable lab:

```sh
python3 tools/database/disposable_environment.py start --work /tmp/fourstory-equipment-test --postgresql-only
python3 tools/database/run_native_verification.py \
  --work /tmp/fourstory-equipment-test --map-runtime-only --build-dir build/linux-debug \
  --snapshot /private/character/manifest.json \
  --routing-snapshot /private/routing/manifest.json \
  --actor-snapshot /private/four-table-actor/manifest.json \
  --report /tmp/native-equipment-debug.json
```

Use the Python environment with psycopg from the database reconstruction setup;
manifest paths must refer to verified exports of the immutable backups. Replace
`--build-dir` with `build/linux-asan` for sanitizer daemons/backend, or add
`--image localhost/fourstory:postgresql-equipment --runtime-bin-dir /opt/fourstory/bin`
for installed Release daemons with the Debug backend integration binary. Execute
CTest Debug and sanitizer presets separately. Stop the owned lab using
`disposable_environment.py stop --work /tmp/fourstory-equipment-test`, then remove
its private credentials/keys. Runtime authentication pacing is intentional.

The suite covers normal equipment items/stat packets, graph authority after two
Map transfers, item/core/timer atomicity, HP/MP clamping, concurrent writes,
rollback, reconnect and the existing process/DB fault matrix. It does not prove
original-client acceptance. No supported executable build/hash or assets can be
listed yet because only uncompiled client sources are available. Finish warrior
postures, active effects and the remaining equipment/gameplay dependencies before
claiming a complete server. See [equipment contract](../_rewrite/docs/modernization/evidence/equipment-contract.json).


## Native maintained postures

The recorded posture increment required schema **001–033**. The current requirement
is in [native effect cancellation](#native-effect-cancellation). Stop Map owners
before upgrading, apply migrations with the schema owner, and reapply
`sql/map-runtime-grants.sql`. Start the services using the existing native catalog,
TLS and Compose procedure above. All running Maps must use the current image.

```sh
podman build --layers --target runtime --build-arg BUILD_JOBS=2 -t localhost/fourstory:postgresql-postures .
python3 tools/database/disposable_environment.py start --work /tmp/fourstory-postures-test --postgresql-only
python3 tools/database/run_native_verification.py \
  --work /tmp/fourstory-postures-test --map-runtime-only --build-dir build/linux-debug \
  --snapshot /private/character/manifest.json \
  --routing-snapshot /private/routing/manifest.json \
  --actor-snapshot /private/four-table-actor/manifest.json \
  --report /tmp/native-postures-debug.json
```

Use the reconstruction Python environment with psycopg and the verified private
manifest exports. Run the sanitizer preset with `--build-dir build/linux-asan`.
For installed Release add `--image localhost/fourstory:postgresql-postures` and
`--runtime-bin-dir /opt/fourstory/bin`; its backend integration binary remains the
mounted Debug test. Run Debug and ASan CTest separately. The wire suite respects
production Login rate limits, so its authentication pacing is intentional.

The posture suite creates synthetic characters/items from backup templates, tests
native/graph persistence, competing equips, late rollback, drift and reconnect,
then exercises original packet order through actual encrypted TCP. It kills the
Map process with an active posture and disconnects during a delayed cancellation
commit. General effect expiry/cancellation, combat and original-client acceptance
remain unfinished; only client source is available, not a verified EXE or assets.
See [posture contract and evidence](../_rewrite/docs/modernization/evidence/postures-contract.json).

Stop the owned lab:

```sh
python3 tools/database/disposable_environment.py stop --work /tmp/fourstory-postures-test
```

Then remove its private credentials and keys.
All work, images and commits remain local; no push or external deployment.

The bounded schema upgrade can be checked independently while that lab is running:

```sh
python3 tools/database/verify_posture_upgrade.py --work /tmp/fourstory-postures-test --report /tmp/postures-upgrade.json
```

It creates/removes its own synthetic database, upgrades untouched 001–032 receipts
through 033, and verifies all eight maintained-field drift guards. This structural
check does not substitute for the native process and original-client tests.

## Native effect cancellation

Current schema: **001–034**, next migration **035**. Stop Map owners, apply all
pending migrations through `tools/database/migrate.py`, then reapply
`deploy/sql/map-runtime-grants.sql` for the existing Map role. Migration 034 adds
only the effect operation ledger and its identity allocator; earlier migration
files, checkpoint fields and backup files remain unchanged. The role receives
INSERT/SELECT on the ledger and USAGE on its sequence, with no UPDATE/DELETE.

Use the same pinned character, routing and four-table actor manifests as the
posture increment. Build and verify locally:

```sh
podman build --layers --target runtime --build-arg BUILD_JOBS=2 -t localhost/fourstory:postgresql-effect-end .
python3 tools/database/disposable_environment.py start --work /tmp/fourstory-effect-end-test --postgresql-only
python3 tools/database/run_native_verification.py \
  --work /tmp/fourstory-effect-end-test --map-runtime-only --build-dir build/linux-debug \
  --image localhost/fourstory:postgresql-effect-end --runtime-bin-dir /opt/fourstory/bin \
  --snapshot /private/character/reference/manifest.json \
  --routing-snapshot /private/routing/reference/manifest.json \
  --actor-snapshot /private/statistics/actor-reference/manifest.json \
  --report /tmp/native-effect-end-release.json
python3 tools/database/verify_posture_upgrade.py \
  --work /tmp/fourstory-effect-end-test --through-cancellation \
  --report /tmp/effect-end-upgrade.json
python3 tools/container_smoke.py --engine podman --image localhost/fourstory:postgresql-effect-end
python3 tools/database/disposable_environment.py stop --work /tmp/fourstory-effect-end-test
```

For Debug use the default build-deps image and omit `--runtime-bin-dir`; for
ASan/UBSan also use `--build-dir build/linux-asan`. Compile the corresponding preset
first. The installed Release test still mounts the Debug backend integration test;
its actual Login/World/Map daemons come from `/opt/fourstory/bin`. Use separate labs
for concurrent configurations, because TLS files belong to the PostgreSQL process.
See [the contract and recorded evidence](../_rewrite/docs/modernization/evidence/effect-end-contract.json).

The native wire suite uses original-source packet oracles, a labelled synthetic
routing partition and backup-derived catalogs. It covers a live posture transfer,
replica ACK, successor cancellation, return, malformed requests, late rollback,
disconnect during commit and SIGKILL recovery. No original executable/assets are
available. General combat/effects and full gameplay remain unfinished; this image
is for local verification and has not been published or deployed externally.


## Native cast attack profiles

The preceding local image is `localhost/fourstory:postgresql-cast-powers`. Use the
same native schema **001–034**, runtime grants and pinned character/routing/four-table
actor catalogs described above. This increment requires no new migration. It
populates source-derived outgoing cast fields; complete combat and actual-client
acceptance remain pending. Do not use it as a completed production server.

```sh
podman run --rm --network none --userns keep-id -v "$PWD:/src:z" -w /src \
  localhost/fourstory:build-deps cmake --build --preset linux-debug
podman build --layers --target runtime --build-arg BUILD_JOBS=2 \
  -t localhost/fourstory:postgresql-cast-powers .
python3 tools/database/disposable_environment.py start \
  --work /tmp/fourstory-cast-powers-test --postgresql-only
python3 tools/database/run_native_verification.py \
  --work /tmp/fourstory-cast-powers-test --map-runtime-only --build-dir build/linux-debug \
  --image localhost/fourstory:postgresql-cast-powers --runtime-bin-dir /opt/fourstory/bin \
  --snapshot /private/character/reference/manifest.json \
  --routing-snapshot /private/routing/reference/manifest.json \
  --actor-snapshot /private/statistics/actor-reference/manifest.json \
  --report /tmp/native-cast-powers-release.json
python3 tools/container_smoke.py --engine podman --image localhost/fourstory:postgresql-cast-powers
python3 tools/database/disposable_environment.py stop --work /tmp/fourstory-cast-powers-test
```

Use a Python environment with the documented psycopg dependency. Remove the
owned lab's private credentials and TLS files after stopping it. The runtime
harness starts the actual local Login/World/Map processes and runs the original
packet oracles against PostgreSQL, then tears the daemons down. For Debug omit
`--image`/`--runtime-bin-dir`; for ASan also build/use `linux-asan`. Keep concurrent
runs in separate labs. Installed Release uses its own daemons and the mounted
Debug backend integration executable. See [recorded results and limitations](../_rewrite/docs/modernization/evidence/cast-powers-contract.json).

Optional offline source-order reproduction needs a separate lab with restored
backup copies, using the existing `inspect_backup.py --restore-disposable` workflow:

```sh
python3 tools/database/verify_skill_source_order.py \
  --work /private/owned-restored-source-lab \
  --snapshot /private/character/reference \
  --report /tmp/cast-powers-source-order.json
```

SQL Server is used only for that historical query observation. Native server
execution and the cast-profile wire tests use PostgreSQL. The explicit ascending
source-key ordering and its historical-query uncertainty are documented in the
protocol contract. All images, tests and commits stay local; nothing is published.

## Native accepted cast transactions

Migration **035** is required for the current accepted-cast path. Stop local Map
writers, apply migrations through 035 with the existing migration runner, then
reapply `deploy/sql/map-runtime-grants.sql` to the configured Map role before
starting the new binaries. Keep both backups and previously applied migration
files unchanged. Existing checkpoints and historical item receipts are preserved;
old receipts retain a NULL accepted-cast reference. No historical cast is invented.
The character/routing/four-table actor manifest pins remain unchanged.

Local reproduction (Python requires the documented psycopg environment):

```sh
podman run --rm --network none --userns keep-id -v "$PWD:/src:z" -w /src \
  localhost/fourstory:build-deps cmake --build --preset linux-debug
podman build --layers --target runtime --build-arg BUILD_JOBS=2 \
  -t localhost/fourstory:postgresql-accepted-casts .
python3 tools/database/disposable_environment.py start \
  --work /tmp/fourstory-accepted-casts-test --postgresql-only
python3 tools/database/run_native_verification.py \
  --work /tmp/fourstory-accepted-casts-test --map-runtime-only --build-dir build/linux-debug \
  --image localhost/fourstory:postgresql-accepted-casts --runtime-bin-dir /opt/fourstory/bin \
  --snapshot /private/character/reference/manifest.json \
  --routing-snapshot /private/routing/reference/manifest.json \
  --actor-snapshot /private/statistics/actor-reference/manifest.json \
  --report /tmp/native-accepted-casts-release.json
python3 tools/database/verify_posture_upgrade.py \
  --work /tmp/fourstory-accepted-casts-test --through-casts \
  --report /tmp/accepted-casts-schema-upgrade.json
python3 tools/container_smoke.py --engine podman --image localhost/fourstory:postgresql-accepted-casts
python3 tools/database/disposable_environment.py stop --work /tmp/fourstory-accepted-casts-test
```

Use separate labs for concurrent configurations. For Debug omit image/runtime-bin
arguments; for sanitizers build and use `linux-asan`. Installed Release exercises
installed daemons with the mounted Debug backend integration executable. Remove
owned credentials/TLS keys after stopping the lab. Preserve operator handling of
unknown commits: no automatic retry, no stale final save, fenced process recovery.
This is still an incomplete gameplay server. See the
[accepted-cast contract and verification status](../_rewrite/docs/modernization/evidence/accepted-casts-contract.json).
