# Native Map party recovery and manual metadata — 2026-10-14

Baseline: `622fb2e`. Status: **IMPLEMENTED / INTEGRATED_UNVERIFIED**. Evidence is static source/API review only. No build, test, server/client/service, database statement, migration, import, extraction or runtime acceptance was executed, and no commit/push was created by the Map agent.

## Truthful post-Ready recovery

The previous primary-only status probe required a complete `payload.statistics`. It could not recover a fresh replica: original World `SSSender.cpp:258–340` / Map `SSHandler.cpp:1447–1628` send identity, level/class and cluster metadata without player HP/MP/maxima. The later four vitality fields belong to recall monsters. Native `handlers_native_entry.cpp:OnMWNativeEnterCharReq` deliberately marks replica vitals unknown and keeps its payload absent. Sending those fields as a MANSTAT proof would manufacture player state.

`RefreshReadyParty` now repeats the **existing entered identity ACK** `MW_ENTERCHAR_ACK`: exactly DWORD char and DWORD key, encoded by unchanged `EncodeEnterCharAck` (`world_senders.cpp`; original `TMapSvr/SSSender.cpp:730–741`). It carries no status, guessed vitals, extra role/epoch field or new opcode. It is a deliberate modern trigger after a real SQL/local Ready transition, **not an original source Ready acknowledgment or registration success**.

`handlers/session.cpp:OnConReadyReq` invokes it for:

- actual initial primary or replica SQL `MarkReady` followed by local Ready and bound position;
- an outgoing primary that has confirmed its committed transfer, become a ready replica and stored its partial observer state.

An already-Ready duplicate CONREADY still returns without another trigger. `IPlayerService::HasNativePartyAuthority` is an immutable adapter capability (false for legacy adapters, true for the concrete PostgreSQL service); it preserves the old adapter entry flow and never becomes an error fallback. Native helper proof remains mandatory: pooled `AuthorizeParty`, exact current actor/key/connection/role/epoch, live local and primary owner, current unlocked login and configured catalog/encoding checks. Missing native state or invalid authority closes normally; failed/unknown transport writes are not replayed.

World's native **already-completed connection** branch independently proves the current primary or ready replica and sends actual registry PARTYATTR, including actual partyless zero. It does not restart World connection checks, change connection verdicts, emit another CONRESULT or reset the roster. The source/legacy branch is separate. Ordinary MANSTAT remains a real primary status producer and suppresses effective partyless/SOLO0 exactly as before; it is no longer a readiness workaround.

## Lease and visibility ordering

Admission's exclusive lease ends after the SQL/local Ready boundary and presence binding, before recovery or entry AOI/quest awaits. This prevents a legitimate Ready primary from being held behind entry flooding until the independent three-second party-projection deadline closes it.

The old AOI loop captured character metadata before `MarkReady` and reused that ENTER packet after awaits. A newer ATTR could arrive first, be ignored by a client that had not seen ENTER yet, and then be overwritten by stale ENTER. The new loop samples the live primary actor for each real ENTER enqueue under a short actor gameplay lease; it never holds two actor leases together or an admission lease over the entire flood. Captured identity/open/role/key/generation/epoch are rechecked, including recipients after a lease wait. The wait is bounded to three seconds; a stale/busy actor is skipped without a substitute packet. This is a bounded repair, not complete AOI/movement/backpressure acceptance.

`EncodeEnterAck` also applies original SOLO1 effective getters for party/chief/commander (`TMapSvr/CSSender.cpp:453–455`, `TObjBase.cpp:2240–2243`, `TPlayer.cpp:1448–1457`). It does not alter packet layout or create vitals for partial replicas. Existing primary-only actor advertisement remains.

## Manual chief and type protocol

