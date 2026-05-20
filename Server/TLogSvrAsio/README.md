# TLogSvrAsio — modernized 4Story audit log collector

Production port of `Server/TLogSvr/`. UDP-only collector that receives
the legacy `_UDPPACKET` audit frame the login + world servers emit, and
persists each record into the `TLOG_AUDIT` table. Wire-faithful with
the existing legacy ingestors — the modernized `TLoginSvrAsio` emits
the same struct (via `UdpAuditLogger`) that the original `TLogSvr` was
designed to consume.

## Status — feature-complete vs legacy

| Area | State |
|---|---|
| UDP receiver | ✅ Boost.Asio `async_receive_from`; bound `0.0.0.0:2000` by default |
| `_UDPPACKET` decode | ✅ wire-faithful struct match; `LogPacket.h`-compatible offsets |
| `_LOG_DATA_` parse | ✅ 11 int search keys + 7 string search keys + opaque payload |
| Sink interface | ✅ `ILogSink` — production `SociLogSink` + dev `StdoutLogSink` |
| SOCI INSERT | ✅ INSERT INTO `TLOG_AUDIT` with `i_null` indicator for empty blobs |
| Dialect-aware blob | ✅ MSSQL `CONVERT(VARBINARY)`, PG `CAST(... AS bytea)`, SQLite pass-through |
| Schema | ✅ `schema/tlog-audit.sql` — single table, `LT_*` columns matching legacy SP layout |
| Schema validator | ✅ `tlogsvr::db::ValidateAuditSchema` boot-time fail-fast on missing LT_* columns |
| Drop counters | ✅ `packets_received` + `drops_bad_format` via spdlog |
| `/healthz` endpoint | ✅ optional `[health]` block in TOML; reuses `fourstory::ops::HealthEndpoint` |
| Graceful shutdown | ✅ SIGINT/SIGTERM → `io.stop()` |
| Tests | ✅ pure-unit decoder test + SOCI integration test (skips without DB env var) |

## Wire format

Inbound datagram layout (matches
`Server/TLoginSvrAsio/services/udp_audit_logger.cpp` emitter and the
legacy `Server/TLoginSvr/UdpSocket.cpp` consumer):

```
struct _UDPPACKET {
    DWORD    dwSize;            // total UDP payload size
    DWORD    dwID;              // LOG_DATA discriminator
    BYTE     bData[...];        // _LOG_DATA_ blob
};

struct _LOG_DATA_ {             // ~/LogPacket.h
    DbTimestamp  timestamp;     // 16 bytes — wire-faithful CTime
    DWORD        dwServerID;
    char         szClientIP[16];
    DWORD        dwAction;
    WORD         wMapID;
    INT32        iPosX, iPosZ, iPosY;
    INT64        ilSearchInt[11];
    char         szSearchStr[7][32];
    DWORD        dwFormat;
    BYTE         bPayload[0..512];
};
```

A malformed datagram (too short, wrong magic, payload > 512 bytes)
increments `drops_bad_format` and is dropped silently. We don't ack
UDP datagrams — the legacy collector didn't either; clients accept
audit-log loss as a deliberate trade-off vs blocking the request
path on a logging backend.

## Sinks

| Sink | Use case |
|---|---|
| `SociLogSink` | Production — INSERTs each record into `TLOG_AUDIT`. Schema in `schema/tlog-audit.sql`. |
| `StdoutLogSink` | Dev — pretty-prints each record through spdlog. Wired automatically when `[database]` is empty in TOML. |

`Write()` is synchronous; if the DB is slow we'd rather drop UDP packets
than queue unbounded. The legacy SP-based ingestor had the same shape.

## Configuration

See `tlogsvr.example.toml` for the annotated reference. Minimal
production config:

```toml
[server]
bind = "0.0.0.0"
port = 2000                          # legacy UDP collector port
target_table = "TLOG_AUDIT"          # destination table

[log]
level = "info"

[health]
port = 8800                          # /healthz HTTP endpoint; 0 to disable

[database]                           # TGLOBAL (or a dedicated audit DB)
backend = "odbc"
connection_string = "DRIVER={ODBC Driver 17 for SQL Server};SERVER=localhost;DATABASE=TGLOBAL_RAGEZONE;Trusted_Connection=yes;TrustServerCertificate=yes"
pool_size = 4
```

Operations note: production deploys typically point `[database]` at a
dedicated audit DB (separate from the live game DB) so log retention
policies don't touch player data.

## Schema validator

`tlogsvr::db::ValidateAuditSchema` runs once between pool creation
and UDP socket bind in `main.cpp`. It checks every `LT_*` column the
`SociLogSink` INSERT binds against `INFORMATION_SCHEMA.COLUMNS` and
throws `fourstory::db::SchemaError` listing each missing entry on a
mismatch. The configured `target_table` is also identifier-checked
(`[A-Za-z_][A-Za-z0-9_]{0,127}`) before it's inlined into the
INFORMATION_SCHEMA probe, so a malformed TOML value can't smuggle
SQL through.

Pair this with `schema/tlog-audit.sql` — applying that DDL once is
sufficient to satisfy the validator on a fresh DB. Operators running
the legacy date-partitioned `ITEMLOGTLyyyymmdd` schema will see the
validator refuse to start, which is the desired signal that a
one-time migration is needed.

## Schema

`schema/tlog-audit.sql` — single `TLOG_AUDIT` table with `LT_*` columns
matching what the legacy `_LOG_DATA_` stored procedure expected.
Apply once with `sqlcmd`:

```powershell
sqlcmd -S localhost -E -d TGLOBAL_RAGEZONE -i Server\TLogSvrAsio\schema\tlog-audit.sql
```

## Build

```powershell
cmake --build build --config Release --target tlogsvr_asio
build\bin\Release\tlogsvr_asio.exe --config Server\TLogSvrAsio\tlogsvr.toml
```

Links `fourstory_common` (SOCI pool) + `tnetlib_portable` (the
`_LOG_DATA_` struct definitions, shared with the login server's emitter).

## Smoke test

With the login server running and `tlogsvr_asio` listening on UDP:2000,
every successful `CS_LOGIN_REQ` against the login server emits a
`_UDPPACKET` that lands in `TLOG_AUDIT` within milliseconds. Verify with:

```sql
SELECT TOP 10 LT_LOGDATE, LT_ACTION, LT_KEY1, LT_CLIENTIP
FROM TLOG_AUDIT
ORDER BY LT_LOGDATE DESC;
```

## Tests

```sh
ctest --test-dir build -R tlogsvr --output-on-failure
```

* `tlogsvr_asio_log_decoder` — pure-unit decoder test. Builds
  `_UDPPACKET` / `_LOG_DATA_` frames byte-for-byte the way
  `UdpAuditLogger` emits them and round-trips them through
  `DecodeLogDatagram`. Covers happy-path, payload blob, wrong
  command, short datagram, declared-size > received, oversized
  payload (>512), zero-year timestamp sentinel, and embedded-NUL
  handling on fixed-width strings.
* `tlogsvr_asio_soci_log_sink` — integration test against a live
  MSSQL database. Skips cleanly when `TLOGSVR_TEST_MSSQL_CONN` is
  unset; covers schema-validator accept/reject (including the SQL
  injection guard on `target_table`) and a sink → DB round trip.

## What's intentionally not ported

| Legacy piece | Why skipped |
|---|---|
| `RegCrypt` registry config | Replaced by TOML |
| Per-action stored procedure family (`CSPLOG_*`) | Single parameterized INSERT covers the cases the live servers emit |
| Win32 IOCP UDP loop | Replaced by Boost.Asio `async_receive_from` |
