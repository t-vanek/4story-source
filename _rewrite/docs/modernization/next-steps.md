# Continuation plan and acceptance gates

The master mission remains active. This file records the next coherent slices;
it does not defer ordinary implementation for permission. Source backups remain
authoritative for historical values, original code/client for supported behavior.

1. **Preserve verified Login boundaries; finish remaining contracts.** Migrations
   008–010 and existing services implement atomic auth/session writes, key-specific
   cleanup, process ownership/fencing, sequential dispatch and graceful drain.
   Source non-direct 0x2918 email verification now follows client-driven LOGIN retry:
   per-connection grant, code/issuance limits, expiry, current rule revalidation,
   no persistent IP trust, and SMTP worker/deadline behavior. Preserve the actual
   PG/TCP/SMTP evidence in `login-security*.json`. Historical issuance was disabled;
   direct-login/JP/TW and non-ASCII retry behavior still need independent evidence.
   Remote Map duplicate handling and historical TAgreement gifts remain partial.
   Historical credential import, full collation/code-page equivalence and the
   actual client executable remain evidence gates; they do not block the next
   character/world implementation slice.
2. **Extend the verified native character/world boundary.** Preserve migrations
   001–021 and the checkpoint, retirement and replica contract evidence. Actual
   Login/World/Map TCP now covers single-owner fresh admission, complete inventory
   and learned-skill wire lists, source-based max stats, core save, logout and
   reconnect. The Map owner and connection generation fence all native operations;
   delayed Login termination cannot delete consumed claims. Initial and periodic core
   checkpoints now support fenced recovery of ready v1 claims; old/orphaned or
   drifted state stays blocked. Transient changes since the last checkpoint can be
   lost on a crash. World-link loss now closes clients, waits for durable teardown,
   and gates admission on a new registration ACK; see `world-link-contract.json`
   and its run evidence. World→peer writes now serialize complete frames with
   bounded per-peer backlog, and World accepts only expected secondary endpoints;
   see `world-writes-contract.json`. The subsequent retirement fix distinguishes
   unannounced/rejected connections from World-announced characters. INVALIDCHAR,
   DELCHAR and CLOSECHAR now retire the exact local identity without close-all echo;
   durable ready primaries still save. World close-all requires an accepted registered
   Map connection. See `map-retirement-contract.json` and its two-Map rejection
   regression; previously a failed second Map claim deleted the valid World char.
   Migration 019 now supplies separate one-use replica grants bound to both Map
   tokens, the primary generation, exact target endpoint and catalog releases.
   Actual ADDCONNECT→secondary CONNECT→CHARDATA/ENTERCHAR→CONREADY works for fresh
   admission without guilds/recalls; the replica never loads/saves the primary graph.
   The pinned source deployment separates channels rather than neighboring cells;
   tests explicitly override one compatibility view in a disposable DB and restore
   its exact definition, without changing imported historical rows. See
   `map-replica-contract.json`. Replicas observe local primary actors and track
   movement; other secondary gameplay dispatch remains unported.
   The subsequent World handoff coordinator binds CHECKMAIN/RELEASEMAIN/ENTERSVR
   to expected peers and phases and bounds incomplete transfers; see
   `world-handoff-contract.json`. It does not itself transfer native DB ownership.
   The complete source transfer DTO/codec is now byte-verified against an
   independent 861-byte fixture covering every variable section. Native item
   hydration retains raw magic and all six DWORD extensions alongside narrowed
   client projections. Fresh native load now restores remaining skill cooldowns
   into the live gate; snapshot/restore carries durations across process clocks.
   See `main-transfer-state-contract.json` for precise scope and evidence.
   Migration 020 now integrates actual movement-triggered primary transfer,
   full source-format graph capture/hydration, process/connection/authority fencing,
   exact prepare/consume receipts, replica/checkpoint rebinding and original client
   CONNECT/CONREADY reactivation. Version-2 checkpoints retain the graph after
   transfer. Source prepared recovery and target loaded/ready recovery preserve
   the committed state, while mismatches remain orphaned. Native TCP tests include
   same-socket round trips, skill rejection after graph-backed relogin and a client
   close during delayed target COMMIT. Gameplay drain is bounded and interrupted
   by client close. Preserve `native-primary-transfer-contract.json` and its
   Debug/sanitizer/installed evidence; no new client fields were introduced.
   Migration 021 now persists fresh-primary skill cooldowns using contract-3
   core/skill receipts and narrowly granted updates to TSKILLTABLE.dwRemainTick.
   Exact retries, rollback, relogin and crash recovery retain learned ranks and
   remaining durations; a skill drift blocks recovery. Full transfer graphs keep
   contract 2. Preserve `skill-checkpoints-contract.json` and its evidence.
   Native casts now use learned ownership/rank and pinned resource definitions,
   calculate source type-1/2 HP/MP costs, and check/deduct under one state lock.
   Unknown skills, forged caster/route and malformed bodies cannot mutate resources;
   success ACKs retain the learned rank. See `skill-costs-contract.json`.
   Normal native new-use cooldown generation now follows `TObjBase::SkillUse`
   and `CTSkill::Use`: rank increments, source physical/ranged/magic formulae,
   powered weapon slots, passive/item speed rates and same-kind extension are
   integrated. See `skill-timing-contract.json`. Next add active buff/disarm/disguise
   timing and authoritative effect expiry; speed-dependent casts currently close
   without mutation when a transferred graph has buffs. Native owned-PC loop
   timing/resource/wire gates and non-consuming weapon checks are now integrated;
   see `skill-loop-contract.json`. Port consumable inventory transactions, full
   effects and cancellation; recompute timing/item eligibility when equipment or
   learned skills mutate.
   CHARINFO and initial readiness now sample live skill durations without
   restarting timers; preserve `admission-timers-contract.json`. Implement active
   effect/quest/recall/companion timer semantics and simulation
   using the existing typed graph. Merely retaining these sections is not native
   gameplay. Trace original bStartAct=2/tutorial/Bow/dbload=1 branches and classify
   the source quest sender's shadowed dwTick before adopting its arithmetic.
   Add multi-neighbor churn and full cell/AOI actor enter/exit ownership, including
   delayed updates, dropped connections and simultaneous nearby movers. Preserve
   World peer/phase checks and SQL authority epochs; the source protocol itself has
   no epoch in CHECKMAIN, so do not claim every same-socket replay is distinguishable.
   Cell-boundary routing and primary role movement are verified; complete AOI
   and entity ownership across processes are still incomplete. Native replica admission does not prove
   those gameplay paths. Never treat an orphan as a successful
   save or clear it merely to make a login test pass. Native admission currently
   refuses persisted guild memberships and active recalls; implement those graph
   dependencies, mail/client ancillary synchronization and full World social
   persistence. The World test registry is in memory, not a completed native World
   repository. Runtime DB/time semantics remain separate from historical rows.
   Movement save is verified, but movement validation/AOI/pathfinding parity is not.
   Do not wire the legacy item/skill repositories to app_world via search_path.