| Step | Original source | Native Map |
| --- | --- | --- |
| Chief request: map + main; DWORD target; BR no-op; World DWORD own id/key/target | `CSHandler.cpp:3377–3398`, `SSSender.cpp:2643–2655`; `CSProtocol.h:2025`, `MWProtocol.h:287/288` | `OnPartyChangeReq`, explicit CS dispatch, exact pooled native actor authority without stats requirement |
| Chief response: DWORD recipient/key + BYTE result; client gets BYTE result; metadata belongs to later ATTR | `SSHandler.cpp:10392–10408`, client `CSHandler.cpp:5193–5208`; World `SSHandler.cpp:2334–2445` | Explicit MW dispatch + `OnWorldParty`; no direct chief cache mutation, success10 preserved |
| Type request: map + main; BYTE type; **no source BR drop**; World DWORD own id/key + BYTE type | `CSHandler.cpp:3400–3418`, `SSSender.cpp:2657–2670`; `CSProtocol.h:2368–2373`, `MWProtocol.h:325/326` | Same read-only actor authority and exact original body; no local membership/chief assumption |
| Type response: DWORD recipient/key + BYTE result/type; success0 updates raw server type; client packet **always has both bytes**, including failure | `SSHandler.cpp:7808–7836`, `CSSender.cpp:1695–1704`, client `CSHandler.cpp:5086–5120`; World `SSHandler.cpp:2447–2480` | Cache update on result0 only, exact two-byte CS response, no extra ATTR |

Chief source error precedence is missing target3 → no party9 → not chief8 → different party3 → self6; success is10, followed by ATTR. World owns those checks and actual memory mutation under its party-operation gate/current native participant proofs. Map does not replace them with cached zero/default membership checks.

Original TYPE success fanout targets party primaries and does not send ATTR. Client TYPE ACK changes its item-type byte while `GetPartyID` still returns raw actor party data (`Client/TClient/TClientGame.cpp:22821`); the Map server's SOLO getters hide effective fields. That quirk is preserved. A later, separately documented Ready recovery can legitimately project current ATTR; a manual type operation itself does not invent that refresh. Changing the mode does not implement its unported loot distribution.

Complete tails/NUL/enum checks remain intentional modern hardening. Obtain type is restricted to the source enum0..5; original TYPE handler did not enforce that range. Client chief/type writes have no stat fields and do not require a supported stat sheet. Native World participants still need the agreed full current/guildless/corps-free/arena-free supported capability; source broader group producers are not inferred as registry zeroes.

## Roster and broader OPEN scope

Original client `OnCS_CHARINFO_ACK` calls `ReleaseParty` (`CSHandler.cpp:238–286`) but actual original CHARINFO sender/parser have no roster rows. The stale `CSProtocol` party-count comment is not a producer contract. Normal handoff CONNECT (`CSHandler.cpp:93–166`) retains the existing roster; World normal ENTERSVR handoff skips fresh CHARINFO. `CS_PARTYJOIN_ACK` appends members. Therefore this recovery sends idempotent metadata only: **full roster reconciliation/replay remains OPEN**, including membership changes while a different member transfers. No blind JOIN resend, synthetic self DEL, invented CHARINFO footer or fake reset barrier was added.

Also OPEN: original `MW_CONNECT_ACK`/complete World bootstrap (current RW-as-Map remains an explicit extension), guild/corps/tactics, party loot/rewards/XP, party-aware equipment/level dependencies, arena/INDUN/instance exit, BR team chat and broader BOW/BR behavior, full maintained-stat producers, prototype combat/REVIVAL/AI, generic Map high-bit account/claim identifiers, same-actors/session/obtain invitation nonce ambiguity and full AOI/backpressure behavior.

Phase B must verify true primary/replica Ready → repeated eight-byte entered ACK → authoritative ATTR; loaded/pre-Ready refusal, owner/token/key/generation/epoch takeover, primary→replica demotion, duplicate CONREADY silence, no fabricated stats/no MANSTAT imports from replicas, admission/ATTR/AOI ordering and saturated client queues, ordinary stat fanout, malformed tails/enum values, manual chief error precedence10+ATTR and type0/type failure tails, BR chief no-op/type behavior, SOLO client/server distinction, concurrent join/leave/type/chief/transfer/logout, unknown write outcomes, and the explicitly unreconciled roster cases. None of these has been executed in this Phase A.
