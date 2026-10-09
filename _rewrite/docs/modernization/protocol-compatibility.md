# Original-client compatibility contract

The original client must remain unchanged. Database/domain migrations must pass
through the existing fixed-width packet boundary. A passing codec round trip is
not sufficient evidence of a particular original executable working.

| Contract | Evidence and current scope | Status / remaining proof |
|---|---|---|
| Exact client executable | Client source exists; shared `ProtocolBase.h` reports `TVERSION=0x2918`; source headers and backups can be fingerprinted independently | BLOCKED for a named binary build: executable identity and a runnable client environment/capture are not established |
| Opcode registry | `tools/modernization_inventory.py` independently evaluates all 1,542 legacy base-plus-offset expressions against `MessageId.h`; nine collisions preserved | VERIFIED for numeric definitions only |
| Packet framing and crypto | Portable codec, crypto and Asio tests plus independent Python RC4/MD5+XOR peer; queued final ACK flushes before close; uint64 modular checksum golden vectors avoid undefined signed overflow | INTEGRATED; independent captures and client-build validation pending |
| Login and session ordering | `TLoginSvrAsio/handlers.cpp`, auth/connection registry, existing handshake/control-gate tests; original `TLoginSvr` and `Client/TClient` are references | VERIFIED synthetic native Login transaction/duplicate/abandoned-login subset plus process failover, sequential pipelining and shutdown drain via login-lifecycle.json; original executable, non-ASCII wire credentials and direct-login 2FA/world ownership remain pending; login-security.json verifies source non-direct 0x2918 confirmation → client LOGIN retry without unsolicited LOGIN_ACK |
| Characters and enter-world | Native Login create/list/delete; source CN codes and CONNECT checksum; pending/loaded/admitted/ready Map phases; keyed World responses; real encrypted Login/World/Map TCP | VERIFIED native lobby, cell routing, handoff, fresh single-owner Map claim/load/core save/reconnect; normal-character primary transfer and recovery verified by native-primary-transfer-contract.json; full ancillary synchronization and original-client proof pending |
| Content → domain → packet widths | Native test compares all 3,495 mapped attribute keys/HP/AP/DP with current C++ narrowing; 14 unchanged chart SELECT contracts use compatibility views | VERIFIED for tested catalog conversion scope; not every gameplay serialization field |
| Historical strings and timestamps | Original CP1252 bytes retained; derived text uses PostgreSQL; SQL timestamps preserved without assumed UTC | ANALYZED; client text encoding and time policies need independent round-trip wire fixtures before mutable migration |
| Original-client session | No real client connected, authenticated, entered a map, persisted state and reconnected in this run | PENDING; no compatibility claim for any exact client build |

All packet definitions and production/test references, including unreviewed
packets, remain in `evidence/source-inventory.json`. `behavior_status=NOT_ANALYZED`
there means a per-packet behavior review has not been completed; it does not mean
the referenced modern handler is absent. Add reviewed evidence as each slice is
completed rather than converting references into coverage percentages.

## Behavior change review: missing monster attributes

Original essential spawn setup in `Server/TMapSvr/TMap.cpp:629` looks up
`FindMonAttr(MAKELONG(wMonAttr, level))`, deletes the candidate and returns when
attributes are absent. `Server/TMapSvr/TAICmdRegen.cpp:76` also refuses missing
attributes. The backup has 39 absent combinations; none were repaired.

The portable spawn manager formerly fabricated HP from level for these cases.
It now skips the invalid candidate and logs the count, with no changes to opcode,
field order, widths or error packets. It also rejects zero-HP live candidates;
this is an explicit validation policy, not proven original zero-HP behavior.
The pinned catalog yielded zero such candidates.

Remaining differences are explicit: the portable manager distributes candidates
round-robin, treats zero `bCount` as one, allocates sequential runtime IDs and
continues after an invalid candidate. Original code separates ordinary/essential
spawns, uses probability/leader rules and packs spawn/channel/index into IDs; an
essential attribute failure exits the whole spawn setup. Those rules and regen
events need a dedicated behavior slice before claiming full monster parity.
The current verified result establishes authentic combat attributes, not exact
original monster population or client-visible spawn order.

Acceptance for the next gameplay change: derive ordinary/essential selection and
ID contracts from original initialization + regen + client consumers, add
independent expected packet bytes/order, test missing/zero/weighted/leader cases,
then run real PG and the supported client when available. Do not invent missing
stats to get through initialization.

