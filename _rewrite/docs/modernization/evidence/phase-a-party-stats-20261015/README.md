# Basic party statistic context and unsent departures — 2026-10-15

Baseline: local `main` at `b251cdf`. Status: **INTEGRATED_UNVERIFIED**.
This increment uses static source, API, SQL-text, packet and diff review only.
No build, test, service, client, container, SQL, migration, import, backup
extraction or runtime acceptance ran. Local Git only; no push or deployment.
Original server/client/library/protocol sources and all migrations are unchanged.
Applied001–043 and written/unapplied044–069 retain their previous status; next070.
Private extracts and pre-existing `_rewrite/docs/audits/` remain outside Git.

## Map: supported statistics while in a basic party

Original `Server/TMapSvr/TObjBase.cpp:1409/1558/1624/2011/2019` derives abilities,
item modifiers and HP/MP maxima from actual actor state. `TPlayer.cpp:5739/5760`
adds the actual companion contribution to CON/MEN. Raw party membership is not a
formula input. The old native `RefreshEquipment` blanket party guard could abort
otherwise supported equipment, item, skill, quest and progression transactions.

`Server/TMapSvrAsio/services/postgresql_map_load.cpp` now shares
`SupportedStatisticsContext` between derivation and equipment refresh. It checks
the supported maintained/posture graph, guild absence, actual recall rows and
transfer companion/companion-item/recall/recall-buff/local state. Inactive account
pet inventory is preserved. Refresh requires an existing supported sheet; requested
rederivation must produce a fresh sheet before publishing the copied payload.
Actual item/skill/level/class/race inputs and ownership/checkpoint transactions
remain authoritative. No fake party bonus, zeroed metadata or default statistics
are introduced. [Map implementation and call graph](map-stats.md),
[independent source analysis](analysis.md).

Soulmate `CheckSoulHP` (`TPlayer.cpp:4686`) and `CheckSoulEXP` (`4667`) are separate
cross-actor HP/XP producers and remain OPEN. This change does not implement party
combat, loot, reward distribution or arbitrary effect/companion simulation.

## World: retain only actual removals which were never sent

Source `Server/TWorldSvr/TWorldSvr.cpp:2986–3064` broadcasts actual departure to
the pre-removal member list. `SSSender.cpp:749` writes recipient DWORD, key DWORD,
departed member DWORD, chief DWORD, commander WORD, party WORD and kick BYTE.
Survivors receive the actual successor/party; the leaver receives chief/party0.
Two-person dissolution also emits a second, real self departure for the survivor.

Native `LeaveParty` now retains these exact event fields only when its pre-send
current-route check fails or an earlier real DEL is already pending. This is
limited to native corps0 parties and a still-registered, nonclosing/nonlogout
`TChar` with the same session key. The queue belongs to that live character object
and disappears with it. Native character keys are assigned on construction, not
reused for a replacement login. No guessed roster or SQL party table is created.
An event which reaches any send attempt is never retained or automatically retried.

`RefreshReadyParty` drains FIFO under `party_operations` only after the existing
native SQL Ready/owner/claim proof and current exact actor/key/typed/open primary
checks. A replica trigger additionally proves its exact ready-primary relationship.
The event is removed **before** its single write attempt; failure or unknown
delivery cannot replay a client DEL on the next probe. If a replica initiated the
drain, actual final ATTR is also sent to the primary and already-ready routes.
Primary recovery still publishes actual attributes to all already-ready routes;
an independent later replica probe updates that replica.

Original Map `SSHandler.cpp:7994` forwards client DEL with its previous cached
effective chief/commander/party, rather than installing the World DEL tail.
Native Map keeps that behavior. Pending DELs therefore drain before publishing
current registry ATTR. This delayed recovery has an explicit timing difference:
source chief succession sends ATTR before DEL, while a deferred DEL may expose
the old cached chief until the final current ATTR arrives. No source packet layout
or ordinary immediate succession order changes.

The original ENTERCHAR acknowledgement also tries this repair after an actual
World route-ready update. This covers a Map already SQL/local Ready whose duplicate
client CONREADY is silent; a merely loaded claim still fails SQL authority. After
that awaited repair, World rechecks exact character/current open peer, key,
closing/logout/handoff phase and reporting connection, then recomputes all-ready
before advancing normal connection coordination. No SQL Ready or CONRESULT is
fabricated or repeated by the repair.

Pending removals block new native invitations, joins, chief/type roster preflight
and explicit leave/kick actors/targets. Existing automatic departure/close may
append further real events in order. This prevents an old self/other DEL from
erasing newly appended JOIN rows in a later membership generation. The queue is
bounded by departures from the existing source party (max7): joining that roster
is refused while any member has pending removals. Final ATTR cannot overtake them.

This is process-local recovery of known unsent deltas. Full roster reconciliation,
uncertain attempted output, client application acknowledgement and process restart
retention remain OPEN. Original client `CSHandler.cpp:4937` appends JOIN without
deduplication, `4976` removes a member/self releases party, and `5021` ATTR does not
clear the roster. Blind JOIN replay or synthetic self DEL would corrupt it.

## Additional audit and deferred acceptance

The Map audit records signed SQL INT versus DWORD bind mismatches across native
account/key/character admission, replicas and persistence. It also records the
independent positive-only identity constraint in `012-native-characters.sql:24`.
No partial binding fix or schema change is claimed; preserving the full high-bit
identity domain needs a coherent source/schema-approved increment.

Phase B must compare otherwise identical solo/party actor sheets through real
equipment, posture, item, skill, quest and level mutations, including HP/MP clamp
and actual party status output. Unknown guild/local/companion/recall/effect graphs
must stay refused. Exercise unsent chief departure, ordered two-person dissolution,
multiple departures, rejected new membership, main and replica Ready triggers,
already-ready route reconciliation, route/key/owner/epoch replacement during waits,
and single-attempt failure without replay. Confirm final primary and replica cache
attributes with the original client. Later high-bit work needs explicit signed
DWORD boundary cases separate from genuine BIGINT connection/epoch values.

Phase A remains incomplete. Source Map admission/full bootstrap, complete roster,
guild/corps/arena/instances/BR/BOW, party XP/loot/combat and all other unported
responsibilities remain in scope. Full functionality and compatibility with the
original executable are unverified; historical test results do not cover this code.
