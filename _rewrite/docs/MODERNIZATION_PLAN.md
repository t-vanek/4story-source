# C++ Modernization Plan — 4Story Server

Date: 2026-05-18
Status: Phase 0 — Foundation (in progress)
Replaces: abandoned C# rewrite (commit `0511bd3`)

## TL;DR

Modernize the existing C++ server codebase (388 655 LOC, 8 servers + shared libs) from Visual Studio 2017 / Win32 / ATL / IOCP / Windows-only stack to **C++20, Boost.Asio, OpenSSL, CMake + vcpkg, unixODBC, spdlog** — running in Linux containers. Wire protocol preserved bit-for-bit; legacy clients continue to work unchanged.

**Estimated effort:** 9–15 months for a 2–3 person team. Architectural refactors (async DB in World, quest VM in Map, lock partitioning) are *not* included in that estimate — they're flagged as separate work items.

## Codebase Inventory

| Component | LOC | Files | Role |
|---|---:|---:|---|
| **TMapSvr** | 112 843 | 136 | Gameplay engine — entity, AI, quests, combat |
| **TWorldSvr** | 38 851 | 30 | Cluster coordinator — char persistence, guild, war |
| **TLoginSvr** | 11 444 | 37 | Auth + lobby |
| **TControlSvr** | 7 285 | 23 | GM/admin dashboard |
| **TLogSvr** | 3 908 | 17 | UDP audit log aggregator |
| **TPatchSvr** | 3 825 | 30 | Client patch file transfer (has own IOCP, not TNetLib) |
| **TBRSvr** | 92 | 3 | **Empty shell** — BR feature compiled into TMapSvr via `#ifdef BR_COMPILE_MODE` |
| **TBoWSvr** | 92 | 3 | **Empty shell** — same pattern with `#ifdef BOW_COMPILE_MODE` |
| Lib/Own/TNetLib | 4 489 | 23 | Shared network library — IOCP, packet codec, ODBC base |
| Lib/Own/TProtocol | 8 778 | 8 | Wire protocol definitions (CS/SS/CT/MW/DM message IDs) |

## Critical Path Insight

The Win32-specific networking infrastructure (IOCP, packet framing, crypto, ODBC base) lives **almost entirely in `Lib/Own/TNetLib/`**. Modernizing this one library unlocks all servers except TPatchSvr (which has its own IOCP loop).

Win32 surface area (from grep):
- `WSARecv`/`WSASend`/`CreateIoCompletionPort` — only in `Session.cpp:385,417,574` + `TPatchSvr/Session.cpp`
- `CRITICAL_SECTION` — 11 files, RAII wrapper `CCSLock` (alias `SMART_LOCKCS`) already exists in `TNetDef.h:130-146` but not used everywhere
- `CryptAcquireContext`/`CALG_RC4` — `CryptographyExt.cpp` + `TLogSvr/RegCrypt.cpp`
- ATL `CString` — **1 112 occurrences across ~50 files** (biggest mechanical work)
- ODBC (`DEFINE_QUERY`, `CSqlDatabase`) — 7 server files + TNetLib base

## Target Stack

| Layer | Current | Target |
|---|---|---|
| Build | VS 2017 v141, `.vcxproj` | CMake 3.20+ + vcpkg |
| C++ standard | C++14-ish (mostly C++98 patterns) | C++20 (coroutines for async I/O) |
| Async I/O | Win32 IOCP | Boost.Asio (epoll/io_uring on Linux, IOCP on Windows — same code) |
| Threading | `CRITICAL_SECTION`, `CreateThread` | `std::mutex`, `std::shared_mutex`, `std::jthread` |
| Time | `GetTickCount`, `CTime` | `std::chrono` |
| Strings | ATL `CString`, `_T()`, `LPCTSTR`, `lstrcpy` | `std::string`, `std::string_view`, `std::format`, `snprintf` |
| Crypto | Win32 CryptoAPI | OpenSSL EVP (already vendored at `Lib/3rdParty/openssl/`) |
| SQL | ODBC via `CSqlDatabase` | **Keep ODBC, link unixODBC on Linux** (standard API, MSSQL has native Linux ODBC driver) |
| Logging | `LogEvent`, `OutputDebugString`, `ATLTRACE`, `printf` | spdlog |
| Config | `CRegKey` (Windows registry) | toml++ (typed config file) |
| Service framework | `CAtlServiceModuleT` | Plain `main()` + systemd unit (Linux) / NSSM (Windows) |
| Anti-cheat (3rd party) | Apex, HShield, XTrap, NPGame (Windows-only proprietary) | Out of scope — server modernization keeps API hooks for these as no-ops on Linux |

