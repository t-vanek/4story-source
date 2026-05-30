# TMapSvrAsio — dispatcher shape decision

Resolves the second open roadmap item: *"Dispatcher shape — the legacy
`SSHandler.cpp` switch recompiles every server when one packet id
changes; a register-based dispatch (handler table indexed by
`MessageId`) or a schema-versioned codec (flatbuffers / protobuf) would
decouple them."*

**Decision: keep the thin central `switch` for wire-message dispatch;
use register tables only for *internal* game-logic dispatch (quest terms,
AI commands). Reject the schema-versioned codec — the wire is fixed.**

## TL;DR

The premise is half-stale. The legacy pain was a **fat** switch — 20k
lines of handler *logic inlined into one `switch`*, so touching any
handler recompiled the whole file. The modern servers already escaped
that: the switch is **thin** (each case is one `co_await OnXxx(...)`),
and the logic lives in per-family sibling files. So "the switch
recompiles the world" no longer applies — editing a handler recompiles
only its own `.cpp`.

What's left to decide is whether to replace the thin switch with a
register table. The answer, grounded in the codebase: **no, for wire
dispatch.** The two layers want different things:

| Layer | What flows | Shape | Why |
|---|---|---|---|
| **Wire dispatch** | `MessageId` → handler | **thin `switch`** | fixed id set, compile-time exhaustiveness + type-safety, zero overhead, already consistent across all 5 modern servers |
| **Internal logic** | `QTT_`/`QT_` term & action types, `TAICmd*` | **register table** | open-ended type catalogs, data-driven content, see [`QUEST_ENGINE.md`](QUEST_ENGINE.md) |

## What the modern dispatch already is

`handlers/dispatch.cpp` (TMap) and `TWorldSvrAsio/handlers/dispatch.cpp`
are both a `switch (ToMessageId(wId))` where every case is a two-line
`co_await OnXxx(peer/sess, std::move(body), ctx); co_return;`. The
handler **bodies** live in per-family files (`handlers/session.cpp`,
`handlers_guild.cpp`, `handlers_party.cpp`, …). Cross-cutting concerns
(rate-limit, metrics counter + latency, audit event, exception capture)
wrap the switch *once* in the outer `Dispatch` — not per case.

**The scaling proof is already in the tree:** `TWorldSvrAsio` dispatches
**181 handlers** through this exact shape. It's a long switch, but it's a
flat, grep-able manifest organised by feature blocks (`// ---- W3a-4:
…`), each case trivially readable, and adding a handler touches it by
**one line**. No recompile-the-world, no merge-conflict storm in
practice (the conflict surface is a single line in a file sorted by
feature).

Contrast the legacy `SSHandler.cpp`: ~20k LOC because the handler logic
*was the switch body*. The modern split (thin switch + sibling files) is
the fix the README was reaching for — and it's already shipped.

## Why not a register table for wire dispatch

A `std::unordered_map<MessageId, HandlerFn>` with self-registering
handlers trades real properties for a marginal one:

* **Loses compile-time exhaustiveness + type-safety.** The switch binds
  each id to a handler with its exact signature; a misspelled or
  duplicate registration in a table is a *runtime* surprise. The two
  dispatch domains even take different first args (`AsioSession` for the
  client, `PeerSession` for the world peer) — a type-erased
  `std::function` table blurs that the switch keeps honest.
* **Adds machinery for a non-problem.** Registration order / static-init
  lifetime, the type-erasure wrapper, and a map lookup replace a
  compiler jump table to avoid… a one-line edit to a central file. That
  edit is actually a *feature*: `dispatch.cpp` is the one place you can
  read every message the server answers.
* **Breaks consistency.** All five modern servers (Login / Patch / Log /
  Control / World) use the thin switch. A bespoke table in TMap alone is
  a maintenance tax for a single-maintainer codebase.

The one genuine benefit of a table — uniform middleware across every
handler — is **already achieved** by wrapping the switch in `Dispatch`
(TMap does metrics + audit + rate-limit there). You get the cross-cutting
behaviour without giving up the switch.

## Why not a schema-versioned codec (protobuf / flatbuffers)

This option is excluded by the project's founding constraint and is
orthogonal to dispatch anyway:

* **The wire is fixed.** The whole point of the emulator is byte-for-byte
  compatibility with the shipped client, which speaks the legacy binary
  protocol (RC4 + XOR + `operator<<` framing). You cannot put protobuf in
  front of `CS_*` client traffic — the client wouldn't parse it.
* **Server↔server could, but shouldn't here.** `MW_/DM_/SS_` traffic is
  between our own daemons, so it *could* use protobuf — but the cluster
  already speaks the legacy 8-byte SS frame, it works, and introducing a
  second codec fragments the wire layer for no functional gain on a
  preservation server. Defer until there's a concrete need (e.g. a gRPC
  migration), tracked separately in `PEER_PROTOCOL_PLAN.md`.
* **It's a serialization decision, not a dispatch one.** Changing the
  codec wouldn't change how messages are routed to handlers; conflating
  the two is what made the option look attractive.

## The reconciliation with the quest engine (#1)

"Register table vs switch" isn't one decision — it's two, by layer:

* **Wire messages** → thin `switch`. Fixed id catalogue, type-safe,
  consistent, proven to 181 handlers.
* **Internal game-logic types** → register tables. The `QTT_` condition
  evaluator and `QT_` action registry from `QUEST_ENGINE.md`, and the
  eventual `TAICmd*` dispatch, are *open* catalogues driven by DB
  content — exactly where a data-driven table earns its keep.

Same primitive, opposite call, because the inputs differ: a closed,
compiler-known id set wants a switch; an open, data-defined type set
wants a table.

## Optional follow-up (not blocking)

`TWorldSvrAsio`'s `Dispatch` lacks the metrics/audit/rate-limit wrapper
TMap's has. If the cross-cutting plumbing is wanted cluster-wide, extract
TMap's outer-`Dispatch` wrapper into a small shared helper
(`fourstory_common`) that both servers call around their switch. Pure
cleanup; doesn't change the dispatcher shape.