## Login result-code correction

`Lib/Own/TProtocol/include/NetCode.h:267` and restored `TLogin` agree on internal
error 5 and agreement 8; security is 10 in the original enum. The previous modern
auth enum incorrectly used 8/9/5 respectively. These values were corrected;
internal account-ban classification maps to backed-up return 7, and `TCheckIP`
restriction maps to 6. The TCP fixture independently expects original numeric
codes and a 41-byte ACK. This is a correction toward original source semantics,
not proof that a named client build accepts the entire server.

Local duplicate handling now closes both Login connections after flushing reply 3.
Map-owned remote duplicate termination is not implemented by this change. New
session cleanup is account-and-key specific; delayed old cleanup is intentionally
safer than the source procedure's account-only deletion. Protocol failure after
DB commit is tested using a delayed synthetic audit and TCP disconnect.

Login now awaits each packet handler before decoding the next packet on the same
connection. The encrypted fixture sends `CS_GROUPLIST_REQ` before receiving the
login reply and independently expects `CS_LOGIN_ACK` then `CS_GROUPLIST_ACK`.
Shutdown closes actual sockets and awaits their cleanup, preserving `MapHandoff`
for completed `CS_START_REQ`; the synthetic socket test also covers an unlimited
connection cap. PostgreSQL process ownership is operational metadata, with no new
client packet, field, opcode or credential encoding.

An additional ordering conflict remains recorded in `auth-source-contract.json`:
the original C++ wrapper calls `CSPCheckIP` before credential validation and can
return 6 immediately, while the backed-up `TLogin` procedure checks `bIPCheck`
after credentials. The current native service follows the backed-up procedure
order. Its synthetic test proves that chosen behavior, not complete parity with
the original wrapper on a restricted IP with invalid credentials.

## Native character lobby increment (2026-10-09)

Original NetCode.h defines DELCHAR results success=0, invalid password=1,
no group=2, internal=3, guild=4. The rewrite enum and handler now preserve these
values. CREATECHAR's final count is the backed-up global live-character count;
the level and all 13 trailing BYTE fields preserve the original layout. The
source-derived encrypted peer checks real native create/list/delete and process
restart. Equipment remains 12 bytes, including the explicit modern empty custom
texture extension. The source client executable was not run. Native supported
names are US ASCII; full encoding/collation acceptance remains pending. The later
Map section covers native fresh lifecycle. See `evidence/characters*.json` and the
character implementation contract.

## Map admission boundary (2026-10-09)

`domain/connect.h` uses CN_SUCCESS=0, CN_NOCHANNEL=1, CN_NOCHAR=2,
CN_ALREADYEXIST=3, CN_INVALIDVER=4, CN_INTERNAL=5 from the original NetCode.h.
The CONNECT accumulator starts with wrapped DWORD products/sum before 64-bit
mixing. Fixed vectors cover every mixing count and overflow. Wrong checksum and
malformed bodies close silently; wrong version sends code 4 and closes after flush.
A terminal queued response suppresses further packet dispatch on that connection.

One client ACK follows the matching World verdict, after identity load. Duplicate
verdicts and old-key close/enter messages cannot affect a newer reservation.
A deliberate modern policy rejects a competing socket with code 3 while the prior
reservation is live; the original code instead closed/suspended competing sockets.
The earlier controlled-service admission tests do not certify complete client
entry. The native increment below adds actual PostgreSQL ownership, inventory and
skill lists, cell checks and core save. A failed ready save still blocks reload;
durable crash recovery and full ancillary client synchronization remain unfinished.

## Native Login START boundary

The production handler requires exactly six body bytes (group/channel/DWORD char).
It carries the authenticated session key into PostgreSQL rather than trusting the
wire character ID alone. Original result bytes 0/1/2/3 and the 8-byte ACK layout
are preserved: result, four IPv4 octets, little-endian port, server byte. Successful
ACK is terminal: flush, then close the Login socket, suppressing buffered later
requests. These behaviors pass an independently encoded encrypted TCP test.

Logical/physical channels and map-unit ownership come from the pinned routing
release; the original spawn fallback updates X/Z and sets Y to zero. Cell indices
use truncation of coordinate/1024 rather than PostgreSQL float-to-integer rounding.
No missing cell is synthesized. Earlier selected-character stamping, all-or-nothing
rollback, repeated-request idempotence and 60-second unclaimed reservation expiry
are explicit modern repairs. The database and handler support normal application
worlds; no claim is made for BR/BOW or unimplemented TFindServerID side effects.
See `evidence/map-handoff-contract.json` for exact source and remaining Map work.