## Phase Plan

```
Phase 0  Foundation                                   ~3 weeks    [in progress]
         CMake/vcpkg skeleton + safety fixes + CI

Phase 1  TNetLib modernization                        ~6 weeks
         IOCP → Asio, lock RAII, crypto wrapper

Phase 2  TPatchSvr + TLoginSvr                        ~6 weeks
         Smoke test of new infrastructure on smallest servers

Phase 3  TLogSvr + TControlSvr                        ~4 weeks
         Parallel work — neither blocks the other

Phase 4  TWorldSvr                                    ~8–12 weeks
         Includes async DB + lock partitioning (architectural)

Phase 5  TMapSvr                                      ~12–24 weeks
         Largest, gameplay risk, may need staged refactor

Phase 6  Cleanup                                      ~1 week
         Remove TBR/TBoW shells, dead code, conditional compile flags
```

## Library Scope Clarification (post-inventory)

Per-vcxproj grep across `Server/T*Svr/` shows which `Lib/Own/` modules are actually used by server processes (vs. client/Tools-only):

| Lib | LOC | Used by servers? | Scope |
|---|---:|---|---|
| **TNetLib** | 4 499 | **7 servers** (canonical at `Lib/Own/TNetLib/TNetLib/`) | **In scope — Phase 1** |
| **TProtocol** | 8 778 | **8 servers** (mostly headers) | **In scope — Phase 1** |
| HwidLib | 1 533 | 0 servers — 11 refs in Client/Tools | Out of scope (client) |
| TCML | 5 332 | 0 servers — 300 refs in Client/Tools | Out of scope (client) |
| TChart | 15 342 | 0 servers — 406 refs in Client/Tools | Out of scope (client) |
| TComp | 13 047 | 0 servers — used by Tools | Out of scope (Tools) |
| TachyonControl | 50 328 | 0 servers — GUI lib for Tools | Out of scope (Tools) |
| Engine Lib | 0 | (empty in tree) | N/A |

Server modernization targets `TNetLib` + `TProtocol`. Other libs stay as-is.

### TNetLib consolidation (DONE — commit `dd18d8d`)

Two TNetLib copies existed (`Server/TNetLib/` v140 + `Lib/Own/TNetLib/TNetLib/` v141, semantically identical bar formatting). Consolidated onto canonical (v141) path. TMapSvr.vcxproj re-pointed, stale prebuilt `Server/Lib/TNetLib.lib` removed.

### TNetLib Linux build attempt (Phase 1 kickoff — this commit)

Added `CMakeLists.txt` at repo root + `Lib/Own/TNetLib/CMakeLists.txt`. CMake configures cleanly with GCC 13 / CMake 3.28; build of the `tnetlib` target on Linux fails at `TNetLib.h:4` (`#include <winsock2.h>`) — expected and useful. Full inventory of headers / APIs that block Linux compilation:

| Win32 dependency | Headers / APIs | Linux replacement |
|---|---|---|
| Socket APIs | `<winsock2.h>`, `SOCKET`, `WSARecv`/`WSASend` | `<sys/socket.h>`, `<netinet/in.h>`, `<arpa/inet.h>` + Boost.Asio for async |
| IOCP extensions | `<mswsock.h>`, `AcceptEx`, `ConnectEx`, `CreateIoCompletionPort`, `OVERLAPPED` | Boost.Asio (epoll/io_uring backend) |
| Crypto API | `<wincrypt.h>`, `<Security.h>`, `CryptAcquireContext`, `HCRYPTKEY` | OpenSSL EVP (vendored at `Lib/3rdParty/openssl`) |
| NetBIOS | `<nb30.h>` (`GetComputerName` family) | POSIX `gethostname()` |
| ATL | `<atlbase.h>`, `<atlstr.h>`, `<atltime.h>` — `CString` (14 sites), `CTime` (4) | `std::string`, `std::chrono`, `using namespace` removed from headers |
| MS ODBC headers | `<SQL.h>`, `<SQLExt.h>` | unixODBC ships the same headers — no change needed, just install `libodbc2-dev` |

