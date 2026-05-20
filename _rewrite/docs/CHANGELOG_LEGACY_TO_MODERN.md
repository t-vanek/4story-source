# Changelog — Legacy → Modernizovaný server cluster

Stav k 2026-05-19, branch `claude/analyze-legacy-changelog-fmm2D`.
Srovnání legacy `Server/T*Svr/` (Win32 / ATL / IOCP / VS 2017, ~389 kLOC)
proti modernizovaným `Server/T*SvrAsio/` + `Lib/Own/FourStoryCommon`.

Strategie: legacy zdrojáky zůstávají v repu netknuté a jsou autoritativní
pro odeslané chování. Modernizované binárky běží vedle nich, mluví
**bit-for-bit stejný wire protokol** s legacy klientem a sahají do
původních MSSQL databází (`TGLOBAL_RAGEZONE` + `TGAME_RAGEZONE`) bez
destruktivní migrace.

---

## 1. Globální změny stacku

| Vrstva | Legacy | Modernizovaný |
|---|---|---|
| Jazyk | C++14-ish (převážně C++98 styl) | **C++20** (Asio coroutines `co_await`) |
| Build | VS 2017 v141, `.vcxproj`, `.sln` | **CMake 3.20+** + **vcpkg** (manifest mode) |
| Platforma | Pouze Windows | **Linux + Windows** (cross-platform) |
| Async I/O | Win32 **IOCP** (`WSARecv`/`WSASend`/`CreateIoCompletionPort`) | **Boost.Asio** (epoll/io_uring na Linuxu, IOCP na Windows — stejný kód) |
| Threading | `CRITICAL_SECTION`, `CreateThread` | `std::mutex`, `std::shared_mutex`, `std::jthread` |
| Čas | `GetTickCount`, `CTime` | `std::chrono`, `asio::steady_timer` |
| Stringy | ATL `CString`, `_T()`, `LPCTSTR`, `lstrcpy` (1 112 výskytů) | `std::string`, `std::string_view`, `std::format` |
| Krypto | Win32 CryptoAPI (`CryptAcquireContext`, `CALG_RC4`) | **OpenSSL EVP** (RFC 6229 RC4) přes vcpkg |
| Hashy hesel | Plaintext / slabý hash v `TACCOUNT_PW` | **libbcrypt** (cost 10) s transparentním upgradem z plaintextu |
| SQL | ODBC přes `CSqlDatabase` (Win32) | **SOCI 4.x** + unixODBC (Linux) / MS ODBC (Windows) |
| Logování | `LogEvent`, `OutputDebugString`, `ATLTRACE`, `printf` | **spdlog** (strukturované logy, pluggable sinky) |
| Config | `CRegKey` (Windows Registry) | **toml++** (verzovatelný `.toml` soubor) |
| Service framework | `CAtlServiceModuleT` | plain `main()` + `systemd` unit / NSSM |
| Service manager shutdown | `SM_QUITSERVICE_REQ` IPC packet | `SIGINT`/`SIGTERM` přes `asio::signal_set` (+ legacy paket je stále uznán) |
| Testy | žádné — pouze ruční smoke testy | **CTest** + 10 in-process suite, SOCI suite skipne bez DB |

### Sdílené knihovny

| Lib | Legacy | Modernizovaný |
|---|---|---|
| **TNetLib** | `Server/TNetLib/` (v140) **a duplikovaně** `Lib/Own/TNetLib/TNetLib/` (v141) | Konsolidováno do `Lib/Own/TNetLib/` (commit `dd18d8d`) — modernizovaný `tnetlib::AsioSession` + `tnetlib::packet_codec` |
| **TProtocol** | Mix ATL `CString` v hlavičkách | Doplněn **`MessageId` enum + `NameOf()` generátor** (commit `765d82b`) |
| **OpenSSL** | Vendorovaný 0.9.8l (15 let starý, dead reference) | **Smazán** (commit `7b4b79e`) — vcpkg si tahá 3.x s `legacy` providerem pro RC4 |
| **FourStoryCommon** | — *(neexistovalo, kód byl duplikovaný napříč servery)* | **Nová statická knihovna** s `db::SessionPool`, `schema_validator`, `audit::*`, `smtp::*`, `ops::AdminShell/HealthEndpoint/RateLimiter/RegistryRefresher` — komitnuto v `60dccf7` |

