# Capability traceability and acceptance backlog

## Active Phase A changes, starting at `2fa2396`

All current changes are **UNVERIFIED**. Historical VERIFIED entries below apply
only to their recorded commits. See [working coverage](full-rewrite-status.md)
and [source-to-runtime rows](legacy-modern-coverage.csv). Added native quest
progress/conditions, NPC conversation, mission/item/skill/routing actions, peer
trust/audit, Patch metadata and Control topology/auth/ban/event/peer persistence
are integrated in existing services. Control now uses owning topology snapshots
across offload/await boundaries. Code `ea7c811` adds bounded completion/crafting
rewards, magic item/turn-in transactions, ordinary level progression, World level
relay and pure918/923 timers. Code `fa4c282` adds the ordinary EXP-item lifecycle,
level benefits and903 refresh, ITEMUSE consumption/quest triggers and player ENTER
effects. `ff50e58` adds ordinary resource/direct-EXP consumables and complete
item cooldown load/save/transfer plumbing; `6a91e4a` removes the automatic retry
of a promotion write after an uncertain commit. These are UNVERIFIED. Premium/PCBang, monster reward benefits, other consumable kinds, remaining
quest actions, switch/territory state, World persistence and full parity stay open.
Code `c4385af` adds Soul Lottery/effect820, MONEY/AP/special-box item paths,
source ITEMSELL pricing/debit and DURATIONREP quote/repair transactions. Trade/shop/
tournament producers, conditional NPC discounts and premium branches are still
missing; these bounded ordinary-player paths are INTEGRATED_UNVERIFIED.
Migrations044–063 and archival/import tools are written, not executed. Earlier increments compiled all six modern binaries at their recorded revisions.
The current changed Map/World targets compile in Debug/Release; tests and runtime
startup are deferred by the Phase A gate. No named client binary is available.
[Latest source scope and limitations](evidence/phase-a-refine-events-20261011/README.md).

Code `45d130d` integrates the World minute timer and native reserved-mail delivery:
source batch limit30, new item IDs at delivery, raw attributes, post/status and
Map notification receipts commit together. Written063, grants and historical
queue importer are unexecuted. Map/World compile/link in Debug/Release; no tests
or runtime were run. Quest/admin/event/BR/rank producers and complete native
World bootstrap remain open. [Scope, source conflict and compilation evidence](evidence/phase-a-reserved-mail-20261013/README.md).

Code `85bf923` / `feea4a1` / `b4ff581` adds **INTEGRATED_UNVERIFIED** native ordinary
player mail sending, protected-list CRUD/admission/handoff, recipient live notification
time and superseding. Migrations061–062 and the exact historical protected importer
are written only. Map/World Debug/Release compile/link passed; tests, migrations,
runtime and containers were not run. Full World social persistence, reserved/
quest/event/admin mail, offline jobs and gameplay remain required.
[Source contracts and compilation evidence](evidence/phase-a-mail-send-protected-20261013/README.md).

| Capability | Implemented runtime path | Remaining scope / acceptance |
|---|---|---|
| Player mail normal/package/bill | Original53A2 → source recipient/country/block/capacity/funds/raw item gates → fenced PG transaction → ordered53A3 response | Trade reservation producers; native name/collation profile; World/event/offline mail; no Phase B/client acceptance |
| Protected lists | Original5377/5379/5472 → native remembered bytes/options → admission5376 and original graph transfer | Chat/invite/deal/duel consumers; atomic World friend persistence; no database/import/client acceptance |
| World reserved delivery | World minute → PG pending queue → atomic post/new raw item/status/receipt → fenced Map53A4 | Quest/admin/event/BR/rank producers; full native World bootstrap; no PG/client acceptance |
| Queued mail notifications | Enqueue audit retained; original recipient live second; return cancels obsolete receipt; current primary timer delivers53A4 | Failure/recovery/counter tests and receipt retention |

