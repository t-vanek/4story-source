# Source World transport and Control service types — 2026-10-14

Baseline: local `main`, `acbcaaf`. This increment is **INTEGRATED_UNVERIFIED**
within the explicit transport/type/monitor scope. The complete modernization
and original Map registration/bootstrap are unfinished. No build, test, SQL,
import, service, client, container or runtime verification ran. No new migration;
001–069 and original server/client/libraries/backups are unchanged; next070.

## Production integration

- Actual Map `AsioWorldClient` and World `WorldSession` now use the original
  packed16-byte header: little-endian WORD total/opcode, DWORD number, INT64
  checksum. Valid total is16–65534; maximum body65518. Fresh output metadata is0.
  Source SESSION_SERVER receive ignores both metadata fields, including numbers
  retained by `CPacket::Copy` from client packets; there is no RC4/XOR/body fold.
  The packed implementations explicitly require little-endian targets. The
  encrypted CS client codec remains separate and unchanged.
- Exact header/body reads, per-connection owned frame buffers, complete composed
  writes and captured current Map socket remain. World retains256-frame/4MiB
  bounded writes; malformed output now closes the World writer, avoiding a
  silently dropped response on a retained peer. Map rejects known oversized
  sends before acquiring its write permit. No attempted write is replayed onto
  a replacement connection; Map disconnect drain precedes reconnect.
- The shared modern RW acknowledgement now parses message keys as DWORD followed
  by source STRING, with bounded counts and exact end of body. Production World
  already emits that layout. The existing admission gate still opens only after
  the full current acknowledgement; a completed TCP write establishes no remote
  application acceptance. This is a retained extension, not source Map admission.
- Control chooses SourcePlain16 for actual Map4 and World3 inventory targets.
  Requested TLS on these plaintext listeners fails explicitly before dial;
  there is no silent downgrade. Other endpoints/operator sockets retain their
  modern SS8 profile and require later source ports. Source profile receive
  ignores metadata; the bounded Map Control receiver still requires fresh zero
  metadata for its five audited operations (an explicit modern restriction).
- Control inventory constants now match directly linked `CTProtocol.h:26–35`:
  Control1, Login2, World3, Map4, Patch5, FTP6, Log7, Relay8, PreFTP9. Guessed
  installer-order constants previously made World filters target Patch5 and
  Relay filters target Log7. Existing admin/event/shell/lifecycle/profile filters
  now read the correct source IDs. The native importer/repository preserve raw
  database IDs; neither archival data nor earlier migrations are remapped. No
  additional BR/BOW inventory type is invented. NetCode peer roles are a distinct
  namespace (Relay5 versus Control inventory Relay8).
- Actual World monitor counts all listener sessions through WorldServer's live
  counter, including anonymous sockets, plus current characters and historical
  unique accounts. It accepts only current/open Control, exactly one DWORD tick;
  reset accepts only an empty body and rebuilds historical accounts from current
  characters. Existing Control periodic producer consumes the four-DWORD reply
  without an echo cycle. This changes no durable gameplay state or SQL contract.

## Source mapping and required remaining work

[Analysis with exact source locations](analysis.md) and
[Map implementation review](map-framing.md) distinguish the source contracts,
modern repairs and known admission/bootstrap gaps. The former claim that the
original server CPacket header was8 bytes is superseded. Historical records stay
in place with their bounded scope; modern synthetic peers prove no source parity.

Original Map `SendMW_CONNECT_ACK` uses opcode0x9002, WORD typed server ID,
BYTE count and source-derived BYTE channel entries, with **no registration ACK**.
World source registers this message and sends local/mission, BOW/BR, optional
Relay, event/cash, castle/rank/tournament state. Source channel keys/count casts
have a proved ambiguity; no guessed configuration list is substituted. Modern
World lacks this handler and Map currently uses Relay-only RW registration.
This remains **IMPLEMENTING**, with full native bootstrap/route ownership,
accepted role lifecycle and Map consumers required together. Header correction
alone does not establish original peer admission or full client compatibility.

Party follows correct admission/bootstrap and ready-current actor authority:
source seven-member ephemeral party registry, one-use invite/join/leave, source
Map state projections and original CS replies need integrated production paths.
Existing low-byte/placeholder social handlers are insufficient; no historical
party table or successful empty response may be invented. Guild/corps/tactics,
party rewards/XP, BR/BOW and full World PostgreSQL startup remain in scope.
Private complete archival manifests, missing Patch pre-version SQL and original
client executable/assets remain external inputs, not removed responsibilities.

## Static review and deferred Phase B acceptance

Source declarations, callers, dispatchers, source constants and diffs were
inspected only. Existing World fixture helpers were statically aligned with the
removed8-byte checksum API; independent write fixture uses explicit16-byte decode,
zero output metadata, valid/invalid source limits and closed invalid-output writer.
The Map reconnect fixture now emits a DWORD message key. Existing Control
fixture inventory IDs were corrected; its synthetic default8 sessions and old
monitor echo assumptions still need a separate acceptance update. None were run;
no historical green result applies to this code.

Later authorized verification must cover:

1. Debug/Release builds and linking of all affected targets/fixtures; independent
   Plain16 decoders and source-generated fresh/copy packets, nonzero unused
   metadata, fragmented/coalesced reads, empty body and total15/16/65534/65535.
2. Concurrent/stalled writes, byte/frame bounds, invalid output, EOF/reset,
   suspended old writes/handlers, owner loss and teardown before reconnect; no
   replacement-socket replay or leaked reservations.
3. Current modern RW full DWORD-key catalog ACK, truncated/oversized counts,
   negative string lengths/trailing bytes, missing/wrong/stale ACK and timeout.
   Keep source MW_CONNECT/full bootstrap acceptance separate until implemented.
4. Native Control topology with all original type IDs: World3 receives World
   commands/profile/event routing; Patch5 receives Patch work; Log7 is not Relay8;
   inventory reload/old machine/port/socket cannot route to a replacement. TLS
   requests cannot downgrade onto the plaintext Map/World listeners.
5. Actual Control–World handshake, periodic monitor with anonymous sockets and
   connected roles, four exact DWORDs, no response loop, historical users after
   logout and reset, malformed/current-role rejection and World owner loss.
6. Unchanged original client with identified binary/assets and matching data,
   source Map admission/bootstrap, retained CS crypto, full gameplay/state
   acceptance and PostgreSQL fault/recovery invariants. These gates remain open.
