# Source Map AI request, timer and ACK — Phase A, unverified

Baseline: local `main`, `8bc9008`. The ordinary-map, actor-free AI request /
timer / batch / ACK path is **INTEGRATED_UNVERIFIED**. Full monster gameplay,
hosted actors, party instances, battle/bootstrap and full Phase A remain
**IMPLEMENTING**. No `IMPLEMENTATION_COMPLETE_UNVERIFIED` claim. No new migration:
044–081 remain written/unapplied, next082. No build, test, SQL, publisher, import,
service, client, container, push or deployment was run.

## Source authority and preservation

Original server/client lineage remains `c77f79017d7940c1500ba36a5dbf8bda6fefb67c`.
TVERSION0x2918 and the Git version label do not establish executable/assets/region
compatibility. This slice follows the ordinary non-BOW Map source; original-client
compatibility remains **UNVERIFIED**. Original trees, headers, migrations and
historical snapshot data are unchanged. No private rows/order captures were
consumed; existing private-data publication prerequisites remain open. Local
audit output stays outside Git.

Actual backups were rehashed in this increment:

| Archive | SHA256 |
|---|---|
| TGAME_RAGEZONE.bak | `2cff1635d523322d3de8ddafaeacc3335581dc338dca901239ac80288f51d259` |
| TGLOBAL_RAGEZONE.bak | `fca545bcb90bcc72aa4f3514a6177c2b88d2b0bf71bf847b0128f090a20877fb` |

## Implemented source flow and actual integration

| Source file / symbol | Modern file / symbol and consumer |
|---|---|
| `TMonster.cpp447::OnEvent`; `TMapSvr.cpp6558::DoAICMD` | `native_monster_ai.cpp::NativeMonsterAiExecutor::Event/DoCommand`; actual constructor Initialize and ACK completion/repeat producers |
| `SSSender.cpp6::SendSM_AICMD_REQ`; `TMapSvr.cpp6271::SayToAI`; `SSHandler.cpp564::OnSM_AICMD_REQ` | `NativeMonsterAiQueue::Publish/Turn`; actual factory Schedule callback, FIFO requests and one timer insertion per AI turn |
| `TMapSvr.cpp5113::AIThread/6055::OnTimer` (AIBUF branch); `TMapType.h32::MAP_TIMER` | `native_monster_ai_queue.cpp::Turn`; `main.cpp` native AI coroutine, measured monotonic DWORD samples and100ms idle timer |
| `SSSender.cpp34::SendSM_AICMD_ACK`; `SSHandler.cpp1159::OnSM_AICMD_ACK`; `TChannel.cpp::FindTMap` | Separate owned batch FIFO / posted IO delivery; `PostgreSQLMapService::DispatchNativeMonsterAi`; `NativeMonsterAiExecutor::Acknowledge` |
| `TAICommand.cpp::ExecAI/GetDelay/CanRun` | Existing executor extended in place; AICOMPLETE before loop/GetDelay, then original DoAICMD repeat on the original monster |
| `TMap.cpp554::InitMonster`; `TAICmdRemove.cpp::ExecAI` | Generation on every ordinary/essential/WAIT allocation; exact-lifetime queue cancellation before cell/vector/slot destruction |
| `TMonster.cpp2392::ResetHost`; `TObjBase.cpp295::ChgMode` | Explicit executor clock: source LoadData0, runtime AI DWORD tick; actor-free ResetHost updates actual last-attack tick |

`NativeMonsterBootstrap` becomes `NativeMonsterAiExecutor`; it is the same
implementation extended to ACK execution, not a parallel interpreter. Catalog080
now also indexes the actual binding by source row identity. The producer captures
that binding independently of later monster/AI-template changes, host key, event
host, real host/type, delay, monster ID and owning map's physical channel/map/party.
The map metadata is populated by actual cell/default-spawn producers. Physical
channel is not inferred from a possibly mutable monster logical channel.

GetDelay still runs before CanRun. Positive delay publishes without evaluating
conditions. Zero delay executes synchronously and ignores the ExecAI repeat
return as the source does. LEAVE's template-remove override and ROAM's DWORD /
random delay remain intact. Initialize still calls CanRun/Exec directly, ignoring
delays and repeat results.

