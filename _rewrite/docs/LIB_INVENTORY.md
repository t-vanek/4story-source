# `Lib/` inventory — what's ported, what's relevant, what's skipped

Date: 2026-05-17. Question: do we have everything from `Lib/` covered in the .NET rewrite?

## Short answer

**Yes for server-relevant parts.** Of the ~131k LOC in `Lib/Own/`, only ~3.6k LOC is consumed by the server processes. The remaining ~127k LOC is **client-only** (DirectX rendering, MFC GUI, anti-cheat client modules) and out of scope for the server rewrite.

## `Lib/Own/` — first-party libraries

| Library | LOC | Server uses? | Status |
|---------|----:|:------------:|--------|
| **TNetLib** | ~2.0k | ✅ Yes (all server processes) | **Ported (protocol+crypto)**. SQL parts replaced by EF Core. See below. |
| **TProtocol** | ~8.8k (headers only) | ✅ Yes (all server processes) | **Extracted** as `MessageId.g.cs` (1542 enum members). |
| **HwidLib** | ~1.5k | ✅ Yes (TLoginSvr only) | **Not yet ported** — small, deferred to Phase 2. See §HwidLib below. |
| Engine Lib | ~46.5k | ❌ No | Client-only (DirectX 9 + MFC). **Skip entirely** for server rewrite. |
| TCML | ~5.3k | ❌ No | Client-only (lex/yacc parser, likely for in-game scripts). **Skip.** |
| TChart | ~15.3k | ❌ No | Client-only (loads `T*CHART` data files for local rendering — server reads same data from DB). **Skip.** |
| TComp | ~13.0k | ❌ No | Client-only (GDI UI components — TButton, TCombo, TEdit, THtmlDocWnd, etc.). **Skip.** |
| TachyonControl | ~50.3k | ❌ No | Client-only (MFC dock bars, color dialogs, custom UI controls). **Skip.** |

### Verification (how we know server doesn't use them)

```
Server includes "(TCML|TChart|TComp|Engine Lib|TachyonControl)":     ZERO matches
Server references "CTCML|CTChart|TCMLParser":                        ZERO matches
Server references "HwidManager":                                     5 files (TLoginSvr only)
```

Server `*.vcxproj` files do not link against any of the client-only `*.lib` outputs either — they only depend on `TNetLib.lib` (verified by inspecting `Server/Lib/`).

---

## Drill-down: TNetLib

`Lib/Own/TNetLib/TNetLib/` — 12 files, ~2k LOC. Split by purpose:

| File | Purpose | Status |
|------|---------|--------|
| `Packet.h` / `Packet.cpp` | Wire framing + XOR body + header obfuscation + INT64 checksum | ✅ Ported → `FourStory.Protocol.PacketHeader`, `XorLayer`, `PacketReader/Writer` |
| `Session.h` / `Session.cpp` | Per-connection state, encrypt/decrypt orchestration, IOCP socket I/O | 🟡 **Partial**: crypto orchestration ported (`PacketCodec`); TCP/IOCP framing NOT YET ported (planned `PacketSession` with `System.IO.Pipelines`) |
| `CryptographyExt.h` / `CryptographyExt.cpp` | Win32 CryptoAPI wrapper for RC4 + MD5 hash key derivation | ✅ Ported → `Rc4Layer` (hand-rolled RC4) + `Rc4Layer.DefaultKey` (MD5 of `KeyTable.RawSecretKey`) |
| `Rijndael.h` / `Rijndael.cpp` | AES-128 in pure C (dead code — never referenced) | ❌ **Not ported** — dead code in legacy too. Skip. |
| `BindDesc.h` / `BindDesc.cpp` | ODBC `SQL_PARAM_*` binding descriptors | ❌ **Not ported, replaced** — EF Core handles parameter binding. |
| `SqlBase.h` / `SqlBase.cpp` | Base ODBC handle wrapper | ❌ **Not ported, replaced** — EF Core / `DbConnection`. |
| `SqlDatabase.h` / `SqlDatabase.cpp` | Connection pool, transaction control | ❌ **Not ported, replaced** — EF Core `DbContext` + connection pooling. |
| `SqlDirect.h` / `SqlDirect.cpp` | Direct SQL execution (no SP) | ❌ **Not ported, replaced** — `DbContext.Database.ExecuteSqlRaw` (rarely needed). |
| `SqlQuery.h` / `SqlQuery.cpp` | Stored-procedure invocation framework (`CSqlQuery`, `BEGIN_PARAM`, etc.) | ❌ **Not ported, replaced** — EF Core LINQ. The 323 stored procs from the DB will become C# code. |
| `ErrorCode.h` | `EC_NOERROR` and other return codes | 🟡 **TBD** — port to `enum class EcError` in `FourStory.Shared` when handlers start using it. |
| `TNetDef.h` | Port constants, thread limits, completion keys | ✅ Ported → `ProtocolConstants` (ports). Thread/IOCP constants N/A for .NET. |
| `TNetLib.h` | Umbrella header | ❌ No equivalent needed in .NET. |

