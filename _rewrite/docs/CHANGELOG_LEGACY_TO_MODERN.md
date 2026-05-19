# Changelog — 4Story server: legacy vs. modernizovaná verze

**Pro komunitu 4Story.** Tohle je srovnání původního (legacy) serveru
z roku 2007 — toho, co jste znali pod `TLoginSvr.exe`, `TPatchSvr.exe`,
`TLogSvr.exe` — a nové, přepsané verze (`*_asio`), která žije vedle
něj v repu. **Wire protokol je úmyslně stejný** — váš shipped klient
se k novému serveru přihlásí beze změny, žádný patch klienta není
potřeba.

Repo: `Server/T*Svr/` = starý kód (nesahá se na něj, je tam pro
referenci). `Server/T*SvrAsio/` = nová verze. Obojí kompiluje. Můžete
si pustit starý nebo nový, klient to nepozná.

---

## TL;DR pro netrpělivé

* **Klient se nemění.** Wire protokol je byte-by-byte stejný. Stejné RC4, stejný XOR, stejný checksum.
* **DB se nemění.** Čte se stejné `TGLOBAL_RAGEZONE` + `TGAME_RAGEZONE` schéma. Žádné destruktivní migrace, jenom přidání pár tabulek pro 2FA a audit log.
* **Login server běží i na Linuxu.** Můžete ho hostit v Dockeru / na VPS místo Windows Serveru.
* **Hesla jsou konečně bezpečně.** Bcrypt místo plaintextu, s automatickým upgradem starých účtů při prvním přihlášení.
* **Opraven kritický bezpečnostní bug**, kterým šel server **shodit (nebo hůř) ještě před přihlášením** — `CS_LOGIN_REQ` packet s podvrženou délkou username dokázal poškodit paměť.
* **OpenSSL aktualizován** z roku 2009 (0.9.8l, 15 let zranitelností!) na současné 3.x.
* **Anticheat (HShield/XTrap/NPGame/HwidManager) zatím není.** Záměrně — jsou to Windows-only proprietary bloby a nemá smysl je řešit, dokud běží jen login.
* **TMapSvr (samotný gameplay), TWorldSvr a TControlSvr jsou pořád starý kód.** Modernizovaný je zatím Login, Patch a LogSvr. Ostatní zůstávají v původní podobě.

---

## 1. Co je nového / jiného obecně

### Build a platforma

| Co | Dřív | Teď |
|---|---|---|
| Visual Studio | 2017 (v141) | 2022 + CMake (univerzální projekty) |
| Kompilátor / standard | C++14 (psané stylem C++98) | **C++20** |
| Platforma | **Jenom Windows** | Linux **i** Windows — stejný kód |
| Závislosti | Ručně dotažené knihovny, vendorované staré verze | **vcpkg** stáhne a postaví aktuální verze (`vcpkg.json`) |
| Konfigurace | Windows Registry (`CRegKey`) | Obyčejný `.toml` soubor — dá se verzovat v Gitu, edituje ho každý editor |
| Spouštění | Windows Service (`CAtlServiceModuleT`) | `systemd` na Linuxu / NSSM na Windows / prostě v terminálu |
| Vypínání | Vlastní IPC paket `SM_QUITSERVICE_REQ` | `Ctrl+C` / `SIGTERM` (+ původní paket pořád funguje, kvůli kompatibilitě) |
| Logy | `printf` + `OutputDebugString` (musíte DebugView) | **spdlog** — strukturované logy, dají se posílat do Seq/Loki/ELK |
| Testy | Nebyly. | **CTest** + 10 testovacích suit (155+ kontrolních bodů) |

### Sítě a krypto

| Co | Dřív | Teď |
|---|---|---|
| Síťový engine | **Win32 IOCP** (jenom Windows) | **Boost.Asio** s coroutines (`co_await`) — na Linuxu epoll/io_uring, na Windows IOCP, stejný zdroják |
| RC4 (šifrování paketů) | Win32 CryptoAPI (`CryptAcquireContext`, `CALG_RC4`) | **OpenSSL EVP `EVP_rc4()`** |
| **OpenSSL** | **Vendorovaná verze 0.9.8l z roku 2009** (15 let staré zranitelnosti — Heartbleed, POODLE atd. — nikdy nezáplatované; navíc to byl mrtvý kód) | **Modern OpenSSL 3.x** přes vcpkg, s `legacy` providerem aby RC4 (deprecated v 3.x) pořád fungoval kvůli wire kompatibilitě |
| Hashy hesel | Plaintext nebo slabý hash v `TACCOUNT_PW.bPasswd` | **bcrypt** (cost 10) přes vendorovaný **libbcrypt** + **transparentní upgrade**: při prvním úspěšném loginu se starý plaintextový záznam přehashe a rovnou se přepíše v DB |
| Vlastní AES (`Rijndael.cpp`, 1 572 řádků) | Bylo v kódu, **ale nikdo to nevolal** ("dead code") — risk pro security audity (cache-timing atd.) | **Smazáno** |

