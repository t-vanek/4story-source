# Native Control to Map continuation — 2026-10-14

Status: **IMPLEMENTED / INTEGRATED_UNVERIFIED**. Static source comparison only.
No build, test, server, client, migration, SQL execution, import or extraction was
run. This record does not establish runtime acceptance or full Control parity.
Tracked `git diff --check` and the new-file whitespace checks passed; no build or
runtime result is implied.

## Source and native entry

| Original contract | Native continuation |
| --- | --- |
| [Packet.h](../../../../../Lib/Own/TNetLib/TNetLib/Packet.h), lines36–43: WORD size/ID, DWORD number, INT64 checksum | [Map Control transport](../../../../../Server/TMapSvrAsio/services/control_session.cpp): raw16 header, zero number/checksum, total strictly below65535 |
| [Session.cpp](../../../../../Lib/Own/TNetLib/TNetLib/Session.cpp), lines68–84,309–349,454–474: no crypt for SESSION_SERVER | Explicit Plain16 transport, independent of the encrypted `tnetlib::AsioSession`; no inferred Server PeerType bypass |
| [Map Accept](../../../../../Server/TMapSvr/TMapSvr.cpp), lines1017–1034,1078–1080; CloseSession1754–1762 | [MapServer](../../../../../Server/TMapSvrAsio/map_server.cpp): configured IPv4/CIDR transport role before client World admission, crypto and client cap; a single current Control slot |
| [Five Control handlers](../../../../../Server/TMapSvr/CSHandler.cpp), lines27–107,214–237; [monitor sender](../../../../../Server/TMapSvr/CSSender.cpp), lines8–24 | [Typed Control dispatch](../../../../../Server/TMapSvrAsio/handlers/control.cpp), isolated from game-client CS dispatch |

The previously implemented modern Control transport used SS8. That historical
rewrite transport is not evidence of the original Map Control framing. The
matching Control dialer must explicitly select its SourcePlain16 Map profile.
No shared network-library change or client-cipher change is introduced here.

`security.control_ip_allowlist` is empty/denied by default. Its configured isolated
transport addresses select the Control role; a client opcode cannot promote a
game session. The original empty CTRLSVR body carries no identity proof and is
still a no-op with no ACK. The modern endpoint requires it as the first frame
within five seconds. Duplicate live Control peers are rejected; callbacks retain
and recheck the exact current peer after suspension. Clear/monitor do not require
a registered World link. Control-to-Map TLS remains unsupported: an enabled Map
TLS setting fails startup and must never silently downgrade to plaintext.
Configuring a Control allowlist without the native owner callback also fails
startup; a stub/fake ownership success cannot authorize this transport.

The source sender clears all packet bytes, producing zero number/checksum. The
native Plain16 endpoint validates those zero fields explicitly; the original
server parser simply skipped crypt and did not validate them. This stricter
profile is intentional. Output is serialized across the complete async write,
with at most256 pending frames/4MiB. Failed writes are never retried on a new peer.

## Actual operations and ownership

Announcement reads one original STRING, preserving the ignored source
`Left(1024)` return. Shared [notice delivery](../../../../../Server/TMapSvrAsio/handlers/communication.cpp)
broadcasts to ready primaries as CS_CHAT_ACK group3, sender0, published raw059
NAME_OPERATOR6 and BuildNetString(empty, full message). Protected CHAT1 for sender0
still applies. There is no player chat-ban, flood or country gate. Caption width,
lossless configured profile and complete CS packet bounds remain checked; an
oversized notice is dropped without truncation or closing the recipient. The
current Control callback is checked throughout fanout and after awaited SQL.
A recipient whose operation cannot be acquired within three seconds is skipped
without closing that client; this bounded fanout policy is a modern limitation.

Kick matches a character name, not an account name. Source InitMap8133–8139
registers the name before the main-only branch8141, so the native independent
slice includes real ready primary and replica players. It holds a bounded player
operation, then [native PostgreSQL authority](../../../../../Server/TMapSvrAsio/services/postgresql_map_communication.cpp)
checks the current Map owner, account, exact ready primary/replica claim,
current-user identity, nondeleted character and canonical wire-name projection.
After awaiting it, the same local key, connection, authority epoch, role, phase
and name must still be current. Only the captured client is closed.

