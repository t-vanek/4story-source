# Source Map monster AI bootstrap — Phase A, unverified

Baseline: local `main`, `d61a3ae`. Full Phase A and the monster lifecycle remain
**IMPLEMENTING**. The catalog, source conditions, direct initialization and
fresh Regen preparation below are **INTEGRATED_UNVERIFIED** within their stated
scope. They do not complete live monster realization.080/publisher/grants are
written only;044–080 remain unapplied, next081. Original sources, archives,
applied migrations, private extracts and pre-existing audit output are unchanged.

Native construction now executes actual source `Initialize` instead of rejecting
every initializer. **It still refuses a required EnterMAP/cell/visibility effect
or delayed AI publication before opening the listener.** A graph that actually
has no matching initialization commands follows the source empty-query branch;
missing AI/catalog data does not become successful empty state. No anonymous or
prepared slot is inserted into the prototype live registry. Populated native
gameplay, client compatibility and full server implementation remain unfinished.

## Authority and source profile

Local Git confirms original Map/client lineage
`c77f79017d7940c1500ba36a5dbf8bda6fefb67c`. Source TVERSION0x2918 and the commit's
version label are not proof of an executable/assets/region profile. This slice
uses the ordinary, non-BOW source Map with SKYGARDEN. The client is unchanged.

The actual archives were hashed again:

| Archive | SHA256 |
|---|---|
| TGAME_RAGEZONE.bak | `2cff1635d523322d3de8ddafaeacc3335581dc338dca901239ac80288f51d259` |
| TGLOBAL_RAGEZONE.bak | `fca545bcb90bcc72aa4f3514a6177c2b88d2b0bf71bf847b0128f090a20877fb` |

Critical unchanged reference file hashes:

| File | SHA256 |
|---|---|
| Server/TMapSvr/TAICommand.cpp | `f1ec94018b27d0a90c12fa039a32fcd14fab4c895a16d6b9166fcbe8dd4d226c` |
| Server/TMapSvr/TAICmdRegen.cpp | `8fe1aa3492bcc34daccf93000b94cf1e4967d0ef10ec5f5d7ca4c8b29e41a965` |
| Server/TMapSvr/TObjBase.cpp | `9c6b97b0616605b9ee50e169cd1a93a45cf34e2be178663fc58005ba17764181` |
| Server/TMapSvr/TAICmdSetHost.cpp | `ae44f3cbf57f0030b5bb83b88aeb5445bb8178d4f55ff5c649f0e6ae5f80ef89` |
| Server/TMapSvr/TAICmdRemove.cpp | `ad6d1a83dc6b6db3789599bcd7280b51bd7d5b3b38174b92557fd33785b665cc` |

No private source rows or original query-order captures were consumed. Their
absence from checked data paths is not replaced with synthetic metadata. The
existing public profile/index is navigation, not a successful new publication
or evidence of original client behavior.

## Producers and PostgreSQL

`Server/TMapSvr/DBAccess.h::CTBLAICommand/CTBLAICondition/CTBLAIChart` and
`TMapSvr.cpp2405–2481` produce command templates, per-command condition vectors,
and ordered AI-type/trigger/trigger-ID binding vectors. Each binding has its own
delay and BYTE loop flag; duplicate command references across triggers are not
collapsed into one executor. Unknown command kinds take the original
`CreateCMD` base-command branch. Invalid trigger-array indexes, missing command
templates and duplicate command identities refuse instead of copying unsafe
legacy pointer access.

080 adds a world/server-scoped AI publication tied to the exact079 full import,
manifest and allocation-order SHA. It has three complete source receipts and
independent binding-query/per-command condition-query orders. Raw archive rows
remain unchanged, including conditions whose command template does not exist;
those are not selected by original per-command loading. Runtime counts compare
the actual source-selected condition population and complete command/binding
tables with the visible projection. All integer widths and signed SQL INT to
source DWORD bit conversions are explicit.

`import_map_monster_ai.py` requires the complete verified pinned historical
manifest, current079 and current content/character dependencies. Under offline
Map/World barriers and stable locks it freshly compares values, original byte
sidecars and exact contiguous archival ordinals for all three AI and seven
allocation source tables, plus seven active content/character equalities. It
then publishes all orders and receipts in one transaction. Existing publication,
partial/wrong-run order input, invalid references or a hidden/incomplete runtime
view refuse. Unknown COMMIT is never retried. No operational monster HP, AI tick
or pending job is invented as a database row. Map grants are SELECT-only.