Code `d3904c5` adds **INTEGRATED_UNVERIFIED** POSTRETURN/payment, sender-owned
online bill expiry and durable current-primary POSTRECV receipts. Server-only
watermarks prevent admission/list double counting across handoff. The historical
mailbox importer and migration060 are **IMPLEMENTED_UNVERIFIED**, not executed.
Full sending, World producers and offline jobs remain required.
[Exact source contracts and compile-only evidence](evidence/phase-a-mail-returns-20261013/README.md).

Code `18ee496` adds **INTEGRATED_UNVERIFIED** carried-item expiry/alarm, atomic
expired-bag mail, POSTLIST/POSTVIEW/POSTGETITEM/POSTDEL and entry POSTINFO. Native
storage2 ownership retains raw items across graphs/recovery. Claims include
capacity, items, funds, quest hooks and checkpoints. Sending, returns/settlement,
reserved/event producers and offline mail import remain missing; full mail is
not complete. [Source scope and acceptance](evidence/phase-a-item-mail-20261013/README.md).

Current code `3bc3bd1` adds **INTEGRATED_UNVERIFIED** NPC/PvP paths; Map compiled
in Debug and Release, with no tests or runtime. See [source scope](evidence/phase-a-npc-shops-20261011/README.md).

| Capability | Implemented runtime path | Remaining scope / acceptance |
|---|---|---|
| Item/trainer/magic lists | Original NPCITEMLIST → published catalogs → source filters, prices and descriptors | Portal/monster shops; territory/BOW; actual client/PG acceptance |
| NPC purchases | ITEMBUY/MAGICITEMBUY → primary transaction → caller/item/purse/title/PvP/quest/checkpoint → ACK | Native deal/tournament producers, expiry/mail; BOW; failure/concurrency acceptance |
| PvP monetary state | Migration058, explicit historical import, CHARINFO/handoff/recovery and fenced available-point spending | Full records/ranks/rewards/rollover; importer not executed |

Code `0fe24d2` adds these **INTEGRATED_UNVERIFIED** paths. Only Map
Debug/Release compilation was executed; see [source and build evidence](evidence/phase-a-item-upgrades-20261011/README.md).

| Capability | Implemented runtime path | Remaining scope / acceptance |
|---|---|---|
| Item identification/unsealing | GAMBLECHECK/OPEN → native owner transaction → original price/random/capacity/item/purse/title/quest responses | Territory discounts/probability; BOW; full native-role/PG/failure/client acceptance |
| Item upgrades | Original ITEMUPGRADE → all ordinary source switch branches → same atomic item/effect/purse/checkpoint path | Territory/PCBang and shop/deal producers; general effects; unexecuted random/protocol/recovery acceptance |
| Upgrade probability items | Pure905/911/919/921/922/939 → ITEMUSE self-Defend → maintained state and one-use CalcProb | General skills and exact durable timer sampling; zero-magic recovery difference acceptance |
| Gamble/gem/accessory source content | Additive056/057; complete offline archive profiles and matching catalog gates | Migrations/publication not run; no original query order or client binary evidence |

Current code `0457d3d` / `750c797` adds these **INTEGRATED_UNVERIFIED** paths:

| Capability | Implemented runtime path | Remaining scope / acceptance |
|---|---|---|
| REFINE and WASTEREFINE | Original requests → primary owner fence → native item/purse/title/effect transaction → original ACK; full-stack materials and source magic order | Conditional territory bonus/discount authority; BOW variant; PG/concurrency/client acceptance |
| Probability consumables912/913 | ITEMUSE pure self-Defend → retained status/save/transfer/expiry → one-use CalcProb consumption | General item skills/effects, regeneration and exact durable sub-tick sampling |
| World numeric events | Full EVENTINFO validation → ordered registry/live delivery/replay → Map values → receipt context055 | Spawn/regen12/13; complete crash/deactivation and one-shot reward recovery; no replay-complete marker |
| Control event delivery | Native CRUD with shared scheduler/replay lease; bounded plain/TLS frame serialization; publish after role/replay; exact-connection retirement | Slow-reader/DB/process/reconnect scenarios unexecuted; whole admin/peer security and Relay replacement incomplete |


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