---

## 2. TLoginSvr → TLoginSvrAsio

**Legacy:** `Server/TLoginSvr/` — 37 souborů, 11 453 LOC, ATL/IOCP/ODBC/Windows-only.
**Nový:** `Server/TLoginSvrAsio/` — 45 souborů, 9 464 LOC (vč. testů), Asio coroutines + SOCI + spdlog.
**Stav:** produkčně kompletní, 15/15 `CS_` + 5/5 `CT_*` handlerů.

### Pokrytí handlerů (15/15 wire handlerů + 5/5 control protokolu)

| Handler | Legacy chování | Nový stav |
|---|---|---|
| `CS_LOGIN_REQ` | `CSPLogin` / `CSPLoginJP` SP, IP block, duplicate kick | ✅ real — BCrypt + transparent upgrade, IP banlist, TUSERPROTECTED, llChecksum validace (algoritmus z `CSHandler.cpp:185-202`) |
| `CS_AGREEMENT_REQ` | `CSPAgreement` upsert do `TUSERINFOTABLE` | ✅ real — `IAuthService::SetAgreement` + per-session gate flip |
| `CS_GROUPLIST_REQ` | `CTBLGroupList` enumerace TGROUP | ✅ real — `IMapServerLocator::ListGroups` s živým TCURRENTUSER countem |
| `CS_CHANNELLIST_REQ` | `CTBLChannel` enumerace TCHANNEL | ✅ real — `IMapServerLocator::ListChannels` |
| `CS_CHARLIST_REQ` | `TCHARTABLE` + `TITEMTABLE` (equip slot) + guild join | ✅ real — items + guild fame + trailing `CS_BOWPLAYERNOTIFY_ACK` |
| `CS_CREATECHAR_REQ` | `TCreateChar` SP + starter inventory | ✅ real — TCHARTABLE + TALLCHARTABLE + starter items |
| `CS_DELCHAR_REQ` | password check + guild block + level-5 soft-delete | ✅ real — `ICharService::Delete` + `IAuthService::VerifyPassword` |
| `CS_START_REQ` | `TSERVER`+`TIPADDR` JOIN + BR override | ✅ real — TFindServerID port (TSVRCHART + TCHANNELCHART + TSPAWNPOSCHART fallback), BR/BOW shard override |
| `CS_VETERAN_REQ` | Čte `TVETERANCHART` thresholds | ✅ real — cached `TVETERANCHART` s 30s refreshem |
| `CS_TERMINATE_REQ` | `TLogoutAll` na shodu magic key | ✅ real — magic check + `ISessionTerminator` cleanup |
| `CS_HOTSEND_REQ` | Exec-file integrity check | ✅ silent no-op — anti-cheat **mimo scope by design** |
| `CS_SECURITYCONFIRM_ACK` | `LR_SECURITY` 2FA flow (v legacy **dead code**) | ✅ real — TSECURECODE compare + `ISmtpClient` issue path (vylepšeno proti legacy) |
| `CS_TESTLOGIN_REQ` | Debug-only TTESTLOGINUSER | ✅ real — gate `test_handlers_enabled = false` |
| `CS_TESTVERSION_REQ` | Vrací protocol version | ✅ real |
| `SM_QUITSERVICE_REQ` | Service manager shutdown signal | ✅ real — wire trigger pro `io.stop()` (vedle SIGINT/SIGTERM) |
| `CT_SERVICEMONITOR_ACK` | Posílá session count control serveru | ✅ real — `IConnectionRegistry::Count()` |
| `CT_SERVICEDATACLEAR_ACK` | Přestaví `m_mapACTIVEUSER` z `m_mapTUSER` | ✅ no-op (registry je canonical, žádná derived mapa) |
| `CT_CTRLSVR_REQ` | Heartbeat | ✅ |
| `CT_EVENTUPDATE_REQ` | GM event sync (`m_mapEVENT`) | ✅ — `IEventRegistry` upsert/remove |
| `CT_EVENTMSG_REQ` | Event broadcast | ✅ |