## Native Map fresh-entry verification (2026-10-09)

The actual Login, World and native PostgreSQL Map processes now complete the
0x2918 fresh single-owner admission sequence with source-derived encrypted TCP.
CHARINFO includes real bags/items (including magic), skills and all twelve keys
per hotkey inventory. Maximum HP/MP are separate from current values; zero current
HP means dead. Aid country defaults to the source neutral sentinel 3. World
MW_CHARINFO_REQ now emits its original four-byte BOOL trailer. The native Map
parses the complete ENTERCHAR composite before acknowledging it, checks routing
ownership and notifies World on disconnect, allowing reconnect on the same World.

This is protocol conformance for the exercised flow, not original-client execution
or full world/gameplay parity. Mail/cabinet/pet client synchronization, maintained
effects, neighboring Map transfers and persisted social graphs are unfinished.
See `evidence/native-map-runtime*.json` and `map-runtime-contract.json`.


## Checkpoint/reconnect boundary (2026-10-09)

Client opcodes, encrypted framing and packet field layouts are unchanged. The
actual peer can reconnect through the same World after Map SIGKILL, reading the
last committed core and unchanged inventory. This uses an independently encoded
Python peer, not the original executable. The native checkpoint scope remains
fresh single-owner characters; secondary connections and social hydration are
unfinished. Map's inter-server ID now follows original
`MAKEWORD(server_id, SVRGRP_MAPSVR=4)` (`TMapSvr/SSSender.cpp:SendMW_CONNECT_ACK` and
`TProtocol/include/CTProtocol.h`). The existing modern peer registration envelope
remains `RW_RELAYSVR_REQ`; this is not proof of interoperability with an original
World binary. Unexpected Map TCP loss executes character cleanup while native
PostgreSQL ownership and checkpoint recovery retain responsibility for the account.

## World-link lifecycle boundary (2026-10-09)

Native admission requires the existing modern `RW_RELAYSVR_ACK` envelope: BYTE
nation, WORD operator count plus DWORD IDs, WORD message count plus pairs of
length-prefixed strings. The complete body must parse with no trailing bytes.
The ACK changes local readiness only; no original client packet is added or changed.
A missing ACK expires after five seconds. World loss closes client sockets and
awaits durable save/release before replacement registration; reconnect requires
fresh Login/START as in the supported workflow. Failed persistence keeps the prior
account reserved. The source Map instead queued `SM_QUITSERVICE_REQ` on World loss
(`TMapSvr.cpp::OnCloseSession`) and saved/logged out characters on exit. Surviving
Map-process reconnect is an explicit modern recovery policy, not legacy restart
parity or an assertion that an original World binary speaks this registration.

`test_world_reconnect.cpp` uses a synthetic TCP peer for missing/malformed ACKs,
teardown ordering and simultaneous Map→World frame writes. Real PostgreSQL and
Login/World/Map process evidence is tracked separately in `native-world-link*.json`.
Neither provides original-client execution evidence. Silent TCP blackholes still
need a heartbeat/timeout contract; the five-second deadline applies to registration.

## World writer and expected secondary connections (2026-10-09)

`WorldSession::SendPacket` now owns its frame and serializes complete writes.
The 8-byte SS header, checksum and opcode/body bytes stay unchanged. A stalled
peer exceeding 256 outstanding frames or 4 MiB (headers included) is disconnected;
queued writes are not replayed. Read EOF, malformed frames and write failure close
that writer and wake waiting senders. This is a modern resource/lifetime policy.

Original `Server/TWorldSvr/SSHandler.cpp::OnMW_ADDCHAR_ACK` accepts an existing
character only through a preplanned connection with the matching server/IP/port,
key and live main. It marks that connection valid/not-ready, then requests CHARDATA
from the main only when all expected connections are valid. Modern World now
follows this success path through the real dispatch loop. It additionally checks
the account and exact registered Map peer. On bad secondary input it sends the
original INVALIDCHAR body with release-main=0 while preserving the valid main;
the original called CloseChar too. This deliberate failure-policy change prevents
a stale or foreign connection from evicting the active character.

