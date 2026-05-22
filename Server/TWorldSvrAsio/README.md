# TWorldSvrAsio — modernized cluster coordinator

Wire-compatible replacement for `Server/TWorldSvr/` (38 851 LOC,
30 files) running on the `FourStoryCommon` infrastructure (SOCI
pool, spdlog audit, health endpoint) and the `boost::asio` reactor
that the four shipped Asio daemons already use.

> Cluster context: [main README](../../README.md#overall-progress) ·
> patch catalog vs legacy Araz sources:
> [`_rewrite/docs/PATCH_README.md` §6](../../_rewrite/docs/PATCH_README.md#6-tworldsvr)

## Status — W1 scaffold

| Phase | Scope | Status |
|---|---|---|
| **W1** | Scaffold + transport + dispatch stub | ✅ |
| W2 | Char persistence (CHAR + USER + OnRW + ADDCHAR / CHANGECHARBASE / ACTIVECHARUPDATE / CHECKCONNECT) + SociCharRepository | ⏸ |
| W3a | Guild — 76 handlers, own PR | ⏸ |
| W3b | Party + Corps | ⏸ |
| W4 | Friend + Chat + Soulmate | ⏸ |
| W5 | War + Castle + Tournament / TNMT | ⏸ |
| W6 | BR + Bow + Event + RPS + APEX / ARENA / BATTLEMODE | ⏸ |
| W7 | Item + Cash + MonthRank + CMGift + cutover hardening | ⏸ |

The W2..W7 split was sized against a real `grep` of
`Server/TWorldSvr/SSHandler.cpp` (287 unique handlers across 4
families: OnMW=160, OnDM=88, OnCT=23, OnSM=16) plus 3 handlers in
`RWHandler.cpp`. See §4 for the per-feature handler counts that
drove the split.

> **Gating clause** (from PATCH_README §6): the legacy World↔Map
> wire contract is the bigger constraint for this port. W1 ships a
> transport-only scaffold that is safe to land now because the
> packet IDs aren't dispatched yet. **Per-feature handler work in
> W2+ should wait until [`TMapSvrAsio`](../TMapSvrAsio/README.md)'s
> MW wire shape stabilises**, otherwise W2 handlers may need to be
> reshaped when TMap's side lands. Holding the scaffold is OK; the
> infrastructure work in W2 (Soci char repo, dispatch family-file
> split) is independent of the wire shape.

## 1. Directory layout

```
Server/TWorldSvrAsio/
├── CMakeLists.txt                  — wired into root CMakeLists.txt
├── README.md                       — this file
├── tworldsvr.example.toml          — annotated reference TOML
├── main.cpp                        — boot, signal handling, HandlerContext
├── config.{h,cpp}                  — toml++ → AppConfig POD
├── world_server.{h,cpp}            — accept loop on DEF_WORLDPORT,
│                                     max_connections gate
├── world_session.{h,cpp}           — CPacket framing per peer
│                                     (plain TCP, no RC4 on SS link)
├── handlers/
│   ├── handlers.h                  — HandlerContext + Dispatch decl
│   └── dispatch.cpp                — W1: logs + drops every wID
├── wire_codec.h                    — POD reader/writer + length-prefixed
│                                     string (matches CPacket layout)
└── tests/
    └── test_dispatch.cpp           — known wID + unknown wID + checksum
                                      mismatch on one loopback socket
```

W2 lands `db/schema_validator.cpp`, `services/char_registry.{h,cpp}`,
`services/soci_char_repository.{h,cpp}`, and the per-family
`handlers/handlers_char.cpp`. The dispatch.cpp switch grows one
case per ported handler; the family-file split keeps the switch
flat (parallel to `Server/TControlSvrAsio/handlers/handlers_*.cpp`).

## 2. Build

The scaffold is wired into the root `CMakeLists.txt` next to the
four shipped Asio daemons:

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target tworldsvr_asio
ctest --test-dir build -R tworldsvr_asio --output-on-failure
```

W1 has one test — `tworldsvr_asio_dispatch` — that stands the
server up on an ephemeral port, frames a known wID, an unknown
wID, and a corrupt-checksum frame, and verifies the framer accepts
the first two and closes on the third.

## 3. Running

```bash
./build/Server/TWorldSvrAsio/tworldsvr_asio --config Server/TWorldSvrAsio/tworldsvr.example.toml
```

The W1 binary accepts SS connections on `DEF_WORLDPORT` (3815),
logs every inbound packet through the dispatch stub, and exposes
a `/healthz` HTTP endpoint on port 18087. It does NOT touch the
database — `[database]` in the TOML is parsed but unused until W2.

## 4. Phasing — how the SSHandler.cpp surface splits

Real counts from `grep` of the 287 unique `On(MW|DM|CT|SM)_*` names
in `Server/TWorldSvr/SSHandler.cpp` (the legacy monolithic switch),
plus 3 `OnRW_*` in `RWHandler.cpp`. The per-feature totals drive
the W2..W7 PR sizing — guild is its own PR (W3a) because at 76
handlers it is bigger than every other phase combined.

| Phase | Buckets | Handler count |
|---|---|---|
| W2 | CHAR (4) + USER (2) + OnRW (3) + ACTIVECHARUPDATE + GETCHARINFO + ADDCHAR + CHANGECHARBASE + CHECKCONNECT | ~15 |
| W3a | GUILD | 76 |
| W3b | PARTY (8) + CORPS (6) | 14 |
| W4 | FRIEND (18) + CHAT (3) + SOULMATE (8) | 29 |
| W5 | WAR (2) + CASTLE (5) + TOURNAMENT (15) + TNMT (3) | 25 |
| W6 | BR (3) + BOW (1) + EVENT (11) + RPSGAME (3) + APEX / ARENA / BATTLEMODE | ~30 |
| W7 | ITEM (5) + CASH (5) + MONTHRANK (3) + CMGIFT (7) + HELPMESSAGE + PVPRECORD + RESERVEDPOST + cutover hardening | ~30 |

Each ported handler in W2..W7 also gets its matching sender —
`Server/TWorldSvr/SSSender.cpp` is 4046 LOC / ~196 sender funcs,
so realistic effort per phase is **2× the handler count** (handler
+ sender + DB wrapper where applicable).

## 5. Architectural patches that land alongside the handler work

From PATCH_README §6:

| ID | Severity | Patch | Lands in |
|---|---|---|---|
| W-1 | 🟡 | Async DB + per-shard write queue (legacy `m_hDB` is a single DB thread serving all TMapSvr instances) | W2 (introduces `db_pool`) + each subsequent SOCI repo |
| W-2 | 🟡 | Partition the global `m_mapTCHAR` / `m_mapTGuild` locks (per-char actor model, per-guild grain) | W2 (char registry) + W3a (guild registry) |
| W-3 | ❌ | TWorldSvrAsio binary doesn't exist yet | **✅ W1 — closed by this scaffold** |

Other related concerns from `_rewrite/docs/MODERNIZATION_PLAN.md`:

* **`SSHandler.cpp` 14 615 LOC monolithic switch** — replaced by
  one `handlers/handlers_<feature>.cpp` per W2..W7 phase, dispatched
  through the W1 `Dispatch` stub which grows one `case` per ported
  handler. Same shape as `Server/TControlSvrAsio/handlers/`.
* **`DMSender.cpp` raw-pointer handoff** — 48 `new` / 1 visible
  `delete` for outbound DB write packets. The Asio port owns
  outbound bodies through `std::vector<std::byte>` + move semantics
  (see `WorldSession::SendPacket`), so the lifecycle hazard never
  reappears.
* **Threading model** — legacy `_ControlThread` + `_WorkThread` +
  `_BatchThread` (plus a `MAX_THREAD` worker pool) collapse onto a
  single `io_context`. CPU-bound batch work that needs off-reactor
  execution rides on the `db_pool` introduced in W2.
