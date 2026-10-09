# Original-client acceptance gate

Updated 2026-10-09. The owner confirmed that only uncompiled client source is
available, with no additional client files. No supported executable build can
currently be declared. `TVERSION=0x2918` identifies the checked source protocol;
it does not identify a released binary, regional build or matching data package.

The repository contains `Client/TClient/TClient.vcxproj` and regional resources.
The inventory of executables under `Client/` found no game executable. Bundled
executables under `Lib/3rdParty/HShield/` are SDK utilities, not a playable client.
See [recorded evidence](evidence/client-acceptance-blocker.json). No client source,
packet layout or behavior has been changed to accommodate the modern server.

Acceptance is **BLOCKED**, separately from server implementation, on:

1. An original game executable with exact filename, SHA-256, version metadata,
   region/build flags and provenance. Building existing client source would
   identify a new locally built artifact; it would not establish equivalence to
   an unknown released executable.
2. Its matching game data, configuration and launcher/patch/security dependencies,
   recorded by file hash. Server database backups do not replace client assets.
3. A runnable client environment with the required graphics/runtime dependencies
   and local network access to the test cluster. No such environment is established
   in the current workspace.

Once these are available, keep the client unchanged and record the executable and
data identities before running the acceptance matrix. Use disposable accounts and
database fixtures. Retain packet capture identities, exact actions, expected source
behavior, observed results, server build/image identity, migrations/catalog hashes,
database invariants and recovery outcomes. A failure remains a failed scenario;
do not substitute a synthetic peer result for the missing original-client run.

| Acceptance family | Required original-client scenarios | Current result |
|---|---|---|
| Login/lobby | Authentication, rejection/duplicate/reconnect, character create/list/delete, encoding and regional profile | BLOCKED |
| World lifecycle | Admission, movement/AOI, entities, map transitions, reconnect, server replacement and interrupted handoff | BLOCKED |
| Combat/progression | Spawns/AI, attacks, skills/effects, death, rewards and all quest action types | BLOCKED; substantial server implementation also remains |
| Inventory/economy | Moves, swaps, split/merge/caps, equipment, storage, currencies, shops, trades, crafting and upgrades | BLOCKED; only stated native subsets are server-verified |
| Social/persistent systems | Party/guild/friends/chat/mail, companions, events, ranks and wars | BLOCKED; native persistence and gameplay remain incomplete |
| Original modes | Normal worlds, tutorial/special branches, BOW/BR and other indexed modes | BLOCKED; mode parity remains incomplete |
| Operations/integrations | Control/admin tools, Patch/launcher, Log/audit and original security dependencies | BLOCKED; process smoke is insufficient |

The [capability matrix](capability-matrix.md) and complete
[source inventory](evidence/source-inventory.json) retain the server-side backlog.
Continue independent implementation and native PostgreSQL/protocol verification;
do not mark the overall modernization goal complete while this gate or any
required implementation remains open. All commits and test images stay local on
`main`; no external publication is authorized before complete gameplay.
