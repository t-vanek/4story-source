# Native Map PC cells — Phase A, unverified

Baseline `85a9aa2`, local `main`, 2026-10-17. **INTEGRATED_UNVERIFIED** for
ordinary PC-only maps in a process with no surviving source monster slots/jobs
at bootstrap. Full monster visibility/hosted gameplay and full Phase A remain
**IMPLEMENTING**. The admission barrier for populated processes remains in force.
This is a dependency of that work, not permission to call empty-host AI branches
after actors enter or a declaration of a usable populated world.

No build, test, SQL, import, migration, service, client, container, runtime check,
push or deployment ran. Static source/interface/CMake/diff and inventory metadata
inspection only. Private extracts and `_rewrite/docs/audits/` remain outside Git.
No migration was added. Written044–081 remain unapplied; next free number082.

## Authority and source scope

Original server and available client source lineage remains
`c77f79017d7940c1500ba36a5dbf8bda6fefb67c`. The files inspected here still have
that last modifying commit; they were not edited. TVERSION0x2918 is a source
marker, not proof of matching client executable/assets/region data. Executable
compatibility remains **UNVERIFIED**. This slice follows ordinary non-BOW/non-BR
PC code, not another version's mechanics.

The actual local backups were rehashed, without opening/executing their contents:

| Archive | SHA256 |
|---|---|
| TGAME_RAGEZONE.bak | `2cff1635d523322d3de8ddafaeacc3335581dc338dca901239ac80288f51d259` |
| TGLOBAL_RAGEZONE.bak | `fca545bcb90bcc72aa4f3514a6177c2b88d2b0bf71bf847b0128f090a20877fb` |

Existing081 source channel/owner publication supplies the cell graph;079/080
remain dependencies of the guarded monster state. Their private full manifests
and actual original query-order evidence have not been supplied to the checked
publication paths. Public metadata is not historical row proof; no rows or
fallback owners were invented. This increment adds no source data transformation.

## Original flow and concrete consumers

| Source file / symbol | Modern file / symbol and actual path |
|---|---|
| `TMap.cpp910/936::GetNeighbor(MAPTCELL)/EnterMAP(CTPlayer)`; `TCell.cpp61::EnterPlayer` | `native_player_cells.cpp::EnterNativeCellPlayer`; `PostgreSQLMapService::EnterNativePlayer`; actual both-role `handlers/session.cpp::OnConReadyReq` after durable Ready/party refresh |
| `TMap.cpp1176::OnMove(CTPlayer)`; `TCell.cpp::LeavePlayer/LeaveChannel` overloads | `MoveNativeCellPlayer`; actual native `handlers/movement.cpp::OnMoveReq`; existing MovementServers/MW_CHECKCONNECT precede the local cell turn |
| `TMap.cpp770::GetNeighbor(VPLAYER)`; `CSHandler.cpp439::OnCS_MOVE_REQ` | Main-only reverse neighbor/player iteration emits MOVE notices; replicas still move their observation cells |
| `TMap.cpp1089::LeaveMAP(CTPlayer)` | `LeaveNativeCellPlayer`; actual MapServer close before any final save/release suspension, including replica/failed-save paths |
| `SSHandler.cpp6823::OnMW_RELEASEMAIN_REQ`; `CSHandler.cpp402::OnCS_CONREADY_REQ` | `RebindNativeCellPlayer/RebindNativePlayer`; confirmed transfer promotion and old-primary demotion; CONREADY refreshes the same member with new-member FALSE |
| `CSSender.cpp395::SendCS_ENTER_ACK`; `TObjBase.cpp4962::GetWarCountry/CanFight`; `TMapType.h149::GetAttackCountry` | `native_cell_runtime.cpp::PublishNativeCellNotices/PlayerColor`; current full primary and actual observer country/aid/peace state, including source comment tint |
| `TObjBase.cpp7/2379::CTObjBase/OnDie`; `TPlayer.cpp3610::Revival`; `CSSender.cpp` ENTER movement fields | `SourceMovementState`; native MOVE updates pitch/key/mouse/action, native death sets TA_DEAD7, revival sets TA_STAND0; actual `EncodeEnterAck` consumes the fields |
| Source owned packet/send-buffer-full boundary; existing modern Asio transport | `AsioSession::PostPacket`; native cell events use existing strand/channel/drain with owned bodies and bounded non-suspending enqueue |
| `Client/TClient/CSHandler.cpp1090/1366` ENTER/LEAVE; source MOVE reader | Existing explicit little-endian writers; LEAVE is DWORD ID + BYTE exit-map; MOVE remains27 bytes. Client source is unchanged |