3. **Items/inventory transactions.** Starter allocation is now atomic and preserves
   original world 0 high-water 735812. `item_world` is separate from client group ID.
   Extend the current native schema/repositories for ownership transfers, other
   storage kinds, expiry, currency and durable rewards after reading their source
   contracts. Current Login item rows deliberately support character inventory only;
   use a new migration to extend them, not edits to applied 012/013.
4. **Spawn and core gameplay parity.** Current templates use real recovered stats.
   Port original ordinary/essential/weighted/leader selection, packed entity IDs,
   regen states and level-dependent attributes with explicit missing-content
   behavior. Continue damage/skills/death/quests into real durable rewards. See
   `protocol-compatibility.md` for known differences, including zero-count handling.
5. **Existing advanced modules.** Preserve implemented World handlers. Work through
   guild/social/mail/events/war/tournament/Bow/BR and original tool duties using
   the complete file/opcode index. Replace remaining T-SQL dependencies individually,
   with verified return values and transaction semantics.
6. **Original-client and reliability acceptance.** Identify/hash the actual supported
   executable and assets; confirm `0x2918` packet behavior with authorized captures
   or client execution. Test startup→login→character→world→gameplay→save→disconnect→
   reconnect. Profile real tick/DB behavior, run sanitizers/concurrency/fault tests,
   pin build dependencies, and remove the temporary SQL Server runtime package only
   after all persistent paths use PostgreSQL. Windows native PG build is unverified.

## Known technical follow-ups