## Historical native account security slice (`2fa2396`)

| Capability | Implementation/integration | Verification and limit |
|---|---|---|
| Seven secure-code requests | Existing Map dispatcher → fenced PostgreSQL account state/checkpoint → original ACK | VERIFIED bounded slice: final Debug/ASan/installed Release each1,455 checks; `security-contract.json`; no original executable |
| Attempts, disable and unlock | Durable ten-attempt lock, database-clock deadline, current Login key and revision fence | Concurrent writes, rollback, fresh/full-graph relogin, same-login handoff and process recovery |
| Protected native operations | Inventory/equipment and skill learning retain original unlock prompts and existing mutation repositories | Exact prompt bytes, guard-before-decode behavior and unlocked native paths |
| Remaining security behavior | Other economy operations, BOW entry/reset and administrative recovery remain required | IMPLEMENTING; no blanket security/gameplay acceptance claim |

## Previous native title slice

| Capability | Implementation and evidence | Remaining acceptance |
|---|---|---|
| Owned title list / selection | VERIFIED bounded slice: existing Map dispatcher, migration041 transaction, original World all-Map relay, exact source list/change packets | Final Debug/ASan/installed Release each 1,243 focused native checks; `titles-contract.json`; original executable unavailable |
| Selection persistence | VERIFIED bounded slice: fresh title flags or authoritative full graph, append receipt, separate stale-snapshot fence | Scope includes concurrency, rollback, transfer and restart; complete failure acceptance remains scoped |
| Gold-title learning | VERIFIED bounded slice: source live-ID promotion and multi-selected flags; original reload/transfer derivation | Other title awards, resets and achievement conditions remain pending |
| Notification order / audience | VERIFIED bounded slice: exact-connection FIFO World echoes; source three-by-three cells, replicas receive state only | Controlled socket and native TCP evidence remain separate from actual-client visual acceptance |

## Previous native learning slice

| Capability | Implementation and evidence | Remaining acceptance |
|---|---|---|
| Skill purchase / rank | VERIFIED bounded slice: existing dispatcher → backup NPC/skill/point/title rules → fenced PostgreSQL → exact 30-byte ACK; 039/040; `skill-learning-contract.json` | Debug/ASan/installed Release full and focused native suites pass; original binary unavailable |
| Learning persistence | VERIFIED bounded slice: atomic money/points/rank/title receipt; purchase head; original cooldown retained; fresh rows or complete transfer graph | Concurrent writes, stale snapshots, late SQL rollback, primary handoff, fresh/full-graph relogin and fresh SIGKILL tested; complete fault acceptance remains scoped |
| Trainer content | VERIFIED catalog: 3,364 merchandise rows + 111 title definitions with original text bytes; 13 adversarial publication checks | Dynamic territory/guild/hero rules and NPC quest UI still incomplete |
| Gold title side effects | VERIFIED unselected-title source `UseMoney` behavior and ordered TITLEGAIN packets; current increment adds selected titles | Complete title awards and actual-client lifecycle remain incomplete |

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

Native posture/finish integration (038): **IMPLEMENTED/INTEGRATED** ACTION and
own-PC Cure509/First Aid240 with permanent warrior postures 131/132. Original
flags preserve them at ACTION; Defend removes them at zero MP before its reply.
Empty filtered finishes skip Defend cleanup and still check equipped items.
Effect diff, response audience, healing and recovery commit atomically. Tests
separate native PostgreSQL/protocol evidence from the controlled seven-socket
handler audience fixture. **VERIFIED** for these recorded scenarios in Debug,
ASan/UBSan and installed Release: 3,276 native checks per configuration and 65
upgrade checks. See [executed scope and limits](evidence/finish-effects-contract.json).
General timed/hide/status effects, dependent-posture effects and original-client
acceptance remain **IMPLEMENTING/UNVERIFIED**.

