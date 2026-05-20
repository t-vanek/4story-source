# TMapSvrAsio — modernized 4Story gameplay server

Port of `Server/TMapSvr/` (the gameplay engine — entity, AI, quests,
combat, items, NPCs, skills, channels — **~113 000 LOC across 144
files**) onto the same portable Boost.Asio / SOCI / spdlog / TOML
stack as `TLoginSvrAsio`, `TControlSvrAsio`, `TPatchSvrAsio`,
`TLogSvrAsio`. Wire-format byte-for-byte compatible with the shipped
legacy client.

## Status — **F1 + F2a + 22 ported handlers (characterization-tested)**

The binary:

* Loads TOML config, binds the configured TCP port (default 5815 —
  legacy `TSERVER.bType=TMAP` row).
* Accepts client connections, runs the AsioSession codec (RC4 + XOR
  + sequence numbers + checksum) — identical to the production
  `TLoginSvrAsio` wire path.
* Implements 22 wire handlers (out of ~621):

  | Handler | Legacy ref | Active branches | Pending |
  |---|---|---:|---:|
  | `CS_CONNECT_REQ` | CSHandler.cpp:249 | 4 | 4 |
  | `CS_CONREADY_REQ` | CSHandler.cpp:402 | 1 | 3 |
  | `CS_KICKOUT_REQ` | CSHandler.cpp:417 | 1 | 2 |
  | `CS_TERMINATE_REQ` | CSHandler.cpp:14876 | 1 | 0 |
  | `CS_WINLDIC_REQ` | CSHandler.cpp:9 | 1 | 0 |
  | `CS_ARENARANKING_REQ` | CSHandler.cpp:21106 | 1 | 1 |
  | `CS_DISCONNECT_REQ` | CSHandler.cpp:12138 | 1 | 0 |
  | `CS_VERIFYSESSION_ACK` | CSHandler.cpp:19649 | 1 | 0 |
  | `CS_PINGMEASUREMENT_REQ` | CSHandler.cpp:19662 | 2 | 0 |
  | `CS_REGISTERBOW_REQ` | CSHandler.cpp:19678 | 1 | 1 |
  | `CS_CANCELBOWQUEUE_REQ` | CSHandler.cpp:19691 | 1 | 1 |
  | `CS_CHGMODE_REQ` | CSHandler.cpp:1220 | 1 | 1 |
  | `CS_CHGCHANNEL_REQ` | CSHandler.cpp:12145 | 1 | 1 |
  | `CS_CORPSASK_REQ` | CSHandler.cpp:8092 | 1 | 1 |
  | `CS_HEROLIST_REQ` | CSHandler.cpp:14862 | 1 | 1 |
  | `CS_PETCANCEL_REQ` | CSHandler.cpp:15443 | 1 | 2 |
  | `CT_SERVICEMONITOR_ACK` | CSHandler.cpp:27 | 1 | 1 |
  | `CT_CTRLSVR_REQ` | CSHandler.cpp:230 | 1 | 0 |
  | `CT_SERVICEDATACLEAR_ACK` | CSHandler.cpp:214 | 1 | 1 |
  | `CT_ANNOUNCEMENT_ACK` | CSHandler.cpp:72 | 1 | 1 |
  | `CT_USERKICKOUT_ACK` | CSHandler.cpp:92 | 1 | 1 |
  | `SM_QUITSERVICE_REQ` | SSHandler.cpp:9 | 2 | 0 |

* Boot-time `ValidateGlobalSchema` fails fast if the required
  `TCURRENTUSER` columns (`dwUserID` / `dwCharID` / `dwKEY`) are
  missing.
* Every other inbound packet id is logged at `debug` level and
  dropped.

### Test-first methodology

Each ported handler ships with a **characterization spec test** —
one ctest binary per handler, citing the legacy `file:line` for each
branch. Branches that depend on still-pending infrastructure
(`IMapState`, world peer, …) are recorded as `PENDING` with a
source-line reference. See [`tests/LEGACY_SPEC.md`](tests/LEGACY_SPEC.md)
for the workflow.

The ~614 remaining handlers (movement, combat, items, skills, NPCs,
quests, AI, party, guild proxy, GM commands, …) ship in later
phases. **The legacy `TMapSvr.exe` remains the authoritative gameplay
server** — this binary is a scaffold for the modernization work, not
a drop-in replacement.

## Phased roadmap