Tests cover absent routes, wrong key/account/address/port, partial completion,
duplicates and final CHARDATA, with independent SS bytes also exercised against
the actual daemon. Empty rank/tournament registration replays are checked exactly,
not discarded as arbitrary noise. See `evidence/world-writes-contract.json`.
These checks do not prove original-client execution or original World binary
interoperability. The later native-secondary increment below implements fresh
replica admission and summary synchronization; primary handoff remains pending.

## Map retirement and unaccepted connections (2026-10-09)

Legacy `Server/TMapSvr/SSHandler.cpp:1212` DELCHAR reads DWORD char/key and BYTE
logout/save, then closes the exact player without a client reply. At line 1249,
INVALIDCHAR reads DWORD char/key and BYTE release-main, sends empty
CS_INVALIDCHAR_ACK, and closes. CLOSECHAR at line 2196 sends empty CS_SHUTDOWN_ACK
with close-all disabled. Modern Map now handles all three via its actual World
socket, rejecting truncated, trailing and non-boolean flag bodies and stale keys.

A separate local World-presence state records unannounced, announced or retired.
A failed native database claim sends neither ADDCHAR nor CLOSECHAR. World-driven
retirement suppresses CLOSECHAR echo, including instructions received while an
existing final save is pending. Local phase is preserved so a ready native primary
still saves/releases atomically; a wire save=0 cannot discard its durable ownership.
This is an explicit modern persistence policy. The later native-secondary
increment below supplies a separate role-specific replica release path.

World CLOSECHAR additionally requires the exact registered typed Map peer and an
already valid character connection. A stale/unaccepted sender receives DELCHAR
without evicting the valid World character/account. This guard goes beyond the
legacy key-only lookup; it closes a verified path where a rejected second native
Map could otherwise evict the primary. Client packet layouts are unchanged.
See `evidence/map-retirement-contract.json`. Original-client execution and primary
transfer completion remain unverified.

## Native fresh secondary admission and client hydration order (2026-10-09)

Original `Client/TClient/CSHandler.cpp:176` opens each ADDCONNECT endpoint.
At line 93, CONNECT success selects the main session, sends empty CONREADY to
every listed Map ID and activates the game frame with `OnRegionChanged`. Original
`Server/TMapSvr/SSHandler.cpp:2019` CHARINFO processing sends CHGCHANNEL followed
by full client CHARINFO before that final CONNECT. The rewrite had deferred
CHARINFO until CONREADY; an independent native TCP regression now rejects that
order. Native Map sends hydration at World CHARINFO, ignores a repeated initial
metadata message and does not send a duplicate full character reset on CONREADY.
Original auxiliary quest/stat/pet/post/social packets remain separate unfinished
contracts; fixing the full-character order does not certify the complete client.

The fresh neighbor path now runs through actual primary ROUTE, World ADDCONNECT,
secondary CONNECT, primary CHARDATA and World ENTERCHAR. Migration 019 binds a
60-second one-use grant to the primary token/generation, target token and exact
published IPv4/port, plus all three source catalog releases. Unconsumed repeated
grant requests retain the original deadline. An explicit new route decision may
replace an expired/unconsumed or retired grant. Successful consumption records
the secondary generation separately from the unique mutable primary.

Original `Server/TMapSvr/SSHandler.cpp:1447` supplies a partial secondary summary
and recalls, rather than loading inventory from durable tables. The supported
native no-recall path now validates that summary against the granted location,
records a loaded replica and confirms ENTERCHAR. World sends CONRESULT only to
the main; the replica accepts the client's CONREADY without a separate verdict.
It sends no invented CONNECT/CHARINFO. Original `TCell.cpp:56` exposes primary
actors to observing replicas; `CSHandler.cpp:599` broadcasts movement only from
the primary. Native readiness/visibility/movement now preserve that distinction.
Other secondary gameplay dispatch remains unported and cannot mutate a fabricated
full character graph.

Ready replicas never run primary load/checkpoint/save or release current-user
ownership. Exact-generation retirement affects only the replica row; primary
close/recovery cascades all replica grants. A target restart cleans only its
old-token replicas. Actual accepted secondary disconnect still requests the source
close-all through World and lets the primary finish its save. These process and
connection fences are modern persistence policies, with unchanged client fields.