- Preserve the TCP session strand and atomic `IsOpen` checks. All channel and
  socket operations, including composed-read/write continuations and cancellation,
  must share that strand. Shutdown must cancel backpressured producers before
  closing the channel. The separate optional `TlsAsioSession` still needs a
  concurrency/shutdown audit; native PostgreSQL TLS is provided by libpq and is
  unrelated to that optional game transport. Shared application registries are
  not made globally thread-safe by a per-session transport strand.

- Review the explicit TCheckIP wrapper/procedure ordering difference in
  `evidence/auth-source-contract.json` before claiming complete login error parity.
  Native behavior currently follows the backup procedure.

- Preserve native hydration order: source `TMapSvr/SSHandler.cpp:2019`
  sends CHGCHANNEL and CHARINFO before final CONNECT; the original client loads
  the map in CHARINFO and activates its frame in CONNECT. CONREADY must not
  duplicate CHARINFO. `native-map-charinfo-order-before.json` reproduces the
  previous error; final `native-map-replica*.json` verifies the correction.
- Primary transfer must carry transient state. Original
  `TMapSvr/SSSender.cpp:1999 SendMW_RELEASEMAIN_ACK` serializes current core,
  inventory/cabinet items, skills and remaining cooldowns, buffs, quest progress,
  hotkeys, item cooldowns and further companion state. World
  `SSHandler.cpp:2284` forwards it to the new main. A stale database reload or
  promotion of the existing summary-only replica would discard state. Migration
  020 now provides the fenced transfer transaction, authority epochs and graph
  recovery. Preserve its source widths, order, supported-state checks and tests
  while implementing the remaining active gameplay represented by that graph.

- Preserve the implemented replica split: World sends CONRESULT to the main;
  the client sends CONREADY to every returned server ID. Secondary ENTERCHAR
  hydrates only the source summary; full recall graphs remain refused. Grants
  bind the exact primary owner/generation and target token/endpoint with one-use
  expiry, keeping mutable primary ownership unique. Avoid acquiring another runtime
  owner's row lock after the account lock: owner replacement uses owner→account
  ordering and can deadlock with the reverse order. Replica teardown must never
  release the primary's current-user reservation.

- Native login now uses account-row locking, a unique current-user constraint and
  one transaction. Preserve those boundaries; the ODBC path retains older behavior.
  Key-specific logout and abandoned-login cleanup are verified. Do not regress
  these while implementing durable character/world handoff.
- Existing eight legacy CTest executables silently return success when their DB
  fixture is missing. Track them as internal skips; new native tests return 77 so
  CTest reports a skip explicitly. Do not combine mock/transport success into a
  claim that auth/inventory repositories work on PG.
- Existing ORM compiler warnings in `peer_auth_repository.h` use `st, soci::use(...)`
  rather than a demonstrated statement bind. Review and exercise these production
  writes against PG before enabling peer-auth persistence; do not silence warnings.
- Pool disconnects surface failures; automatic reconnection and transaction hygiene
  after arbitrary caller misuse remain unimplemented. Never replay writes with an
  unknown commit outcome. The test reconnects via a fresh pool explicitly.
- `[content]` supports a secret environment variable. Mutable configurations and
  repository error paths need a complete secret/logging audit during their port.
- Immutable caches are loaded atomically from one PG snapshot and remain fixed
  until restart. Coordinated cluster-wide live catalog activation is not implemented.
- CI is wired for Debug and ASan/UBSan synthetic native pool/Login tests without backup data; remote execution is not claimed. Real-catalog evidence
  must be refreshed locally when queries, mappings or content migrations change.

## Preserved evidence and local workflow

Keep all private extracts/credentials outside Git. Current private reference
manifest is under `/tmp/fourstory-db-forensics/reference-snapshot-pinned/manifest.json`;
its fingerprint is `c4a43e0657002913be45a096caa4b329f198abcc5553737a98a8d586ae572d95`.
The helper creates labelled disposable containers; reuse no unrelated databases.
Build in the Ubuntu image with `/src` bound to the checkout because the current
`build/linux-debug` CMake cache contains container paths.

Do not overwrite migrations `001`–`021` after application. Add migrations `022` onward.
Do not revert unrelated pre-existing client/binary/library working-tree changes.