The `using namespace ATL;` + `using namespace std;` at `TNetLib.h:16-17` are global header pollution — a separate cleanup item.

## Phase 0 — Foundation

Goal: cross-platform build skeleton + safety fixes, **no semantic changes**.

| Task | Files | Status |
|---|---|---|
| Root `CMakeLists.txt` + per-project subdirs | New | TODO |
| `vcpkg.json` with: openssl, boost (asio, system), zlib, spdlog, toml++ | New | TODO |
| Hardcoded `C:\4s\...` paths → env var/config | `Lib/Own/TNetLib/SqlBase.cpp:106,170`, others | TODO |
| Fix `lstrcpy` off-by-one in `UdpSocket.cpp` (50-char name → szKey[50] overflow) | `Server/TLoginSvr/UdpSocket.cpp:114-130` | **In progress** |
| Fix `sprintf` without bounds | `Server/TControlSvr/PlatformUsage.cpp:58` | **In progress** |
| Manual `EnterCriticalSection` → existing `SMART_LOCKCS` macro | ~140 sites (audit undercounted due to ISO-8859 encoding in source files) | **Partial** — 4 files done in this pass (TLoginSvr/UdpSocket, TLoginSvr/DebugSocket, TMapSvr/DebugSocket, TControlSvr/DebugSocket, TPatchSvr/Session.Close); ~135 sites remaining in TMapSvr/TWorldSvr/TControlSvr/TLogSvr |
| clang-tidy baseline (modernize-* + bugprone-*) | Whole tree | TODO |
| CI pipeline (GitHub Actions): Windows + Linux build matrix | New | TODO |

**Exit criteria:** Codebase compiles on Linux with CMake (at least TNetLib + one minimal server). CI runs on every push. No safety regressions.

## Phase 1 — TNetLib

Key files and their refactor scope:

| File | LOC | Refactor |
|---|---:|---|
| `Session.cpp` | 612 | IOCP loop → `asio::ip::tcp::socket` + C++20 coroutines |
| `Packet.cpp` | 507 | Keep XOR codec bit-for-bit (wire compat), modernize `BYTE*` → `std::span<std::byte>` |
| `CryptographyExt.cpp` | 196 | Win32 `CryptCreateHash`/`CALG_RC4` → OpenSSL `EVP_CIPHER_CTX` with `EVP_rc4()` |
| `SqlBase.cpp` | 369 | Keep ODBC API, fix `strcpy`→`strncpy`, Win32 paths→config, MFC `CTime`→`std::chrono` |

**Testing strategy:** wire-faithful round-trip tests. Port the RC4 RFC 6229 vectors, XOR layer tests, and header obfuscation tests from the abandoned C# rewrite (documented in `_rewrite/docs/COMPLETENESS_ANALYSIS.md §6`) to Catch2/GTest. These validate that the modernized TNetLib speaks the same wire protocol as the legacy server.

## Phase 2-5 — Per-Server Order

Recommended order based on dependency graph + risk:

```
1. TPatchSvr      — isolated, has own IOCP, simplest, validates new Asio infrastructure
2. TLoginSvr      — small, contained, validates UDP audit + ODBC + auth flow end-to-end
3. TLogSvr        — consumer of TLoginSvr UDP, isolated dialog→headless refactor
4. TControlSvr    — admin/GM dashboard, parallel-able with TLogSvr
5. TWorldSvr      — cluster coordinator, must precede TMapSvr (protocol contract)
6. TMapSvr        — gameplay, largest, highest risk
```

## Phase 6 — Cleanup

- Delete `Server/TBRSvr/` and `Server/TBoWSvr/` — empty shells (no `.cpp`). BR/BoW logic lives in TMapSvr behind compile flags.
- Convert `#ifdef BR_COMPILE_MODE` / `#ifdef BOW_COMPILE_MODE` → runtime feature flag from config. Eliminates two parallel compile targets and the test-matrix explosion they cause.
- Remove dead `// LogEvent(...)` / `#if 0` blocks.

## Technical Debt Hit List

From the 4-axis audit. **Caveats first**: bulk `new`/`delete` grep ratio (338 vs 86) is misleading — destructor-paired `new` is RAII, and IOCP packet handoff (e.g., `Say(packet)` → send queue → delete in dispatcher) is a deliberate ownership transfer, not a leak. Audit by lifecycle, not by raw count.

