# Native ordinary revival — Map implementation, 2026-10-15

Baseline: local `main` at `1c83b5f`. Status: **IMPLEMENTED / INTEGRATED_UNVERIFIED**. Only static source/API/diff review was performed. No build, test execution, SQL query, service, client, migration, import, extraction, runtime verification, commit or push was performed by this agent. Root owns integration/global documentation; the independent source audit is [analysis.md](analysis.md). Existing `_rewrite/docs/audits/` was untouched.

## Original contract and implemented transaction

`Server/TMapSvr/CSHandler.cpp:1067` reads FLOAT x/y/z and BYTE type. It requires a map, sets WAKEUP/STAND and Y, invokes the actual `TMap::OnMove(x,z)`, then only the main player calls Revival. NPC0 maps to AFTERMATH_ATONCE20; **every nonzero type** maps to AFTERMATH_GHOST10. Client `CSSenderAll.cpp:23` sends the request to all Map connections; these are current ghost coordinates, not a request for a guessed spawn warp. `TMap.cpp:1176` changes admission/AOI when a source64-unit cell changes.

The native path in `handlers/combat.cpp::OnRevivalReq` now routes a hydrated primary into `OnNativeQuestReq`; it cannot reach the prototype's cache-only full refill. Native replica requests do not perform another durable mutation. Ordinary **PvE** is the explicit selected runtime mode. Missing native dependencies and unsupported states fail closed; there is no full-heal fallback.

`services/postgresql_map_revival.cpp::ReviveNative`, called by `ChangeQuest`, uses the existing locked account/current-user/Ready-primary claim, World/Map owner, catalog release, core/children/checkpoint/graph and operation-ledger boundary. It decodes exactly13 bytes and finite x/y/z, requires a genuinely dead actor with zero HP and supported actual statistics/effects, and checks both unchanged source x/z cell and actual `CellOwner` at the requested position. Position, resources, aftermath, mode, effect rows, core and checkpoint commit atomically. The handler publishes committed state and updates presence only after success. Unknown commit/save outcomes retain the existing uncertain-state close/no-replay handling. A rejected branch cannot publish its temporary transaction copy.

Source `TPlayer.cpp:3610` orders aftermath, actual maxima, normal mode, 30/40-percent bars, companion respawn, neighbor revival/bars, then ForceMaintain800. The supported implementation retains this order using actual statistics and an empty supported companion/recall context:

1. A changed aftermath step produces neighbor AFTERMATH and private CHARSTAT.
2. Normal mode produces neighbor CHGMODE.
3. NPC resources use DWORD(actual maximum * **double0.3**); all nonzero request types use double0.4. HP has source minimum1, MP may be0.
4. Neighbor REVIVAL precedes neighbor HPMP, followed by the actual supported800 DEFEND effect packet if its template exists.

Original body widths are unchanged: `CSSender.cpp:1404` REVIVAL is DWORD +3FLOAT =16 bytes; AFTERMATH at5810 is DWORD +BYTE =5; CHGMODE at1172 is DWORD +BYTE object type +BYTE mode =6; HPMP at1315 is DWORD +BYTE type +4DWORD =21. No result, level byte, synthetic full bars or new opcode was added. Own committed HPMP retains the existing effective-party MANSTAT producer once per owner.

## Stored aftermath and private clock

The complete original fresh-load chain **preserves the stored step**: `SSHandler.cpp:3365` is initialization before query.Open3377; `DBAccess.h:192/235` selects/binds actual bAftermath; successful DM serialization3435 and common parse4516/SetAftermath5674 carry it. Native fresh and full transfer/recovery hydration therefore keep bAftermath and initialize only a process-local DWORD deadline. This private payload clock is absent from the wire, durable graph encoding and fingerprint; normal checkpoints/stat refreshes do not restart it. Rehydration after a crash rebases the local clock as a documented recovery policy, rather than claiming elapsed offline simulation.

`TPlayer.cpp:3444/3497` supplies the implemented interval `DWORD(27 + step*0.3)*1000`, strict deadline<tick, one decrement per timer pass and increment from the **previous** deadline. `AdvanceProgressEffects` invokes aftermath after maintained-effect expiry, preserving that part of `TPlayer::OnTimer:955–959`; general Recover remains separate. Actual powered equipment magic MTYPE_AFTERMATH113 supplies BYTE(1+bonus), using the existing supported item/posture/stat derivation. Changed step persists through the existing quest tick and emits AFTERMATH → HPMP → own CHARSTAT. No invented server cooldown multiplier was added: the original m_fReuseInc has assignments but no established runtime reader.

The production one-second native timer in `main.cpp` selects the real progression opcode0 whenever bAftermath is nonzero or800 has finite remaining time, even if no other item/entitlement/status needs a tick. Idle actors therefore cannot fall into mail-only opcode2 and strand either deadline. Eternal800 with remaining0 has no expiry timer. Source OnTimer/CheckMaintainSkill does not invoke CheckEternalBuff, so zero MP alone adds no periodic erasure.

Current native stat semantics are bounded to step0..100, nonzero step at level>=10 and decrement without BYTE underflow. A loaded larger/low-level nonzero step, decrement>step, or unexpected current-resource clamp is refused instead of silently claiming original wrapped-step semantics. Source BYTE wrap above100 and its corresponding stat model remain OPEN.

## Actual skill800 and lifecycle