Prior security-increment Debug verification on 2026-10-09 (Europe/Prague): 187 CTest entries, 176 passed, eight
legacy DB tests skipped internally, three native tests explicitly skipped without
fixtures. Native Login/pool integration and six selected ASan/UBSan crypto/transport/Login
tests passed, including actual PG failover and shutdown with leak detection; 22 Python DB tests passed on a fresh DB.
The installed Map binary also passed the pinned original catalog verification
again in `evidence/security-catalog-release.json`. See the
modernization README and evidence files for image/cleanup state of the latest run.

Prior security Release image `localhost/fourstory:postgresql-security` (ID
`f8335e8dac8f03f15f85a152eb20479fb28eb7393af610f4ceb1bcd0d5176b25`)
passed six-daemon smoke, installed native Login TCP/SMTP/database verification,
and installed Map pinned-catalog verification as UID 10001. Native Login runs
record 97 C++ checks, the established Login protocol checks and 54 security-wire
checks in each Debug, ASan and Release verification. No external mail was sent.

The packet read close guard now retains its session through reactor teardown.
The previously failing echo test passed thirty instrumented repetitions and is
part of the six-suite sanitizer preset/CI. Full Debug CTest and 22 reconstruction
DB tests passed. See `evidence/sanitizers.json` and
`evidence/transport-teardown-regression.json` for scope and exclusions.
The deployment grants are exercised directly by the real native fixture.

Earlier character increment: 188 CTest entries (176 passed, eight legacy internal
skips, four explicit native skips), 25 Python reconstruction/publication tests.
Dedicated real-PostgreSQL character tests run separately in Debug and ASan/UBSan;
see `characters*.json` for exact checks. No original-client executable was run.

Private character manifest:
`/tmp/fourstory-character-20261009/reference/manifest.json`, normalized fingerprint
`0a2a42d8f62eb8d80bffa2f44f54e5c4920793d9cea25c927e565fa6e063ebb8`.
It contains 107,977 source rows in 22 tables. The original Map manifest remains
unchanged. Extract using `extract_reference.py --profile characters`; import with
`migrate.py`, then explicitly publish with `activate_character_catalog.py`.

Do not treat the earlier lobby evidence as complete client/world acceptance. The
native Map follow-up below extends it while preserving the verified Login and
character boundaries. See `character-implementation-contract.json` for
unresolved source/runtime differences and `character-cleanup.json` for lab state.

Earlier verified image: `localhost/fourstory:postgresql-characters`, ID `684c7dbddae8411dfe1e5469b9ff86eead661cf21ee8ad00b6bb281b98336b9d`, UID/GID 10001:10001. Installed Login encrypted character workflow and six-daemon smoke passed; details in `character-container-verification.json`.

Map admission follow-up verified on 2026-10-09: 190 Debug CTest entries (178
passed, eight internal legacy DB skips, four explicit native DB skips). Each plain
and encrypted Map TCP fixture passed 85 checks. The six current Debug binaries
were mounted read-only in the existing character Release runtime image for health,
Map–World DNS and SIGTERM verification; this is not a new installed Release image.
Current backup procedure/routing review is `evidence/map-entry-backup-contracts.json`.
Private routing extracts remain in `/tmp/fourstory-map-entry-20261009/routing-review`;
the owned lab and secret files were removed. Subsequent migrations 014–017 now
implement Login routing/handoff and native Map ownership; preserve those applied files.

Native Map runtime follow-up (2026-10-09): migrations 016–017, independent actor
publication and actual native Map services are integrated. Resume from
`evidence/map-runtime-contract.json`, not the earlier unimplemented-claim notes.
This increment was initially verified with mounted Debug/sanitizer binaries.
The later checkpoint Release image below now includes it. The older
`localhost/fourstory:postgresql-characters` image does not contain these Map changes.
Verification: 192 Debug CTest entries (178 passes, eight legacy internal skips,
six explicit native skips); separate Debug and ASan runs each pass 36 native Map,
57 encrypted three-daemon TCP and 29 pool/TLS checks; all 19 sanitizer preset
entries and all 31 Python tests pass. The character regression passes as well.
The SIGTERM-during-ready-commit test verifies no stranded account reservation.
Actor manifest: `/tmp/fourstory-map-runtime-20261009/actor-reference/manifest.json`,
SHA256 `e47806df593b8646d6e52c90e7638131f1baac7f90eac3fb1c3c957940739381`.
It contains 746 source rows in two tables; publish independently using
`activate_actor_catalog.py`. Check `map-runtime-cleanup.json` before reusing a lab.