### Servisní vrstva (nová abstrakce)

Legacy měl všechno splácané v `CTLoginSvrModule` + statické mapy
(`m_mapTUSER`, `m_mapACTIVEUSER`) + přímá ODBC volání v handlerech.

Nový server používá **service interface pattern** — každá doména má
SOCI produkční impl a `Fake*` test impl:

| Interface | Produkce | Test fake |
|---|---|---|
| `IAuthService` | `SociAuthService` | `FakeAuthService` |
| `ICharService` | `SociCharService` (TGLOBAL + TGAME split) | `FakeCharService` |
| `IMapServerLocator` | `SociMapServerLocator` | `FakeMapServerLocator` |
| `ISessionTerminator` | `SociSessionTerminator` | `FakeSessionTerminator` |
| `IConnectionRegistry` | `LocalConnectionRegistry` | (in-process canonical) |
| `IAuditLogger` | `SpdlogAuditLogger` + `UdpAuditLogger` decorator | — |
| `IEventRegistry` | `LocalEventRegistry` | — |
| `ISmtpClient` | `SpdlogSmtpClient` (log-only, prod si zapojí reálné SMTP) | — |
| `LoginRateLimiter` | token bucket per peer IP (5 attempts / 10s) | — |

### Provozní zpevnění (nové, v legacy neexistovalo)

* **`/healthz` HTTP endpoint** — k8s liveness/readiness probes
* **Pre-auth idle timeout** (60s) přes `asio::steady_timer` watchdog
* **Schema validator** na startupu — 40 TGLOBAL + 23 TGAME sloupců přes `INFORMATION_SCHEMA`, fail-fast na drift
* **AdminShell** — line-based TCP shell na localhostu (status, kick, …)
* **Audit log:** strukturovaný `SpdlogAuditLogger` **+** `UdpAuditLogger` decorator s wire-faithful `_UDPPACKET` / `_LOG_DATA_` frame pro back-compat se starým TLogSvr
* **2FA flow** doopravdy zapojený (`TSECURECODE` + `TUSEREMAIL` + `TUSERTRUSTEDIP` whitelist) — v legacy zakomentováno
* **Duplicate-kick policy:** "newest wins" (UX improvement — uživatel umí zotavit zaseknutou session); legacy mu obě killne
* **Rate limit** pro pre-auth login pokusy; legacy nemá nic

---

## 3. TPatchSvr → TPatchSvrAsio

**Legacy:** `Server/TPatchSvr/` — 30 souborů, 3 824 LOC, **vlastní IOCP loop** (nesdílí TNetLib).
**Nový:** `Server/TPatchSvrAsio/` — 11 souborů, 1 155 LOC, Asio coroutines + SOCI.
**Stav:** Všech 9 `CT_*` handlerů portováno (commit `60dccf7`).

| Vlastnost | Legacy | Nový |
|---|---|---|
| Wire codec | 8B-header plain (ne RC4 — patch traffic je veřejný) | 8B-header plain — bit-for-bit shoda |
| DB queries | `CSqlDatabase` pro `TVERSION`/`TPREVERSION` | SOCI proti `TGLOBAL` |
| IOCP loop | Vlastní (nepoužívá TNetLib) | Sdílí Asio infrastrukturu s ostatními |
| Schema | Žádné migrace | Stejné schema — žádné destruktivní změny |

---

## 4. TLogSvr → TLogSvrAsio

**Legacy:** `Server/TLogSvr/` — 17 souborů, 3 908 LOC, ATL/UDP/ODBC, MFC dialog GUI.
**Nový:** `Server/TLogSvrAsio/` — 13 souborů (4 prod TUs + 3 testy + schema + db validator + retry queue), UDP-only headless služba.
**Stav:** Funkčně 1:1 s legacy DB writem — UDP `_UDPPACKET` ingest, SOCI INSERT do `TLOG_AUDIT`, schema validator, dialect-aware blob, RAM retry buffer + drain coroutine pro výpadky DB. Pokrýt unit + SOCI testy.

