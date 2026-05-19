# TPatchSvr → TPatchSvrAsio — byte-level parity audit

Audited on 2026-05-19 against restored `TGLOBAL_RAGEZONE`. The
modernized server (`Server/TPatchSvrAsio/`) was already documented as
"all 9 CT_* handlers ported"; this round combs through the SQL
semantics and the wire serialization for divergences the handler
catalogue alone doesn't surface.

## Summary

| ID | Severity | Area | Status |
|---|---|---|---|
| P-1 | HIGH | `ListPatchesSince` off-by-one (`>=` vs `>`) | ✅ fixed |
| P-2 | HIGH | `ListPrePatchesSince` off-by-one (`>=` vs `>`) | ✅ fixed |
| P-3 | HIGH | `ListInterfaceFiles` queries non-existent `TINTERFACECHART` (real table is `TUSER_INTERFACE` + version subquery) | ✅ fixed |
| P-4 | HIGH | `MinBetaVersion` uses `MIN(dwBetaVer)` instead of calling `TMinBetaVer` SP | ✅ fixed |
| P-5 | MEDIUM | `MarkPreVersionComplete` is a no-op (legacy SP `TPreCompleteAdd` not deployed) | documented |
| P-6 | LOW | Stale-client sweep on `CT_SERVICEMONITOR_ACK` not ported | documented |

The 4 HIGH-severity items were observable wire bugs against any live
DB. P-5/P-6 are documented but unfixed for the reasons below.

## Verified non-gaps

* Wire framing — `CT_PATCH_ACK`, `CT_NEWPATCH_ACK`, `CT_CHANGEIF_REQ`
  (uses `CT_NEWPATCH_ACK` shape), `CT_PREPATCH_ACK`,
  `CT_SERVICEMONITOR_REQ` are byte-identical to legacy, including the
  `INT64(0)` padding quirk and the `WORD sin_port` ordering.
* `OnERROR(dwResult)` in legacy is an empty stub (`TPatchSvr.cpp:45`);
  modern's "log + close" on a handler error is no worse than legacy.
* Legacy `SESSION_SERVER` flip on `SERVICEMONITOR_ACK` gates only the
  `SendQueueFull` cap (4096 entries for SESSION_CLIENT, unbounded for
  SESSION_SERVER). Asio handles backpressure via its own scheduler;
  no equivalent classification is needed in modern.
* `OnCT_PATCHSTART_REQ` returns `EC_SESSION_EXIT` in legacy; modern
  calls `session->Close()`. Both close the socket.

## P-1 / P-2 — off-by-one in patch listing

Legacy `CTBLVersion.sql`:

```sql
SELECT dwVersion, szPath, szName, dwSize, dwBetaVer
FROM TVERSION WHERE dwVersion > ?
ORDER BY dwVersion
```

Modern (pre-fix):

```sql
... FROM "TVERSION" WHERE "dwVersion" >= :v ...
```

`>=` returns the client's *own* current version as a "new patch". For
each poll the client wastes a download round-trip on a file it
already has. Fixed by switching to strict greater-than in both
`ListPatchesSince` and `ListPrePatchesSince`.

## P-3 — CHANGEIF queries wrong table

Legacy `CTBLInterface.sql`:

```sql
SELECT szName, dwSize,
       (SELECT MAX(dwVersion) FROM TVERSION) AS dwVersion
FROM TUSER_INTERFACE WHERE bOption = ?
```

Modern (pre-fix):

```sql
SELECT "dwVersion", "szName", "dwSize"
FROM "TINTERFACECHART" WHERE "bOption" = :o
```

The deployed schema has `TUSER_INTERFACE` (3 columns: `bOption`,
`szName`, `dwSize`), not `TINTERFACECHART`. Modern's catch block
swallowed the "invalid object name" error in debug logs, so
`CT_CHANGEIF_REQ` was silently returning 0 files in every deploy.

Two further details legacy gets right and modern needed to match:

* `dwSize` is declared `float` in `TUSER_INTERFACE` (not `int`) —
  modern now binds as `double` and narrows to `uint32_t` C++-side.
* There is no `dwVersion` column on the row; legacy synthesizes one
  via a `(SELECT MAX(dwVersion) FROM TVERSION)` subquery so the
  client's per-interface-file version check has a non-zero number to
  compare against.

## P-4 — TMinBetaVer SP vs. heuristic

Legacy `CSPMinBetaVer.sql`:

```sql
{? = CALL TMinBetaVer}   -- @dwMinVer OUTPUT, returns operator cutoff
```

`TMinBetaVer` body in the restored DB:

```sql
CREATE PROCEDURE [dbo].[TMinBetaVer] @dwMinVer INT OUTPUT
AS  SET @dwMinVer = 2
```

The SP returns a **hardcoded operator-configured value** (currently
2). Modern (pre-fix) ran:

```sql
SELECT TOP 1 "dwBetaVer" FROM "TPREVERSION" ORDER BY "dwBetaVer"
```

i.e. the lowest pre-version row in the table — completely unrelated
to the operator cutoff. On the restored DB this returned the lowest
beta row instead of `2`, so `CT_NEWPATCH_ACK.dwMinBetaVer` was wrong
on every deploy that had any TPREVERSION row with `dwBetaVer < 2`.

Fixed by calling the SP via a `DECLARE @v INT; EXEC TMinBetaVer
@dwMinVer = @v OUTPUT; SELECT @v` wrapper — captures the OUT param
in a local and returns it as a single-column rowset SOCI can bind.

## P-5 — MarkPreVersionComplete no-op (documented, not fixed)

Legacy calls `{CALL TPreCompleteAdd(?)}` from `OnCT_PREPATCHCOMPLETE_
REQ`. The SP is **not deployed** on the restored DB (`sys.objects`
returns no row for `TPreCompleteAdd`). Modern correctly observes
this — it just logs the request and closes the session.

Operators promoting a pre-version row into TVERSION today do it by
hand:

```sql
INSERT INTO TVERSION (dwVersion, szPath, szName, dwSize, dwBetaVer)
SELECT  dwBetaVer, szPath, szName, dwSize, dwBetaVer
FROM TPREVERSION WHERE dwBetaVer = <ver>;
DELETE FROM TPREVERSION WHERE dwBetaVer = <ver>;
```

If the SP is ever shipped, swap the no-op block in
`PatchRepository::MarkPreVersionComplete` for the equivalent `EXEC
TPreCompleteAdd :v` call.

## P-6 — Stale-client sweep (documented, not fixed)

Legacy `OnCT_SERVICEMONITOR_ACK` walks the session map every monitor
heartbeat and closes any SESSION_CLIENT whose `m_dwTick` (= time the
connection was accepted, never updated) is older than 60 seconds:

```cpp
for(it=m_mapTSESSION.begin(); it!=m_mapTSESSION.end(); it++)
  if((*it).second->m_bSessionType == SESSION_CLIENT &&
     (*it).second->m_dwTick &&
     GetTickCount() - (*it).second->m_dwTick > 1000 * 60)
    CloseSession((*it).second);
```

This reaps client connections that opened the TCP socket but never
completed the patch handshake. Modern relies on Boost.Asio's own
read-timeout + EOF detection; in practice a half-open connection
disappears as soon as the OS-level keepalive fires, but the explicit
60-second cap is more aggressive.

Worth porting when we add operational metrics — until then leave it
unported and document so future work doesn't re-discover the gap.
