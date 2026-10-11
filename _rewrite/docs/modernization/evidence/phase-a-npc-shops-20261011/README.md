# Native NPC shops and PvP balances — Phase A

Code `3bc3bd169b31631409abc8c4583bbfa656dadb50`, based on `f2a7060`, local
`main`. Status **INTEGRATED_UNVERIFIED** for the bounded paths below. The full
Phase A gate remains open. [Build metadata and hashes](implementation.json)
record successful Debug/Release Map compilation/linking only. Initial logs and
the final incremental logs are retained. No tests, migrations, imports, database
reconciliation, server/client processes or containers were run.

## Source and integrated contracts

- `TMapSvr/CSHandler.cpp:6192,6247,6484`, `CSSender.cpp:2756,2942,2957`,
  `TNpc.cpp` and `TMapSvr.cpp:3719` define NPC merchandise, listing and purchase.
  Original `CS_NPCITEMLIST_REQ`0x5302, `CS_ITEMBUY_REQ`0x5304 and
  `CS_MAGICITEMBUY_REQ`0x54D6 now enter the existing ready-primary dispatcher,
  gameplay lease and native `ChangeQuest` transaction. Requests remain2/11/9
  bytes. Buy ACKs remain15/13 bytes; security prompts precede purchase decoding.
- Listings preserve unsigned source map order/deduplication, class/country
  filters, ordinary/PvP price calculations and the distinct trainer-level gates.
  Trainers1/19, item2, PvP21 and magic25 are integrated. Magic descriptors omit
  the inventory slot and do not allocate live item identities. The display-only
  expiration is source days plus59 minutes; count and raw options stay intact.
- Purchases retain deal/tournament validation, quest-item terms, stack clamp,
  capacity before NPC-call debit, original caller sentinels/results, item
  ADD/UPDATE, price/title side effects and ordinary GETITEM hooks. Magic purchase
  intentionally has no source CanTalk/GETITEM call. It charges requested/clamped
  count but copies the source template's count1. Its expiry is current time plus
  source days, including immediate expiry for day0; expiry timers remain open.
- Fresh inventory rows use fingerprints; transferred characters use their
  authoritative complete graph. Owner/token/connection/epoch and checkpoint,
  cast/quest/item heads fence writes. Item/caller/purse/PvP/title/quest/checkpoint
  changes and ordered response receipts commit together. No write is retried
  after an uncertain COMMIT; identical bytes can be a new valid purchase.
- `TMapSvr.cpp:9476` uses the template's base attribute grade (or source grade0
  when missing), unlike item resale. Float multiplication, DWORD narrowing and
  discount wrap are explicit. `TPlayer.cpp:5357` spends only useable points and
  emits total/useable/event9/month before buy ACK; zero cost sends no point ACK.

## PostgreSQL and historical data

New unapplied058 adds `app_world.pvp_balances`, nullable PvP triples in existing
quest receipts and an offline import receipt. CHARINFO, primary handoff,
checkpoint/recovery and native spending carry the three original DWORD fields.
Fresh balances are checked against normalized rows; graph balances stay in the
graph. An older checkpoint cannot restore spent points. Runtime grants add only
UPDATE of `available`; they do not grant arbitrary point awards.

Backup `TGetPvPRecord` and `TGetMonthPvPoint` explicitly initialize missing rows
to zero. Their monetary portion is implemented, **not** their class records,
ranks, history, rewards or monthly rollover. Those responsibilities remain open.
`ReadQuestMagicItem` shares the existing quest reward's raw template projection;
039/051 complete catalog publications remain prerequisites.

`import_pvp_balances.py` requires the full pinned archival manifest, both original
PvP tables and an exact injective character map of their union. Signed INT bits
are preserved as unsigned DWORDs. Multiple monthly rows for one character are
rejected because the original getter omits country; no arbitrary row is chosen.
The importer checks archival values, offline Map owner locks, settled sessions
and absent recovery/native state, then inserts all balances and receipt atomically.
Class/recent/rank data stay in the archive pending their native realization.

For **later Phase B only**, apply the complete001–058 chain with the existing
`migrate.py schema`, refresh `deploy/sql/map-runtime-grants.sql`, and supply the
already documented matching character/actor/trainer/quest catalogs. Once the
complete archive and explicit character mapping exist, the additional command is:

```sh
python3 tools/database/import_pvp_balances.py \
  --manifest /private/complete-archive/manifest.json \
  --world 0 --character-map /private/character-map.json
```

These are input placeholders, not newly created files or executed commands.
Earlier migrations001–057, source files and backups are unchanged; next schema059.

## Explicit differences and remaining work

- Reject the original unknown-nonzero-quest free-purchase exploit. Selecting a
  remote BOW NPC cannot activate the non-BOW build's no-payment branch. Real BOW
  bonus-point currency remains an explicit missing contract, never fabricated.
- Reject undefined float/DWORD conversions, invalid class shifts and listings
  over255 entries instead of reproducing undefined behavior/count wrap.
- Existing source-ordered inventory placement and modern commit-before-response
  policy apply. Calendar operations retain the documented native UTC policy;
  the original executable/timezone comparison remains unavailable.
- Territory country/conditional discounts, portal8/monster22 shop branches,
  deal/store/tournament producers and BOW remain required. Portal/monster branches
  throw an explicit missing-contract error; other original intentional no-op NPC
  types retain their four-byte header. This is not complete NPC parity.
- Item expiration/alarms, expired bag mail, general equipment/effects and full
  PvP/World/social persistence remain open. No PostgreSQL-only cluster claim.

Later acceptance must cover source-independent packet/order checks; trainer
gates and duplicate catalog keys; ordinary/quest/magic/PvP purchases; full bags,
caller aliases/sentinels, insufficient funds, title effects and repeated valid
commands; fresh/graph reconnect, concurrent owner loss, rollback, interrupted
handoff and unknown COMMIT; complete import/upgrade/grants and ambiguous monthly
rows. Real client EXE/assets are still missing; no supported binary is declared.
