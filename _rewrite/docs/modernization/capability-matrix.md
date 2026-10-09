# Capability traceability and acceptance backlog

Status vocabulary: NOT_ANALYZED → ANALYZED → SPECIFIED → IMPLEMENTING → IMPLEMENTED
→ INTEGRATED → VERIFIED; BLOCKED identifies a specific missing contract. Status
always applies to the stated scope, not the whole server. Tests of fake services
are useful domain/protocol tests but do not verify native persistence.

The complete file/opcode index is [source-inventory.json](evidence/source-inventory.json).
It includes original tools, generated C translation units, packet collisions and
unreviewed markers; no file or opcode is dropped because it has no modern match.
Its per-file NOT_ANALYZED entries require a detailed behavioral review beyond the
role-level assessment below. Shared packet names identify candidate counterparts,
not proof of replacement. Regenerate with `python3 tools/modernization_inventory.py`.

## Processes, shared infrastructure and persistence

Paths below are repository-relative. All six modern daemons are CMake targets.

Current fresh-primary timer refinement: migration 021 verifies atomic core/skill
checkpoints, exact retry confirmation, rollback, ordinary relogin and crash
recovery without requiring a Map transfer. Learned sets/ranks are preserved;
full skill/effect gameplay and original-client acceptance remain pending. See
`evidence/skill-checkpoints-contract.json`.

Current primary-transfer refinement: migration 020 and the actual Map handlers
implement movement-triggered release/consume/promotion with a new authority epoch,
full graph journal/checkpoints and source/target failure recovery. Native two-Map
round trips, graph-backed relogin, lost-response confirmation, expiry/rollback,
same-socket stale-write rejection and disconnect during COMMIT are VERIFIED for
the tested normal-character scope. See `evidence/native-primary-transfer-contract.json`.
Typed effect/quest/companion state preservation does not implement those subsystems;
full AOI, dynamic multi-neighbor churn and original-client execution remain pending.

The shared plain TCP session now serializes socket and channel operations on a
per-session strand and cancels blocked producers during close. The reproduced
UBSan close race and full-queue shutdown regression are covered by multithreaded
tests. Native Login/Map use atomic liveness checks; shared application state and
the separate optional TLS transport still require their own concurrency audit.

Current replica refinement: migration 019 and `postgresql_map_replica.cpp` separate
secondary admission from mutable primary ownership. The actual Map route,
ADDCONNECT, CONNECT, ENTERCHAR, CONREADY and retirement handlers now use that
contract. Primaries keep the full graph; replicas hold only the source summary,
observe local primary actors and track movement without duplicate broadcasts.
Real PostgreSQL/process tests cover fresh two-Map admission and both disconnect
directions using a clearly labelled synthetic cell partition; the original
compatibility view is restored and historical rows remain untouched. See
`evidence/map-replica-contract.json`. Migration 020 subsequently verifies the
primary transfer path; full cell/AOI ownership, multi-neighbor churn and other
secondary gameplay remain IMPLEMENTING.