| Phase | Scope | Estimated effort |
|---|---|---|
| **F1** | Scaffold + `CS_CONNECT_REQ` handshake + dispatcher | 1–2 days ✅ |
| **F2a** | SOCI-backed `IMapSessionValidator` (`TCURRENTUSER`) + schema validator | ~1 day ✅ |
| **F2b** | SS init handshake to TWorldSvr (`MW_ADDCHAR_REQ`/`_ACK`, `DM_LOADCHAR_REQ`) so the client lands in the world | ~1 week 🚧 |
| **F3** | Map state foundation — `MapGrain` equivalent, AOI registry (cell-based, matching legacy `TCell`), `CS_MOVE` / `CS_ROTATE` broadcast | ~1 week |
| **F4** | Combat loop — `CS_ACTION` / `CS_NORMAL_ATTACK` / `CS_SKILL`, monster spawn (`TMonSpawn`), basic AI tick (`TAICmdRoam` / `TAICmdAttack`) | ~2 weeks |
| **F5** | Items + inventory — `CS_ITEM*` / `CS_EQUIP`, `TItem`/`TInven` port; equipment broadcast | ~2 weeks |
| **F6** | NPCs + chat — `CS_NPC` / `CS_CHAT` / `CS_REVIVAL` | ~1 week |
| **F7** | Quests — port of the 24 `Quest*` subclasses behind an `IQuestEngine` interface | ~2–3 weeks |
| **F8** | Server-server — `MW_*` (192 handlers) + `DM_*` (104 handlers) for cluster integration | ~3–4 weeks |
| **F9** | Special modes — BR (`BRSettings.h`) + BoW (`TBowSettings.h`) inlined into the build with feature flags | ~1 week |
| **F10** | Cleanup — telemetry, OpenTelemetry traces on hot path, regression suite vs. legacy | ~1 week |

Total: **12–16 weeks** in a single seat, longer for a 2-person team
working in parallel on different handler categories. The modernization
plan in `_rewrite/docs/MODERNIZATION_PLAN.md` estimates 12–24 weeks
including risk margin.

## Architecture — interface-driven services

Same pattern as `TLoginSvrAsio` to give every Asio server a consistent
shape:

| Interface (planned) | Production impl | Test fake | F-phase |
|---|---|---|---|
| `IMapSessionValidator` | `SociMapSessionValidator` (`TCURRENTUSER` lookup) | `FakeMapSessionValidator` (allow-list) | **F1 fake / F2 SOCI** ✅ |
| `IPlayerService` | `SociPlayerService` ✅ (TCHARTABLE) | `FakePlayerService` ✅ | F2b ✅ standalone |
| `IMapState` | `LocalMapState` (cell-based AOI) | — | F3 |
| `IMonsterAI` | `LegacyMonsterAI` (port of `TAICmd*`) | `StubMonsterAI` | F4 |
| `IItemService` | `SociItemService` (`TITEMTABLE`) | `FakeItemService` | F5 |
| `IQuestEngine` | `LegacyQuestEngine` (port of 24 `Quest*` subclasses) | `StubQuestEngine` | F7 |
| `IWorldClient` | `AsioWorldClient` (outbound to TWorldSvr) | `FakeWorldClient` | F8 |

## Build

```sh
cmake --build build --target tmapsvr_asio -j
```

Test:

```sh
ctest --test-dir build -R tmapsvr_asio --output-on-failure
```

The F1 handshake test (`tmapsvr_asio_handshake`) stands the server up
on an ephemeral port, runs three scenarios (happy path, wrong version,
validator deny), and asserts the expected `CN_*` result on each.

## Run

```sh
cp Server/TMapSvrAsio/tmapsvr.example.toml ./tmapsvr.toml
./build/bin/tmapsvr_asio --config ./tmapsvr.toml
```

The binary listens on the configured port. Without a TOML `[database]`
section it falls back to the **allow-all** `FakeMapSessionValidator`
so the dev path works without a TWorldSvr peer feeding `dwKEY`
tokens. F2 adds the SOCI-backed validator that reads `TCURRENTUSER`.

## Files

```
Server/TMapSvrAsio/
├── CMakeLists.txt
├── README.md                       — this file
├── tmapsvr.example.toml
├── main.cpp                        — CLI, signal handling, service wire-up
├── config.{h,cpp}                  — TOML loader
├── map_server.{h,cpp}              — accept loop + per-session coroutine
├── handlers.{h,cpp}                — Dispatch + OnCS_CONNECT_REQ; log-and-drop default
├── wire_codec.h                    — POD + length-prefixed-string helpers (local copy)
├── services/
│   ├── session_validator.h         — IMapSessionValidator
│   └── fake_session_validator.h    — allow-list dev impl
└── tests/
    └── test_handshake.cpp          — F1 wire round-trip
```
