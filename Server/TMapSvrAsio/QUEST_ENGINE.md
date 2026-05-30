# TMapSvrAsio — Quest engine design decision (T7)

Resolves the open roadmap item *"Quest VM vs data-driven engine —
Lua-via-sol2 vs YAML interpreter"*. **Decision: neither. A data-driven,
DB-sourced engine with register-based dispatch.** This doc records why,
grounded in what the legacy quest engine actually is.

## TL;DR

The "Lua vs YAML" framing assumes quests carry *arbitrary logic* that
needs either a scripting runtime or a re-authored data format. They
don't. Reading the legacy engine, a quest is:

* a **fixed action type** (one of ~20 `QT_*`), each a small bounded
  operation on game state — give item, kill a spawn, flip a switch,
  complete + reward, teleport, …;
* parameterised by **data terms** (one of ~20 `QTT_*`) — which monster,
  which item, which area, how many — that already live in the canonical,
  client-shared DB tables;
* wired into an **event-trigger graph** (`CheckQuest`) that advances term
  counters when the world fires an event (mob killed, item collected,
  NPC talked to, area entered, timer elapsed).

That is a **typed state machine over DB data**, not a script host. So:

* **No Lua** — there is no arbitrary logic to script; it would add a
  runtime + sandbox surface for zero expressiveness gain and duplicate
  data that already lives in the DB.
* **No YAML files** — the quest content is canonical in `TQUESTCHART` /
  `TQUESTTERMTABLE` / `TQREWARDCHART` (the same data the legacy client
  reads); re-authoring it to YAML duplicates it and risks divergence.
* **Yes: a register-based engine** — load the definitions from the DB,
  evaluate `QTT_*` terms as conditions and run `QT_*` types as actions
  through two dispatch tables. This is the same primitive design
  decision #2 (dispatcher shape) lands on for the packet layer.

## What the legacy engine actually is

