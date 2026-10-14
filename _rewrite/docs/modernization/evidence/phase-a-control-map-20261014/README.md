# Direct Control–Map source transport and administration — 2026-10-14

Baseline: local `main`, `0b97c18`. Status: **INTEGRATED_UNVERIFIED** for the bounded
five-operation path. Full Phase A implementation is incomplete. Source/static
review only: no build, test, server, client, container, SQL, migration, importer,
extraction or runtime verification was performed. No new migration is required:
written chain001–069 remains unapplied044–069; next070. Original server/client/
protocol libraries/backups and migrations001–069 are unchanged. The pre-existing
untracked audits directory is excluded. Local changes/commits only, no push.

[Source audit and conflicts](analysis.md) and [Map implementation review](map-control.md)
record precise references, source lifecycle dependencies and deferred acceptance.

## Connected production paths

| Source responsibility | Original source | Actual modern implementation |
| --- | --- | --- |
| Direct Control connection | Map TMapSvr1017–1034/1754–1762; shared Packet36–43, Packet177–184; Session68–84/309–374/454–474; Control TServer senders | Existing MapServer accepts a configured IPv4/CIDR as a single Control role before CS World/admission/crypto gates; Map-local Plain16 session; Control PeerDialer selects SourcePlain16 for Map4 |
| Empty Control handshake | Map CSHandler230–233 | Required first empty CT_CTRLSVR_REQ, no invented ACK; current Control socket checked before effects |
| Global announcement | Control Handler708–761; Map CSHandler72–88; CSSender2621–2640; message6/BuildNetString | Existing GMLevel1 Control Relay preference/Map fallback → CT_ANNOUNCEMENT_ACK → ready-primary groupWORLD3/sender0/raw059 caption6/full NetString → source client CS_CHAT_ACK |
| Character-name kick | Control Handler764ff; Map CSHandler92–109; TMapSvr InitMap8133–8139/close lifecycle | Existing GMLevel2 Control request → each current Map → exact ready primary/replica name/claim/native SQL authority → captured client Close → ordinary fenced save/release/World close |
| Service monitor | Control Handler250–336, TControlType CTRL_TIMER1000; Map CSHandler27–67/CSSender8–24 | Real periodic CT_SERVICEMONITOR_ACK(DWORD tick) → Map four-DWORD CT_SERVICEMONITOR_REQ(tick,sessions,players,active users) → Control current status/GUI; no response echo loop |
| Active-user data clear | Control Handler1243–1266; Map CSHandler214–227 | Existing logged-in Control empty DATACLEAR → source historical account set rebuilt from accepted players of both roles; not a gameplay cache flush |

Both new Map transport files are included in its existing CMake target. The
native owner check and kick queries extend the existing PostgreSQLMapService.
No replacement daemon, client protocol change or new ownership writer is added.

## Framing correction and explicit retained gaps

The linked original CPacket header is16 bytes: WORD size, WORD opcode, DWORD
sequence and INT64 checksum. Clear() zeros metadata. SESSION_SERVER/!UseCrypt
transmits the raw packet without calculating checksum or ciphering it. Original
Map's configured Control IP selects this role; other accepted clients use crypto.
A plain `tnetlib::AsioSession` Server role is insufficient: that modern class
still transforms headers/body and validates sequence/checksum.

This increment implements that source Plain16 Control–Map profile, total<65535,
zero sequence/checksum, no RC4/XOR. The zero-metadata rejection, first-handshake
requirement, five-second handshake deadline, bounded256-frame/4MiB serialized
writes and strict current socket gates are explicit hardening. Configured
`security.control_ip_allowlist` defaults empty/denied; it is transport/network
trust, not identity proof in the empty source handshake. An opcode cannot promote
an ordinary game socket. Known unsupported commands close the Control connection.

Earlier modern SS8 checksum/header claims are **not original framing evidence**.
The retained modern8 Control/operator/other-peer endpoints and current native
Map–World SS8 envelope require their own original16-byte source port. This new
source finding supersedes the old chat evidence's phrase “original eight-byte SS
header”; its measured native bounds remain historical facts, not source parity.
Existing client CS16 cipher/body contracts are not modified here. Full service/
operator framing compatibility remains required OPEN implementation.

Map Control TLS is unimplemented. Explicit TLS settings fail closed in Map config
and Control's Map dialer; requested TLS is never silently downgraded. Other modern
TLS endpoints retain their existing profile. The unchanged original controller/
client binaries are still absent, so neither profile has executable acceptance.

## Native ownership, counters and teardown

- Every Map Control command checks native Map ownership using a separate pool
  lease/BeginMapTransaction, then rechecks its exact current Control socket.
  The dedicated ownership heartbeat session is not used concurrently. Owner
  loss stops acceptance/Control, drains tracked operations and client persistence,
  and stops without applying the command or replaying writes.
- Accepted player membership starts only after successful native claim and role
  selection, before World loading. TryBind alone is not proof of accepted CS
  identity. Historical unique user IDs survive logout until DATACLEAR. Raw
  transport sessions include pre-auth clients and Control; no fabricated zero
  counters or SQL-derived guess replaces the source sets.
