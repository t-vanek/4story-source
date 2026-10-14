# Party attributes and manual changes — Phase A, 2026-10-14

Baseline: `622fb2e`, local `main`. **INTEGRATED_UNVERIFIED**. This continuation
uses original source/client reads and static API, packet, SQL-text, dispatcher
and diff review. No build, test, SQL, executable, server, client, container,
importer, migration or runtime acceptance ran. Local commits only; no GitHub push
or deployment. Pre-existing private `_rewrite/docs/audits/` stays outside Git.
Applied001–043 and written044–069 are unchanged; next unused migration070.
Full Phase A and original-client/native PostgreSQL runtime acceptance remain OPEN.

## Connected behavior

- After actual SQL/local Ready, both primary and secondary Map sessions repeat
  the existing `MW_ENTERCHAR_ACK` body: DWORD character, DWORD key. The same helper
  covers the source which becomes a replica after handoff. It requires explicit
  native authority and current local identity; legacy/test adapters do not enter
  the native recovery path. No unknown replica bars or unsupported stat sheet
  becomes a MANSTAT, fake roster, success response or character graph.
- World's native completed-connection branch does not mark a connection ready
  again, reset CHECKMAIN requests or repeat CONRESULT. It serializes with party
  decisions, proves actual SQL-ready primary or exact ready replica, then sends
  actual registry ATTR. A primary refresh covers all already-ready routes; a
  secondary becoming ready later refreshes its own route. Either client Ready
  order is supported by the two producers. Source fields/opcode remain unchanged.
- Replica proof locks live World, sorted primary/target Map owners, account and
  primary claim, then the ready replica. It requires the exact primary token,
  connection and authority epoch relation, target token/connection, current
  unlocked login, canonical lossless name and native guildlessness. Both registered
  peers' Map tokens and current cache/route/object/key are rechecked after SQL.
  The runtime grant/probe is written only; World does not grant or change replicas.
- The earlier post-ready MANSTAT probe and pending refresh flag are removed.
  Ordinary actual HPMP/level MANSTAT remains. A stale supplied party ID can repair
  ATTR before status is accepted only for the actual effective party. Missing
  statistics remain a genuine gameplay/roster dependency, but no longer prevent
  a truthful read-only Ready membership refresh.
- CHECKMAIN's handoff winner waits for the party permit before changing main or
  handoff phase and repeats current-peer/object checks after the wait. It releases
  the permit before release/connection writes and PopConCess, whose cleanup can
  acquire it. Handoff therefore cannot start halfway through a join/chief/type
  fan-out. A previously active handoff cannot pass native roster preflight.
- Map releases the admission projection lease after committed Ready and presence
  binding, before the independent refresh and entry flood. Own ENTER packets use
  a fresh live snapshot under a short gameplay operation per addressed neighbor;
  pending party projections cannot be overwritten by the old admission snapshot.
  The whole entry flood does not hold the party projection lease.
- Real CS dispatch now reaches manual chief and obtain-type changes. World
  validates the incoming current actor and all native party members before
  mutation, under the existing party permit. Native ordinary parties require
  actual non-corps/non-arena registry state; dependent modes remain OPEN.
- CHIEF preserves source error order: missing target3, missing party9,
  non-chief8, different party3, self6; success is10, followed by ordered ATTR
  to actual members/ready replicas. Map forwards the original result byte and
  leaves chief cache changes to ATTR.
- TYPE preserves source missing-party silence and non-chief8/requested-type
  refusal. Success mutates the real obtain policy and broadcasts result0/type
  to member primaries. Map updates raw type only on success and sends the original
  two-byte client reply. Source does not send an extra ATTR here. Source BR drops
  CHIEF requests, while TYPE has no BR request drop; native full mode integration
  remains a separate requirement.

## Source contracts and explicit differences

[analysis.md](analysis.md) and [map-recovery.md](map-recovery.md) contain detailed
source mapping and static findings. Source authorities include Map
`SSSender.cpp:730–741`, `CSHandler.cpp:402–415,3377–3417`, World
`SSHandler.cpp:1038–1092,1095–1185,2334–2482`, Map
`SSHandler.cpp:7808–7836,10392–10408`, and client
`CSHandler.cpp:93–166,238–286,4937–5054,5086–5130,5193`.

The extra post-ready entered-identity repetition is an explicit **modernization
extension**, not an original source Ready ACK or registration ACK. SQL/local
Ready is proved independently; the original entered body is truthful and carries
no new field/opcode/type or placeholder. An early duplicate without ready SQL
authority cannot advance the state machine. The legacy duplicate handler retains
its previous INVALIDCHAR behavior. Native SQL/owner failures do not fall back to
an adapter or replay writes on a replacement socket.

Native full-body/key/enum0..5/current-generation/roster checks reject malformed or
stale operations that original source trusted. All-member preflight may refuse a
request during another member's handoff rather than publishing an incomplete
native change. This is distinct from source's synchronous batch assumption.

Source TYPE does not immediately refresh client party/chief/commander fields,
including SOLO changes; the client TYPE handler only changes its obtain policy
and `GetPartyID` returns the stored character field. That source timing is
preserved. A later actual ATTR uses source SOLO masking. This does not complete
SOLO loot/XP/instance gameplay or eliminate the source UI quirk.

## Open responsibilities

Full roster reconciliation remains OPEN. Ordinary client CONNECT retains its
roster; CHARINFO clears it but neither the actual sender nor parser includes a
member-list block. The protocol-header comment claiming such a block conflicts
with both implementations. JOIN unconditionally appends, and self DEL invokes
ReleaseParty. A blind roster replay or invented self-leave would therefore be
unsafe. Leave/logout during another actor's active handoff can still lose a roster
delta; ATTR repairs metadata, not the member list.

Completed ATTR writes do not prove Map/client application before gameplay, and
AOI/neighbor delivery remains subject to current recipient/claim availability.
Whole visibility/movement/ordering and post-ready acceptance must still be tested.
Identical live-generation invitations remain indistinguishable without a source
nonce. Generic high-bit claim limitations and complete Windows/ANSI name lookup
profiles remain OPEN.

Source Map admission/full native World bootstrap, connected Relay chief notices,
corps/general/squad/enemy updates, guild-bearing actors, arenas, dynamic INDUN exit,
loot distribution/party XP/equipment/stat sharing, full BOW/BR modes, native AI/
REVIVAL/combat and all other original responsibilities remain required. No new
party table or fabricated missing registry was introduced, and previous build/test
results do not validate this continuation.

## Deferred Phase B acceptance

After the implementation/execution gate, verify actual unchanged-client manual
chief/type requests and all source failure precedence/result widths; chief result10
before ATTR; all six obtain policies and source SOLO client-field timing; exact
current actor/recipient/owner/key/epoch and same-transaction replica relations;
primary-first and replica-first Ready, late new replicas and retained source
demotion; repeats before/after SQL Ready, stale probes and role replacement;
unsupported stat sheets without fake bars; fresh own ENTER versus concurrent ATTR;
checkpoint/admission/close/handoff and party decision races; no nested-gate deadlock,
connection-verdict replay or write retries; and eventual source/client metadata
application. Independently close the full roster recovery, bootstrap, mode and
complete-gameplay requirements above. None of these executions occurred here.