Native `CS_ACTION_REQ`: **IMPLEMENTED/INTEGRATED** own-PC validation, exact result
ACK and durable charge origin; migration 037 fences actions and binds one charge
to a later accepted cast. First Aid 240 self-finish enforces its source 300ms
minimum. Rejected actions preserve the pending charge; explicit zero-skill action
cancels it. Source resource/reuse checks do not debit resources at charge start.
**VERIFIED** in Debug, ASan/UBSan and installed Release with native PostgreSQL and
independent synthetic TCP: original ACK bytes/order, First Aid, pure percentage
costs, concurrent/stale writes, early finish, primary handoff, crash/reconnect and
rollback. See [executed action evidence](evidence/skill-actions-contract.json).
Non-PC/general-active-effect/recall/trap action side effects, general interruptions and
actual-client acceptance remain **IMPLEMENTING/UNVERIFIED**.

Original `CS_FINISHSKILL_ACK`: **IMPLEMENTED/INTEGRATED** for the bounded native
509 self-Cure and empty filtered target list. Actual original packet parsing,
accepted-cast consumption, current-stat calculation, atomic resources/recovery
and exact ordered response receipts use PostgreSQL migration 036. Broader finish
and exception DEFEND paths remain **IMPLEMENTING**; native requests refuse the
old client-authoritative DEFEND prototype. **VERIFIED** for the bounded native
PostgreSQL/synthetic TCP scenarios in Debug, ASan/UBSan and installed Release:
exact source response bytes/order, concurrent consumption, transfer, crash/reconnect,
old-login replay and late SQL rollback. Executed verification and unverified
scope are recorded separately in [finish evidence](evidence/skill-finish-contract.json).
No binary client acceptance or universal replay discrimination is claimed.

Accepted native casts: **IMPLEMENTED/INTEGRATED** in the actual ordinary/loop
request handler and PostgreSQL service. Immediate costs/timers/items/recovery,
raw request and independently checked ACK, stale ledger-head fencing, and ordinary
zero-MP removal of 131/132 postures are implemented. **VERIFIED** for the recorded Debug, ASan/UBSan and installed Release synthetic/native scenarios;
see [accepted-cast evidence](evidence/accepted-casts-contract.json). The accepted
ledger now authorizes the bounded self-Cure finish described above; other target
hits still require their authoritative transaction. Complete Defend, rewards,
full effects and original-client acceptance remain **IMPLEMENTING/UNVERIFIED**.

Native outgoing SKILLUSE/LOOPSKILL powers: **IMPLEMENTED/INTEGRATED** selected-skill
instance modifiers at learned rank, physical/magic/ranged powers, source ordinary
versus loop AL selection, critical and aid country. Fresh loads, equipment/effect
refresh and graph hydration derive from pinned PostgreSQL catalogs. Verification
and ordering limits are recorded in [cast-powers-contract.json](evidence/cast-powers-contract.json).
**VERIFIED** for the recorded native PostgreSQL/synthetic TCP scope in Debug,
ASan/UBSan and installed Release: rank modifiers, source packet fields, current
stats after equipment/effect changes, reconnect/restart and two-Map handoff.
This projection alone does not complete Defend, damage,
generic effects, durability or original-client acceptance. Those remain
**IMPLEMENTING/UNVERIFIED**. Accepted-cast persistence is tracked separately above.