| Legacy responsibility | Current modern implementation / required work | PostgreSQL dependency | Protocol / tests | Status and limit |
|---|---|---|---|---|
| `TLoginSvr`: authentication, groups/channels, characters, start routing | `TLoginSvrAsio` handlers + auth, char and map services; replace actual repositories and startup validators in place | Native auth/session transactions verified for synthetic fixtures; normal-world create/list/delete, START routing and fresh single-owner Map claim/load/core save verified | CS_LOGIN/CS_GROUPLIST/CS_CHARLIST/CS_START; existing handler and wire suites | VERIFIED for synthetic fresh Login→World→Map flow; full login profiles/gameplay remain IMPLEMENTING |
| `TWorldSvr`: world/session coordination, social/event state | `TWorldSvrAsio` extensive handlers/registries/repositories; retain implemented W-series behavior, port each persistent operation | Guild/friend/item/event/rank/war repositories still contain SQL Server contracts | MW/DM/RW; many specific handler tests under `tests/` | IMPLEMENTING for full parity and PG; do not replace with a new empty coordinator |
| `TMapSvr`: simulation, player/entity lifecycle | `TMapSvrAsio` handler context, caches, registries, services and AI tick; fill behavioral gaps incrementally | Catalog bootstrap and fenced native character core/checkpoint persistence verified; transactional gameplay mutations pending | CS/MW/DM; map sender, combat, quest and lifecycle suites | IMPLEMENTING; map startup alone is not playable parity |
| `TBoWSvr`, `TBRSvr` | Legacy projects compile Map sources with BOW/BR flags (not missing source trees); modern Map modes and World Bow/BR handlers exist | Schedule/rank/reward transactions pending | Battle packets; `test_bow_handlers`, `test_br_handlers` and related World suites | ANALYZED at process boundary; mode-specific gameplay comparison pending |
| `TControlSvr`: cluster/admin control | `TControlSvrAsio`, peer dialer, registry refresh, security gate and shell | Registry/auth and admin side effects pending | CT/SM, peer TLS/admin-shell tests | INTEGRATED for transport/control subset; PG persistence unverified |
| `TPatchSvr`: patch/version metadata | `TPatchSvrAsio`, patch repository/session | Metadata queries/schema validators need native contract | CT_PATCH; packet, session, repository tests | IMPLEMENTING for native PG |
| `TLogSvr`: audit collection | `TLogSvrAsio`, audit transport/writer | Migration 029, append-only runtime grants, native queries and byte-preserving UDP→PG writes verified with synthetic events; durable spool/reconciliation and retention remain pending | UDP event format; native-audit Debug/sanitizer/installed fault, grants and restart tests | VERIFIED for native LP_LOG ingest/read/committed recovery; full Log service remains IMPLEMENTING |
| `Server/Tools`, including Happy | Tool sources indexed individually; identify retained admin/offline duties | Offline-only until requirements are verified | No assumption that tooling is a runtime role | NOT_ANALYZED for full replacement; not silently out of scope |
| Win32 IOCP/MFC/ATL `TNetLib` | `tnetlib_portable` Asio transport, explicit codecs and OpenSSL adapter | None | Codec/crypto/Asio/TLS tests | INTEGRATED; original-client proof pending |
| Shared configuration, logging, lifecycle | C++ TOML, spdlog, health/metrics, RAII, daemon SIGTERM | Secrets via content env; other repositories/configs still need review | Existing lifecycle/security tests; native map health/shutdown | VERIFIED for tested Linux startup scope; dependency versions/base image not fully pinned |
| SOCI connection infrastructure | Native libpq + existing ODBC; lease isolation, timeout, sanitized connect failures; no unsafe automatic write retry | Native SOCI backend linked in existing common library | `test_fourstory_postgresql`: TLS, rollback, contention, permission, disconnect | VERIFIED for recorded native fixture; automatic dead-session replacement pending |
| Historical DB reconstruction | Existing import/checkpoint/value/byte reconciliation and source schemas | `001`–`004`, 268 source tables, selected reference import | 22 Python tests and earlier forensics evidence | VERIFIED for captured/imported scope; no historical player corpus |
| Explicit catalog publication + bootstrap | `activate_catalog.py`, `005`–`007`, Map `postgresql_catalog.cpp`, ten existing cache services | Fifteen read-only TGAME views; one snapshot per startup | All 14 actual C++ chart SELECTs and actual daemon startup | VERIFIED for pinned manifest, no hot reload |
| Stored procedure replacement | Keep [`procedure-modernization.md`](../database-reconstruction/procedure-modernization.md) and evidence for 37 modern call names; replace in existing repositories | Parameters/returns/transactions must come from backup modules or explicit modern extension | SpCall refuses PG; no generic T-SQL translation | ANALYZED inventory; implementations pending |

## Gameplay and durable state

Current stat inspection increment: **IMPLEMENTED/INTEGRATED** native derived
statistics and local/World relay handlers. **VERIFIED** native self-inspection,
all response bytes, original grade/gem/broken-weapon/aftermath rules, two-Map
round-trip preservation and regression recovery; **VERIFIED** World relay
loopback/wrong-peer rejection. Full remote-player real-Map and original-client UI
acceptance remain **UNVERIFIED**. Buff/companion/guild/local-battle stat variants
and equipment mutations remain **IMPLEMENTING**. See
[evidence/character-statistics-contract.json](evidence/character-statistics-contract.json).
Migration 030 requires a complete four-table actor release. Migration 031 adds
**IMPLEMENTED/INTEGRATED/VERIFIED** offline two-table → four-table transition:
real old installed Map graphs, new Map logout/ready-crash/prepared-crash recovery,
unchanged receipt bytes, graph-only inventory/timers, fenced inactive owners and
atomic publication. Arbitrary content changes, transitive compatibility and actual
client acceptance remain **UNVERIFIED/UNSUPPORTED**. See
[evidence/actor-transition-contract.json](evidence/actor-transition-contract.json).