### Databáze

| Co | Dřív | Teď |
|---|---|---|
| Přístup do MSSQL | ODBC přímo přes `CSqlDatabase` (Windows-only build) | **SOCI 4.x** + unixODBC (Linux) / MS ODBC (Windows) — stejný kód |
| Connection pool | Žádný, jedno spojení per server | **`SessionPool`** v `FourStoryCommon` — recycle při chybě, tunable size |
| Schema check | Žádný — když chybí sloupec, server padne za běhu | **Schema validator** na startu — zkontroluje 40 sloupců v TGLOBAL + 23 v TGAME a hned řekne, co chybí |
| Nové tabulky | — | Jen **additivně**: `TSECURECODE` + `TUSEREMAIL` + `TUSERTRUSTEDIP` (pro 2FA), `TLOG_AUDIT` (pro nový log server). SQL skripty jsou v `schema/`. |

---

## 2. Login server (TLoginSvr → TLoginSvrAsio)

Místo, kde se přihlašujete a vidíte výběr postav. Tady se nejvíc šahalo.

**Pokrytí:** **15 z 15** klient-server handlerů + **5 z 5** control handlerů (komunikace mezi servery). 100% funkční parita s tím, co dělal starý server.

### Co se zlepšilo z pohledu hráče / admina

* **Heslo se konečně neukládá v plaintextu.** Bcrypt cost 10 + transparentní upgrade — žádný admin zásah, žádné password reset emaily. Starý plaintext účet se přehashe sám při prvním loginu.
* **Pravidlo pro duplicitní login je teď "newest wins".** Když se přihlásíte z druhého PC, starou session to vykopne a nová projde. Starý server killnul **obě**, takže člověk musel počkat 5 minut, než ho TCURRENTUSER pustil zpátky.
* **2FA opravdu funguje.** V původním kódu byl `LR_SECURITY` flow zakomentovaný / dead code. Nový server ho má naživo: `TSECURECODE` se přečte z DB, srovná s tím, co poslal klient, a teprve pak pošle `CS_LOGIN_ACK`. Mailování kódu jde přes pluggable `ISmtpClient` (default jenom loguje, prod si tam zapojí reálný SMTP).
* **Whitelist IP** přes `TUSERTRUSTEDIP` — z trusted IP přeskočí 2FA.
* **Rate-limiting na pre-auth pokusy.** Starý server nemá vůbec nic, dalo se ho brute-force scriptem. Nový má token bucket: 5 pokusů / 10 sekund per IP.
* **Pre-auth idle timeout.** Když klient otevře TCP spojení a 60 sekund nic nepošle, server ho zavře. Předtím tam ty zaseknuté sessions visely.
* **Schema validator.** Když restartujete server a v DB chybí sloupec, **dozvíte se to hned**, ne až někdo zkusí vytvořit postavu.
* **AdminShell** — line-based TCP shell na localhostu. Příkazy `status`, `kick <user>` atd. Žádná potřeba restartovat kvůli vyhození zasekané session.
* **`/healthz` HTTP endpoint** — pro k8s / load balancer health checky. Vrací `200 OK` nebo `503` podle stavu DB connection poolu.

### Co se opravilo z pohledu kódu

* **Stack je 9 464 LOC vs. 11 453 LOC** — funkčně skoro paritní, jenom mnohem čitelnější, bez ATL/MFC balastu.
* Servisní vrstva: každá doména (Auth, Char, MapLocator, SessionTerminator) má **interface + SOCI implementaci + Fake pro testy**. Starý kód měl všechno splácané ve `CTLoginSvrModule` se statickými mapami.
* TFindServerID port — start postavy teď respektuje `TSVRCHART` + `TCHANNELCHART` + `TSPAWNPOSCHART` fallback (per-character routing), místo starého "dej jí první server v group".
* BR/BoW shard override (`TBRPLAYERTABLE`) funguje doopravdy.

### Wire bity, na kterých vám záleží (kompatibilita s klientem)

Wire packety se zarovnaly **doslova** s `Server/TLoginSvr/CSSender.cpp`:

