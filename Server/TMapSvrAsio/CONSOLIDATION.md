# TMapSvrAsio — Consolidation guide

How to port a legacy handler from `legacy_src/` into the modern
scaffolding. Read `ARCHITECTURE.md` first for the layer map.

## When to port what

The audit in PR #25 found:
- 297 `OnCS_*` handlers in legacy → 14 wired here
- 300+ `OnDM_/MW_/SS_` handlers → 1 wired here
- 358 `SendCS_*` ACK senders → 3 wired here
- 24 chart tables still un-loaded

**Priority signal**: how often does the legacy code grep for the
handler? PR #25 lists the top-10 hot ids. The deepest gameplay
gaps (damage calc, AI tick, quest engine) need design work before
the handler-by-handler porting helps.

## The recipe

For one `OnCS_XXX_REQ` from `legacy_src/CSHandler.cpp`:

```
┌───────────────────────────────────────────────────────────────┐
│  Step 1 — locate                                              │
│    grep "OnCS_XXX_REQ" legacy_src/CSHandler.cpp               │
│    Find the matching Send in legacy_src/CSSender.cpp.         │
│    Note any DB queries (DEFINE_QUERY in the function).        │
└───────────────────────────────────────────────────────────────┘
┌───────────────────────────────────────────────────────────────┐
│  Step 2 — verify message id                                   │
│    grep "CS_XXX_REQ" Lib/Own/TProtocol/include/MessageId.h    │
│    Confirm the id exists (the modern dispatch needs it).      │
└───────────────────────────────────────────────────────────────┘
┌───────────────────────────────────────────────────────────────┐
│  Step 3 — pick or add a handler file                          │
│    handlers/session.cpp        connect/disconnect family       │
│    handlers/movement.cpp       move / jump / position         │
│    handlers/npc.cpp            NPC interaction                │
│    handlers/skill.cpp          skill cast                     │
│    handlers/quest.cpp          quest accept / exec / drop     │
│    handlers/social.cpp         chat / party / friend          │
│    handlers/bow.cpp            BR / Bow mode-gated            │
│    handlers/control.cpp        CT_* (operator)                │
│    Or create a new file if the domain is new.                 │
└───────────────────────────────────────────────────────────────┘
┌───────────────────────────────────────────────────────────────┐
│  Step 4 — declare in handlers.h                               │
│    Add a `boost::asio::awaitable<void> OnXxxReq(sess, body,   │
│    ctx);` next to its peers. Keep the file sorted by family.  │
└───────────────────────────────────────────────────────────────┘
┌───────────────────────────────────────────────────────────────┐
│  Step 5 — implement                                           │
│    1. Decode the body via wire::Reader. Pattern:              │
│         wire::Reader r(body.data(), body.size());             │
│         std::uint16_t wXxx{}; if (!r.Read(wXxx)) co_return;   │
│    2. Validate via session_reg, presence, etc.                │
│    3. Call services (player_service, npc_service, …).         │
│    4. Encode the response with wire::WritePOD / WriteString.  │
│    5. Send via sess->SendPacket (CS_XXX_ACK).                 │
│    6. Optionally broadcast via presence.ForEachInChannel.     │
│    7. Optionally emit audit event from audit/event.h.         │
└───────────────────────────────────────────────────────────────┘
┌───────────────────────────────────────────────────────────────┐
│  Step 6 — dispatch                                            │
│    Add `case MessageId::CS_XXX_REQ: co_await OnXxxReq(...)`   │
│    to the switch in handlers/dispatch.cpp.                    │
└───────────────────────────────────────────────────────────────┘
┌───────────────────────────────────────────────────────────────┐
│  Step 7 — verify                                              │
│    Build, then sanity-check the dispatch parity:              │
│      grep "On[A-Z]" handlers.h | wc -l        # decl count    │
│      grep "case MessageId::"   handlers/dispatch.cpp | wc -l  │
│      grep -hE "^On[A-Z]" handlers/*.cpp | wc -l  # impls      │
│    All three numbers should match.                            │
└───────────────────────────────────────────────────────────────┘
```

## Adding a new SOCI service

For a new per-char table or chart:

