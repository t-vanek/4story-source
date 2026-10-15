# Native ordinary self-drop damage and death — Map, 2026-10-15

Baseline: local `main` at `d86d407`. Status: **IMPLEMENTED / INTEGRATED_UNVERIFIED**. Static source/API/diff review only: no build, test execution, SQL query, service/client/runtime, migration/import, backup extraction, commit or push was performed by this agent. Root owns migration070, its publisher/grants and global integration documentation; the independent source review is [analysis.md](analysis.md). Existing `_rewrite/docs/audits/` was untouched.

## Original full request and death chain

`Server/TMapSvr/CSHandler.cpp:10608–10742` reads DWORD target, BYTE type, DWORD damage, BYTE channel and WORD map: **12 bytes**. Client `CSSenderAll.cpp:1007` sends the request to all Map connections. This producer also serves real client drowning/lava/fall paths; it is not an authoritative monster or PvP hit. Original channel/map fields are parsed but **unused**. Source resolves an existing primary PC with map and HP>0, then refuses a target ID different from the sender (with a source anti-hack chat on selected foreign targets). Other object kinds have separate resolution/death dependencies.

For self-PC damage, source sets HP to HP-damage or0, then sets **both recovery ticks to now+5000 even for damage0**. It leaves the activity clock and mode unchanged. Each actual3×3-cell neighbor receives:

1. `CS_DROPDAMAGE_ACK`: DWORD target / BYTE type / DWORD original reported damage = **9 bytes**, including overkill or0.
2. `CS_HPMP_ACK`: original **21-byte** actual maximum/current HP/MP. This occurs **before OnDie clears MP**.
3. Only when HP became0: subsequent OnDie effects and `CS_DIE_ACK`, DWORD target / BYTE type = **5 bytes**.

`TPlayer::OnDie:1971–2141` first handles actual angel companion items, then GodBall/MapKillPoint, recall/companion/self-object deaths, base death, arena/local kills and attacker-dependent PvP. With attacker0/typeNONE0, there is no invented XP loss, item durability loss, inventory drop, aftermath change or PvP reward. DROP directly changes HP and never invokes the Defend/SoulHP/skill800-immunity path. The packet must not be turned into an artificial negative skill hit.

`TObjBase::OnDie:2379` sets TA_DEAD/statusDEAD, HP=MP=0, restores PC attack/size state, invokes ReleaseMaintain and broadcasts DIE. Crucially, `TObjBase.h:230` declares **ReleaseMaintain(bNotify=FALSE)**. Its full implementation at4928 silently deletes **nonstatic** maintained effects and retains static ones; it does not call notifying EraseMaintainSkill, send SKILLEND/STAT/another HPMP or renew an EXP benefit. Client `OnCS_DIE_ACK:3552` → `OnDIE:7484` → `ClearMaintainOnDie:556` performs the corresponding client nonstatic cleanup. Source's old-MP HPMP before DIE is preserved rather than replaced with a fabricated final-MP packet.

## Native network, authority and atomic mutation

The existing CS opcode now dispatches into `OnNativeQuestReq`, with exact12-byte parsing, a current Ready native primary, actual socket/channel/map presence and the explicitly selected ordinary **PvE** runtime. Replica requests do not perform duplicate durable damage. Only self-PC targets are supported; foreign/other-object and already-zero-HP cases produce no false success. The selected native path does not reproduce the foreign-target anti-hack chat text. Wire channel/map remain unused as in source; actual claim/context/physical `CellOwner` supplies route authority.

`services/postgresql_map_death.cpp::DropNativeDamage` runs inside the existing ChangeQuest transaction: current account/user/session/primary claim, runtime owner and catalog/checkpoint/graph/head validation precede mutation. It refreshes actual supported statistics under the current graph and cluster, refuses unexpected resource clamping and checks the actual physical Map owner. Native actual companions/recalls/local graph, guild and unsupported effect/stat branches retain their existing supported-statistics gate; riding/tactics, arena/tournament and maps500..532 are explicitly excluded. Map500 is the same conservative extra lounge exclusion used by the prior revival bound, beyond source tournament501..532.

The source companion angel path reads active companion items, not arbitrary inactive account pet inventory. Native fresh state has no active companion producer; supported transfer graphs require actual companions, companion items, recalls/recall buffs and local ID to be empty/zero. GodBall, arena-team and self-object assignment producers remain unported and cannot be inferred as completed from constructor defaults. Any future producer must join these death gates before becoming native authority. No dormant graph is silently discarded to make death appear supported.

Nonlethal damage, including0, uses the existing ledger/core/checkpoint independently of migration070. A lethal request requires the additional published absence proof below. Only after all gates pass does the transaction copy change resources/clocks. Lethal cleanup sets HP/MP0 and bDead, silently removes actual nonstatic supported effects, rederives the durable next stat state without adding output packets, and records DROP/old-MP HPMP/DIE in source order. Core/effects/receipt/graph commit together; cache and original client packets are published only after commit. Unknown outcomes retain the existing uncertain-state close/no-replay behavior.