`TObjBase.cpp:4397` ForceMaintain needs the actual skill/template and SA_BUFF data, uses actual duration/rank1/self identity and emits complete DEFEND. Production `ReadProgressEffects` reads the pinned `character_compat.TSKILLCHART/TSKILLDATA`; it never fabricates a row or duration. An absent template retains the source FindTSkill failure/no-effect branch. An existing unsupported template makes revival unavailable **before commit**.

The selected800 shape is deliberately pure marker: positive, no object radius, posture, hiding, weapon or duration-increment effects, and a nonempty set of actual SA_BUFF / SDT_STATUS / STATUS_NONE rows. Actual duration, priority, static, zero-MP and attack/defend erase metadata are retained. Ability rows and nontrivial statuses are refused; therefore insertion/removal cannot leave a stale derived stat sheet or require an unimplemented conditional maxima/bar emission. This does not claim that the deployed row has been executed or even that its actual shape passes the guard.

Finite/eternal800 participates in supported maintained sampling, checkpoint/transfer hydration, expiry, zero-MP handling and own-PC cancellation. A dead graph containing actual nonstatic800 is refused before hydration publication, matching the boundary identified in source `PushMaintainSkill:4614`; source-filtered durable dead loading itself remains OPEN. Missing effect storage is checked safely. Source one's-own-PC SKILLEND ignores bCanCancel (`CSHandler.cpp:10418–10453`); no invented cancellation gate was added.

BEA_ATTACK erasure is wired into supported ACTION and ordinary SKILLUSE, corresponding to source CSHandler1362/2971. The actual template flags decide erasure; supported posture actions retain the source exception. Committed end events send SKILLEND and private STAT before their action/cast result. ACTION's existing64-metre audience remains independent from effect-end's source3×3-cell audience. General negative PC defend/damage is still unsupported before mutation; no fabricated protection MISS or incorrect positive-self DEFEND erasure was introduced. Full negative800 immunity/defend erase ordering, recall protection propagation and automatic enter-map800 provisioning remain OPEN with their real producers.

Whole-function review distinguishes `OnCS_SKILLUSE_REQ:2429` (attack erasure and CheckEternalBuff2971–2972) from `OnCS_LOOPSKILL_REQ:3040`, which invokes neither operation. The native cast mutation preserves this distinction: ordinary SKILLUSE performs actual zero-MP/template-flag removal after attack erasure; LOOP does neither. The similar hooks at20069–20070 belong to the separate `OnCS_ACTITEMSUSE_REQ:19875` producer, which remains OPEN. The earlier snippet attribution to LOOP was corrected before integration. Eternal800 can therefore survive a source loop reaching zero MP until a real applicable source hook occurs; no periodic zero-MP rule or invented loop removal was added.

## Publication fences and intentional limits

Revival neighbors use actual channel/map/source3×3 cells rather than the whole channel. Dispatch already owns the reviver's gameplay operation; it is reused for own delivery. Other recipients use only a nonblocking gameplay claim, avoiding cross-actor wait cycles. Before a recipient's first output, the existing read-only native authority operation checks live World/Map owner, account/current user, claim and actual name; tuple char/user/key/channel/connection/epoch/role/Ready and current presence channel/map/3×3 cells are rechecked after the SQL await and before each enqueue. A busy/stale/closed recipient may miss the event; no retry or application-delivery success is invented. An unavailable owner proof closes the reviver while leaving the already-committed mutation unreplayed. Current registry permits nested gameplay counts, so removing the redundant own lease is structural hardening, **not evidence of a proven prior own-packet drop**.

Explicit OPEN boundaries remain:

- Full native positive-PC damage/death and SoulHP upstream production; persisted genuine dead state is supported, not a claimed complete combat loop.
- Source secondary WAKEUP/STAND/position revival projection, cross-cell movement/AOI/admission and remote replica cache/AI updates.
- BOW/BR, tournament/special revival and ordinary PvP runtime. Maps500..532 are conservatively excluded, including lounge500 beyond the source tournament501..532 range. Specific instance lifecycle is OPEN; no unsupported claim that all instances were identified/excluded is made.
- Riding/tactics/guild, real companions/recalls/local graph or other unsupported maintained/stat contexts; zero maximumHP and already-present800 replay/collision branches.
- Ability-bearing/nontrivial800, dead nonstatic source-filtered loading, wrapped aftermath decrement/step model, ACTITEMSUSE attack/zero-MP producer, richer negative defend and recall propagation.

## Deferred Phase B acceptance

Execute separately under explicit authorization: exact13-byte/finite/range parsing; NPC0 and nonzero ghost percentages/HP floor/MP0; true dead state and same-cell physical owner; rollback versus committed core/effects/checkpoint/receipt and uncertain COMMIT with no replay; actual800 absent/supported/rejected shapes, finite-only idle expiry, eternal ordinary-cast zero-MP removal and absence of loop/timer removal, cancellation, ACTION/SKILLUSE attack erasure, dead/static hydration and transfer; aftermath-only idle tick, strict deadline/one decay/equipment bonus/saturation/underflow refusal/fresh and transfer rebase; exact body/order/owner+neighbor+party audience, including effect-end spectators beyond ACTION64m; owner/socket/key/epoch replacement, current replica recipients, moved/busy/closed recipients and skipped publication without invented success. Full client execution and database/runtime evidence are not supplied by this Phase A implementation.