| Vlastnost | Legacy | Nový |
|---|---|---|
| UI | MFC dialog | Headless, log do spdlog |
| Wire | Custom `_UDPPACKET` UDP frame | Bit-for-bit kompatibilní — čte stejný frame, který TLoginSvrAsio emituje přes `UdpAuditLogger` |
| Storage | ODBC INSERT do legacy date-partitioned `ITEMLOGTL{yyyymmdd}` tabulek | SOCI INSERT do jedné `TLOG_AUDIT` tabulky s indexem na `LT_LOGDATE` (additivní migrace `schema/tlog-audit.sql`) |
| Schema check | ❌ žádný | ✅ `tlogsvr::db::ValidateAuditSchema` boot-time fail-fast na chybějící LT_* sloupce + identifier whitelist na `target_table` |
| Krypto pro reg klíče | `RegCrypt.cpp` (Win32 CryptoAPI) | Není potřeba — config přes TOML |
| DB-down chování | RAM requeue (`m_listReadCompleted`) + `WorkTickProc` reconnect každých 30 s; bounded `MAX_IO_CONTEXT=1000` | `RetryQueue` (cap 1000 default) + drain coroutine (30 s tick default); FIFO ordering preserved, queue depth + drop counts logované na shutdown |
| Vlákna | Win32 IOCP + dedikované listen/read/work-tick thready | Jedna Boost.Asio coroutine + drain timer; SOCI session pool pro DB |
| Crash dump | `TMiniDump` (Win32-only) | Není portováno — OS-level core dumps stačí |
| Service install/uninstall | `-I` / `-U` flagy → SCM Win32 service | Daemon spravovaný systemd / docker / atd. mimo binárku |
| `LP_CHAT` packet | Definovaný, ale neimplementovaný (`Packet_Nothing`) | Stejné — datagramy s `command != LP_LOG` se dropí + countují |

---

## 5. Bezpečnostní fixy v TNetLib (audit + cílené opravy)

Z auditovaného běhu `Lib/Own/TNetLib/`:

### Critical (RCE / heap corruption)
| # | Soubor | Issue | Fix |
|---|---|---|---|
| S1 | `Packet.cpp::operator>>(CString&)` + `Read()` + `CanRead()` | Pre-auth remote heap corruption — `int nLength` z wire se cast-uje na `DWORD` → wrap modulo 2^32 pro negativní → `memcpy(buff, src, (size_t)-1)` = ~16 EB write. **Reachable přes CS_LOGIN_REQ** (`m_strUserID`). | Validace `nLength ∈ [0, MAX_PACKET_SIZE]` ve třech vrstvách (commit `d90eece`) |

### Defensive cleanups
| # | Soubor | Změna |
|---|---|---|
| S2 | `CryptographyExt.cpp:135,190` | `CryptDeriveKey(..., CRYPT_EXPORTABLE, ...)` — povolení exportu key materiálu. **Odstraněno.** |
| S3 | `Rijndael.cpp/.h` (1 572 LOC) | Celá vlastní AES implementace byla **dead code** (zero references v repu). **Smazána** — eliminován risk cache-timing na S-box lookups + key-schedule underflow. |

### 8 funkčních bugů opravených v auditu (commit `1428844`)
| # | Soubor | Bug | Závažnost |
|---|---|---|---|
| 1 | `SqlDatabase.cpp::Open` | `SQLAllocHandle(ENV)` failure path → `DeleteCriticalSection` leak | High |
| 2 | `SqlDatabase.cpp::Close` | Bezpodmínečné `DeleteCriticalSection` = UB pokud `Open()` nikdy nevolal | High |
| 3 | `SqlBase.cpp::Init` | Query ≥ `MAX_QUERY_LEN` → nezinicializované members → garbage read | High |
| 4 | `SqlBase.cpp::IsNull` | `int nCol` mohl být negative → OOB index v `m_liCOLS` | Medium |
| 5 | `BindDesc.cpp` default ctor | `m_size`/`m_type` neinicializované → `malloc(garbage)` | Medium |
| 6 | `Packet.cpp::EncryptHeader/DecryptHeader` | `BYTE i` counter → silent wrap při `PACKET_HEADER_SIZE > 255` → infinite loop | Low (latent) |
| 7 | `Packet.cpp::Write` | Kapování `m_wSize` na `MAX_PACKET_SIZE` → `IsValid()` pak odmítne → packet s bajty + "invalid" marker | Medium |
| 8 | `Packet.cpp::CopyData` | Žádný `MAX_PACKET_SIZE` guard před `+=` na `WORD` → 16-bit wraparound | Medium |