**Verdict for TNetLib**: protocol/crypto layer ✅ done. SQL layer **intentionally not ported** — that's the whole point of moving to EF Core. TCP/socket I/O still missing (planned `PacketSession`).

---

## Drill-down: HwidLib

`Lib/Own/HwidLib/HwidLib/` — 7 small files, ~1.5k LOC. **Mostly client-side** (collects hardware info on the player's machine), but the **server has its own copy of the validator** at `Server/TLoginSvr/HwidManagerSvr.{h,cpp}`:

```cpp
// Server/TLoginSvr/HwidManagerSvr.h
class HwidManagerSvr
{
public:
    CString WinDeviceSegment;
    CString WinSerialSegment;
    CString WinUserSegment;
    CString MoboSegment;
    CString CpuSegment;

    BOOL GetSegmentChecksum(DWORD hwidParams, CString& hwidChecksumResult);
};
```

The server side just stores HWID segments received from the client and computes a checksum to validate them against a banlist (`CSPCheckHwid` stored proc).

**Plan for rewrite (Phase 2)**:
- Port `HwidManagerSvr` (server side) → `FourStory.Login.HwidValidator`. ~30 lines of C#.
- The full client-side HwidLib (CPU/Motherboard/Windows info collection) is **not needed** until client rewrite (Phase 6).
- **Recommendation**: in the modernized version, replace HWID anti-cheat with one of:
  - Just-server-side: account-based rate limiting + IP reputation (Cloudflare / similar).
  - Modern client-attested HWID: collect from client via signed JSON payload.
  - Third-party: EasyAntiCheat / BattlEye SDK (Phase 6+).

**Status**: 🟡 Not yet ported. Trivial when needed.

---

## Drill-down: TProtocol

`Lib/Own/TProtocol/include/` — 8 header files, ~8.8k LOC of `#define`s and payload comments. NO `.cpp` files (header-only).

| File | Status |
|------|--------|
| `ProtocolBase.h` | ✅ Ported → `ProtocolConstants` (base offsets, version, server group IDs, ports) |
| `SSProtocol.h` (45 IDs) | ✅ Ported → `MessageId.g.cs` |
| `CSProtocol.h` (740 IDs) | ✅ Ported → `MessageId.g.cs` |
| `CTProtocol.h` (139 IDs) | ✅ Ported → `MessageId.g.cs` |
| `DMProtocol.h` (209 IDs) | ✅ Ported → `MessageId.g.cs` |
| `MWProtocol.h` (409 IDs) | ✅ Ported → `MessageId.g.cs` |
| `LogPacket.h` (~79 IDs) | 🟡 **Not yet ported** — TLogSvr UDP logging packets. Phase 3 work. |
| `NetCode.h` | 🟡 **Not yet ported** — error codes (`EC_NOERROR`, etc.). Port when handlers need them. |

**Verdict for TProtocol**: packet ID catalog ✅ done. Payload structures (inline comments in headers) are extracted **on demand** per packet — only CS_LOGIN payloads fully documented so far (sufficient for Phase 2).

---

## `Lib/3rdParty/` — third-party libraries

| Library | Purpose | Server uses? | .NET replacement |
|---------|---------|:------------:|------------------|
| **DirectX9 (June 2010)** | 3D graphics | ❌ Client only | N/A (server rewrite scope) |
| **HShield** | NHN Hyper Shield anti-cheat | ❌ Client only (server checks via packets) | Replace with modern anti-cheat in Phase 6 |
| **XTrap** | XTrap anti-cheat | ❌ Client only | Same as HShield |
| **Apex** | Apex middleware (client+server) | ⚠️ Probably proprietary middleware — both sides | **Skip, can't redistribute.** Replace with custom or modern alternative |
| **NPGame** | NHN game framework SDK (client+server) | ⚠️ NHN platform integration | **Skip, can't redistribute.** Most NPGame APIs are auth/billing — replaced by our own services |
| **VLD** | Visual Leak Detector | ❌ Dev tool (debug builds) | N/A — .NET has `dotnet-counters`, `dotnet-trace` |
| **dbghelp** | Windows debug helpers (minidumps) | ⚠️ Used in `TMiniDump.cpp` (crash dumps) | .NET has `System.Diagnostics.Process.GetCurrentProcess` + `Microsoft.Diagnostics.NETCore.Client` for dumps, or just use `dotnet-dump`. Skip — not needed for protocol layer. |
| **openssl** | Crypto primitives | ⚠️ Used in client SHA1/SHA512/RegCrypt | Replaced by `System.Security.Cryptography` |
| **zlib** | Compression | ❌ Not directly referenced by server | Replaced by `System.IO.Compression` if needed |

**3rdParty verdict**:
- **All anti-cheat (HShield/XTrap/Apex/NPGame)** is proprietary, can't be redistributed, and is mostly client-side anyway. Server-side hooks (if any) will be re-engineered cleanly during Phase 6.
- **DirectX, VLD, dbghelp** are platform/dev tooling — not part of server logic.
- **openssl, zlib** functionality is fully covered by the .NET BCL.

---

## What's missing right now (gap analysis)

To get to a **runnable Phase 2 TLoginSvr**, we additionally need:

1. **`PacketSession`** — TCP framing over `System.IO.Pipelines`. Planned, designed in COMPLETENESS_ANALYSIS.md §4.
2. **`HwidValidator`** — port of `HwidManagerSvr` (~30 LOC). Trivial.
3. **`ErrorCodes`** — port `NetCode.h` to a C# enum. Mechanical.
4. **`FourStory.Persistence`** — EF Core `DbContext` scaffolded from MSSQL. Done via `dotnet ef dbcontext scaffold`.
5. **`FourStory.Login.Auth`** — port of `CTLoginSvrModule::OnCS_LOGIN_REQ` + the `TLogin` stored procedure body (in C#, not as a DB SP).

Items 2–5 are Phase 2 deliverables. Item 1 is the last bit of Phase 1.

## What we are NOT going to port (consciously)

- All MFC/GDI/DirectX client code (~125k LOC) — replaced by future client engine (Phase 6, separate decision).
- All proprietary anti-cheat — replaced or skipped.
- The ODBC SQL framework (`SqlBase/SqlDatabase/SqlDirect/SqlQuery/BindDesc`) — replaced by EF Core. **This is intentional and ~80% of the value of the rewrite.**
- The 323 stored procedures — their business logic will be **re-implemented in C#** inside service classes, not literally translated to PL/pgSQL.
- `TCML` (lex/yacc client config parser) — unless we find a runtime dependency, skip.
- `TChart` (client static data loader) — server reads same data from DB.

## Bottom line

**Of the legacy `Lib/` directory, we have ported or have a defined plan for 100% of what the server actually uses.** The "missing" ~127k LOC is all client/GUI/anti-cheat code that doesn't belong in a server rewrite.

The honest current status of server-relevant ports:

```
TNetLib protocol+crypto:   ████████░░  80%   (PacketSession TCP framing remains)
TProtocol packet IDs:      ██████████ 100%   (all 1542 IDs as MessageId enum)
TProtocol payloads:        █░░░░░░░░░  10%   (only CS_LOGIN — rest extracted on-demand)
HwidLib (server side):     ░░░░░░░░░░   0%   (Phase 2, ~30 LOC port)
NetCode (error codes):     ░░░░░░░░░░   0%   (Phase 2, mechanical)
SQL layer:                 N/A — replaced by EF Core (Phase 2)
```