```
1. domain/  — define the POD struct(s) returned by the service.
              No SOCI / Asio includes here.

2. db/queries.h — add the named query constant:
       inline constexpr const char* XxxByCharId = "SELECT … FROM …";

3. db/schema_validator.{h,cpp} — add ValidateXxxSchema with the
       column list the SOCI query expects. Wire it into main.cpp's
       schema-check chain.

4. services/xxx_service.h — interface with one virtual method per
       use-case (LoadXxx, SaveXxx, …).

5. services/soci_xxx_service.{h,cpp} — production impl. Use
       db::Narrow8/16/32, db::SafeString from db/row_helpers.h.
       Wrap the SOCI block in try/catch; return empty/nullopt on
       throw and log via spdlog::error.

6. services/fake_xxx_service.h — header-only in-memory impl for
       tests. Same interface; data set via Add / SetRows.

7. handlers.h — add `IXxxService* xxx_service = nullptr;` to
       HandlerContext.

8. main.cpp — instantiate the soci impl alongside the others;
       wire ctx.xxx_service = soci_xxx.get().

9. CMakeLists.txt — add services/soci_xxx_service.cpp to the
       static core.
```

## Adding an outbound MW_ / DM_ message to the World peer

```
1. Find legacy SendMW_XXX in legacy_src/SSSender.cpp.
   Note the field list + order.

2. In the appropriate handler:
     std::vector<std::byte> body;
     wire::WritePOD<std::uint32_t>(body, dwCharID);
     wire::WriteString            (body, name);
     // …
     if (ctx.world_client && ctx.world_client->IsConnected()) {
         co_await ctx.world_client->SendPacket(
             static_cast<std::uint16_t>(MessageId::MW_XXX_ACK),
             std::move(body));
     }

3. SendPacket is strand-serialized (T3) so concurrent handler
   coroutines won't race on the outbound socket.
```

## Adding an audit event

```
1. audit/event.h:
   - Add EventKind enum value.
   - Define the typed POD struct that follows EventHeader.

2. audit/audit_log.h:
   - Add a new `virtual void Emit(const NewEvent&) = 0;` to
     IAuditLog. Same for the AuditLog concrete impl.

3. audit/audit_log.cpp:
   - Add the AuditLog::Emit(NewEvent) body: stamp the header,
     spdlog::info(...) the human form, peer->Send(AsBytes(ev)).

4. Emit from any handler:
     if (ctx.audit) {
         audit::NewEvent ev{};
         ev.hdr.corr = ctx.audit->NextCorrelation();
         ev.foo = …;
         ctx.audit->Emit(ev);
     }
```

## Common pitfalls

- **CS_XXX_REQ vs CS_XXX_ACK.** Legacy uses `_REQ` for incoming
  client→server, `_ACK` for outgoing or for replies. Verify
  direction before assuming it's a handler.

- **Body buffer lifetime.** `tnetlib::AsioSession`'s wire callback
  hands a `std::span<const std::byte>` into its own recv buffer —
  valid only during the synchronous callback. The dispatch already
  copies into a `std::vector<std::byte>` before co_spawn; never
  store the span directly.

- **`SendPacket` thread safety.** `tnetlib::AsioSession::SendPacket`
  isn't thread-safe by itself; `WorldClient::SendPacket` wraps it
  in a strand (T3) but the per-client `sess->SendPacket` from a
  handler must remain serialized (it is — only the dispatch
  coroutine for that connection touches it).

- **Schema mismatch.** A column rename / type change in the live
  DB will surface at the boot-time `Validate*Schema` step with a
  clear error. Don't bypass the validators.

- **Encoding.** The legacy code was CP949 in places; modern files
  are UTF-8 since commit 75b29ea. Keep new files UTF-8.

## Where to find things

| Looking for…                 | Look in                       |
|------------------------------|-------------------------------|
| What a CS_ message does      | legacy_src/CSHandler.cpp      |
| What a CS_ ACK contains      | legacy_src/CSSender.cpp       |
| MW_/DM_/SS_ handler          | legacy_src/SSHandler.cpp      |
| MW_/DM_/SS_ ACK              | legacy_src/SSSender.cpp       |
| Wire id catalog              | Lib/Own/TProtocol/include/    |
|                              |   MessageId.h                 |
| SQL column names             | _rewrite/docs/schema.old-     |
|                              |   dump-2019/…                 |
| Modern service interfaces    | services/                     |
| Domain types                 | domain/                       |
| Wire codec                   | wire_codec.h                  |
| TODO inventory               | CMakeLists.txt (header block) |