Current stat inspection increment: **IMPLEMENTED/INTEGRATED** native derived
statistics and local/World relay handlers. **VERIFIED** native self-inspection,
all response bytes, original grade/gem/broken-weapon/aftermath rules, two-Map
round-trip preservation and regression recovery; **VERIFIED** World relay
loopback/wrong-peer rejection. Full remote-player real-Map and original-client UI
acceptance remain **UNVERIFIED**. Buff/companion/guild/local-battle stat variants
remain **IMPLEMENTING**; ordinary equipment transactions now have the narrower
verified scope below. See
[evidence/character-statistics-contract.json](evidence/character-statistics-contract.json).
Migration 030 requires a complete four-table actor release. Migration 031 adds
**IMPLEMENTED/INTEGRATED/VERIFIED** offline two-table → four-table transition:
real old installed Map graphs, new Map logout/ready-crash/prepared-crash recovery,
unchanged receipt bytes, graph-only inventory/timers, fenced inactive owners and
atomic publication. Arbitrary content changes, transitive compatibility and actual
client acceptance remain **UNVERIFIED/UNSUPPORTED**. See
[evidence/actor-transition-contract.json](evidence/actor-transition-contract.json).


Native equipment (032) is **IMPLEMENTED/INTEGRATED** in the existing Map request
and PostgreSQL paths. **VERIFIED** ordinary armor equip/unequip/swap and exact
source-ordered TCP responses; original eligibility and two-hand/displacement
rules have unit coverage. Real PG covers zero-diff requests, forced one-unit
split/displacement, allocator/receipt rollback, concurrent writers, stale claims,
unchanged-item/rank drift, HP/MP clamp and authoritative transferred graphs.
Two real Maps exercise graph equipment after a round trip. Local neighboring-cell
EQUIP delivery is integrated but multi-character and complete cross-Map visibility
acceptance is still **UNVERIFIED**. General active-effect cancellation/expiry, special
combat flags, party/guild/companions/local battle, race costumes, timed/security/
trade/store cases and actual original-client UI remain **IMPLEMENTING/UNVERIFIED**.
See [equipment contract](evidence/equipment-contract.json); no branch is dropped
from scope.

Maintained postures (033) are **IMPLEMENTED/INTEGRATED** in the existing equipment
transaction. Permanent 131/132 creation, replacement, equipment-driven removal,
source stats and eight-field persistence use fresh recovery contract 4 or the
existing authoritative graph 2. Ordinary core saves cannot rewrite fresh effects.
**VERIFIED** native PG covers rollback, contention, drift, stale replay, graph owner recovery
and relogin; TCP checks compare original DEFEND, SKILLEND, CHARINFO and intermediate
stat packets. See [posture evidence](evidence/postures-contract.json) for executed
configurations and limits. The next cancellation increment (034) integrates
own-PC `CS_SKILLEND_REQ`, current-owner atomic removal, source absent-match ACKs and
read-only replica ACKs. Native/TCP scenarios cover live-posture handoff, cancellation
on the successor, return, rollback, concurrency and process replacement; see
[cancellation evidence](evidence/effect-end-contract.json). The scope is permanent
131/132 postures, not all effects. Non-PC cancellation, generic erase side effects,
zero-MP removal inside `Defend`, timed effects, posture-dependent skills, full combat
and remote visibility remain **IMPLEMENTING**. No original-client executable has
been verified.


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
| Inventory, bags, equipment, storage | Native starter creation, Map hydration, fresh/transferred-primary consumption and whole-stack carried moves/swaps; legacy gameplay inventory and World storage repositories | Starter ownership/slot constraints; actual bags/items on wire; atomic moves/splits/merges via 027–028; ordinary equipment via 032 and permanent postures via 033; general effects/advanced equipment and expiry pending | Native character/Map and `skill-reagents-contract.json` / `graph-reagents-contract.json` / `ammunition-contract.json` / `ammo-batch-contract.json` / `multi-attack-contract.json` evidence plus existing inventory/loot tests | VERIFIED starter load, one-reagent transactions, atomic ammunition batches and whole-stack moves/different-template swaps via 027 (`inventory-moves-contract.json`); native splits/merges and same-template unequal swaps via 028 are integrated with server evidence in `inventory-stacks-contract.json`; ordinary equipment integrated via 032 (`equipment-contract.json`); permanent 131/132 equipment effects integrated via 033 (`postures-contract.json`); general effects and advanced equipment IMPLEMENTING; historical expiry/timezone import remains gated |
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