The pinned backup topology separates the three normal channels across Map IDs.
Two-Map tests explicitly split one cell via a temporary compatibility view in an
owned disposable PostgreSQL database and restore its exact definition in cleanup.
Imported historical rows and original backups remain unchanged. This is a
synthetic deployment test, not a claim that this split was present in the backup.
Full distributed actor ownership/AOI, movement-driven connection-set changes,
primary role transfer and original-client execution remain pending. See
`evidence/map-replica-contract.json` for scope and actual-process evidence.


## World main-handoff coordination (2026-10-09)

The original `Server/TWorldSvr/SSHandler.cpp:1095` CHECKMAIN handler chooses the
responding owner, requests RELEASEMAIN from the old main and points the World
character at the candidate. `:2284` forwards the source RELEASEMAIN body verbatim
as ENTERSVR; the ENTERSVR acknowledgment then requests the Map server list before
final CHECKMAIN/CONRESULT. The Map source at `TMapSvr/SSSender.cpp:1999` contains
the full transient transfer graph. World must not rebuild it from stale SQL rows.

A regression against the previous actual Release daemon showed that a registered
but unaccepted Map could send unsolicited CHECKMAIN, reassign the primary and
trigger RELEASEMAIN. The coordinator now requires a current typed Map peer, an
accepted ready character connection and an outstanding CHECKMAIN request to that
exact socket. One response consumes the round. A transfer pins both source and
target sockets before sending RELEASEMAIN, advances through release/load/final
confirmation, and rejects competing, repeated, stale or out-of-phase replies.
Only the source may forward the opaque state and only the target may finish load
and confirmation. The source state bytes remain unchanged on the wire.

Fresh ENTERSVR is also consumed once from the actual registered main. Exact body
length, Boolean fields and finite coordinates are checked before metadata changes.
ROUTE/CHARDATA remain primary decisions; ENTERCHAR cannot make an unaccepted
connection ready or repeatedly re-arm CHECKMAIN. Connection-list reconciliation
cannot disturb release/load stages. During final confirmation it accepts only
the target's list and may still add newly required secondary connections.

A modern five-second total deadline covers release, target load and final
confirmation. Expiry or an explicit target failure closes the World character
through the existing per-Map retirement path; this is coordination cleanup, not a
claim that a native database transfer committed. Losing either pinned peer also
closes the pending exchange, including when the source has already moved out of
the live connection list. Close is idempotent and cancels the timer. Closure and
dead-connection notifications use the original Map server type byte 4.

No client opcode, field or server-to-server header was added. The original
CHECKMAIN ACK has only character/key, so it cannot distinguish an arbitrarily
late response from an earlier round on the same still-current socket if another
round has since asked that socket again. This is an explicit source-wire limit;
the new checks establish expected phase and connection identity, not a new wire
epoch. Future native PostgreSQL transfer must independently fence its transaction
with durable owner/connection/transfer identity.

See `evidence/world-handoff-contract.json` for before/after evidence. The independent
World peer fixture uses a labelled opaque transport payload; it does not certify
a complete Map graph encoder/decoder. Native Map primary transfer, dynamic border
routing, full gameplay and actual original-client execution remain unfinished.

The same verification exposed a transport defect in `tnetlib::AsioSession`:
concurrent `Close` calls touched the same socket while composed reads were still
running, producing a UBSan null descriptor access. Packet producers also used a
channel without thread-safe shared access. All internal TCP read/write/channel
operations now execute on one per-session strand, including composed-operation
continuations. Public `Close` publishes a closed flag immediately and schedules
socket cancellation on that strand; native Login/Map liveness checks use that
atomic flag. Setup/raw socket access still requires external synchronization,
one read loop is allowed, and raw writes must be awaited serially.