`NativeMonsterMap::players` owns player map affiliation, including an absent or
disabled origin cell. `NativeMapCell::players` owns the enabled-cell membership
index in source DWORD character order; no borrowed player/session pointers enter
the map. Entries carry user/key/connection generation/logical channel/role/epoch.
Same-map entry and all movement use the source logical-to-physical mapping and
the same graph already used by CHECKMAIN and routing. Party instance templates
still refuse entry because there is no live party-map lifetime producer.

Entry adds the enabled origin member before walking neighbors in z/x cell order.
The visibility tests intentionally differ: the newcomer sees a primary when
the **neighbor** cell is main for its logical channel or both channels match;
existing observers see a new primary when its **origin** cell is main for their
channel. A replica is an observer, never a fabricated full actor with zero bars.
Native entry now bypasses the generic whole-channel player/prototype-monster
flood. No native source monster was inserted into the development registry.

Movement preserves `WORD(FLOAT)/64`, enabled old/new origins and separate old/new
neighbor sets. Source OnMove narrows channel-vector size to BYTE and visits that
prefix; duplicate channels collapse only in the MAPBYTE lost-main set. Old-only
cells receive LeavePlayer(FALSE), then coordinates change, then the new origin
member is inserted; new-only cells receive EnterPlayer(FALSE), followed by exact
PC branches of both LeaveChannel overloads. Same-cell moves update the actual
member without manufacturing ENTER. Main-only MOVE then walks neighbor cells and
their DWORD players in reverse, as source GetNeighbor/pop_back does. Replicas
perform the visibility transition without duplicate MOVE actor broadcasts.

Movement's existing PostgreSQL owner/route check and World connection packet may
suspend before the local turn; full socket/role/epoch identity is checked after
each suspension. Graph, snapshot, presence, packet sampling and publication then
execute without coroutine suspension on the current single Map IO game executor.
No cell mutex is held during networking. The existing AI mutex protects graph
mutation against other service readers. This is not proof of atomicity against an
external database owner replacement between periodic authority checks.

The existing native revival path permits only same-cell relocation. After its
confirmed commit, `handlers/quest.cpp` synchronizes that actual position into
the same player-cell state without a manufactured MOVE_ACK, before existing
REVIVAL/HPMP outputs. Its cross-cell refusal has not been removed by this work.

Closing removes the member before a potentially slow or failed save. Source
LeaveMAP sends exit-map TRUE to neighbors; ordinary movement sends FALSE. Cleanup
matches the connection and never erases a newer authority tenure. If durable
handoff succeeded but RAM rebind failed, a close may remove the same socket's
older stored tenure; it cannot remove a different connection or a newer epoch.

Ownership change retains map/cell membership, as original RELEASEMAIN does.
Rebind requires the exact previous identity, same socket and a strictly newer
confirmed opposite role. It then applies source OnMove without manufacturing
CS_MOVE_ACK (that broadcast belongs to CS_MOVE_REQ). The existing durable modern
handoff publishes that change after commit/confirmation, rather than source
RELEASEMAIN's immediate pre-DB m_bMain change. This ordering difference remains
explicit. Promotion CONREADY reuses the member and refreshes EnterMAP(FALSE).
Entered observers continue to receive cell deltas during TransferOut/TransferIn
and promoted Loaded state; full actor descriptors still require Ready primary
state. Failure closes and cleans up rather than publishing uncertain ownership.

## Packet ownership, limits and intentional boundaries

`PostPacket` takes an owned body. It dispatches to the existing session strand;
the channel and drain still own framing, encryption, sequence and socket writes.
No native cell event awaits a full recipient queue while holding a half-applied
game turn. Dispatch also queues before a subsequent ordinary SendPacket when
the caller already runs on that same session strand. Bodies/snapshots are sampled
before any subsequent cell event can mutate the graph. There is no separate
visibility daemon, transport or unbounded coroutine-per-packet waiting list.

Posted envelopes and the existing send channel are independently bounded by the
configured per-session capacity (default256); the drain has at most one active
write. Oversize or exhausted capacity closes the peer, replacing successful
silent packet loss or suspended cell turns with explicit retirement. This is a
modern resource policy, not a claim that its exact capacity matches Win32 send
buffers. A true return means scheduled only; a later queue/write failure closes.
Close cancels the existing queue/drain and discards outstanding packets. Slow-peer
capacity, exact output ordering and shutdown still require Phase B acceptance.
Player affiliation is limited to65536 per physical map, with rejection rather
than truncation. Existing session/cell graph limits also remain in force.