* `CS_LOGIN_ACK`, `CS_GROUPLIST_ACK`, `CS_CHANNELLIST_ACK`, `CS_CHARLIST_ACK`, `CS_CREATECHAR_ACK`, `CS_DELCHAR_ACK`, `CS_START_ACK` — stejná struktura, stejné padding bajty
* `CS_LOGIN_REQ` trailing **XOR/add checksum** (`llChecksum`) — algoritmus z `CSHandler.cpp:185-202` se na nové straně **vynucuje**. Pokud jste si psali nějaký vlastní bot/tester, který tam dával nulu, oprava: musí se počítat doopravdy.
* `CS_BOWPLAYERNOTIFY_ACK` — pošle se za `CS_CHARLIST_ACK`, pokud má účet BR postavu (jako v originále)
* RC4 stream + XOR layer + 4-bajtová "secret key" salt — stejný `g_strSecretKey`
* Header obfuscation — stejné 7-slot key tabulky

---

## 3. Patch server (TPatchSvr → TPatchSvrAsio)

Server, který klient stáhne při startu, aby si zjistil, jestli má aktuální verzi.

**Stav:** Všech **9 `CT_*` handlerů** portováno.

| Co | Dřív | Teď |
|---|---|---|
| Velikost kódu | 3 824 LOC, 30 souborů | 1 155 LOC, 11 souborů |
| Síťový engine | **Vlastní IOCP loop** (nesdílel ani TNetLib!) | Boost.Asio, sdílí infrastrukturu s ostatními novými servery |
| Wire codec | 8B-header plain (patch traffic je veřejný, nešifruje se) | Stejný 8B-header plain |
| DB | `CSqlDatabase` proti `TVERSION` / `TPREVERSION` v `TGLOBAL` | SOCI proti stejným tabulkám |
| Schema | Beze změn | Beze změn |

---

## 4. Log server / audit (TLogSvr → TLogSvrAsio)

UDP collector, do kterého TLoginSvr posílá audit packety (přihlášení, vytvoření postavy, atd.).

**Stav:** UDP `_UDPPACKET` příjem + zápis do `TLOG_AUDIT`.

| Co | Dřív | Teď |
|---|---|---|
| Velikost kódu | 3 908 LOC, 17 souborů | 605 LOC, 7 souborů |
| UI | **MFC dialog okno** (musíte ho mít otevřené) | **Headless** služba, loguje do spdlog |
| Wire | `_UDPPACKET` UDP frame | Bit-for-bit stejný frame — modernizovaný TLoginSvr ho posílá přes `UdpAuditLogger`, novej LogSvr ho čte |
| Storage | ODBC INSERT do legacy TLOG tabulek | SOCI INSERT do `TLOG_AUDIT` (additivní migrace `schema/tlog-audit.sql`) |

---

## 5. Sdílená knihovna `FourStoryCommon` (úplně nová)

Předtím se sítí, audit log, SMTP a admin shell duplikovaly v každém serveru zvlášť (copy-paste). Bylo to nebezpečné — bug fix se musel udělat několikrát.

Teď je všechno sdílené v `Lib/Own/FourStoryCommon/`:

* `db::SessionPool` — connection pool pro SOCI
* `db::schema_validator` — kontroluje sloupce na startu
* `audit::SpdlogAuditLogger` + `audit::UdpAuditLogger` — strukturovaný audit log nebo legacy UDP shim
* `smtp::SmtpClient` — interface pro odesílání mailů (2FA)
* `ops::AdminShell` — TCP shell na localhostu
* `ops::HealthEndpoint` — `/healthz` HTTP probe
* `ops::RateLimiter` — token bucket
* `ops::RegistryRefresher` — periodická refresh smyčka

Konzumují to **všechny tři** modernizované servery (Login, Patch, Log).

---

## 6. Bezpečnostní opravy (důležité)

### Kritická díra — opraveno

Ve **starém TNetLib** byl bug, kterým šlo poslat `CS_LOGIN_REQ` se zápornou délkou username a server začal zapisovat **~16 EB** paměti. Reálně to znamená crash, ale teoreticky i RCE (remote code execution). Bug byl **dosažitelný bez přihlášení** — stačilo otevřít TCP spojení.

**Soubor:** `Lib/Own/TNetLib/Packet.cpp::operator>>(CString&)`

Fix má **tři vrstvy** (defense in depth), aby se to nedalo znovu trefit jinou cestou:

1. `operator>>(CString&)` — validuje délku `[0, MAX_PACKET_SIZE]` rovnou
2. `Read(LPVOID, int)` — odmítá `<= 0` nebo `> MAX_PACKET_SIZE`
3. `CanRead(DWORD)` — kontroluje overflow `offset + length`

