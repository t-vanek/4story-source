# Bottom-up port of `Server/TMapSvr/`

Direct, layer-by-layer port of the legacy gameplay engine. The goal
is functional parity (legacy is the spec) with a modern C++20 / Asio
/ SOCI stack underneath. Wire-protocol bytes stay identical so a
shipped client connects unchanged.

## Why bottom-up

The legacy code is a god-class architecture: `CTObjBase` references
`CTMap`, `CTSkill`, `CTInven`, `CTLevel`, `CTSpawn`, …; each of
those references back into the central type. Top-down porting forces
half-written stubs for every dependency, which become a maintenance
tax when the real implementation lands. Bottom-up — port the leaf
POD types first, then containers, then aggregates, then the central
class — lets every layer compile and test against the previous one
without forward declarations of yet-to-be-written code.

## Dependency layers (rough order)

1. **Leaf POD types** (this batch): `Stat`, `SpawnPos`, …
   No internal references; small enough to port verbatim with type
   cleanup (`WORD → std::uint16_t`, `BYTE → std::uint8_t`,
   `FLOAT → float`, `m_` prefix dropped).
2. **Compound POD** (`Level`, `Magic`, `Aibuf`, …) — depend on the
   leaf types from layer 1.
3. **Inventory + items** (`Item`, `Inven`) — small classes.
4. **Spatial** (`Cell`, `Map`) — references items + AOI.
5. **Entity aggregates** (`ObjBase`, `Player`, `Monster`, `Npc`) —
   the central god-class plus its subtypes.
6. **Behavior** (`AiCommand` + `MonsterAi`) — depends on entity
   layer.
7. **Quest engine** (24 quest subclasses) — depends on player.
8. **Module-level** (`MapSvrModule` equivalent) — orchestrates
   everything above; wire handlers slot on top.

## Conventions

* **Types**: every legacy `tagTXXX` POD struct becomes a modern
  `struct Xxx { … };` in `namespace tmapsvr::legacy`. Member names
  drop the Hungarian-notation prefix (`m_wMapID → map_id`).
* **Naming**: snake_case for fields, PascalCase for type names. The
  legacy CamelCase is preserved only in `// Source:` comments so
  grep against the legacy still finds the matching definition.
* **Citations**: every modern type carries a `// Source:` comment
  with the legacy file:line so a future reviewer can diff against
  the original.
* **No CString / no MFC**: `CString → std::string`, `CTime →
  std::chrono::system_clock::time_point`, Win32 `TCHAR / LPCTSTR
  → char / std::string_view`.
* **Tests**: every ported type ships a unit test that verifies the
  member count + value-init + (where relevant) wire-faithful layout.
  Tests live in `tests/legacy_port/` and share `spec_fixture.h`.

## Status

| Layer | Type | Source ref | Ported |
|---|---|---|---|
| 1 | `Stat` | `TMapType.h:1713-1722` | ✅ |
| 1 | `SpawnPos` | `TMapType.h:1505-1514` | ✅ |
| 1 | `Level` | `TMapType.h:1724-1740` | ✅ |
| 1 | `AiBuf` | `TMapType.h:1697-1711` | ✅ |
| 1 | `InvenDesc` | `TMapType.h:1742-1746` | ✅ |
| 1 | `TtnmtInven` | `TMapType.h:1927-1933` | ✅ |
| 1 | `SkillData` | `TMapType.h:1945-1955` | ✅ |
| 1 | `QuestTerm` | `TMapType.h:2009-2014` | ✅ |
| 1 | `Aftermath` | `TMapType.h:1272-1278` | ✅ |
| 1 | `Portal` | `TMapType.h:1289-?` | ✅ |
| 1 | `CharAppearance` | `TPlayer.h` + `SSHandler.cpp:DM_LOADCHAR_ACK` | ✅ |
| 1 | `CharPosition` | `TPlayer.h` + `SSHandler.cpp:DM_LOADCHAR_ACK` | ✅ |
| 1 | `HotKey` | `TMapType.h:1448-1453` | ✅ |
| 1 | `InvenItem` | `SSHandler.cpp:DM_LOADCHAR_ACK inven loop` | ✅ |
| 1 | `ActiveSkill` | `CSSender.cpp:CS_CHARINFO_ACK skill loop` | ✅ |
| 1 | `MaintainSkill` | `CSSender.cpp:CS_CHARINFO_ACK maintain loop` | ✅ |
| 2 | `PartyMember` | `TMapType.h:1465-1490` | ✅ |
| 2 | `PlayerPresence` | `CSSender.cpp:SendCS_ENTER_ACK field set` | ✅ |
| 3 | `CharSnapshot` | `SSHandler.cpp:DM_LOADCHAR_ACK` + `CSSender.cpp:CS_CHARINFO_ACK` | ✅ |