Private `--source-order` JSON must have exactly `source_manifest_sha256`,
`server_id`, `bindings_query` and `condition_queries`. The first list is the full
original CTBLAIChart result expressed as TAICHART archival ordinals. The dictionary
has every command's canonical unsigned decimal DWORD key, including commands
with an actual zero-row query, with that CTBLAICondition result as TAICONCHART
archival ordinals. Command-load ordering is immaterial after unique keys because
the source subsequently iterates its DWORD map. Original condition/binding order
must be separately evidenced; a supplied permutation alone is not proof.

## Actual native consumers

`PostgreSQLMapService` now loads080 after079, before battle initialization and
source slot allocation, inside the existing owner-fenced repeatable-read
constructor transaction. It owns the immutable AI catalog. Battle/cache release
checks also pin the AI order SHA. The existing CMake core target includes both
new translation units. No new service, language, runtime or parallel world is
introduced; legacy/dev prototype population and AI remain excluded from native
ownership.

`native_monster_ai.cpp::NativeMonsterBootstrap` is deliberately restricted to
original **LoadData before threads/network/players**:

- `TMonster.cpp::Initialize`: choose an actual own-template AI if it exists,
  otherwise DEFAULT_AI1; absence of the fallback is not an empty graph. Capture
  AT_DELETE6/trigger0's original command vector once. Run ordered CanRun/Exec
  directly, ignoring initial delay and returned repeat flag. Changing template
  during Regen does not switch this captured initialization vector.
- `TAICommand.cpp::CheckCondition/CanRun`: ordered short-circuit probability and
  mode checks. Probability always draws even for0 or an over100 threshold.
  AN_CHGHOST2 has no return in the original and actually passes, as do unknown
  condition kinds. Native preserves that behavior. REMOVE bypasses conditions.
- Synchronous `OnEvent/DoAICMD`: resolve the current object's AI, calculate delay
  **before CanRun**, retain the source remove-template/LEAVE override1, and
  ignore the immediate command's repeat result. AICOMPLETE0 is dispatched before
  the base command tests loop and recalculates delay. ROAM delay consumes its
  original15-bit modulo draw and uses DWORD multiplication/addition wrap.
- `TAICmdSetHost`: with the proven empty pre-network actor population, the source
  no-host branch increments the DWORD host key, resets host state and returns
  false without completion. It does not set WAKEUP. All other known command
  types first require WAKEUP or DEAD, so fresh DISAPPEAR objects follow their
  actual early false predicate without evaluating later conditions. Eligible
  awake/dead behavior is explicitly unsupported, not a fake executor.
- `TAICmdRemove`: for an unentered DEFAULT boot slot, no players/cell membership
  exist; remove its actual map entry and spawn-vector element. Keep the wrapper
  and any stale leader ID as source does. Dynamic/entered removal is not claimed.

The source static module's tick is0 during LoadData: `TMapSvr.cpp` declares the
static module, calls LoadData before CreateThreads, and only AIThread assigns
GetTickCount. Thus fresh ResetHost/ChgMode's last-attack observation is0 here,
not a guessed runtime timestamp. Uninitialized source tick/leave counters remain
optional until written. This class is not the later runtime AI worker.

## Fresh Regen preparation

`TAICmdRegen.cpp::ExecAI` now has an actual native calculation consumer:

1. On DEFAULT group selection, consume rand before narrowing the modulo result
   to BYTE. Redirect to the selected existing wrapper's **last current slot**,
   without a second CanRun/condition/host-key check. The allocation callback
   remains synchronous between insertions, so later slots do not exist yet.
2. Roll that target's spawn probability. Failure completes through the target's
   current AI and base loop/delay behavior. Success does not complete the base
   command. An anonymous success draws once before summing nonessential DWORD
   weights, including zero-total cases, then walks the original candidate order.
3. Assign the actual selected monster and class/race/level references. Missing
   attributes preserve the selected template assignment and return false before
   setting level/skills. Existing essential slots keep their actual template and
   attribute. Missing templates are never template0 or fabricated stats.
4. Preserve radius/angle draw order, FLOAT pi/cos/sin operations, source origin
   versus an awake leader's next position, path inheritance, spawn Y and WORD
   direction. The no-range/no-leader branch assigns the original coordinate
   directly, including signed zero. Size is actual template size/2.