### Aktualizovaný OpenSSL

* **Bylo:** vendorovaný 0.9.8l z **listopadu 2009**. 15 let nezáplatovaných zranitelností (CVE listina je dlouhá). V kódu navíc visel jako dead reference (nikdo ho nelinkoval správně), ale byl by to časovaný miny pro jakoukoliv produkci.
* **Je:** **OpenSSL 3.x z vcpkg**, držené aktuální. RC4 je v 3.x deprecated kvůli své slabosti, takže ho voláme přes **`legacy` provider** (samostatný OpenSSL modul) — díky tomu zůstává wire kompatibilní se shipped klientem, ale audit-friendly modul pro veškeré ostatní krypto.
* Wrapper `tnetlib_crypto` má **18 testů** (RFC 6229 RC4 vektory + symmetry + legacy 4Story secret key).

### Dead AES smazáno

`Rijndael.cpp` + `Rijndael.h` — 1 572 řádků vlastní AES implementace, **nikdy nikdo nevolal** (`grep CRijndael` po celém repu vrátil 0). Eliminovaly se tím dva teoretické problémy: cache-timing na S-box lookups a key-schedule underflow. Smazáno.

### Win32 krypto hardening

Před tím, než se v `CryptDeriveKey` přešlo na OpenSSL, byl flag **`CRYPT_EXPORTABLE`** — povoloval exportovat key materiál z krypto provideru. Nikdy se to nepoužilo, ale visel tam. Pryč.

### 8 dalších funkčních bugů (audit `Lib/Own/TNetLib/`)

| # | Co | Co se dělo |
|---|---|---|
| 1 | `SqlDatabase.cpp::Open` | Při selhání `SQLAllocHandle(ENV)` zůstal viset `CRITICAL_SECTION` — leak při každém failovaném connectu |
| 2 | `SqlDatabase.cpp::Close` | `DeleteCriticalSection` se volal i když `Open()` nikdy neproběhl → undefined behavior |
| 3 | `SqlBase.cpp::Init` | Query delší než `MAX_QUERY_LEN` → všechny členy zůstaly neinicializované → garbage read |
| 4 | `SqlBase.cpp::IsNull` | `int nCol` mohl být negativní → out-of-bounds index do `m_liCOLS` |
| 5 | `BindDesc.cpp` ctor | `m_size`/`m_type` neinicializované → `malloc(garbage)` |
| 6 | `Packet.cpp::EncryptHeader/DecryptHeader` | Loop counter `BYTE i` se přetočil při `PACKET_HEADER_SIZE > 255` → nekonečná smyčka |
| 7 | `Packet.cpp::Write` | Kapování velikosti na `MAX_PACKET_SIZE`, potom `IsValid()` to odmítal — packet zůstal "invalid" |
| 8 | `Packet.cpp::CopyData` | Neměl `MAX_PACKET_SIZE` guard, `WORD += WORD` se uměl přetočit |

### Legacy hotfixy

V původním kódu se ještě opravily nejostřejší hrany:

* `lstrcpy` off-by-one v `TLoginSvr/UdpSocket.cpp:117` — 50-znakový username přetekl 50-bajtový buffer
* `sprintf` bez bounds v `TControlSvr/PlatformUsage.cpp:58`
* Hardcoded `C:\4s\dberror.log` → env var `TNETLIB_DB_ERROR_LOG`

---

## 7. Co je **záměrně** mimo scope

Nečekejte tohle v modernizované verzi — bylo to rozhodnutí, ne opomenutí:

* **HwidManagerSvr / HShield / XTrap / NPGame** — anticheat. Jsou to **Windows-only proprietary bloby**, na Linuxu nepoužitelné. Server-side hooky pro ně jsou no-op. `CS_HOTSEND_REQ` (exec-file integrity heartbeat) je silent drop, aby klient nepadl.
* **Japan channeling** (`CSPLoginJP`, `bChanneling`, `m_bNation == NATION_JAPAN`) — odpadlo, neexistuje JP deploy.
* **TWorldSvr** (38 851 LOC) — cluster coordinator, **pořád běží starý**.
* **TMapSvr** (112 843 LOC) — samotný gameplay, **pořád běží starý**. Tohle je největší a nejrizikovější port, je v plánu na později.
* **TControlSvr** (7 285 LOC) — GM/admin dashboard, **pořád běží starý**.
* **TBRSvr / TBoWSvr** — prázdné shelly (po 92 LOC). BR/BoW logika je v TMapSvr za `#ifdef`. Phase 6 cleanup je smaže.