`CQuest` (`legacy_src/Quest.h`) is a base class with a `CreateQuest`
factory and two virtuals: `CheckComplete` (are this quest's terms
satisfied?) and `ExecQuest` (run the type's effect). Each quest **type**
is a subclass — ~20 of them (`legacy_src/Quest*.cpp`). The polymorphism
is just *dispatch on quest type*; the bodies are short and bounded.

Four representative bodies (all verified):

| Type | `ExecQuest` does | Terms it reads |
|---|---|---|
| `CQuestDieMon` | force-kills / arms the monsters at the quest's spawn points | `QTT_SPAWNID` |
| `CQuestGiveItem` | builds item instances, inventory-pushes them, fires a `QTT_GETITEM` trigger | `QTT_ITEMID`+count, `QTT_COMPQUEST` |
| `CQuestComplete` | re-checks a sub-quest's terms, awards the reward, bumps complete-count, chains the next trigger | `QTT_COMPQUEST` |
| `CQuestSwitch` | flips a per-char quest switch | `QTT_SWITCH` |

The shape is identical every time: `CanRunQuest` (level / precondition
gate) → iterate `m_vTerm` switching on `m_bTermType` → perform a fixed
operation with the term's `dwTermID` / `bCount` → `CQuest::ExecQuest`
to propagate the trigger chain. There is no branching beyond
"which term types are present", no loops beyond "for each term", no
expression evaluation.

Events advance quests through `_AtlModule.CheckQuest(player, …, id,
QTT_type, TT_type, count)`: when the player kills mob *id*, collects
item *id*, talks to NPC *id*, enters an area, or a timer fires, every
running quest with a matching term gets its counter advanced; a quest
whose terms are all satisfied becomes completable.

## The catalogs (the whole surface)

**Action types — `QT_*` (~20):** CHAPTERMSG, COMPLETE, CRAFT,
DEFENDSKILL, DEFTALK, DELETEITEM, DIEMON, DROPITEM, DROPQUEST, GIVEITEM,
GIVESKILL, GUILD, MISSION, NPCTALK, REGEN, ROUTING, SENDPOST, SPAWNMON,
SWITCH, TELEPORT.

**Condition terms — `QTT_*` (~20):** MONID, SPAWNID, HUNT (kill),
GETITEM, USEITEM, ITEMID (items), SKILLID, TALK, SWITCH, MAPID,
TOP/BOTTOM/LEFT/RIGHT/HEIGHT (spatial bounds), TIMER, COMPQUEST,
QUESTCOMPLETED, TCOMP, TSTART (chain / lifecycle markers).

Both are **closed sets**. New game content is new *rows* in the DB
referencing these types — not new code, and not new scripts.

## Proposed architecture

```
            TQUESTCHART / TQUESTTERMTABLE / TQREWARDCHART   (canonical data)
                              │  SOCI load at boot (like the monster/spawn charts)
                              ▼
                    QuestDef { id, type:QT_, terms:[TermDef{QTT_, id, count}], reward }
                              │
   world event ──► CheckQuest(player, evt, QTT_, id, count) ──► advance matching TermDefs
   (kill/get/talk/                         │
    area/timer)                            ▼  when all terms satisfied
                         ActionDispatch[QT_type](player, questDef, ctx)   ← ~20 handlers
                                           │
                         CS_QUESTEXEC_ACK / CS_QUESTCOMPLETE_ACK + reward + chain
```

Two register tables, both `MessageId`-style switches turned into data:

* **Condition evaluator** keyed by `QTT_*` — given a world event, find and
  advance the matching term counters. Pure, testable against fakes.
* **Action handler registry** keyed by `QT_*` — `using QuestAction =
  std::function<void(IQuestCtx&, const QuestDef&)>`. ~20 small handlers,
  one per type, each the modern port of one `CQuestXxx::ExecQuest`.

Plugs into what already exists: `IQuestService::LoadProgress`
(per-char `TQUESTTABLE` state) stays; a new `IQuestEngine` owns the
definitions + the two tables; `handlers/quest.cpp`
(`OnQuestExecReq` / `OnQuestDropReq`, currently "no objective eval")
calls the engine; the kill / item / talk handlers call
`engine.OnEvent(...)`. The `IQuestCtx` abstracts the game-state
operations the actions need (give item, spawn mob, teleport, send post)
so the engine is unit-testable without a live map.

## Why not Lua-via-sol2

* No requirement it satisfies — quests are data-parameterised instances
  of fixed types, so Lua would be used only to *re-encode* the ~20
  existing handlers as scripts. Expressiveness we don't need.
* Adds a runtime + a sandbox / resource-limit surface (untrusted content
  authors running arbitrary code) for a preservation server that
  explicitly avoids third-party lock-in and prizes hermetic builds.
* The quest data already lives in the DB; Lua scripts would become a
  *second* source of truth that can drift from the client's view.
* It can still be added later as **one** action type (`QT_SCRIPT`) if a
  future quest genuinely needs custom logic — without making it the
  foundation. YAGNI until a real quest demands it.

## Why not YAML + interpreter

* The interpreter half is right (and is what this design is). The YAML
  *files* half is wrong: it relocates content that is already canonical
  in `TQUESTCHART` & co., which the original client and the rest of the
  cluster read. Two copies → drift and a migration burden for nothing.
* The map already SOCI-loads sibling charts (`TMONSTERCHART`,
  `TMONSPAWNCHART`, `TNPCCHART`); quest charts are the same pattern.

## Implementation phasing (for when the gameplay phase resumes)

1. `domain/quest_def.h` — `QuestDef` / `TermDef` / `RewardDef` PODs.
2. `IQuestEngine` + `Fake` + `Soci` (load `TQUESTCHART` /
   `TQUESTTERMTABLE` / `TQREWARDCHART`); schema validators.
3. Condition evaluator + the kill / get-item / talk / area / timer
   event hooks (the combat & item handlers already exist to call it).
4. Action registry — start with the high-frequency types (DIEMON,
   GIVEITEM, COMPLETE, TALK, SWITCH, TELEPORT), then the long tail.
5. Wire `OnQuestExecReq` / `OnQuestCompleteReq` to the engine; reward
   award via the existing inventory / exp services.

Tests mirror the connection-lifecycle work: pure condition / action
units against `Fake*` ctx, plus a characterisation test per ported
`QT_*` type citing its legacy `Quest*.cpp`.