Pitch/mouse/key/action are real connection state; TKDIR_N is4, not0. These fields
are not new DB columns or invented transfer DTO fields. Promotion retains the
receiving CTPlayer object's transient movement, as its other connection-local
state already does. New objects start with source constructor defaults. Dead
ENTER retains existing dead-action hardening. The native revival/death producers
now maintain the new action member, preventing a stale TA_DEAD after revival.
Other unsupported ENTER fields and their producers are not certified by this
movement/color increment.

Native nonzero `bGhost` is now explicitly refused/closed: source MoveGhost uses
separate ghost coordinates, host eligibility and monster events which are not
implemented here. Moving a corpse using ghost coordinates would corrupt this
new spatial state. This refusal is protective, not completed ghost movement.
Full MOVE still needs real CanHost/move clocks, speed/operator checks,
stand-hide cleanup, position quests and the remaining source actor branches.

The process-wide source-slot/job admission barrier is unchanged. PC-only cell
membership is a real path for already supported source-empty native processes;
it is not player/monster coexistence. No AT_ENTER/LEAVE to a populated monster,
ADDMON descriptor, maintained monster effect, host selection, host request,
awake/dead command, recall/companion/self, switch/gate or GodBall support is
claimed. The parent `MAP.MONSTER.CELLS.HOST` remains IMPLEMENTING. The inventory
now has300 unique traceability IDs/13 columns; this is not a completion metric.

## Deferred Phase B scenarios — not executed

1. Admit real primary and replica observers in same/adjacent/distant physical
   cells, two logical channels, primary/extended edges and absent/disabled cells.
   Compare exact original ENTER recipient order, new-member flags, real bars,
   maintained effects, aid-country tint and comment visibility. Distinguish
   neighbor-main from origin-main predicates and whole-map/channel flooding.
2. Move within a cell, across adjacent boundaries and across nonoverlapping
   neighborhoods. Compare DWORD/byte conversions, old-only LEAVE(FALSE), new-only
   ENTER(FALSE), lost-channel PC branches and reverse MOVE order. Include256+
   source channel rows/duplicates, channel51 refusal and invalid FLOAT casts.
3. Move a replica, then promote/demote it using a real durable handoff. Confirm
   no synthetic map exit; both affiliation and observer view survive. Check
   transfer-position relocation, same-member promotion refresh, current epoch,
   late old output and failure after DB commit but before RAM rebind.
4. Close before/after Ready, during handoff, on failed save and after retirement.
   Confirm one cell removal, exit-map TRUE, saved live position, same-socket
   older-epoch cleanup and rejection of stale/different-socket removal.
5. Enter near an already moving primary; compare actual pitch/key/mouse/action.
   Die/revive, then enter another observer to prove no stale dead action. Check
   real original-client decoding, appearance/effects and all existing packet
   fields; source/client profile compatibility remains unverified.
6. Saturate a recipient channel and posted-envelope capacity, close during a
   burst and mix native cell output with ordinary actor replies. Confirm bounded
   memory, owned buffers, output order, no sequence corruption and explicit
   retirement instead of dropped successful transitions or stalled game state.
7. Preserve populated-process actor refusal on claim/load/replica/transfer/
   confirmation/Ready paths. Prove the new PC graph never licenses empty-host
   AI with actors and never uses prototype monster descriptors. Check nonzero
   bGhost refusal and maintain the full movement/ghost backlog.

## Exact next implementation handoff

Extend these same cells and NativeMonsterSlot state with ordinary monster
visibility and complete `CSSender.cpp913::SendCS_ADDMON_ACK`, cross-checked against
`Client/TClient/CSHandler.cpp1771`. Include template level versus mutable level,
actual max/current HP/MP, pitch/directions/action/mode/country/region, source
GetColor and full maintained effects; do not reuse prototype zero-MP descriptors.
Then supply actual PC move/ghost/CanHost clocks, source `TAICmdSetHost::ExecAI`,
`TMonster::NotifyHost`, CS_MONHOST/host request consumers and the concrete awake/
dead commands those events can trigger. Remove the latched admission barrier only
when those actual producers and consumers support player/monster coexistence.
Continue non-fresh regen/death/loot, dynamic/battle/quest and party-instance
lifetime afterwards. Keep all Phase B/C execution deferred; next migration082.