| Legacy behavior / references | Modern production path and replacement work | PG dependency | Protocol and test evidence | Status / next acceptance |
|---|---|---|---|---|
| Account validation and reconnect (`TLoginSvr/DBAccess.cpp`, CS handlers) | Existing `services/soci_auth_service.cpp` and auth repository boundary; preserve result codes, credential representation, concurrent session behavior | Migrations 008/009 app_global; account-row lock, unique current session, atomic audit, key-specific logout, process token fencing | login-lifecycle.json / login-lifecycle-asan.json: actual PG, encrypted TCP, duplicate/reconnect, rollback, failover and shutdown during auth | VERIFIED synthetic native scope including single-active-owner failover/fencing and graceful drain; historical import, direct-login 2FA/Map ownership and real client pending |
| Source-client email confirmation/retry | Existing auth/handler/registry path; private per-connection token, code digest, full LOGIN revalidation, no trusted-IP shortcut; SMTP runs on workers | Migration 010; five-minute challenges, five attempts, three issuances/account/five minutes, thirty-second verified retry; ownership fencing | login-security.json / login-security-asan.json: real PG, actual encrypted TCP, local SMTP, concurrency, replay, expiry, changed bans/agreement, duplicates, silent relay | VERIFIED for synthetic non-direct source 0x2918; disabled historical issuance branch, unknown executable/encoding and JP/TW/direct profiles remain explicit limits |
| Character create/list/delete and ownership | Login char service, Map player service and World char registry/handoff | Character schema, ownership constraints, create/delete/save transactions | CS_CHARLIST/CREATECHAR/DELCHAR/START/CONNECT; existing lifecycle tests | VERIFIED for native normal-world lobby create/list/delete and process-restart persistence; VERIFIED for Login START/pending handoff; VERIFIED for native fresh single-owner Map claim/load/core save/reconnect; normal-character primary transfer/recovery VERIFIED in migration 020; full ancillary/social state IMPLEMENTING |
| Player stats / progress / time | Native Map loader/core save, `SociStatChart`, `domain/stat.h` | Source-based fresh HP/MP and core progression persistence verified; reward transactions pending | Native Map/TCP evidence, stat/formula and char sender tests | VERIFIED fresh scope; active effects and complete progression pending; historical timestamps are not presumed UTC |
| Spawn templates and combat attributes | `soci_*_chart`, `SpawnAllStatic` into actual monster registry | 3,495 mapped attrs; 39 explicit gaps | Native catalog test and spawn refusal unit test | VERIFIED values/refusal; weighted/essential/leader/ID semantics pending |
| AI, movement, visibility, respawn (`TMap`, `TMonster`, `TAICmd*`) | Map presence/visibility, `monster_ai.cpp`, move/session handlers | Transient state in runtime; durable checkpoints TBD | Move/sender/AI tests | IMPLEMENTING; fixed AI timing/radii and original pathfinding not fully compared |
| Damage, death, rewards | Existing combat/damage-formula, corpse/loot services | Durable reward/item/currency transactions pending | Combat, damage, corpse and loot tests | IMPLEMENTING; no complete original-client combat loop |
| Skills/effects/buffs | Native learned ownership/rank, pinned cost/timing definitions, atomic HP/MP and same-kind gates | Native character hydration; live CHARINFO/readiness samples; learned-skill cooldown persistence via 021 | `skill-costs-contract.json`, `skill-timing-contract.json`, `admission-timers-contract.json`, `skill-loop-contract.json`, `skill-gates-contract.json` and `skill-reagents-contract.json` / `graph-reagents-contract.json` / `ammunition-contract.json` / `ammo-batch-contract.json` / `multi-attack-contract.json`: encrypted use, delayed admission/expiry, equipment/transfer hydration and persistence | VERIFIED normal native cost/ownership and rank/weapon/passive/item/shared-kind cooldown scope; native owned-PC loop gate/wire and ordinary map/previous-effect/non-consuming weapon gates also verified; active buff timing, cancellation, broader consumables, learning and effects pending; multi-attack uses pinned learned-rank budgets; fresh and transferred-primary single-reagent transactions via 022–023 and 1–16-target per-bag ammunition batches via 024–025 and source multi-attack expansion via 026 are verified |
| Quest types and branches (`Quest*.cpp`) | Map quest engine/chart/service; original per-type sources all indexed | Native definitions, 62 orphan rewards visible; mutable progress/rewards pending | Quest engine/sender tests | IMPLEMENTING; retain unported original quest actions in file index |
| Inventory, bags, equipment, storage | Native starter creation, Map hydration, fresh/transferred-primary consumption and whole-stack carried moves/swaps; legacy gameplay inventory and World storage repositories | Starter ownership/slot constraints; actual bags/items on wire; atomic moves/splits/merges via 027–028; equipment and expiry pending | Native character/Map and `skill-reagents-contract.json` / `graph-reagents-contract.json` / `ammunition-contract.json` / `ammo-batch-contract.json` / `multi-attack-contract.json` evidence plus existing inventory/loot tests | VERIFIED starter load, one-reagent transactions, atomic ammunition batches and whole-stack moves/different-template swaps via 027 (`inventory-moves-contract.json`); native splits/merges and same-template unequal swaps via 028 are integrated with server evidence in `inventory-stacks-contract.json`; equipment IMPLEMENTING; historical expiry/timezone import remains gated |
| Item IDs (`TGenerateDBItemID`) | Native character starter allocator in the create transaction; extend to gameplay creation | Proven `2^56` world ranges and world-zero floor 735812; independent item-world mapping | Native character concurrent allocation/rollback tests | VERIFIED native starter allocation; native stack splitting now shares this allocator; other creators and signed-negative allocation worlds pending |
| Money, trade, shops, crafting/upgrades | Existing money/loot/NPC handlers and World cash/item operations; enumerate original CS branches | Atomic ownership and currency changes, rollback/races | Money/loot/item tool/cashsale tests are partial | IMPLEMENTING; duplication/race/reconnect acceptance pending |
| Parties and corps | World party/corps registries and handler modules | Persistence only where original durability is proven | Party join/order/move/delete/corps suites | INTEGRATED subset; full Map/World/client behavior comparison pending |
| Guilds, peerage, tactics, cabinet | Existing World guild handlers/repos/cache | Backup-vs-validator fields and role-code meanings require separate derived schema | Numerous guild and cabinet codec tests | IMPLEMENTING PG; never invent role codes/fields as historical data |
| Friends, chat, soulmate, social | World friend/chat/soulmate handlers and existing repository interfaces | Friend/chat-ban/soulmate persistence pending | Existing load/persist/group/presence/reply tests | INTEGRATED subsets; PG and complete client proof pending |
| Mail/posts, rankings and events | World post/rank/monthrank/event schedulers and handlers | Persistent post/rank/event state pending | Dedicated World tests | IMPLEMENTING; restart recovery and epoch/time semantics pending |
| Castle/territory/war, arena, tournament, Bow/BR | Existing World war/tournament/Bow/BR registries, schedulers, repositories and Map modes | Event/reward/ranking transactions pending | Existing branch-specific suites | IMPLEMENTING; complete simulation/mode and native persistence acceptance pending |
| Mounts, pets, recalls, companions | Map companion service; World recall/spolecnik handlers | Backed-up procedures and companion state need semantic mapping | Companion/recall handler tests | IMPLEMENTING; dynamic attribute levels require a new catalog contract |
| Obsolete integrations / anti-cheat stubs | Existing Apex handlers and original client security branches are indexed | No new dependency introduced | Stub tests cannot certify original anti-cheat behavior | NOT_ANALYZED for removal; no blanket out-of-scope classification |