### Další security work
* `TRand` audit findings (commit `48f73d4`) + `DetachBinary` bounded overload pro audit-able buffer size
* Hardened Win32 crypto wrapper (commit `d90eece`)

---

## 6. Bug fixy / hardening v legacy kódu (Phase 0)

Před započetím modernizace byly opraveny nejhorší ostré hrany v původním kódu (commit `561ab2b`):

* `lstrcpy` off-by-one v `TLoginSvr/UdpSocket.cpp:117` — 50-char username → 51 bajtů do 50-bajtového bufferu
* `sprintf` bez bounds v `TControlSvr/PlatformUsage.cpp:58`
* Manuální `EnterCriticalSection` → existující RAII `SMART_LOCKCS` wrapper (~140 sites identifikováno, 4 soubory dokončeny v Phase 0)
* Hardcoded `C:\4s\dberror.log` → `TNETLIB_DB_ERROR_LOG` env var
* Case-sensitivity fix pro Linux: 7 `.cpp` souborů `#include "StdAfx.h"` (velkým) → `stdafx.h` (commit `7ae24ce`)

---

## 7. Wire kompatibilita

**Wire protokol je byte-for-byte 1:1 s legacy serverem.** Ověřeno proti:
- `Server/TLoginSvr/CSSender.cpp` — všechny `CS_*_ACK` struktury
- `Server/TPatchSvr/Sender.cpp` — `CT_PATCH_ACK`
- `Server/TLogSvr/LogPacket.h` — `_UDPPACKET` / `_LOG_DATA_`

Legacy `CS_LOGIN_REQ` trailing XOR/add checksum (`CSHandler.cpp:185-202`)
je vynucován i na nové straně; testy, které posílaly dummy
`llChecksum=0`, byly opraveny (commit `60dccf7`).

Zachované sémantiky:
* RC4 layer s legacy secret key (commit `728d697`)
* XOR + header obfuscation
* Stejné DB schéma `TGLOBAL` + `TGAME` (pouze **additivní** migrace `2fa-tables.sql`, `tlog-audit.sql`)

---

## 8. Co je záměrně **mimo scope**

| Feature | Důvod |
|---|---|
| `HwidManagerSvr` (Anticheat) | Out of scope by design — Windows-only proprietary bloby (HShield/XTrap/NPGame) |
| `CS_HOTSEND_REQ` exec-file integrity | Anti-cheat tooling; silent no-op, aby legacy klient nepadl |
| Japan channeling (`CSPLoginJP`, `bChanneling`, `m_bNation == NATION_JAPAN`) | Žádný JP deploy target |
| **TWorldSvr** (38 851 LOC) | Odloženo — legacy je canonical |
| **TMapSvr** (112 843 LOC) | Odloženo — největší a nejrizikovější |
| **TControlSvr** (7 285 LOC) | Odloženo |
| `TBRSvr` / `TBoWSvr` (po 92 LOC) | **Empty shells** — feature je v `TMapSvr` přes `#ifdef BR_COMPILE_MODE` / `BOW_COMPILE_MODE`. Phase 6 cleanup je smaže |

---

## 9. Co je nového oproti legacy