The existing [disconnect coroutine](../../../../../Server/TMapSvrAsio/map_server.cpp)
retains ownership of draining, timer sampling, SaveAuthorized, ReleaseSession and
MW_CLOSE. Replica teardown cannot save a fabricated full graph or clear another
Map's primary login. Failed or unknown saves retain their closed reservation;
there is no direct Unbind, TCURRENTUSER deletion or replacement kick success.
Control requests gain no invented application ACK.

Before each Control command, `CheckControlOwner` opens its own regular service
pool lease and BeginMapTransaction, and proves the owner backend still holds its
exact Map advisory process lease;
a persisted owner row alone cannot authorize Control after its connection dies.
[Main lifecycle](../../../../../Server/TMapSvrAsio/main.cpp)
fails closed on an unconfirmed owner: stop accepting, close clients/Control, drain
pending operations, then stop. It does not concurrently query the dedicated
PostgreSQLMapOwner::Healthy connection. Shutdown and owner-loss draining include
Control operations; World-link loss continues to drain only game clients.

## Monitor and service-data clear

Source monitor writes four DWORDs: incoming tick, all accepted transport sessions,
accepted character identities and historically seen unique accounts. The source
session map includes Control and pre-auth sockets. The player map begins at
accepted CS_CONNECT identity before World loading; it includes replicas. Active
accounts survive logout until DATACLEAR or process restart.

[Session registry](../../../../../Server/TMapSvrAsio/services/session_registry.h)
now marks accepted identities only after the exact native claim succeeds and the
actual role is set. TryBind's earlier provisional reservation is not counted as
an accepted player. DATACLEAR rebuilds only the historical unique-account set
from every accepted local identity, with no primary filter; it does not erase
inventories, social graphs, charts or live sessions. Dirty retained reservations
remain represented in local player statistics while their transport coroutine
can retire; this reflects the native recovery requirement rather than proof of
the original asynchronous save behavior.

After the monitor echo, the source cadence is strictly more than5s between
sweeps. Unaccepted client transports older than10s are closed. Accepted loading
clients are excluded: the source clears its accept tick before load, and the
existing configurable pre-auth watchdog now uses the same accepted marker.
Closing transports older than10s may receive another Close request, but no
pending PG claim is force-released. The normal disconnect coroutine remains the
equivalent of the source can-delete fence.

## Required later acceptance — not performed

1. Verify actual Control dialer SourcePlain16 bytes against the Map endpoint:
   zero number/checksum, empty first CTRLSVR with no response, total65534 accepted
   and65535 rejected. SS8, encrypted client traffic, TLS and nonzero reserved
   fields must not enter this configured Control role.
2. Verify default deny, exact/CIDR profile validation, duplicate peers, replacement
   after close, five-second first-frame timeout and old-peer callbacks while a
   new generation is current. A ready game client choosing any CT opcode must
   produce no administrative effect.
3. Check announcement bytes against the stock client: group3/sender0, raw caption6,
   exact eight-byte net-string length, text above1024, empty text, protected0 and
   full CS envelope limits. Close/reconnect Control during each awaited phase;
   later recipients must not receive the old peer's command.
4. Verify source counts through pre-auth, claim failure, accepted Pending/Loading,
   loaded/admitted/ready primary, replica, logout and retained failed saves.
   Repeated account logins must remain a single historical active account until
   DATACLEAR; clear must reseed both roles without changing durable gameplay.
5. Exercise DWORD wrap, >5s sweep and >10s unaccepted/closing age. Slow accepted
   World/DB loading must not be classified as pre-auth; active checkpoint/save
   work must never lose its reservation through the sweeper.
6. Kick named primary and replica clients while checkpoint/projection/transfer
   or logout awaits. Verify exact generation/epoch/phase/name rechecks, ordinary
   primary save/logout receipt, replica-only release, no fabricated success and
   no repeat of a write with unknown COMMIT outcome.
7. Revoke/replace Map ownership while a Control command awaits its owner/target
   query. Observe fail-stop/drain before command effects, no dedicated SOCI
   connection concurrency and no takeover of the replacement owner's claims.

## Explicitly open

Other Control event/gameplay/admin operations, TLS to Map, exact locale-specific
ANSI case folding, and entered/transferring clients outside the independent
ready-player kick slice remain open. The general native Map high-bit account/
claim binding limitation remains open. An implemented Control transport does not
establish complete Control, Map lifecycle or stock-client parity.
Unmatched Control opcodes close the peer, matching the source invalid-message
path rather than inventing a no-op. Direct Control event/cash bootstrap replay
to Map remains open; a targeted unported event is a preflight failure, not a
successful partial bootstrap or a reason to fabricate an application ACK.