---

## 8. Jak to spustit

```powershell
# Restore originálních dump souborů
sqlcmd -S localhost -E -Q "RESTORE DATABASE TGLOBAL_RAGEZONE FROM DISK='…\TGLOBAL_RAGEZONE.bak'"
sqlcmd -S localhost -E -Q "RESTORE DATABASE TGAME_RAGEZONE  FROM DISK='…\TGAME_RAGEZONE.bak'"

# Seed dev účet + 2FA tabulky + audit tabulka
sqlcmd -S localhost -E -d TGLOBAL_RAGEZONE -i Server\TLoginSvrAsio\schema\dev-account.sql
sqlcmd -S localhost -E -d TGLOBAL_RAGEZONE -i Server\TLoginSvrAsio\schema\2fa-tables.sql
sqlcmd -S localhost -E -d TGLOBAL_RAGEZONE -i Server\TLogSvrAsio\schema\tlog-audit.sql

# Spustit cluster
build\bin\Release\tloginsvr_asio.exe --config Server\TLoginSvrAsio\tloginsvr.toml
build\bin\Release\tpatchsvr_asio.exe --config Server\TPatchSvrAsio\tpatchsvr.toml
build\bin\Release\tlogsvr_asio.exe   --config Server\TLogSvrAsio\tlogsvr.toml
```

Klient nasměrujte na `localhost:4816`, dev login je `dev` / `dev123`.

Na Linuxu: nainstalujte `libsoci-dev`, `unixodbc-dev`, `libspdlog-dev`, `libtomlplusplus-dev`, `libssl-dev`, `libboost-all-dev` — zbytek je stejné `cmake --build`.

---

## 9. Časová osa (z gitu)

```
Phase 0  c77f790…561ab2b  Foundation — safety fixy, CMake skeleton
Phase 1  dd18d8d…f78cca9  TNetLib — IOCP→Asio, OpenSSL 3.x EVP, RFC 6229 testy
                          + 8 funkčních bugů + S1 pre-auth RCE fix
Phase A  6a15eb6…2e32e2c  TLoginSvrAsio scaffold — service pattern, TOML, health
Phase B  08daa55…650f0b6  SOCI backends — Auth/Char/Map/SessionTerm proti MSSQL
Phase B.5 d04b3ab          Production complete — bcrypt + CT_* + audit + rate-limit
Phase D  60dccf7…155fdd2  TPatchSvr + TLogSvr porty + FourStoryCommon + 2FA + READMEs
```

**38 commitů, sestaveno a otestováno na Linuxu (GCC 13) i Windows (MSVC 2022 + vcpkg).**

---

## 10. Co dál

* **TWorldSvr** — odhad 8–12 týdnů; nutný před TMapSvr (jsou na sobě závislé protokolem)
* **TMapSvr** — odhad 12–24 týdnů; gameplay, nejrizikovější, pravděpodobně po fázích
* **TControlSvr** — odhad 4 týdny; paralelně s tím
* **Phase 6 cleanup** — smazat `TBRSvr` / `TBoWSvr` shelly, `#ifdef BR_COMPILE_MODE` → runtime feature flag

Reálné architektonické dluhy (řeší se mimo prostý port):

1. `TWorldSvr` má **jedno DB vlákno** pro všechny TMapSvr instance → bottleneck
2. `TWorldSvr` má globální mapy (`m_mapTCHAR`, `m_mapTGuild`) pod jedním lockem → race conditions
3. `SSHandler.cpp` v TWorldSvr je **14 615 LOC monolitický switch** — každý nový packet ID rekompiluje úplně všechno
4. Quest engine v TMapSvr je psaný v C++ (24× `Quest*.cpp`) — kvest = rekompilace celého serveru. Kandidát na Lua/sol2 VM.

---

## Reference

* [`README.md`](../../README.md) — kořenové shrnutí + build pokyny
* [`_rewrite/docs/MODERNIZATION_PLAN.md`](MODERNIZATION_PLAN.md) — kompletní roadmap
* [`_rewrite/docs/LOGIN_SERVER_COMPARISON.md`](LOGIN_SERVER_COMPARISON.md) — handler-by-handler parita audit
* [`_rewrite/docs/PROTOCOL.md`](PROTOCOL.md) — wire codec reference (RC4 keying, checksum algoritmy)
* [`_rewrite/docs/SCHEMA.md`](SCHEMA.md) — katalog DB sloupců
* [`_rewrite/docs/GAP_ANALYSIS.md`](GAP_ANALYSIS.md) — co není naportováno a proč