The architecture keeps transport/legacy packet DTOs → existing handlers/domain
services → repositories. A PostgreSQL change must reach an actual daemon path,
exercise errors and rollback, and preserve packet behavior. Do not create a
parallel unused server or infer a completion percentage from the table.

### Map admission refinement

The existing production handler now verifies original CONNECT framing, version,
checksum, selected character and exact user/key/group/channel. World fresh login
starts ENTERSVR, Map emits one authoritative ACK, and per-session phases gate
CONREADY/gameplay. TCP tests exercise duplicate, timeout, stale World key, delayed
save, failure retention and terminal-response pipelining in plain/encrypted modes.
These are VERIFIED transport/admission boundaries with controlled services, not a
claim of full Map persistence. Native Login now stamps the selected character in
one fenced transaction with routing and a 60-second pending handoff. Independently
published TMAPCHART/TUNITCHART/TCHANNELCHART/TSPAWNPOSCHART preserve original values
and recreate TSVRCHART. The native refinement below verifies fresh Map
claim/ownership, starter-state lists, cell-based CHECKMAIN and atomic core save.
Complete gameplay persistence and crash recovery remain IMPLEMENTING.
Existing 001–018 migrations are now applied and fingerprinted; add new migrations.

Earlier refinements below retain their original scope and evidence; the current
primary-transfer contract above supersedes their transfer-pending statements.