The existing `LegacyRandom15` retains15-bit draw bounds, modulo bias and source
draw order, but uses the existing native random provider rather than the original
shared Windows CRT sequence. That carried-over deviation and thread-interleaving
differences remain explicit; no exact random-sequence compatibility is claimed.

AIThread establishes its first clock sample before accepting the first queued
request. Each turn advances only already admitted timers, then admits exactly one
request at elapsed0. Elapsed subtraction and accumulation wrap as source DWORD;
they are not saturating deadlines. Due timers enter batch in original timer
insertion order; stable compaction replaces the source invalidated vector
iterator. Timer expiry does not execute gameplay inline. The existing IO executor
gets a separate posted batch turn; at most one ACK executes per turn. Immediate
request/batch work yields and continues; otherwise the loop waits MAP_TIMER100ms.
This is a deterministic safe interleaving of the two original workers, not proof
of the legacy threads' timing or random interleaving.

ACK resolves the physical map, preserving the original ordinary-map behavior
that ignores party; instance templates currently have no live party instance and
follow the source missing-map return. It then resolves the monster, checks exact
object generation and host key **before** any condition/RNG evaluation, and uses
the captured binding identity. CanRun and ExecAI run at delivery time. Only a true
ExecAI result re-enters DoAICMD. Completion's GetDelay and rescheduling's GetDelay
remain separate calls; a new invocation captures the current host key. Generation
is checked again after execution to avoid rescheduling a removed/replaced object.
Regen redirection does not retarget the ACK's outer repeat to the redirected slot.

## Lifetime, authority and limits

The existing PostgreSQL Map service owns all queues and monster state. Queue
records contain values, never a raw command address or a borrowed packet span.
Generation is a nonzero process-local uint64 assigned before synchronous
Initialize, including WAIT allocations; exhaustion refuses rather than wrapping.
It is distinct from the original reused DWORD monster ID and is not added to
client packets, transfers or SQL.

Remove cancels requests, active timers and batch ACKs for the same physical map,
party, monster ID and generation before erasing the slot. A dequeued ACK remains
an owned value, protected by generation checks and safe post-execution lookup.
Host-key changes retain the source stale-job behavior: those jobs reach ACK and
are discarded without consuming condition RNG. The queue admits at most65536
waiting jobs across all three stages, plus the one executing ACK. There is no
silent drop on overflow. Work/recursion limits remain per initialization/batch
execution. All these resource/lifetime protections are safety deviations, not
new gameplay rules.

A service mutex serializes mutable queue/slot turns. The single IO executor
executes gameplay; DB workers only read current owner/catalog authority using a
separate lease. Native owner and source release checks precede active AI turns.
After the posted boundary, explicit stop/retirement is checked again. Immutable
cell topology and source catalogs remain readable by existing DB workers;
runtime membership changes do not mutate the outer map/cell indexes.

SIGINT/SIGTERM, Control-owner failure, Map-owner loss, registered World-link loss,
unsupported World battle mutation and normal IO exit retire the queue. The main
loop also cancels its owned timer on the relevant shutdown/link paths. Exceptions
in authority, timer transfer, ACK execution or resource limits retire the queue
and stop the listener, close sessions and drain existing World/background/control/
session operations before stopping IO. The authority offload participates in the
existing background-operation count even on exceptions. No delayed callback can
publish after retirement. There is no replay across process or release changes.

Authority checks are read snapshots before the local turn, not an atomic durable
gameplay transaction across external owner replacement. This slice has no player
output or gameplay writes. Extending it to actors/persistence must use their real
existing authority/transaction contracts; owner-race and shutdown acceptance are
deferred. No DB job persistence or hidden SQL Server dependency is introduced.

## Explicit actor isolation and remaining implementation gaps

The former throwing Schedule callback now publishes to a real queue. Surviving
entered monsters no longer cause the former constructor-only visibility refusal.
Instead, source slots (including WAIT/invisible ones) or pending source jobs at
completed boot latch
a **process-wide player-admission barrier before sockets exist**. It remains set
after all monsters/jobs disappear; a temporary empty state cannot authorize the
bootstrap's empty-host assumptions after players enter.