### Real security hot spots

| Issue | Location | Fix |
|---|---|---|
| `lstrcpy` off-by-one — 50-char username → 51 bytes into 50-byte buffer | `TLoginSvr/UdpSocket.cpp:117` (`szKey[0]`, 50-byte field; `m_strUserID` ≤ MAX_NAME=50) | `lstrcpyn(dst, src, sizeof_field)` |
| `sprintf` without bounds | `TControlSvr/PlatformUsage.cpp:58` (`szPath`, MAX_PATH_BUFFER) | `snprintf` |
| `EnterCriticalSection` without RAII — exception → deadlock | 16 sites, 4 files (`UdpSocket.cpp`, `DebugSocket.cpp` ×3 servers) | Replace with `SMART_LOCKCS(&cs)` (RAII wrapper already exists at `TNetDef.h:33`) |

### Mechanical refactors (large but no design decisions)

| Issue | Count | Effort |
|---|---:|---|
| `CString`/`LPCTSTR` → `std::string`/`std::string_view` | 1 112 | 2–3 weeks (clang-tidy + manual review, parallelizable) |
| `_T(`/`TCHAR`/Win32 UNICODE macros (blocker for cross-platform) | spread across CString files | 1 week (sed-based) |
| Manual critical sections → `SMART_LOCKCS` | **~140** (audit's 16 was wrong — grep missed ISO-8859 source files; real top offenders: `TMapSvr/TMapSvr.cpp` 42, `TWorldSvr/TWorldSvr.cpp` 31, `TControlSvr/TControlSvr.cpp` 23, `TLogSvr/XPtrList.cpp` 20) | 1–2 weeks |
| C-style casts → `static_cast`/`reinterpret_cast` | 85 | 3–5 days (clang-tidy) |
| `lstrcpy`/`strcpy`/`sprintf` → `lstrcpyn`/`strncpy`/`snprintf` | 66 | 2–3 days |

### Design refactors (judgment + time)

| Issue | Location | Why |
|---|---|---|
| DMSender packet lifecycle audit (48 `new`, 0 visible `delete`) | `Server/TWorldSvr/DMSender.cpp` | IOCP handoff pattern — verify deletion happens in DB thread completion, then introduce `std::unique_ptr` or intrusive ref-count |
| `printf`/`ATLTRACE` → spdlog | 52 sites (excl. generated parser) | 1 week |
| Duplicated handler boilerplate across servers (`CSHandler.cpp`) | `TLoginSvr`, `TMapSvr` | Post-modernization refactor — extract base dispatcher |

## Architectural Risks (beyond language port)

These surfaced during analysis but are *not* solved by modernizing language/runtime. Tracked as separate work items.

1. **TWorldSvr single DB thread** (`m_hDB`) services all TMapSvr instances → bottleneck, blocking I/O. Fix: async DB + per-shard write queue with connection pool.
2. **TWorldSvr global maps** (`m_mapTCHAR`, `m_mapTGuild`) under one lock → race conditions across TMapSvr instances. Fix: partitioning (per-guild grain, per-char actor model).
3. **`SSHandler.cpp` 14 615 LOC monolithic switch** in TWorldSvr (and 20 387 in TMapSvr) — each new packet ID forces recompile of every server. Fix: register-based dispatcher, possibly with protobuf or flatbuffers for schema versioning.
4. **TMapSvr quest engine is C++ code** (`Quest*.cpp` × 24 files) — each quest change = recompile. Fix: quest VM (Lua via sol2) or data-driven (YAML + interpreter).
5. **BR/BoW compile flags** create two parallel binary worlds. Fix: runtime feature flag from config. (Already on the Phase 6 cleanup list.)
6. **DMSender ownership pattern** — IOCP-era raw-pointer handoff is fragile under exceptions. Fix: `std::unique_ptr<PACKETBUF>` + explicit move semantics.

## References

- Wire protocol details: `_rewrite/docs/PROTOCOL.md`
- Packet ID inventory: `_rewrite/docs/packet-ids.csv` (1 542 IDs)
- Stored procedure inventory: `_rewrite/docs/SCHEMA.md` + `_rewrite/docs/schema/procs/`
- C# rewrite postmortem (what was learned, what was missing): `_rewrite/docs/COMPLETENESS_ANALYSIS.md`, `_rewrite/docs/GAP_ANALYSIS.md`