- The pre-auth watchdog now clears its eligibility at accepted identity, allowing
  a legitimate Loading/Loaded wait. Control-driven sweeps use source >5000ms
  cadence and >10000ms unaccepted/closing checks. Repeated Close is allowed;
  forced claim release/deletion is not. Native dirty reservations and failed-save
  quarantine remain retained for reconciliation; teardown/counter timing beyond
  source's asynchronous close must be checked in Phase B.
- A ready kick target holds an operation through account/primary or replica claim
  locks, canonical lossless wire-name SQL proof and post-await key/connection/
  epoch/role/name/Control checks. It closes only the captured socket. It never
  deletes TCURRENTUSER, directly unbinds a reservation or saves through another
  owner's claim. Failed/unknown save outcomes retain existing recovery state.
- Source kick has no application ACK. Wire/admin-shell audit now records
  Requested/intent, not successful target eviction or save. Full admin-shell
  queued-command ownership/recovery remains a separate OPEN integration.
- Announcements reuse raw059 message6, full uppercase eight-hex NetString and
  committed protectedCHAT1 for sender0, with no chat-ban/country/spam gate.
  Source Left(1024) ignored its result; there is no invented truncation. Entire
  CS output bounds are checked. A busy recipient's bounded announcement lease
  wait skips it instead of unexpectedly kicking it. Origin Control checks span
  every authority/catalog/protection await.
- Control's real one-second monitor producer replaces the missing probe path.
  The four-DWORD reply is consumed once without another ACK, eliminating ping-
  pong. Peak time updates on source users>=max. Source DATACLEAR leaves live
  users intact while resetting historical Control counters. Ownership failures
  stop the monitor safely; stale socket/service machine/port identities reject.

## Conflicts and required OPEN work

Original Control SendEventToNewConnect(TControlSvr1838–1860) can target Map4 with
CT_EVENTUPDATE/CT_CASHITEMSALE. Original Map's Control receive list registers
neither; gameplay receives separate World MW messages. This is an unresolved
producer/consumer conflict. Native Control preflights a matching active direct
Map event and refuses connection publication instead of reporting partially
applied replay or accepting a fabricated no-op. Event CRUD/tick routes also need
that contract completed; no source function is removed from scope.

The other original Map Control operations (USERMOVE, MONSPAWNFIND, MONACTION,
CASTLEINFO), complete event/cash replay/TLS/admin-shell recovery, transferring
entered-player kick parity and combat/logout penalties remain OPEN. Source kick
can retire World presence before close/save and apply OnDie to a recently fighting
player (TMapSvr1667–1676). This bounded ready-player native close does not complete
those existing gameplay/lifecycle dependencies. Locale/high-bit identity parity,
party/corps/guild/tactics, BOW/BR, other World repositories and the overall source
inventory remain required implementation. Full private manifests, Patch pre-
version SQL and unchanged real executable/assets remain missing inputs.

## Deferred Phase B acceptance

After the full Phase A implementation gate:

1. Compile all six native targets in Debug/Release; inspect declarations/CMake/
   ownership callbacks and least-privilege existing Map grants. No new SQL is
   needed for this slice; migrations044–069/imports still need isolated acceptance.
2. Byte-compare original raw16 Control packets and independent consumers: zero
   metadata, strict65534/65535 totals, fragmented frames, empty handshake/no ACK,
   malformed/nonzero metadata, SS8/client-cipher rejection and backpressure.
3. Connect the actual Control Map dialer to the configured Map listener, including
   single-slot duplicates, replacement during each await, invalid allowlist,
   TLS refusal, World unavailable, native owner loss and shutdown drain.
4. Verify accepted/pending primary+replica counts, pre-auth sessions, historical
   unique users across logout, DATACLEAR reseed, legitimate >30s World loading,
   DWORD wrap, >5s cadence/>10s sweep and retained uncertain saves.
5. Drive periodic ACK(tick)→one four-DWORD REQ→GUI without a second ACK; check RTT,
   >= peak refresh, selected service replacement,60s offline policy, slow writes
   and failing owner callback. Existing test_peer_monitor's immediate-echo
   expectation must be revised; Map-target fakepeer SS8 helpers need SourcePlain16.
6. Compare primary-only announcement group3/sender0/caption6/full1500-byte text,
   non-ASCII profile, protected sender0, busy skip and near-frame limits. Verify
   each Control/recipient key or owner rollover cannot continue old fanout.
7. Kick primary and replica by character name; account-name/unknown/no-match is
   silent. Race handoff, close, checkpoint, name/key/epoch changes and SQL/commit-
   result loss; no successor socket or another claim may close. Audit remains
   Requested, not confirmed save/eviction. Complete combat/transfer dependencies
   before asserting full source kick acceptance.
8. Reject unresolved direct Map event replay before publication; verify supported
   five-operation config separately. Then complete every remaining original
   Control contract and remaining framing port before unchanged-client acceptance.

Static diff/source checks passed; full functionality and real-client compatibility
are not verified.
