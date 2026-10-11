# Refine, probability items and Control/World delivery — Phase A

Code `0457d3d51816072808e0dc2f804b2850827823f7` and
`750c797c60a816281e3dea97f28e10a6356d4201`, local `main`, based on
`ae121d2349f422aa12fd751d15853db73aa4d888`. These bounded paths are
**INTEGRATED_UNVERIFIED**. Migration055 is **IMPLEMENTED_UNVERIFIED**, not applied.
The complete Phase A gate remains open. No tests, database execution, server or
client session, container build/startup, push or publication occurred.

## Original contracts and native item paths

- `Server/TMapSvr/CSHandler.cpp:14124`, `CSSender.cpp:4890/4901`, independent
  `Client/TClient/CSSender.cpp:1775` and `CSHandler.cpp:14037/14080` establish
  REFINE. Request0x543F contains BYTE quote/bag/slot/count, WORD NPC, BYTE caller
  bag/slot, then count pairs of BYTE material bag/slot. Quote0x5440 contains
  DWORD cost and BYTE discount;0x5441 contains BYTE result/bag and, on success,
  the full original item descriptor. Protected requests prompt before decoding.
- The existing dispatcher, primary gameplay lease, `OnNativeQuestReq` and
  `PostgreSQLMapService::ChangeQuest` reach `RefineNativeItem`. Source inventory,
  equipment exclusion, count/refine/grade checks, power-level normalization,
  material validation order, duplicate rejection, BYTE sums/narrowing and
  DWORD durability arithmetic are preserved. `TItem.cpp::GetPowerLevel` and the
  existing exact economy helper supply the level; template level is not a proxy.
  Quote still performs the source material durability draws before returning.
- Execution preserves `UseMoney(FALSE)` title side effects, optional NPC-call
  debit, money/title/MONEY packets and **whole material-stack deletion before
  the chance result**. Failure spends these resources and leaves the target
  untouched. Success increases maximum durability, restores current durability
  and increments refinement. Raw magic candidates follow material order and
  ascending magic ID, then reverse pop order. Six existing properties skip even
  replacement and random draws; exclusive collisions still draw; existing raw
  values only increase and zero-valued new properties are not inserted.
- `TMapSvr.cpp:9615` (`CalcProb`), `TObjBase.cpp:4789/4802` and
  `TSkillTemp.cpp:457` establish event5/6 and status58/59. A nonzero REFINEPROB
  forces100; TRANSPROB adds base*value/100. WORD wrap precedes the100 cap.
  The first nonzero matching buff supplies the value, but the first skill
  containing that status is erased even if its value is zero. Transfer buffs
  are considered only after successful refinement with nonempty candidates.
  A missing NPC has source chance0 without consuming these buffs. Nonzero
  conditional guild/hero bonuses still reject missing territory authority.
- Pinned backup definitions912/913 are pure static statuses58/59, raw value200.
  Their original IT_USE/IK_SKILL33 consumables now use the existing ITEMUSE path:
  reject the same active skill, sample the self attack level, run the source
  zero-MP eternal-buff removal, push the self-maintained effect, DEFEND to the
  neighborhood, CHARSTATINFO to the owner, then common reuse/debit/result/quest
  hooks. This is the pure self-`Defend` branch, with self host ID/type and actual
  attack level; `ForceMaintain`'s different host/power fields are unchanged.
  It is not general item-skill/Defend support. Read/load, ledger save, graph
  transfer and applicable expiry use the existing maintained-effect machinery.
  `last_attack_ms` retains the player-object write; full combat/regeneration
  consumers and exact durable sub-tick sampling remain unfinished.
- Read-only inspection of existing private TITEMCHART/TSKILLCHART/TSKILLDATA
  exports identified items7612/18171/18449/29201/30015 for912, and
  7613/18172/18440/29202/30016 for913. No source export/import was executed and no
  historical values were created. Hashes of these inspected files are recorded
  in `implementation.json`. Status rows are validated before admission.