## Pinned ordinary-map absence proof

Original MapKillPoint checks the map's actual castle pointer and tournament roster; the later local branch calls FindLocal(m_wLocalID), including0. Routing publication014 alone cannot prove those registries empty. Source castle/local loading joins TBATTLEZONECHART; GodBall loading uses TGODBALLCHART with its actual map ID. Root's new migration070/publisher exposes the complete pinned source rows in `death_compat`.

The lethal helper uses **one SQL statement/snapshot** to require exactly one `death_compat.catalog_release` whose routing_manifest matches configured native routing and status is verified, and simultaneously rejects:

- Any published TBATTLEZONECHART row whose unsigned WORD map ID equals the actual actor map.
- Any published TBATTLEZONECHART wID0 anywhere, since otherwise FindLocal0 absence is unproved.
- Any published TGODBALLCHART row on the actual map.

This conservatively excludes all possible source battle/local/castle/GodBall contexts instead of guessing that a missing native registry means no active war. Missing migration, grants, publisher/receipt, mismatched current routing or any excluded row makes **lethal damage unavailable before mutation**. No SQL was executed and no source row count/absence is claimed as verified here. Source full Map admission/bootstrap, castle, local, arena, tournament and GodBall gameplay remain OPEN.

## Maintained effects, revival and visibility continuity

The current supported templates have actual static flags:131/132 and existing pinned progress statuses are static; skill800 uses the actual published flag. Death keeps **static800**, and DROP bypasses its negative-defend immunity. Nonstatic800 is silently removed, with no END/STAT/HPMP notification and no side-effect EXP renewal. Rich ability/transform/status800 and arbitrary effects remain unsupported through the existing graph/stat guard. Restoring PC attack/size has no additional represented mutation in this bounded no-transform context; a transform/disguise/recall producer is not fabricated.

The preceding native revival intentionally refuses an already-present800. Therefore a static800 death→revival replacement cycle remains **OPEN**; death must not falsely delete static800 merely to bypass that restriction. The closed supported cycle is a character without800 or with the actual supported nonstatic800 removed by death. Actual chart/template shape still requires Phase B evidence; no fixture-derived fake row was added.

Later AOI visibility must retain actual death presentation. Source NetCode TA_STAND0 / TA_DEAD7 and original ENTER sender/client action consumption are confirmed. `EncodeEnterAck` now writes7 for bDead and retains the existing alive0 projection. This changes no body shape, tutorial bStartAct or transfer field. Full live action replication remains a separate port. Immediate DROP/HPMP/DIE output uses the native grid projection fences: exact current tuple and SQL authority, actor-operation reuse, nonblocking other-recipient operation, post-await actual socket/channel/map/3×3 presence, and no application-delivery ACK/retry. The shared sender now rechecks **both actor and recipient** after SQL and immediately before enqueue: holding a gameplay operation alone cannot imply Ready, since BeginTransfer may mark TransferOut before waiting for operations to drain. Actor identity includes char/user/key/channel/connection/epoch/role/Ready, open socket and exact current presence session/map/channel; the final grid comparison uses current actor presence. Moved/busy/stale recipients may miss the event without replay. Secondary cache/AI death projection remains OPEN; it does not justify a second writer.

## Removing the prototype AI bypass

Root found `main.cpp` starting the historical `RunMonsterAi` even with native ownership. That service uses a cache-only HP mutation, zero player defense and channel-wide prototype audiences; it cannot be native combat authority. Native startup now **does not schedule it**. As a separate defense, the service rejects hydrated native payloads and native cluster summaries (including partial replicas) before adding targets or watchers, rechecks current nonnative state/open socket/exact presence session before every prototype send, and repeats the native/dead guard under the target mutation lock.

The old roam/chase/cache-damage prototype remains available only for nonnative actors. Its spatial/defense/source-AI deficiencies are still OPEN and are not recast as implemented native AI. This repair prevents that producer from bypassing durable native damage/death; it does not replace it with fabricated success, frozen fake damage or a partial source monster attack loop.

## Deferred Phase B acceptance

Run separately under explicit authorization: exact12-byte parsing and ignored trailing wire route values; current primary versus SayToAll replica, self versus foreign/object targets, source zero-HP no-op; damage0/nonlethal/exact-HP/overkill with original DWORD report; both5000 recovery clocks and unchanged last-attack/mode; actual physical owner and current graph/stat drift;070 missing/mismatched/absent proof and all excluded map/global0 rows with atomic no-mutation result; DROP9→HPMP21(oldMP)→DIE5, silent nonstatic cleanup/static retention, no false EXP/end/stat packet, source owner party vitals; no800/nonstatic800 revival cycle and explicit static800 replacement refusal; committed graph/core/receipt versus rollback/unknown COMMIT and no replay; relogin/transfer/dead ENTER action7; moved/busy/closed/owner-replaced recipients and skipped projection; native AI not scheduled, direct-helper native/replica target/watchers excluded and under-lock hydration race. No compiler, PostgreSQL, packet-capture or original-client result is supplied by Phase A.