A backpressure regression additionally showed that `channel.close()` alone left
pending producers waiting. Shutdown now cancels waiters before closing the
channel. Tests run packet producers on two executor threads and race four
external closers against partial reads, writes and a capacity-one queue for 24
rounds; every producer and reader must complete before executor shutdown. This
changes internal concurrency and cleanup, not packet bytes. See the retained
sanitizer/backpressure failures and passing transport reports in the handoff
contract. The strand requirement follows the
[Boost.Asio composed-operation contract](https://www.boost.org/doc/libs/1_83_0/doc/html/boost_asio/overview/core/strands.html)
and [channel thread-safety contract](https://www.boost.org/doc/libs/master/doc/html/boost_asio/reference/experimental__basic_channel.html).


## Complete source Map transfer representation and cooldown lifecycle (2026-10-09)

`TMapSvr/SSSender.cpp:1999–2638`, `TItem.cpp:468` and the non-login
`SSHandler.cpp:4424` branch define the complete RELEASEMAIN/ENTERSVR body.
`TMapSvrAsio/domain/main_transfer.h` and `services/main_transfer_codec.cpp`
now represent and encode/decode its full ordered graph: core and security/aid/mail
metadata; inventory and cabinet descriptors; raw server items; skills, effects,
quests, hotkeys, cooldowns, pets, recalls, protected characters, PvP/duel records,
auctions, titles, companions, interleaved companion items and the final counters.
Strings preserve source bytes; integers are explicit little endian, including
signed 64-bit time sentinels and unsigned high bits. The saddle's misleadingly
named `m_wItemID` is actually a DWORD. Server item magic is raw and extended item
attributes are DWORDs; the client projection is insufficient to reconstruct them.
Native PostgreSQL load now retains both representations without changing client
packet widths or historical database rows.

The independent Python fixture contains 861 bytes and 319 named fields, with every
variable section populated. It does not import the C++ layout. All bytes must
round-trip, and all truncated prefixes must fail. Parsing is bounded by the source
WORD frame length (65527-byte body), validates structural selectors/finite core
positions and exact EOF, and allocates entries only after decoding them. This is
wire verification, not proof that every represented subsystem has working gameplay.

Original skill load installs the transmitted remaining duration as the local
reuse delay (`SSHandler.cpp:4880–4915`, `TSkill.cpp:73–117`). Fresh native admission
now restores that gate, including when the optional gameplay catalog is absent.
A changed template cannot shorten an outstanding imported cooldown. Duration/origin
storage avoids deadline overflow and permits tick zero; a backward clock cannot
bypass a live gate. Snapshot/export is deterministic and does not rearm timers.
Successful teardown forgets only that character's timers. Full rank/attack-speed
reuse rules, effect timers and persistence of newly used cooldowns remain pending;
current checkpoints save only core fields. The initial CHARINFO still carries the
loaded duration; refreshing it at the exact send instant is a follow-up.

The original client has not been executed. The new codec does not enable native
primary transfer: embedded ENTERSVR remains refused until live graph restoration,
transactional ownership fencing and fault recovery are implemented. Synthetic TCP
checks prove restored cooldown rejection in actual native Map daemons. See
`evidence/main-transfer-state-contract.json` and `next-steps.md`.


## Native PostgreSQL primary handoff (migration 020)

The preceding codec/coordinator prerequisites are now connected to the actual
Map runtime. Original movement-driven CHECKCONNECT/CHECKMAIN/RELEASEMAIN/ENTERSVR
packets carry the handoff. A ready replica answers cell ownership, then consumes
an exact prepared body and changes database ownership before publishing primary
state. A monotonically increasing SQL authority epoch prevents stale same-socket
writes across a round trip; no epoch is added to the original wire format.

The source freezes mutation and exports current core/raw inventory/skill timers.
Gameplay operations drain for at most three seconds and client close interrupts
that drain. The target reconstructs supported state from the body and pinned
catalogs, restores timers and retains every typed extra section. World retains
its existing expected-peer/phase checks and total five-second coordination deadline.
CONNECT makes the original client choose the new main; empty CONREADY activates
it and demotes the retained source. No duplicate CHARINFO is sent during handoff.

Version-2 checkpoints preserve the transferred graph with a database-checked
SHA-256 and catalog/core identities. Source cancellation/replacement saves the
prepared graph; target replacement recovers committed loaded/ready checkpoints.
Fresh relogin uses the saved graph rather than stale child rows. Same-revision
confirmation checks graph and core. A client close during delayed target COMMIT
waits for local ownership publication before teardown; World sends original
INVALIDCHAR to the old source before closing it in that interrupted-handoff case.

Actual Debug/sanitizer/installed tests exercise two native Maps with an explicitly
synthetic routing partition, source-sized encrypted client packets and unchanged
historical rows. The prior installed daemon fails the new movement handoff.
See `evidence/native-primary-transfer-contract.json`. Original-client execution,
full AOI/effects/quests/summons/secondary gameplay and complete native social
persistence remain pending. Typed preservation does not certify active gameplay;
fresh non-transfer characters still use core-only v1 checkpoints.