- `CSHandler.cpp:15376`, `CSSender.cpp:5511`, client sender2245 and handler17016
  establish WASTEREFINE0x5479 (BYTE bag/slot) and0x547A (BYTE result0–3).
  `ReclaimNativeRefine` requires IT_REFINE14, resolves wUseValue and checks
  capacity **before** consuming one material. The source temporary CTItem has
  no attribute pointer until Copy (`TItem.cpp:6/46/90`, `TPlayer::PushTItem`), so
  it cannot merge with the admitted attributed inventory. It needs an existing
  blank, although actual placement after debit can use the newly freed slot.
  The new instance retains constructor-zero durability/magic/extensions/expiry;
  no template-derived values are invented. No added secure/dead/riding/reuse or
  quest-use gate/hook; those are absent from this source handler.
- Fresh normalized item writes check the exact identity/owner/fingerprint and
  only update fields changed by the operation. Graph-backed characters retain
  the full transfer graph as authority. Money, titles, material debit, target
  magic/durability, effects, operation receipt and checkpoint commit together
  before packets. Existing account, owner, connection, epoch, inventory and
  operation-head fences remain in the call path. Equal request bytes are not
  deduplicated; an uncertain COMMIT closes the client without write replay.
  These are implementation properties awaiting actual PostgreSQL fault tests.

## Control → World → Map event state

- `TWorldSvr/TWorldType.h::EVENTINFO::WrapPacketOut/In`, original World
  `SSHandler.cpp:263/662` and Map `SSHandler.cpp::OnMW_EVENTUPDATE_REQ` define
  the complete event body. Shared `LegacyEventInfo.h` reads fixed-width fields,
  opaque original string bytes and all cash/spawn/regen/lottery tails. World
  validates every tail, not only lottery/gift, and overwrites inner wValue with
  outer wValue as the source does before dispatch/cache/broadcast.
- Active World entries are ordered by unsigned DWORD index. Live delivery uses
  the outer event ID; new-Map replay uses the inner ID, matching the source.
  One coroutine lease spans registry mutation and fan-out, and registration
  through event replay. An old suspended replay cannot overwrite a newer live
  update. Failed recipients are retired; writes are not retried on replacement
  sockets. This does not add an application acknowledgment.
- The real Map World client handles MW_EVENTUPDATE0x9130 and maintains16 WORD
  values plus a server-only connection generation/revision. Each native item
  request samples that state before DB offload. A later update cannot change
  the already admitted operation. Missing registration/projection cannot stand
  in for zero bonuses. Initial zero values come from the original Map
  constructor. Registration ACK still precedes replay; there is **no source
  replay-complete marker**, so this is the value received at admission, not a
  claim of cluster-wide atomic event delivery or completed replay.
- On a replacement World registration, values reset after the existing durable
  player drain. This is an explicit recovery change: a vanished event cannot
  remain cached across a World restart. Control crash/deactivation recovery and
  one-shot reward delivery are still incomplete. Source monster events12/13
  also require spawn/AI effects: Map explicitly retires the link on these
  unported branches instead of silently accepting only their numeric fields.
- World Control registration now requires `[security].control_ip_allowlist`,
  an empty body, no prior Map role and no other live Control connection.
  Default empty denies Control. Subsequent CT commands require that exact
  connection; Control↔Map role reassignment is rejected. The unchanged source
  frame contains no authentication field: the allowlist/private network is a
  transport trust boundary, not cryptographic peer identity or complete peer
  authentication. The actual Control address/CIDR must be configured explicitly.

## Control transport and concurrent lifecycle

`750c797` completes an additional implementation path needed by event delivery:

- Original `CSession::Say/Post` owns a queued packet under its send lock.
  Modern Control previously reused one scratch buffer across concurrent
  `async_write` calls. `ControlSession` now owns each complete frame through its
  write and holds a coroutine permit on both plain/TLS paths. It bounds pending
  output at256 frames /4MiB, closes on overflow/write failure/oversized output,
  and wakes queued senders. Read EOF, framing failure or an escaping handler
  exception also closes the same writer. Source framing/checksum is unchanged.
- A Control event mutation lease now orders native CRUD, scheduler delivery,
  manual pushes and new-peer replay. Deletion cannot race an offloaded edit and
  cannot report an in-memory success without its persistence repository.
  Handlers recheck native Control ownership after waiting for the lease.
- `Handler.cpp::OnCT_NEWCONNECT_REQ` and
  `TControlSvr.cpp:1838::SendEventToNewConnect` establish role-registration before
  running-event replay. Runtime dialing now keeps the peer unpublished until
  both are written. The topology is copied before awaiting, checked again before
  publication, and an unpublished/failed peer closes. No remote-application
  success is inferred from completing a socket write.