Native checkpoint follow-up (2026-10-09): `018-native-map-checkpoints.sql` is applied
and immutable. Read `map-checkpoint-contract.json` and `native-map-checkpoint*.json`
for current behavior. The source interval is 30 minutes; tests explicitly shorten
it to 500 ms. A real Map SIGKILL during a delayed checkpoint retains the prior
committed core, and restart/relogin through the same World succeeds. A concurrent
final save cannot be overwritten by an older checkpoint. Old no-receipt claims and
core drift remain orphaned; do not backfill receipts to bypass this gate.
The Map peer high byte now correctly identifies server type 4, independent of the
application world ID. World TCP-loss cleanup keeps native DB recovery ownership.
Checkpoint increment installed image: `localhost/fourstory:postgresql-checkpoint`, ID
`e061e3a704196ea884b28c20a557b9c5f40c9b777eab0f8df37cd313eea3d58b`, UID/GID
10001:10001. Debug, sanitizer and installed Release runs each pass 59 native Map,
79 encrypted three-daemon TCP and 29 pool/TLS checks. Release verification uses
the installed Login/World/Map daemons with mounted Debug integration executables.
All six installed daemons pass health/DNS/SIGTERM checks; all 20 sanitizer preset
entries and all 31 Python tests pass. Full Debug CTest: 192 entries, 178 actual
passes, eight legacy internal skips, six explicit native DB skips, zero failures.
See `evidence/map-checkpoint-container-verification.json` and
`evidence/map-checkpoint-build-fingerprints.json`. No original-client execution
is claimed. The subsequent World-link increment implements client teardown and
registration gating; consult its contract for current continuation work.
The private lab is `/tmp/fourstory-checkpoint-20261009`; consult
`evidence/map-checkpoint-cleanup.json` for its terminal state. Original reference
snapshots remain at the paths recorded above.


World-link follow-up (2026-10-09): `services/world_client`, Map listener/CONNECT
and main now gate native admission on current-peer registration, close clients
on World loss and await durable teardown before reconnecting. No migration added;
001–018 and both backups are unchanged. Resume from `world-link-contract.json`.
Debug, ASan/UBSan and installed Release each pass 59 native Map, 131 encrypted
three-daemon TCP and 29 pool/TLS checks. Full Debug CTest: 193 entries (179 actual
passes, eight legacy internal skips, six explicit native skips), zero failures.
All 22 sanitizer preset entries pass after rebuilding the preset.
Earlier verified image: `localhost/fourstory:postgresql-world-link`, ID
`529effcfb3c59321a8b3ccd1d7151b00161af6a9da374a0dbb0860dd0eee8f86`, UID/GID
10001:10001. Release uses installed Login/World/Map daemons and mounted Debug
integration executables; six installed-daemon health/DNS/SIGTERM checks pass.
The owned PG-only lab is `/tmp/fourstory-world-link-20261009`; consult
`evidence/world-link-cleanup.json`. Private reference snapshots above remain valid.
The subsequent world-writes and map-retirement increments resolve the writer and
retirement prerequisites. Follow the current README and top-level continuation
steps for dynamic routing and primary transfer. World heartbeat/blackhole detection, complete social
persistence, transactional gameplay and real-client execution remain outstanding.


Latest replica increment (2026-10-09): migration 019 and the actual native Map
handlers now implement fresh secondary admission, summary hydration and separate
replica retirement. Primary CHARINFO now precedes CONNECT, matching the source
client. Debug, ASan/UBSan and installed Release each pass 96 native Map, 29 pool/TLS,
153 four-daemon TCP, 36 replica-wire, 66 World-peer and seven second-Map rejection
checks. CTest: 195 entries, 181 actual passes, eight internal skips, six explicit
native skips, no failures; sanitizer preset: 24 passes. Plain/encrypted admission:
173 checks each. See `evidence/map-replica-contract.json` for boundaries and the
synthetic partition used in replica tests. Current image:
`localhost/fourstory:postgresql-map-replica`, ID
`bebc88410bb885dd7eb6ee0a2b0faa49454681ed5cd3a56b54ce4d6fd238664f`, UID/GID
10001:10001. The owned PG-only lab `/tmp/fourstory-map-replica-20261009` is stopped;
`evidence/map-replica-cleanup.json` records container/network and temporary secret
cleanup. Private snapshots remain available. Continue with the transfer/state
requirements above; the full server and real-client acceptance remain unfinished.


