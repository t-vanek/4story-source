# TControlSvr — analýza legacy a plán portu

Datum: 2026-05-19
Branch: `claude/redesign-control-server-kKk0k`
Status: **návrh — k odsouhlasení**, žádný kód ještě nenapsán.

Tento dokument analyzuje `Server/TControlSvr/` (Win32/ATL/IOCP/ODBC, ~7 285
LOC, 23 souborů) a navrhuje rozsah a tvar modernizované varianty
`Server/TControlSvrAsio/`, navazující na existující `TLoginSvrAsio`,
`TPatchSvrAsio` a `TLogSvrAsio` porty.

---

## 1. TL;DR

Legacy TControlSvr je **dispatcher + scheduler** mezi GM operátory
(klient `TController.exe`) a běžícími servery clusteru
(Login / World / Map / Relay / Patch / Log). Vlastní stav je tenký —
seznam strojů, server-typů, skupin (světů) a aktivní mapa eventů — a
většina ze **65 CT_\* handlerů jen forwarduje pakety** dovnitř clusteru.
Skutečné Win32-specifické kusy jsou tři: **Windows Service Control
Manager** (start/stop vzdálených daemonů), **PDH** (CPU/MEM/NET
counters) a **upload binárky přes UNC share** (`\\machine\c$\Services\`).
Ty se buď nahradí přes interface + Linux/cross-platform implementaci,
nebo se zahodí jako mimo scope (nahrazeno `/healthz`+`/metrics`,
CI/CD pipeline).

Odhadované úsilí: **4–5 týdnů** v dvojici (samostatný binární cíl,
single io_context, znovupoužije `Lib/Own/TNetLib/AsioSession` +
`FourStoryCommon` infra).

---

## 2. Legacy inventář

### 2.1 Soubory (lines / role)

| Soubor | LOC | Role |
|---|---:|---|
| `TControlSvr.cpp` | 2 054 | `CTControlSvrModule` — service entrypoint, threading, config, accept loop, scheduler, event check |
| `Handler.cpp` | 2 302 | 65 `On<CT_*>_REQ/ACK` handlerů |
| `Sender.cpp` | 380 | `CTManager::Send<CT_*>_ACK` (odpovědi operátorovi) |
| `TServer.cpp` | 321 | `CTServer::Send<CT_*>_REQ/_ACK` (zprávy z control svr do peer serverů) |
| `TControlType.h` | 461 | `TSVRTEMP / TGROUP / TSVRTYPE / TMACHINE / EVENTINFO` + ostatní POD struktury |
| `TControlSvrModule.h` | 402 | hlavička modulu — handler protokol, dispatch makra, ATL service plumbing |
| `DBAccess.h` | 409 | ODBC `DEFINE_QUERY` definice pro 18 tabulek/SP |
| `PlatformUsage.cpp/.h` | 191 | PDH counters (Processor/Memory/Network) |
| `TMiniDump.cpp/.h` | 378 | Win32 crash handler |
| `TManager.cpp/.h` | 91 | per-operátor session typ (login state + authority) |
| `TServer.h` | 47 | per-peer-server session typ (outbound conn) |
| `TControlSession.cpp/.h` | 23 | společný předek `CSession` |
| `DBAccess.cpp` | 26 | jen `EXPORT_INIT_QUERY` boilerplate |
| `DebugSocket.cpp/.h` | 82 | UDP log do TLogSvr |
| ostatní (.rc, .idl, stdafx, …) | ~120 | ATL/COM/Resource boilerplate, zahodit |
| **Celkem** | **~7 285** | |

### 2.2 Vlákna (CreateThread)

5 typů vláken + accept socket:

| Vlákno | Role | Modernizovaný protějšek |
|---|---|---|
| `ControlThread` | IOCP accept queue (`m_hIocpControl`) | `co_await acceptor.async_accept()` |
| `WorkThread` × (2*nCPU) | IOCP I/O completion (`m_hIocpWork`) | per-session coroutine v io_context |
| `BatchThread` | serializovaný handler dispatch (`m_qBATCHJOB` + `m_csQUEUE` + `m_csBATCH`) | implicitní strand v jednom io_context (vše single-thread) |
| `SMThread` | service monitor — 1× za vteřinu `QueryStatus()` přes SCM | `asio::steady_timer` |
| `TimerThread` | 1Hz watchdog — vystaví `CT_TIMER_REQ` paket sám sobě, ten kicknu peer keep-alive + `CheckEvent()` | `asio::steady_timer` |

V legacy je threading zdrojem 3 `CRITICAL_SECTION`ů (`m_csSMQUEUE`,
`m_csQUEUE`, `m_csBATCH`) a 3 `HANDLE` events; všechno spadne pod
jeden io_context bez zámků (jako u Login/Patch/Log).

### 2.3 Externí závislosti specifické pro Windows

| Hookem | Použití | Nahrazení |
|---|---|---|
| **Service Control Manager** (`OpenSCManager`, `OpenService`, `StartService`, `ControlService`, `QueryServiceStatus`) | Vzdálené spouštění/zastavování daemonů přes `pPriAddr.front()` (= `\\machine`) | `IServiceController` interface: `SystemdServiceController` (Linux, přes `systemctl --host=user@machine` nebo D-Bus); na Windows lze ponechat starý SCM přes adapter; `FakeServiceController` pro testy. **Doporučení: tuhle větev udělat opt-in** — defaultně vracet `Unsupported` a nechat ops orchestraci v ruce systemd/k8s. |
| **PDH** (`PdhOpenQuery`, `PdhAddCounter`, `PdhCollectQueryData`) | CPU `\Processor(_Total)\% Processor Time`, RAM `\Memory\% Committed Bytes In Use`, net `\Network Interface(...)\Bytes Total/sec` na vzdálených strojích | **Mimo scope** — řešit přes `FourStoryCommon::ops::HealthEndpoint` na každém peer serveru + Prometheus scrape. `CT_PLATFORM_REQ/_ACK` zůstane v protokolu pro wire-kompatibilitu, ale handler bude no-op (vrací nuly, nebo se vůbec nezavolá). |
| **Registry** (`HKLM\SYSTEM\CurrentControlSet\Services\<svc>\Config`) | DSN, DBUser, DBPasswd, Port, AutoStart | TOML soubor (jako Login/Patch/Log) |
| **UNC fileshare upload** (`\\machine\c$\Services\<file>`) | Push binárky daemonu před `StartService` | **Mimo scope** — řešit přes CI/CD (artifact registry, ansible, k8s image). `CT_SERVICEUPLOAD_*` handlery vrátí "feature disabled" pokud někdo zkusí. |
| **ATL `CAtlServiceModuleT`** | Windows service registration, `Install`/`Uninstall`/`RegServer`/`UnregServer` | `main()` + systemd unit (Linux) / NSSM (Windows) — stejně jako ostatní moderní binárky |
| **`TMiniDump`** | Win32 crash dump | `boost::stacktrace` + spdlog flush v terminate handleru, nebo nechat na container restart policy |
| **ATL `CString`** | Stringové parametry packetů + DB queries | `std::string` + `std::string_view` |

### 2.4 DB povrch (TGLOBAL_RAGEZONE)

#### Tabulky (read-only loady při startu + běhu)

| Tabulka | Klíče čtené | Použití |
|---|---|---|
| `TMACHINE` | bMachineID, szName, bRouteID | seznam fyzických strojů |
| `TNETWORK` | bMachineID, szNetwork | PDH counter cesta `\Network Interface(...)` |
| `TGROUP` | bGroupID, szName | herní skupiny / "světy" |
| `TSVRTYPE` | bType, szName | typy daemonů (LoginSvr/MapSvr/...), filtr `bControl=1` |
| `TSERVER` | bGroupID, bServerID, bType, bMachineID, wPort, szName | konfigurace každé service-instance, filtr `bType<>6` |
| `TIPADDR` | szIPAddr, szPriAddr, bActive | veřejná a private adresa pro každý stroj |
| `TEVENTCHART` | dwIndex, bID, szTitle, bGroupID, bSvrType, bSvrID, dStartDate, dEndDate, wValue, wMapID, dwStartAlarm, dwEndAlarm, bPartTime, szStart/Mid/End/Msg, szValue | DB-perzistované GM eventy (CashSale, Lottery, GiftTime, …) |
| `TCASHSHOPITEMCHART` | wID, szName, bCanSell | seznam cash-shop itemů použitý při definici eventů |
| `TPREVERSION` | dwBetaVer, szPath, szName, dwSize | beta patche před promotion do TVERSION |
| `TMANAGER` *(komentář says it's deprecated)* | szID, szPasswd, bAuthority | per-operátor login (volá se přes SP `TOPLogin` místo přímého read) |

#### Stored procedures (writes/lookups)

| SP | Účel |
|---|---|
| `TOPLogin(@szID, @szPW)` | auth GM operátora — vrací `bAuthority` (0–6) nebo 0 při fail |
| `TLoadService(@bWorld, @bServiceGroup, @szIP out, @wPort out)` | lookup vlastní bind adresy/portu z DB (volá se i v Login!) |
| `TUpdateVersion(@szPath, @szName, @dwSize, @dwBetaVer)` | promote do `TVERSION` |
| `TUpdatePreVersion(@szPath, @szName, @dwSize)` | insert do `TPREVERSION` |
| `TBetaToVersion(@dwBetaVer)` | promote jednu betu |
| `TDeletePreVersion(@dwBetaVer)` | smaž betu |
| `TUserProtectedAdd(@szUserID, @dwDuration, @szReason, @bPermanent, @szOperator)` | ban hráče |
| `OPTool_SMSEmergency(@bSvrType, @dwSvrID, @bSvrStatus)` | trigger SMS o tom, že daemon spadl |
| `TEventUpdate(@dwIndex, @bID, @bType, @strTitle, @bGroupID, @bSvrType, @bSvrID, @dStart, @dEnd, @wValue, @wMapID, @dwStartAlarm, @dwEndAlarm, @bPartTime, @szStart/Mid/End/Value)` | CRUD TEVENTCHART |

`FourStoryCommon::db::schema_validator` při startu zkontroluje
sloupce stejně jako u Login (40 TGLOBAL columns). Pro Control přibude:
TMACHINE, TNETWORK, TGROUP, TSVRTYPE, TSERVER, TIPADDR, TEVENTCHART,
TCASHSHOPITEMCHART, TPREVERSION (= ~9 dalších tabulek, ~40 sloupců).
Žádné destruktivní migrace.

---

## 3. Wire-povrch — 65 CT_\* handlerů kategorizováno

Zdroj: `Server/TControlSvr/TControlSvrModule.h:100-164` + `Handler.cpp`.
Pro každou kategorii je uveden plán portu (A = port 1:1 s reálnou
logikou, B = pure forwarder, vyžaduje peer-registry, C = mimo scope
nebo no-op, D = nahrazeno modernější infrastrukturou).

### 3.1 Operátor autentikace & lobby (5 handlerů, plán A)

| Paket | Plán | Poznámka |
|---|---|---|
| `CT_OPLOGIN_REQ` | A | `IOperatorAuthService` (SP `TOPLogin`). Hostname == 127.0.0.1 mimo, jinak duplicate-kick (jako Login `LR_DUPLICATE`). |
| `CT_STLOGIN_REQ` | A | Tenčí varianta — pro stat-tool (read-only). |
| `CT_SERVICEAUTOSTART_REQ` | A | Globální flag `m_bAutoStart` — broadcast `CT_SERVICEAUTOSTART_ACK` všem operátorům. |
| `CT_AUTHORITY_ACK` *(send-only)* | A | Vyvolá se z `CheckAuthority()` při fail — error semantika. |
| `CT_ACCOUNTINPUT_REQ/_ACK` | C | V legacy handlerech není definice — nepřipojeno do dispatch tabulky → pravděpodobně dead code. **Skip**. |

### 3.2 Service lifecycle (10 handlerů, plán A nebo D)

| Paket | Plán | Poznámka |
|---|---|---|
| `CT_SERVICESTAT_REQ` | A | Vrátí `m_mapTSVRTEMP` s aktuálním statusem (RUNNING/STOPPED/...). |
| `CT_SERVICECONTROL_REQ` | A (D pro SCM volání) | Start/stop konkrétní service. Volá `StartService()`/`StopService()` přes `IServiceController` interface. **WORLDSVR start kaskádově zastaví všechny service ve stejné skupině** — důležitá business rule, port 1:1. |
| `CT_SERVICECHANGE_REQ/_ACK` | A | Interní broadcast když se changne status → notifikuj operátory + opcionálně SMS přes `OPTool_SMSEmergency`. |
| `CT_NEWCONNECT_REQ` | A | Control svr inicializuje **outbound TCP connect** k peer serveru, posílá `CT_CTRLSVR_REQ`. Toto je hlavní mechanismus, jak control svr ovládá ostatní daemony. |
| `CT_RECONNECT_REQ` | A | Triggern manuální reconnect — interně to znovu pošle `CT_NEWCONNECT_REQ` do BATCH queue. |
| `CT_TIMER_REQ` *(self-posted 1Hz)* | A | Procházka přes všechny peer connections — pokud `m_dwRecvTick > 60s` ago → mark jako offline, broadcast service-data ACK, SMS alert. Pak `CheckEvent()`. |
| `CT_CTRLSVR_REQ` *(send-only)* | A | Po novém peer connect — control svr říká "ahoj, jsem control". |
| `SM_DELSESSION_REQ` *(internal)* | A | Async close pomocí dispatch queue — modernizovaně přes `co_spawn(close_session)`. |
| `CT_SERVICECLOSE_REQ` / `CT_DISCONNECT_REQ` | C | V dispatch tabulce nejsou — dead. **Skip**. |

### 3.3 Service monitoring (4 handlery, plán A)

| Paket | Plán | Poznámka |
|---|---|---|
| `CT_SERVICEMONITOR_REQ` | A | Peer (Map/World/Login) každou 1s posílá ping s session count + user count + active user count. Control odpoví `CT_SERVICEMONITOR_ACK` (RTT echo) a forwardne data všem logged-in operátorům přes `CT_SERVICEDATA_ACK`. |
| `CT_SERVICEMONITOR_ACK` *(send-only)* | A | RTT echo zpět peeru. |
| `CT_SERVICEDATA_ACK` *(send-only)* | A | Broadcast session+user counts operátorům. |
| `CT_SERVICEDATACLEAR_REQ/_ACK` | A | Resetuje maxUser/stopCount counters, broadcast clear všem peerům. |
| `CT_PLATFORM_REQ/_ACK` | D | Replace PDH → `/metrics` endpoint na každém peer serveru. Wire format zachovat, ale handler na Linuxu vrátí "n/a" (0). |

### 3.4 File upload (3 handlery, plán C — mimo scope)

| Paket | Plán |
|---|---|
| `CT_SERVICEUPLOADSTART_REQ` | C — vrátí kód "upload disabled" |
| `CT_SERVICEUPLOAD_REQ` | C |
| `CT_SERVICEUPLOADEND_REQ` | C |

Důvod: legacy zapisuje binárku přes `\\machine\c$\Services\` UNC
share, což je Windows-only + bezpečnostní anti-pattern. CI/CD má
nahradit. Pokud někdo bude trvat na opt-in implementaci, je možné
postavit nad `scp` nebo HTTPS-uploaded artifact server, ale to není
default-on.

### 3.5 Patch metadata (4 handlery, plán A)

| Paket | Plán | Poznámka |
|---|---|---|
| `CT_UPDATEPATCH_REQ` | A | Iteruje `wCount` × `(path,name,size)` → `TUpdateVersion` SP. |
| `CT_PREVERSIONTABLE_REQ` | A | Selectne `TPREVERSION` → `CT_PREVERSIONTABLE_ACK`. |
| `CT_PREVERSIONUPDATE_REQ` | A | Three-pass batch: `TBetaToVersion` (promote), `TDeletePreVersion` (drop), `TUpdatePreVersion` (insert) + final table reply. |
| `CT_INSTALLVERSION_*` (3 pakety v `.csv`) | C | Nepřipojeno do dispatch tabulky — dead. **Skip**. |

### 3.6 In-game admin operace (16 handlerů, plán B — forwardery)

Všechno forwarduje na World/Map/Relay servery podle `bGroupID`. Plán
B znamená: parsování + filtrování přes `IPeerRegistry::FindByType(SVRTYPE, groupId)` + posílám/odbočuju ack zpět do operátora podle
`dwManagerID`.

| Paket | Forwardováno na | Audit (GM action log) |
|---|---|---|
| `CT_ANNOUNCEMENT_REQ` | Relay (vše), pokud žádný Relay v group → Map | ano |
| `CT_USERKICKOUT_REQ` | všechny MapSvry | ano |
| `CT_USERMOVE_REQ` | konkrétní WorldSvr | ano |
| `CT_USERPOSITION_REQ` | konkrétní WorldSvr | ano |
| `CT_USERPROTECTED_REQ` | jen DB call (SP `TUserProtectedAdd`) | ano |
| `CT_CHARMSG_REQ` | Relay / World | ano |
| `CT_CHATBAN_REQ/_ACK/_LIST_REQ/_LIST_ACK/_LISTDEL_REQ` | World + Relay; sleduje N-vlnnou agregaci ack | ano |
| `CT_ITEMFIND_REQ/_ACK` | WorldSvr (broadcast); ack agreguje | ano |
| `CT_ITEMSTATE_REQ/_ACK` | WorldSvr; ack agreguje | ano |
| `CT_MONSPAWNFIND_REQ/_ACK` | MapSvr; ack agreguje | ne (read-only) |
| `CT_MONACTION_REQ` | MapSvr | ano |

Forwardování v legacy je čistě paket-shape passthrough (`pPacket->Copy()` + nový ID). Modernizovaná verze udělá to samé, jen s `tnetlib::DecodedPacket` místo `CPacket*`.

### 3.7 Castle / siege (5 handlerů, plán B)

| Paket | Forwardováno |
|---|---|
| `CT_CASTLEINFO_REQ/_ACK` | Map → ack zpět operátorovi |
| `CT_CASTLEGUILDCHG_REQ/_ACK` | World → ack |
| `CT_CASTLEENABLE_REQ` | World (broadcast BT_CASTLE bStatus dwSecond) |

### 3.8 Event manager (17 handlerů, plán A + B)

Core kus: `m_mapEVENT` (perzistovaný v `TEVENTCHART`) + 1Hz scheduler
v `CheckEvent()` který emituje wire messages na peer servery podle
fáze eventu (start-alarm / start / end-alarm / end).

| Paket | Plán | Poznámka |
|---|---|---|
| `CT_EVENTLIST_REQ` | A | Vrátí `m_mapEVENT` operátorovi. |
| `CT_EVENTCHANGE_REQ` | A | Add/Update/Del — invariantní kontroly (time overlap, end > start), SP `TEventUpdate`. |
| `CT_EVENTDEL_REQ` | A | DB delete + remove z mapy. |
| `CT_EVENTUPDATE_REQ` | A | Scheduler broadcast: pošle event peers ve své skupině. |
| `CT_EVENTMSG_REQ` | A | Start/end announce text → broadcast přes Relay/World. |
| `CT_CASHSHOPSTOP_REQ` | A | Vypnout cash shop daný event. |
| `CT_CASHITEMSALE_REQ/_ACK` | A | Aktivace/deaktivace cash sale. |
| `CT_CASHITEMLIST_REQ` | A | Read `TCASHSHOPITEMCHART`. |
| `CT_EVENTQUARTERLIST_REQ/_ACK` | B | Forward na World (kvartální events). |
| `CT_EVENTQUARTERUPDATE_REQ/_ACK` | B | Forward na World. |
| `CT_TOURNAMENTEVENT_REQ/_ACK` | B | Forward na World. |
| `CT_HELPMESSAGE_REQ` | B | Forward na World. |
| `CT_RPSGAMEDATA_REQ/_ACK/CHANGE_REQ` | B | Forward (Rock-Paper-Scissors event). |
| `CT_CMGIFT_REQ/_ACK/_LIST_REQ/_LIST_ACK/_CHARTUPDATE_REQ` | B | Forward (Community Manager gift). |

### 3.9 Sumarizace per-handler plánu

| Kategorie | Počet | Plán |
|---|---:|---|
| Operátor auth | 4 | A (1× C) |
| Service lifecycle | 8 | A (SCM volání přes interface) |
| Monitoring | 4 | A (1× D = PDH replace) |
| File upload | 3 | C (out of scope) |
| Patch metadata | 3 | A |
| Admin operace | 16 | B (forwardery + audit) |
| Castle | 5 | B |
| Event manager | 17 | A + B |
| Internal | 1 | A (SM_DELSESSION) |
| Dead / nezapojené | ~4 | skip |
| **Celkem** | **65** | |

---

## 4. Návrh modernizované architektury

### 4.1 Samostatný binární cíl

`Server/TControlSvrAsio/tcontrolsvr_asio` — stejný pattern jako
Login/Patch/Log. Důvod: čistý failure isolation (control svr není
v hot path hráčů), nezávislý deploy, vlastní config a port.

### 4.2 Skladba (přesný layout)

```
Server/TControlSvrAsio/
├── CMakeLists.txt
├── README.md
├── tcontrolsvr.example.toml
├── main.cpp                    # CLI, signal handling, service wire-up
├── config.h / config.cpp       # TOML config (port, RC4 secret, DB, peer dial timeout, …)
├── control_server.h / .cpp     # accept loop + Dispatch()
├── operator_session.h / .cpp   # CTManager → OperatorSession (login state, authority, dwID)
├── peer_session.h / .cpp       # CTServer → PeerSession (outbound to LoginSvr/MapSvr/...)
├── peer_dialer.h / .cpp        # outbound co_await async_connect
├── event_scheduler.h / .cpp    # 1Hz steady_timer → CheckEvent() ekvivalent
├── handlers/
│   ├── handlers.h              # ServerContext + protokol pro všechny handlery
│   ├── handlers_auth.cpp       # OPLogin, STLogin, GROUP/MACHINE/SVRTYPE LIST
│   ├── handlers_service.cpp    # SERVICESTAT/CONTROL/CHANGE, NEWCONNECT, TIMER, MONITOR
│   ├── handlers_admin.cpp      # KICK, MOVE, POSITION, BAN, ITEMFIND/STATE, MONSPAWN/ACTION, CHARMSG, USERPROTECTED
│   ├── handlers_event.cpp      # EVENTLIST/CHANGE/DEL/UPDATE/MSG + CASH + TOURNAMENT + RPS + CMGift + HELP
│   ├── handlers_castle.cpp     # CASTLEINFO/GUILDCHG/ENABLE
│   ├── handlers_patch.cpp      # UPDATEPATCH, PREVERSIONTABLE/UPDATE
│   └── handlers_upload.cpp     # UPLOADSTART/UPLOAD/UPLOADEND (no-op, opt-in)
├── services/
│   ├── operator_auth_service.h           # interface (CSPOPLogin)
│   ├── soci_operator_auth_service.h/.cpp # production: SP `TOPLogin`
│   ├── fake_operator_auth_service.h/.cpp # in-memory
│   ├── operator_registry.h/.cpp          # logged-in GM operátoři (DWORD seq + duplicate-kick)
│   ├── service_inventory.h               # interface — loads TMACHINE/TGROUP/TSVRTYPE/TSERVER/TIPADDR
│   ├── soci_service_inventory.h/.cpp     # production
│   ├── fake_service_inventory.h/.cpp     # in-memory pro testy
│   ├── service_controller.h              # interface (StartService / StopService / QueryStatus)
│   ├── systemd_service_controller.h/.cpp # Linux production (D-Bus nebo ssh+systemctl) — opt-in
│   ├── windows_scm_service_controller.h/.cpp # Windows production — opt-in
│   ├── disabled_service_controller.h/.cpp   # default — vrací NotSupported (safer default)
│   ├── peer_registry.h/.cpp              # map dwID → PeerSession (legacy m_mapTSVRTEMP)
│   ├── event_repository.h                # interface (TEVENTCHART CRUD + TCASHSHOPITEMCHART read)
│   ├── soci_event_repository.h/.cpp      # production
│   ├── fake_event_repository.h/.cpp      # in-memory
│   ├── chat_ban_repository.h/.cpp        # in-memory map (legacy m_mapBanInfo) + opcionálně DB persistence
│   ├── patch_metadata_service.h          # interface (TUpdateVersion, TUpdatePreVersion, TPREVERSION select)
│   ├── soci_patch_metadata_service.h/.cpp
│   ├── user_protected_service.h          # interface (SP TUserProtectedAdd)
│   ├── soci_user_protected_service.h/.cpp
│   ├── alerter.h                         # interface (SMS / e-mail při crashi peer service)
│   ├── soci_alerter.h/.cpp               # production — SP OPTool_SMSEmergency
│   ├── spdlog_alerter.h/.cpp             # default — jen do logu
│   ├── platform_monitor.h                # interface (CPU/MEM/NET pro CT_PLATFORM_REQ)
│   └── noop_platform_monitor.h/.cpp      # default — vrátí 0 (PDH replace)
├── schema/
│   └── control-schema.sql                # additive migrations (žádné)
├── tests/
│   ├── test_operator_login.cpp           # CT_OPLOGIN_REQ duplicate-kick, authority
│   ├── test_service_dispatch.cpp         # SERVICESTAT/CONTROL/MONITOR over fakes
│   ├── test_event_scheduler.cpp          # CheckEvent() — daily vs term events, alarms
│   ├── test_event_crud.cpp               # CT_EVENTCHANGE_REQ overlap validation
│   ├── test_admin_forward.cpp            # KICK/MOVE/BAN routing rules
│   ├── test_chat_ban_aggregation.cpp     # N-svrová agregace ack
│   ├── test_authority_gates.cpp          # MANAGER_GMLEVEL{1,2,3} permission walls
│   └── test_soci_*.cpp                   # production-DB integration suite (skip když env unset)
```

### 4.3 Klíčová designová rozhodnutí

**(a) Jedno io_context, žádné zámky.**
Legacy má 3 kritické sekce a 5 různých vláknových rolí. Modernizovaná
verze běží vše v jednom `boost::asio::io_context::run()`, single-thread.
Performance: control svr handleruje desítky operátorů + ~10 peer
serverů, ne tisíce hráčů. Nulové saturace přes jediné jádro. Pokud by
to bylo někdy potřeba, dá se per-PeerSession strand snadno doplnit
zpětně.

**(b) Outbound dialer jako coroutine.**
Legacy `OnCT_NEWCONNECT_REQ` rozkládá: vytvoří socket, blokující
connect, `CreateIoCompletionPort`, `WSARecv`, posílá `CT_CTRLSVR_REQ`.
Moderně: `co_await peer_dialer.Connect(machine_ip, port)` → vrátí
`AsioSession` ve stavu připraveném na `RunPackets`. Retry budget +
exponential backoff jsou triviální (`co_await asio::steady_timer`).

**(c) Event scheduler jako asio::steady_timer.**
1Hz tick → `co_await CheckEvent()`. Žádné `m_hTimerEvent` +
`WaitForSingleObject` + `SetEvent` — `asio::steady_timer` má `cancel()`
na shutdown a tím to končí.

**(d) Sender = jeden volný funkce per packet.**
Legacy má `CTManager::Send<CT_*>_ACK` a `CTServer::Send<CT_*>_REQ/ACK`
metody. Modernizovaně: `co_await SendOplogin Ack(session, ret,
authority)` jako volné funkce v `senders.cpp`, žádné dědění.

**(e) Operator authority gates.**
Legacy `CTManager::CheckAuthority(bClass)` posílá `CT_AUTHORITY_ACK`
při fail a vrací `FALSE`. Modernizovaně: enum `OperatorRole { All, Control,
User, Service, GMLevel1, GMLevel2, GMLevel3 }` + `RequireRole(session,
required) -> bool`. Při fail → posílá `CT_AUTHORITY_ACK` + audit log
"denied" + handler `co_return`.

**(f) Audit log GM akcí.**
`FourStoryCommon::audit::IAuditLogger` se rozšíří o
`LogAdminAction(actor_id, action, target, outcome)` (nebo se přidá
nový enum case). Každý B-plan forwarder zaloguje "manager X kicked Y"
před tím, než pošle peer broadcast. spdlog/UDP/SQL backendy zůstávají.

**(g) Service controller jako pluggable interface.**
```cpp
class IServiceController {
 public:
  virtual ~IServiceController() = default;
  virtual awaitable<ServiceStatus> QueryStatus(const ServiceInstance&) = 0;
  virtual awaitable<bool> Start(const ServiceInstance&) = 0;
  virtual awaitable<bool> Stop(const ServiceInstance&) = 0;
};
```
Default impl `DisabledServiceController` vrací `Unsupported` u
Start/Stop a `Unknown` u QueryStatus. Operátor v UI vidí service stav
"unknown", tlačítko start/stop nic neudělá. Toto je **bezpečný
default** — ovládání cizích strojů přes SCM/systemd je opt-in feature,
opravdu náročná na deploy infra.

**(h) Žádný `CT_SERVICEUPLOAD_*` v default-on.**
Vrátí "feature disabled" pokud někdo zkusí. Operator si všimne v UI,
že upload button nereaguje, a použije CI pipeline. Pokud někdo trvá,
opt-in adapter přes `scp -i deploy_key` lze přidat zpětně.

**(i) Reuse FourStoryCommon.**
`SessionPool` (SOCI), `IAuditLogger` (spdlog + UDP), `ISmtpClient` (pro
alerter), `HealthEndpoint` (port 18086 nebo dle TOML), `AdminShell`
(localhost TCP), `LoginRateLimiter` (per-IP throttle na CT_OPLOGIN_REQ),
`schema_validator`, `RegistryRefresher` (refresh `service_inventory`
když ops změní TSERVER za běhu).

### 4.4 Wire-kompatibilita s legacy operátorským klientem

`TController.exe` (legacy GUI tool) se připojí na default port a
očekává:

- **Stejnou frame strukturu**: 8-byte header `WORD wSize | WORD wID |
  DWORD dwChkSum` přesně jako u Patch (`tpatchsvr::PacketHeader`).
  Žádné RC4 (server↔operátor je interní, ne přes internet — ověřit
  v `TController` kódu, ale `CTControlSvrModule` to nedělá).
- **Stejné CT_\* IDčka a byte layout těla**: zkontrolováno proti
  `_rewrite/docs/packet-ids.csv` — 110 CT_CONTROL IDček je už
  v inventáři, posíláme 1:1.
- **První 4 odpovědi po loginu**: `CT_OPLOGIN_ACK`, `CT_GROUPLIST_ACK`,
  `CT_MACHINELIST_ACK`, `CT_SVRTYPELIST_ACK`, `CT_SERVICEAUTOSTART_ACK` —
  v tomto pořadí.

Peer-server-side (Login/Map/World/Relay/Patch zaventou outbound
connect z control svr) protokol je rovněž zachován: `CT_CTRLSVR_REQ`
po accept, pak `CT_SERVICEMONITOR_REQ` co 1s, ostatní podle potřeby.

### 4.5 Threading model

```
io_context (1 thread)
  ├── acceptor.async_accept() loop  — přijímá operátory + peer reconnects
  ├── per-OperatorSession coroutine — RunPackets() + Dispatch()
  ├── per-PeerSession coroutine     — RunPackets() + Dispatch()
  ├── peer_dialer dial coroutines   — retry-with-backoff outbound
  ├── service_monitor_timer (1Hz)   — QueryStatus přes IServiceController
  ├── event_scheduler_timer (1Hz)   — CheckEvent() — start/end alarmy
  ├── peer_keepalive_timer (1Hz)    — pingni offline peery, SMS alert
  ├── health endpoint (port 18086)
  └── admin shell (localhost TCP)
```

Pokud SOCI volání blokuje na ODBC (synchronní driver), zabalí se do
`co_await asio::post(thread_pool, [...]{ ... })` jako u Login/Patch
(FourStoryCommon SessionPool obstará pool of N sessions).

---

## 5. Fázový plán implementace

| Fáze | Trvání | Deliverable |
|---|---|---|
| **F1 — Scaffold** | 3 dny | `Server/TControlSvrAsio/` skeleton + CMake target + TOML config + accept loop + AsioSession reuse + `OperatorSession`/`PeerSession` skeletons + `IOperatorAuthService` (fake) + `CT_OPLOGIN_REQ` + `CT_GROUPLIST_ACK` / `MACHINELIST_ACK` / `SVRTYPELIST_ACK` ze `service_inventory`. spdlog + `HealthEndpoint` + `AdminShell`. CTest test pro login flow přes fake. |
| **F2 — Service inventory + peer dial** | 4 dny | `SociServiceInventory` proti reálnému `TGLOBAL_RAGEZONE` (TMACHINE/TGROUP/TSVRTYPE/TSERVER/TIPADDR). `PeerDialer` + `PeerRegistry`. `CT_SERVICESTAT_REQ/_ACK`. `CT_SERVICEMONITOR_REQ/_ACK`. `CT_SERVICEDATA_ACK` broadcast operátorům. 1Hz monitor + keep-alive timery. `CT_NEWCONNECT_REQ` / `CT_RECONNECT_REQ` / `CT_CTRLSVR_REQ` handshake. **`IServiceController` jako disabled-default** — `CT_SERVICECONTROL_REQ` vrátí "feature disabled". Integration test proti TLoginSvrAsio (control získá ping od login). |
| **F3 — Admin operace** | 5 dnů | `handlers_admin.cpp` — 16 forwarderů. Audit log GM akcí přes `IAuditLogger::LogAdminAction`. Authority gate enum + `RequireRole`. `IUserProtectedService` (SP `TUserProtectedAdd`). `ChatBanRepository` + N-vlnný `CT_CHATBAN_ACK` aggregator. CTest pro routing rules + audit emission. Per-IP rate limit na `CT_OPLOGIN_REQ`. |
| **F4 — Event scheduler & manager** | 6 dnů | `handlers_event.cpp` — 17 paketů. `IEventRepository` (SP `TEventUpdate`, table read). `EventScheduler` na 1Hz steady_timer s logikou daily vs term + start/end-alarm + auto-DEL pro lottery/gifttime. Validace overlap při add/update. `CT_CASHITEMLIST_REQ` z `TCASHSHOPITEMCHART`. Forwardery EventQuarter/Tournament/RPS/CMGift/Help/CashShopStop. CTest 8 scénářů pro scheduler. |
| **F5 — Patch metadata + ops polish** | 3 dny | `handlers_patch.cpp` (3 hlavní pakety + SPs). `handlers_castle.cpp` (5 forwarderů). `Alerter` přes `OPTool_SMSEmergency` SP + spdlog default. `handlers_upload.cpp` = no-op feature-disabled. README + tcontrolsvr.example.toml + bring-up notes. |
| **F6 — Validace** | 2 dny | Schema validator pro TControlSvr-specifické tabulky/SP. End-to-end smoke test: legacy `TController.exe` klient se přihlásí na `tcontrolsvr_asio`, vidí seznam strojů/skupin, vidí status každé service. Update sekce v `README.md` (Status tabulka) + `MODERNIZATION_PLAN.md` (Fáze done). Commit + PR. |

**Celkem:** ~23 pracovních dní (~ 4–5 kalendářních týdnů v dvojici,
nebo 5–6 v jednom čase). Paralelní práce s portem TWorld/TMap možná.

---

## 6. Co je mimo scope (zdokumentované záměrné mezery)

| Feature | Důvod | Komentář |
|---|---|---|
| `CT_SERVICEUPLOAD_*` (3 pakety) | UNC share fileshare = Windows-only + anti-pattern | Stub "feature disabled". Operátor použije CI/CD. |
| `CT_PLATFORM_REQ/_ACK` data | PDH counters jsou Windows-only | Wire format zachovat, vracet 0. Replace by Prometheus scrape na `/metrics` každého daemona. |
| `IServiceController` Start/Stop | Cross-machine systemd ovládání má vlastní deploy infra náklad | Default-disabled. Opt-in adaptery (systemd-via-ssh, WinSCM) jako future work. |
| `TMiniDump` | Win32-only crash dump | Container restart policy + spdlog flush v terminate handleru pokrývají. |
| `CT_INSTALLVERSION_*` | Není připojeno do dispatch tabulky v legacy = dead code | Skip. |
| `CT_ACCOUNTINPUT_*` | Není připojeno do dispatch = dead | Skip. |
| `CT_SERVICECLOSE_REQ` / `CT_DISCONNECT_REQ` | Není v dispatch | Skip. |
| `CT_LOCALGUILDCHANGE_*` / `CT_LOCALINIT_*` | Není v dispatch | Skip. |
| Japan-specific větve | Žádný JP deploy target (jako u TLoginSvr) | Skip. |

---

## 7. Otevřené otázky před implementací

1. **Bude se používat operátorský klient `TController.exe`** *jako-je*,
   nebo se počítá s portem na webový UI? Pokud webový UI:
   - protokol bude REST/JSON, ne CT_\* wire, a fáze 1 + 3 se výrazně
     mění (handlers → REST endpoints, autorizace přes JWT, …)
   - pak je doporučení: udělat **gateway** verzi, která exposes CT_\*
     pro legacy client *a* zároveň REST API, plus shared business
     logic vrstvu.
2. **Server-controller**: má někdo zájem ovládat daemony přes
   modernizovaný control svr, nebo to převezme úplně k8s/ansible?
   Pokud druhé, fáze 5 ztratí ~1 den (nepotřebujeme ani opt-in
   systemd adaptér).
3. **PDH replacement**: má sense pushovat `/metrics` data zpět do
   control svr GUI, nebo operátoři půjdou rovnou do Grafany? Pokud
   Grafana, `CT_PLATFORM_REQ/_ACK` může být úplně mrtvý.
4. **RC4 na operátor↔control svr lince**: v legacy se nedělá. Ověřit
   v `TController.exe` zdroji (mimo náš repo) — pokud klient přesto
   RC4 očekává, budeme muset RC4 secret přidat do TOML. Default
   předpoklad: bez RC4 (jako u Patch).
5. **Audit destinace pro GM akce**: SOCI INSERT do TLOG_AUDIT (jako
   u Login), nebo samostatná tabulka TGM_AUDIT? Doporučení:
   stejná TLOG_AUDIT s rozšířeným `audit_kind` enumem.

---

## 8. Reference

- Legacy zdroj: `Server/TControlSvr/` (23 souborů, 7 285 LOC)
- Wire-katalog: `_rewrite/docs/packet-ids.csv` (110 CT_CONTROL IDček)
- Shared infra: `Lib/Own/FourStoryCommon/README.md`
- Pattern Login: `Server/TLoginSvrAsio/` + `_rewrite/docs/LOGIN_SERVER_COMPARISON.md`
- Pattern Patch (server-server only): `Server/TPatchSvrAsio/`
- Pattern Log (UDP collector): `Server/TLogSvrAsio/`
- Plán cluster-wide: `_rewrite/docs/MODERNIZATION_PLAN.md` (Fáze 3 — Control)
