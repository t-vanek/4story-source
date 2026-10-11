# Item identification and upgrades — Phase A, unverified

Code `0fe24d24deb6a2868ada92d7c80b5f9f2934dfc2`, based on `2d9d369`, local
`main`. **INTEGRATED_UNVERIFIED. Phase A is incomplete.** No push, tests, database
execution, server/client startup or container work. Original sources, backups,
migrations001–055 and earlier evidence are unchanged. New056/057 are written only.

## Production path

The existing Map dispatcher accepts GAMBLECHECK5424, GAMBLEOPEN5426 and
ITEMUPGRADE5329. `OnNativeQuestReq` obtains the ready primary gameplay lease,
freezes its snapshot, samples World events and enters the existing native
`PostgreSQLMapService::ChangeQuest` transaction. No alternate server/backend was
introduced. Current account, session, Map owner, connection generation, authority
epoch, inventory fingerprints, quest/cast/checkpoint heads fence the mutation.

`postgresql_map_item_gamble.cpp` and `postgresql_map_item_upgrade.cpp` execute
the source branches, then the same transaction records ordered responses and
World observation, item/purse/title/effect/quest state and the new checkpoint.
Fresh owners change normalized rows; graph owners retain full-graph authority.
New IDs use the existing locked world high-water allocation. Unknown commit
outcomes close the session without replaying the write. Identical request bytes
are not treated as a duplicate action. These guarantees still require Phase B
fault/concurrency acceptance for the newly integrated operations.

## Source contracts

- `TMapSvr/CSHandler.cpp:13323` / `:13404`, `CSSender.cpp:4658` / `:4675`,
  client `CSSender.cpp:1682` / `:1696` and `CSHandler.cpp:13137` / `:13199`:
  quote request BYTE bag/slot + WORD NPC; open adds BYTE caller bag/slot.
  Quote errors send zero defaults. Open always has result + three DWORD currency
  fields + BYTE bag/slot; descriptor omits the second slot byte. Source failure
  omits descriptor even when the original stack still has units. Full inventory
  includes the old descriptor. No secure-code/dead/riding gate exists here.
- Quote and open have different wrapping/appearance precedence. Quote first
  selects gamble/seal cost, then free wrapping/appearance; open clears wrapping,
  else appearance, before the random/cost path. Both preserve FindTLevel's first
  entry fallback. Discount multiplication keeps the source DWORD wrap.
- Identification copies the original item to a new identity, including raw magic,
  six extensions, durability and expiry, and starts at count1. Successful template
  replacement preserves those fields, sets source random count, generates magic
  and resets expiry; grade becomes0 for surviving random outcomes. A single unit
  replaces its old slot without ADD/UPDATE. Multi-unit sources require both an
  existing blank and CanPush **before** debit, then merge/split through PushTItem.
  Full inventory keeps item/caller/money but retains UseMoney(FALSE)'s title side
  effect. Replacement failure/destruction, NPC-call consumption, MONEY, OPEN ACK,
  and conditional GETITEM progress/trigger order follow the original.
- `TMapSvr.cpp:2329`, `DBAccess.h:3302`: use every TGAMBLE row in its type/kind
  group, without filtering min/max levels. Preserve WORD accumulation and the
  original `>=` selection boundary, including selection0. World event10 changes
  gamble chance through CalcProb; it consumes no item-probability effect.
- `TMapSvr.cpp:6859`, `TMapSvrModule.h:385`, `TObjBase.cpp:4911`,
  `TItem.cpp:669`: shared magic generation preserves option-count draws,
  exclusions, raw WORD values, formula34 and Catalyzer's sorted map/six draws.
  The GAMBLE call's fourth argument is **wMagicBuff**, not bMakeType; base remains0.
  Actual scrolls pass IMT_SCROLL and derive the source equip/power/level base.
  LoadData value-initializes TITEMMAGIC but never copies bMinLevel; runtime uses0
  while the original database value remains archival. Do not silently “correct”
  either quirk into a different reward distribution.
- `CSHandler.cpp:7177–8096`, `CSSender.cpp:3349` / `:3367` / `:5498`:
  ITEMUPGRADE reads ten bytes: four BYTE target/material positions, WORD NPC,
  two BYTE caller positions and WORD color. Secure prompt precedes decoding;
  non-ready deal/store states are silent. Preserve all ordinary switch branches:
  numeric upgrade/downgrade, gems, appearance, magic/rare magic, clear magic,
  wrapping, equip-level reduction, clear refine, grade/cloak effects, color,
  undye and accessory scrolls. These are implemented branches, **not verified
  complete gameplay**; the dependency restrictions below still apply.
- Successful numeric upgrade literally assigns **level24** in the reference;
  its unused grade draw remains consumed. Failure checks tutorial protection
  before consuming ITEMGUARD, then keeps the original level11 floor and downgrade
  distribution. DOWNGRADE keeps BYTE subtraction/wrap. Gems keep the special
  value1337, item18191 failure rule and gem2 multiplier condition. Rare magic
  failure destroys the complete target stack. The source has no extra MONEY,
  equipment broadcast or USEITEM quest notification for ITEMUPGRADE.