- Peer retirement and monitor updates require the exact registered connection.
  A delayed old read-loop exit cannot erase/stop its replacement. Monitor and
  keepalive broadcasts own their status/topology snapshots across awaits;
  inventory removal closes the removed connection. This does not complete
  every admin reply correlation or peer authentication scenario.
- The original event-announcement route is restored: global World announcements
  use connected Relay entries, falling back to World peers for disconnected
  Relay groups. Other selectors retain the original exact-group route.
  Relay runtime replacement itself remains in the broader rewrite scope.

## PostgreSQL and explicit differences

Migration055 adds nullable `quest_operations.world_event_context`:
`[connection generation, received revision, 16 WORD values]`. It is new
operational audit data, not reconstructed historical content. Old receipts
remain NULL. Fresh installation uses the numbered chain; upgrade adds055 after
054 without changing earlier checksums. No migration was applied.

After Phase A, the isolated Phase B upgrade must also reapply
`deploy/sql/map-runtime-grants.sql`: it now grants the specific refinement,
durability and six magic-slot columns, including the previously missing repair
`dwDuraCur` grant. No new DB permission was applied in this increment.

Explicit modern differences requiring later acceptance:

- Reject zero refine materials (source division by zero/uninitialized byte),
  last-item NPC-caller aliases (source use-after-free), undefined numeric/kind
  conversions, one-past EVENT_COUNT and malformed/trailing packet data.
- `CTBLSkillData` has no ORDER BY. `NativeItemBuffValue` accepts one distinct
  matching raw value and rejects conflicting values without inventing row
  order. The shared sale PRICEUP reader now uses this rule too.
- Preserve the existing secure15-bit random range/modulo behavior, not the CRT
  sequence; preserve commit-before-response and no unknown-COMMIT retry.
- World reset/replay ordering, connection-role allowlist, Control output limits
  and exact-connection retirement are explicit transport/recovery repairs.
- Direct Control event/cash/announcement pushes now require the same GMLevel3
  category (or All) as event CRUD, closing their old authorization bypass.
  Honor the requested announcement server instead of source `bSvrID==bSvrID`.
  Reject exhausted event IDs rather than wrap and overwrite another event.

## Compilation and remaining acceptance

Both final commands returned0, incrementally compiling/linking the changed
production targets and resolving all six targets, with BUILD_TESTING=OFF:

```sh
cmake --build build/phase-a-compile --target tloginsvr_asio tworldsvr_asio tmapsvr_asio tcontrolsvr_asio tpatchsvr_asio tlogsvr_asio -j1
cmake --build build/phase-a-quest-release --target tloginsvr_asio tworldsvr_asio tmapsvr_asio tcontrolsvr_asio tpatchsvr_asio tlogsvr_asio -j1
```

Final logs: `debug-control-chain-build.log`, `release-control-chain-build.log`.
The preceding full item/event changes are in `debug-buffs-build.log` and
`release-build.log`; earlier intermediate compilation logs remain separate.
`implementation.json` records commands, configuration, toolchain, source/log
and binary hashes. Existing compiler warnings remain; no runtime assertion or
new test was executed. No sanitizer, fuzz, benchmark, database migration/import,
reconciliation, server/client process or Docker/Podman operation was run.

Deferred Phase B scenarios include independent exact quote/error/success packets;
BYTE/WORD/DWORD wrap and grade gaps21–25; whole-stack failure loss; reverse magic
order/collisions/six-property cap; conditional NPC bonuses; caller aliases;
effect912/913 duplicate/use/save/transfer/consume and zero-MP packet ordering;
WASTEREFINE pre-debit capacity/constructor values/no merge; grants and fresh/full
graph persistence; stale owners/checkpoints, DB/process failure and uncertain
COMMIT. Control scenarios include overlapping writes on plain/TLS, slow readers,
backlog/close, connection replacement, topology refresh, CRUD/tick/replay races,
role/authority rejection, relay routing, event delivery failures and restarts.

The complete emulator is **not PostgreSQL-only** yet. Remaining native World
social/war/economy repositories, general combat/effects/AI/rewards, territory,
premium/PCBang, remaining inventory/economy/quest actions, companions/modes,
monster events, one-shot delivery and missing Patch contracts remain required.
There is no original client executable or matching assets; no supported binary
build/hash or real-client acceptance can be claimed. Phases B/C are not started.