| Feature | Proč win | Phase |
|---|---|---|
| Cross-platform build (Linux + Windows) | Docker / k8s deploys, levnější Linux servery | Phase 1 |
| spdlog strukturované logy | Greppable, machine-readable, sink-pluggable (Seq/Loki/ELK) | Phase A.1 |
| `/healthz` HTTP endpoint | k8s liveness/readiness, LB health checks | Phase A.3 |
| TOML config (vs. Registry) | Verzovatelné, code-reviewable, testovatelné | Phase A.2 |
| Service interface pattern | Vyměnitelné backends, unit-testovatelné | Phase A.1 |
| 10 ctest targets + 155+ KATs | Regression pokrytí, které legacy nikdy nemělo | All phases |
| Pre-auth RCE fix v `operator>>(CString&)` | Critical CVE-class bug uzavřen v TNetLib | Audit |
| OpenSSL EVP (vs. ATL/Win32 CryptoAPI) | Modern crypto provider, FIPS-able, audited | Phase 1 step C |
| BCrypt + transparent upgrade z plaintextu | Legacy ukládal plaintext nebo slabý hash | Phase B.5 |
| Duplicate-kick "newest wins" | UX — uživatel umí zotavit zaseknutou session | Phase A.4 |
| `TerminationReason` enum (MapHandoff preserves TCURRENTUSER) | Čistší contract než legacy `m_bLogout` bool gymnastika | Phase A.7 |
| Login rate-limit (token bucket per IP) | Legacy nemá throttling — open pro brute-force | Phase C |
| Schema validator na startupu | Fail-fast na DB drift místo runtime crash | Phase C |
| AdminShell (localhost TCP) | Live ops bez restartu | Phase C |

---

## 10. Časová osa fází (z git logu)

```
Phase 0   c77f790…561ab2b  Foundation — safety fixes, CMake skeleton
Phase 1   dd18d8d…f78cca9  TNetLib modernizace (IOCP→Asio, OpenSSL EVP, RFC 6229)
          + 8 functional bugs + S1 RCE fix v Packet.cpp
Phase A   6a15eb6…2e32e2c  TLoginSvrAsio scaffold — service pattern, TOML, health, registry
Phase B   08daa55…650f0b6  SOCI backends — Auth/Char/Map/SessionTerm + MSSQL parity
Phase B.5 d04b3ab           Production complete — BCrypt + CT_* + audit/admin/rate-limit
Phase D   60dccf7…155fdd2  TPatchSvr + TLogSvr porty + FourStoryCommon + 2FA + READMEs
```

38 commitů celkem. Postaveno na Linuxu (GCC 13, C++20, CMake 3.28) i
Windows (MSVC 2022 + vcpkg).

---

## 11. Co zbývá

Z `_rewrite/docs/MODERNIZATION_PLAN.md`:

* **TWorldSvr** (~8–12 týdnů) — cluster coordinator; nutný před TMapSvr (protocol contract)
* **TMapSvr** (~12–24 týdnů) — gameplay engine, nejrizikovější; pravděpodobně staged refactor
* **TControlSvr** (~4 týdny) — GM/admin dashboard, paralelizovatelné s TLog
* **Phase 6 cleanup** — smazat `TBRSvr` / `TBoWSvr` empty shells, převést `#ifdef BR_COMPILE_MODE` / `BOW_COMPILE_MODE` na runtime feature flag

Architektonické dluhy (řešené mimo prostou language port):
1. `TWorldSvr` single DB thread → bottleneck (potřeba async DB + per-shard write queue)
2. `TWorldSvr` global maps pod jedním lockem → race conditions (potřeba partitioning)
3. `SSHandler.cpp` 14 615 LOC monolitický switch — každý nový packet ID rekompiluje všechno
4. `TMapSvr` quest engine v C++ (24 `Quest*.cpp`) — každá změna = rekompilace (kandidát na Lua/sol2 VM)

---

## Reference

* [`README.md`](../../README.md) — kořenové shrnutí + build pokyny
* [`_rewrite/docs/MODERNIZATION_PLAN.md`](MODERNIZATION_PLAN.md) — cluster-wide phased roadmap
* [`_rewrite/docs/LOGIN_SERVER_COMPARISON.md`](LOGIN_SERVER_COMPARISON.md) — handler-by-handler parita audit
* [`_rewrite/docs/PROTOCOL.md`](PROTOCOL.md) — wire codec reference
* [`_rewrite/docs/SCHEMA.md`](SCHEMA.md) — DB column katalog
* [`_rewrite/docs/GAP_ANALYSIS.md`](GAP_ANALYSIS.md) — co je záměrně neportováno