`RequireNativeActorAdmission` guards ClaimSession, LoadAuthorized, LoadReplica,
AcceptTransferOrConfirm (including unknown-commit confirmation), and initial
MarkReady, before their first service database operation or return of a
publishable actor. The actual native client, replica and transfer handlers
consume these methods before actor publication. Existing handoff connection-phase
bookkeeping/unbinding may precede a service call; it does not admit a new actor.
Retired AI also refuses admission until restart. This is deliberately protective
refusal, **not implemented player visibility or hosted monster gameplay**. The
transport can exist, but a source-populated process cannot admit game characters.

The executor may therefore use the actual empty-player SetHost/EnterMonster/
LeaveMonster branches after LoadData, with a real runtime clock. It cannot fake
a missing host by supplying an empty list after actors exist. No source slot is
inserted into the prototype live registry; generic RunMonsterAi remains disabled
under native ownership. Populated inventory/effects, awake/dead command effects,
non-fresh Regen, dynamic spawn deletion, party-instance creation, player cell
lifetime, complete ADDMON/host requests and the non-AIBUF portions of original
OnTimer remain incomplete. Required unsupported behavior still fails explicitly.
Existing one-second durable character timers are not replaced or duplicated by
this AI-only timer path.

## Deferred acceptance scenarios — not executed

1. Compare captured original trigger vectors: direct Initialize versus zero /
   positive delay; GetDelay-before-CanRun; failed predicates only at ACK; original
   binding retained across a template/AI change; event/real-host arguments.
2. Queue several startup requests, compare first sampling and one admission per
   AI turn. Existing timers advance before the newly admitted request. Check
   exact due boundaries, equal-tick turns, DWORD wrap/accumulation and stable
   batch order when several timers expire together.
3. Change host key while jobs wait, remove/recreate the same source DWORD ID with
   another generation, and reuse it after map/party changes. Stale ACKs must not
   evaluate conditions or consume RNG. Exercise cancellation in all three stages.
4. Compare AICOMPLETE nested events, self-removal, changed host key, false CanRun,
   false/true ExecAI, loop0/nonzero, two distinct GetDelay draws and a second zero
   delay. Regen redirect completion must preserve the original outer repeat.
5. Resolve correct physical versus logical channels, absent maps/monsters,
   ordinary map with a party argument, and missing real instance maps. Never
   fabricate a party0 instance. Validate generation exhaustion and queue capacity.
6. Attempt native primary/replica/transfer/confirmation/ready admission with WAIT,
   invisible, entered and subsequently removed runtime slots. The latched barrier
   must reject before service DB changes or new actor publication; an empty
   source process at completed boot must
   retain its existing bounded native own-PC paths.
7. Delay the authority offload, disconnect World, retire source battle authority,
   replace the Map owner, signal shutdown and force a queue/executor exception.
   Verify background-count release, cancellation between posted stages, no later
   ACK, durable session drain and no reactivation on reconnect.
8. Compare LoadData clock0 and measured runtime ResetHost/ChgMode timestamps;
   prove no fixed0 runtime or saved/offline tick import. Actual scheduling,
   performance, RNG interleaving and client behavior require Phase B/C evidence.

## Next implementation boundary

Only static source/interface/lifetime/diff and inventory/CMake metadata review
was performed. Original source/backup hashes and actual call sites were read;
no executable/compiler/test/SQL validation certifies the new C++ paths.
Inventory metadata parses as295 unique IDs ×13 fields and73 database mappings
×6 fields. Both new CMake sources are registered once; no CMake invocation ran.

Port real player cell entry/exit and ordinary-monster visibility through source
`TMap::EnterMAP/LeaveMAP/OnMove`, `TCell::EnterPlayer/EnterMonster/LeavePlayer`,
`CSSender::SendCS_ADDMON_ACK`, `CanHost/NotifyHost` and the matching client-host
request consumers. Extend the existing monster template/attribute state and
concrete awake/dead commands those events need; serialize actual source fields,
including HP/MP/effects, rather than prototype defaults. Replace the latched
admission barrier only with that complete producer/consumer flow. Then continue
full death/loot/respawn, dynamic/quest/battle and party-instance lifecycle.