Earlier verified skill resource increment (2026-10-09): read
`evidence/skill-costs-contract.json` and the Debug/ASan/installed Release reports.
All three configurations pass 880 native database/network checks; default Debug
has 200 entries (186 actual passes, 14 skips), and all 33 sanitizer entries pass.
That increment used local image `localhost/fourstory:postgresql-skill-costs`,
`42ba521c5b853387551f80892d7977fcc3391813e42d2c45c779a555a2f0f6dc`, UID/GID
10001:10001; six-daemon smoke passed. Test labs were disposable PostgreSQL only;
consult `skill-costs-cleanup.json` before reusing any private work path. Source
snapshots remain at the documented paths. Preserve migrations 001–021. Continue
with the later timing increment and active gameplay as described above;
this increment does not establish a complete combat loop or original-client run.


Earlier verified timing increment (2026-10-09): `skill-timing-contract.json` and its
Debug/ASan/installed Release reports each record 906 native database/network
checks. All 33 sanitizer entries pass; Debug has 200 entries (186 actual passes,
14 skips). That increment's local image `localhost/fourstory:postgresql-skill-timing`
is `81b6c61dca05fae4150e2079de58ba7e6adc685aaaa79e087430974bee3f32ec`,
UID/GID 10001:10001; six-daemon smoke passed. See `skill-timing-cleanup.json` for
owned lab cleanup; original reference manifests are retained. Continue active
buff/disarm/disguise simulation and remaining cancellation branches;
normal rank/weapon/passive/item/same-kind cooldown generation is now integrated.
Equipment/learned-skill changes must rebuild the cache. Migrations 001–021 remain
immutable; no historical source values were changed by this increment.

Earlier verified admission-timer increment (2026-10-09): CHARINFO and initial
CONREADY checkpoints now sample live remaining skill durations independently.
Loaded payloads and timer origins remain unchanged; expired durations become zero.
Read `admission-timers-contract.json` and its Debug/ASan/installed Release reports:
910 native checks per configuration, 186 actual Debug passes (200 entries, 14
skips), all 33 sanitizer entries and six-daemon health/DNS/SIGTERM smoke passed.
The previous installed image fails the delayed-readiness regression. That increment
used `localhost/fourstory:postgresql-admission-timers`, image
`7007a8af428eef454ef1033e286d6b7a2a8ec32b1c6b2f70aaa56833a8f30c52`, UID/GID
10001:10001. Consult `admission-timers-cleanup.json` before reusing lab paths.
Continue active buff/disarm/disguise simulation, effect expiry and source-traced
active loop effects, consumables and cancellation. The original client executable remains an acceptance
gate. Preserve backups and migrations 001–021; the next schema migration is 022.

Latest verified loop increment (2026-10-09): native owned-PC `CS_LOOPSKILL_REQ`
and its original ACK now implement distinct loop timing, cooldown-first resource
gates, HP equality, missing maintained-effect rejection and non-consuming weapon
checks. Shared timer state prevents switching normal/loop opcodes to bypass reuse.
Read `skill-loop-contract.json`: 929 native checks per Debug/ASan/installed Release,
186 actual Debug passes (200 entries, 14 skips), all 33 sanitizer suites and
six-daemon smoke pass. Previous installed admission-timers image lacks LOOPSKILL_ACK.
Current image `localhost/fourstory:postgresql-skill-loop` (also `:main`) is
`d4a8bc5990d871e051853c37097842288f6886b6d5c4e1e8d209bd9cca7b873e`, UID/GID
10001:10001. See `skill-loop-cleanup.json` for owned lab cleanup. Next port
consumable inventory transactions, authoritative active effects and cast lifecycle
before exposing cancellation. Preserve the source's cancellation context: the
client sends CANCELSKILL when an acknowledged cast cannot activate; arbitrary
client timer resets are not an acceptable substitute for a cast lifecycle.
Full combat/AOI/target rules and original-client execution remain unfinished.