5. Apply the actual source keeper/path/attack-wait/mode/flags/leave-count/money/
   remove/next-skill resets, valid occupation-country override and fresh ResetHost.
   The raw079 template projection now includes real AI/size/flags; attributes
   contain actual family/level HP/MP values instead of mere existence.
6. `TMonster::GetMaxHP/GetMaxMP` and `TObjBase::CalcAbilityValue` still cast the
   raw DWORD through Win32 signed int even with empty fresh skills/effects.
   Native explicitly interprets those bits, clamps negatives to0 and then0 to1.
   This includes raw0x80000000..0xFFFFFFFF; it is not simply max(raw,1).
7. Require actual EnterMAP/cell/visibility publication, then AT_ENTER7. The
   production consumer currently throws at that boundary. No live slot,
   completion ACK or client packet is fabricated. A delayed invocation similarly
   carries its stable source binding identity, source host key, object, physical
   channel, map/party and event arguments to an explicit unported queue boundary.

Items/effects/aggro are empty because only the fresh pre-actor constructor can
invoke this path. Re-entered objects, existing hosts or regen-delete death effects
are explicitly outside this scope. Full Regen lifecycle is **IMPLEMENTING**.

## Safety differences, missing work and verification

Existing independent `LegacyRandom15` draws preserve the Windows15-bit range and
modulo bias, not the original global CRT sequence. Event recursion256 and startup
work1,000,000 are explicit resource limits that refuse before listening; they
are not new gameplay rules. ROAM modulo-zero and invalid pointer/data branches
refuse. If Remove deletes the currently executing object, stop its command loop
and avoid the source's later use-after-free; if a prior failed attribute selection
would later dereference NULL, refuse. These source unsafe branches are not client
compatibility requirements. No source file or historical row is rewritten.

Still missing: actual cell/main/ext ownership and registration, monster visibility
and full ADDMON/host packets, client CanHost and hosted movement/combat, the AI
request/timer/batch ACK queue and cancellation/repeat handling, awake/dead command
effects, full death/loot/respawn and local/quest/dynamic/instance producers. Battle
enable effects, switches/GodBall, occupation/rewards, guild/hero mutations and
original full bootstrap remain open. Guarded callbacks must remain until their
actual producers and consumers exist. Full Phase A is not complete.

Only static source/API/SQL-text/CMake/diff review, AST/CSV/JSON parsing and actual
file hashing were performed. **No build, test, SQL, migration, publisher/import,
service, client, container or runtime verification ran.** No push/deployment.
Historical results are not inherited. Implementation and verification have
separate fields in the existing unified inventory/database map.

Deferred Phase B scenarios: actual order captures and wrong-run/partial/duplicate
profiles; fallback versus real AI0; vector capture across template change;
condition short-circuit/RNG counts and AN_CHGHOST; direct versus event delay/loop
order; group BYTE narrowing/redirection to the current last slot; essential-only
and zero-weight candidates; missing template/attribute and later refusal; FLOAT
radius/angle/leader/signed-zero cases; HP/MP0,1,0x7FFFFFFF,0x80000000,0xFFFFFFFF;
empty-host key/status semantics; Remove during a command vector; recursion and
work limits; exact failure before listener at EnterMAP or delayed publication.
These scenarios have not been executed or claimed as passed.

## Exact next implementation point

Replace the actual EnterMAP consumer in `AllocateNativeMonsters` only after
porting `TMap.cpp::InitMap/InitCell/InitExtCell`, original CSPGetServerID ownership,
EnterMAP and `TCell::AddMonster/EnterMonster/EnterPlayer` with actual source
physical/logical channel/main/ext distinctions and native host/visibility state.
EnterMAP must deliver AT_ENTER exactly once; the bootstrap currently owns that
event after the cell-publication callback. Preserve original ownership/neighbor
packet predicates, including absent cells, instead of using the flat prototype
registry/flood as the producer.

Replace the schedule consumer with the real `DoAICMD → OnSM_AICMD_REQ → OnTimer →
OnSM_AICMD_ACK` chain: source physical map/party resolution, delayed CanRun,
host-key and object lifetime fences, repeat rescheduling only after true Exec,
bounded owned queue, cancellation and distinct AI-timer/batch delivery boundaries.
Fence reused source instance IDs with an object generation as well as the source
DWORD host key; a stale job must not address a replacement allocation.
The source OnTimer erase-invalidated iterator must not be copied. Extend beyond
pre-actor bootstrap only when host/actor state and the concrete command effects
exist; never keep using a proven-empty-host branch after admitting players.
Next free migration081; keep Phase B/C execution deferred.