### Native Map runtime refinement

`postgresql_map_{owner,service,load,route}` and `handlers_native_entry.cpp` now
serve the actual PostgreSQL branch of Map main. Fresh single-owner admission,
source-backed inventory/skill lists, core save and reconnect are VERIFIED by real
Login/World/Map TCP and native DB tests. The World registry in that test is the
actual in-memory implementation; social durability is not claimed. Native border
transfers, active recalls, guild hydration, persistent gameplay mutations, complete
movement validation remain IMPLEMENTING. Checkpoint v1 now recovers matching
ready claims; old orphaned/no-receipt or drifted claims remain blocked. Original-client execution remains BLOCKED on a runnable legal
client fixture. See `evidence/map-runtime-contract.json` for precise scope.

### Native checkpoint recovery refinement

`postgresql_map_checkpoint.cpp`, Map main's worker sweep and migration 018 provide
initial/periodic core checkpoints with exact receipts, idempotent confirmation and
atomic final-save release. Owner replacement recovers verified ready v1 claims;
older or inconsistent state remains orphaned. Real SIGKILL-during-write and
same-World reconnect tests verify the committed boundary. Unsaved transient changes
since the latest checkpoint are outside that boundary. General economy/inventory journals beyond single-reagent receipts,
secondary transfers and original-client acceptance remain IMPLEMENTING. World's
TCP-loss cleanup and the corrected server-type peer ID are exercised in the same
flow. See `evidence/map-checkpoint-contract.json`.

### Native World-link lifecycle refinement

Original `TMapSvr.cpp::OnCloseSession` requests service shutdown on World loss;
`OnExit`/`SaveAllCharData` save and log out players. Modern Map now closes client
admission on transport loss, drains existing native save/release paths, and
reconnects after that boundary. Registration requires a parsed ACK with a deadline.
Implementation: `services/world_client.{h,cpp}`, `map_server.{h,cpp}`, Map `main.cpp`
and `handlers/session.cpp`. No migration or client field changes. See
`evidence/world-link-contract.json` and `native-world-link*.json` for actual
PG/TCP verification; `test_world_reconnect.cpp` covers transport-only fault cases.
World→peer writes now use owned frames, one permit through each composed write,
and a 256-frame/4-MiB outstanding budget. World secondary ADDCHAR validates a live
registered Map, the planned endpoint, key/account and a live main; all expected
connections must be valid before CHARDATA. See `evidence/world-writes-contract.json`
for regression and actual daemon evidence. This completes that World admission
subset. The later replica refinement above implements fresh native grants and
summary synchronization. Primary ownership transfer, silent network partitions,
full World durable state and original-client execution remain unfinished.

### Map retirement prerequisite for native replicas

Map tracks whether a local connection was announced to World. Claim failures
send no close-all; World-originated INVALIDCHAR/DELCHAR/CLOSECHAR mark retirement
before closing the client and suppress the teardown echo, including during an
awaited final save. Ready native primaries still persist before releasing ownership.
World accepts CLOSECHAR only from the registered typed Map with a valid connection
and matching key. This fixes a reproduced failure where an ungranted second native
Map deleted the first Map's valid World character. The two-Map regression and
plain/encrypted C++ retirement cases are recorded in
`evidence/map-retirement-contract.json`. This prerequisite is VERIFIED; the later
migration 019 implements fresh native replica admission and summary synchronization.
Dynamic routing, full gameplay synchronization and primary transfer remain
IMPLEMENTING. Both backups and migrations 001–018 are unchanged.


Current acceptance gate: the owner confirms only uncompiled client source, without
additional client files. No binary build or data package is supported by an actual
client run. See [client acceptance](client-acceptance.md); all role/gameplay scopes
above remain separate from that BLOCKED gate. No original feature is removed from
the source inventory or the modernization goal.