- Accessory selection follows CTBLAccessoryMagic order and BYTE index narrowing,
  magic50/51 exceptions, exclusion behavior and the source expression in which
  MinValue cancels. `data-profiles.json` reports **0 accessory rows**, **78 gamble
  rows**, **6 gem rows** in the historical backup. These counts were read from
  existing evidence, not independently recounted in this phase. An actually
  published empty accessory chart deliberately consumes the scroll and sends no
  result, as the source does. Missing publication is an error, never that no-op.
- `TPlayer.cpp:5205`, `TSkillTemp.cpp:543`, `TMapSvr.cpp:9615`:
  ITEMUSE adds pure skills905/911/919/921/922/939 to existing912/913. Pinned raw
  status/value pairs are905→42/100,911→60/100,919→64/100,921→42/150,
  922→42/200,939→42/300. Same-ID use is rejected; all status42 lucky potions
  exclude one another. Self-Defend, zero-MP handling, maintained persistence,
  transfer and one-use upgrade/magic/guard consumption use the existing path.
  HaveItemProbBuff's first nonzero value and EraseItemProbBuff's first matching
  row remain distinct. The tutorial short circuit does not consume the guard.

## Database publication and explicit differences

056 adds `item_gambling` publication and ordered TGAMBLE/magic views.057 adds
`item_upgrades` publication and gem/accessory/magic views. Existing archive rows
and old receipts are untouched. The existing `activate_quest_catalog.py` gains
`--item-gambling` and `--item-upgrades`, alongside `--special-boxes`. It requires
complete verified archive tables, matching active character/actor/Map charts,
pinned backup identities and the existing offline owner barrier. It rejects
zero gambling divisors and ambiguous/missing addressable gem grades. Extended
profiles cannot be silently removed by retrying an older profile. Full archive
import is still independent of which content is currently used by Map.

For later Phase B only, after isolated migration of the complete chain and a
complete archive import, publication uses the existing command:

```
python3 tools/database/activate_quest_catalog.py --manifest /path/to/full-archive/manifest.json --global-database-alias TGLOBAL_GSP=TGLOBAL_RAGEZONE --special-boxes --item-gambling --item-upgrades
```

This command was **not run**. The manifest path is an input placeholder, not an
existing delivered archive. Reapply `deploy/sql/map-runtime-grants.sql` for the
new explicit item attribute-column permissions; no role changes were applied.

Explicit repairs/policies, not claims of original executable equivalence:

- Keep source material fields in an owning copy before deleting its last unit;
  reject target/material/caller alias cases that otherwise dereference deleted
  objects. Source alias rejection results are documented modern safety changes.
- Do not write historical gem row5 past the source five-entry array. Preserve all
  archival rows; require unique grades0–4 for actual runtime indexing. No guessed
  probability is inserted. Invalid divisors, undefined float conversions and
  accessory selections with no terminating candidate abort the transaction.
- Unordered SQL becomes explicit archive extract order. This is reproducible,
  but does not prove the order of an original running SQL Server query. Source
  Windows15-bit modulo bias is retained using existing independent secure draws.
  Floating-point boundary equivalence to the original Windows CRT remains an
  acceptance requirement.
- Expiry uses the existing explicit native UTC policy, source midnight rounding
  and DWORD arithmetic. Historical wall-clock data is unchanged.
- Raw accessory magic0 is allowed in the live projection and derives client
  value1, as GetMagicValue does. Fresh historical loading retains the source zero
  filter; full-graph recovery retains the newer zero-valued live option instead
  of source ReadTITEM discarding it. This is an explicit recovery difference,
  unverified; the pinned empty accessory table does not exercise it.
- Native commits precede all response delivery. Conditional guild/hero territory
  discount/probability authority is still required and rejects missing authority.
  Premium/PCBang admission remains blocked; no zero-bonus fallback completes it.
  BOW's compile-time no-op variant, trade/shop/tournament producers, general
  effects, exact durable timer sampling and other economy systems remain open.

## Compilation and deferred acceptance

Debug and Release `tmapsvr_asio` built/linked successfully with C++20 and native
PostgreSQL enabled, `BUILD_TESTING=OFF`. Exact commands, selected caches, source,
binary and log hashes are in `implementation.json`. The two preceding Debug logs
record intermediate working-tree compilation; `debug-build.log` and
`release-build.log` apply to code0fe24d2. Existing indentation warnings remain in
checkpoint/transfer/replica code. No new warning remains in the two new modules.
Python was AST-parsed without importing/executing the publication tool.

**Tests executed:0.** Phase B must cover independent original wire bytes/order,
quote precedence and errors, full inventory/stack splits/single-slot replacement,
all random boundaries and option exclusions, title/caller ordering, every upgrade
outcome, lucky potion collision and guard consumption, raw high-bit fields,
zero-magic lifetime, stale owner/checkpoint rejection, simultaneous spends,
disconnect before/after commit, lost commit response, DB loss, process restart,
fresh and graph-backed relogin and interrupted cross-Map transfer. Validate full
archive import, upgrade/new-install migrations and least-privilege grants.

No supported original client executable/hash or matching assets exist here.
Actual client acceptance and the full container cluster have not been run.
