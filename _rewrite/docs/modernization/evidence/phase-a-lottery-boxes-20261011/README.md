# Soul Lottery, item grants, sale and repair — Phase A

Features `c4385af4414e3296c44864b9fe5b39f38f75d660`, followed by secure-order fix
`72d6504c0ab7f783090b69e70aaf5e8b82c8e302`, local `main`, based on `7a7d3ab`.
The following bounded paths are **INTEGRATED_UNVERIFIED**. Migration054 and its
offline publication extension are **IMPLEMENTED_UNVERIFIED**, not applied.
The complete Phase A implementation gate remains open. No test, server, client,
database operation or container was run; no push or publication occurred.

## Original contracts and connected paths

- `TMapSvr/CSHandler.cpp:17724,17779`, `CSSender.cpp:6227` and the independent
  `Client/TClient/CSSender.cpp:2665` / `CSHandler.cpp:3398` establish Soul Lottery.
  Empty requests0x54FB/0x54FD reach the existing Map dispatcher, primary operation
  lease, `OnNativeQuestReq`, native `ChangeQuest` and `ChangeSoulLottery`.
  The original exact soul-EXP threshold, four distinct numbers, DWORD prize
  arithmetic, fallback payout, gold titles and response ordering are retained.
  Draw ACK0x54FC is five DWORDs; settlement ACK0x54FE is two DWORDs and one BYTE.
  No invented ready/pending-draw gate, secure-code gate or dead-player gate.
- Lottery fields are CTPlayer-lifetime state (`TPlayer.cpp` constructor and
  `SSSender.cpp` SAVE/RELEASEMAIN; `SSHandler.cpp` LOADCHAR). New admission resets
  them; original transfer does not carry them. Durable money/title/effect writes
  and response receipts share the owner-fenced quest head. Uncertain commits
  close the session; a new login cannot replay the unpersisted pending draw.
- Settlement forces skill820 for900000ms through existing `ForceProgressEffect`.
  The native loader now admits its pinned status and the Map timer expires it.
  `TObjBase::CheckEternalBuff` tests the **template** duration and bIsuse:820 can
  end at zero MP even while its forced duration is positive. Ordinary cast and
  current self-finish paths now preserve this distinction; LOOPSKILL does not
  acquire an invented zero-MP check. General combat/effects remain incomplete.
- **Data/code discrepancy:** inspected backup skill820 is named Experience Bonus,
  but its skill data is status73 (HUNTING_FEVER_EXP), value200. The original
  `TObjBase::GetGainExpBuff` reads status69 (GAINEXP); no original Map reader of
  status73 was found. Native820 preserves status/lifecycle without inventing a
  200% monster-EXP reward. Full monster reward integration is still required.
- `CSHandler.cpp::OnCS_ITEMUSE_REQ`, `TItem.cpp::RandMoney` and client OPENMONEY
  decoding establish IT_USE money51: up to five original float contributions,
  15-bit modulo draws, DWORD accumulation and the template's WORD*1000 floor.
  Gold titles → MONEY_ACK → OPENMONEY_ACK0x5453 (DWORD amount) → common reuse/
  consumption → ITEMUSE result → USEITEM quest hooks commit together.
  IK_AP28 deliberately only sets bUsed in the original; its consumption/reuse/
  result/quest path is preserved without inventing an AP stat mutation.
- Special box115 uses `DBAccess.h::CTBLSpecialBoxChart`, Map load/CreateItem,
  `CSHandler.cpp:9655` and `TPlayer::HaveInvenBlank/PushTItem`. Native
  `OpenSpecialBox` reads published historical group/class rows, retains raw magic
  percentages and all six DWORD extensions, appearance, count, level, grade,
  durability and refinement. Missing templates are skipped as in CreateItem;
  class6 means all classes. Missing valid group or insufficient space returns
  IU_FULL without consumption/reuse. A blank slot is required even for merges
  or no matching class. A valid empty class selection can consume the box.
  Rewards use existing identity/stack/slot planning and native item persistence.
  PushTItem has no extra GETITEM trigger here; common USEITEM hooks still follow.
- `CSHandler.cpp:6623`, `CSSender.cpp:2970` and client sale send/readers establish
  ITEMSELL0x5306: five BYTEs, response0x5307 BYTE result + three DWORD purse fields.
  Native `SellNativeItem` preserves ordinary security/ownership/count/trade-bit
  gates, optional NPC-call debit, source item power/price, first PRICEUP raw
  status value, EarnMoney/title effects, item debit, ACK and GETITEM count0 quest
  refresh. It adds no separate MONEY_ACK or new quest-action trigger.
  The newly explicit shop/deal/tournament live flags have **no complete native
  producers yet**; this scope covers the existing ordinary non-shop/non-deal
  admission only. Tournament Maps still reject absent authority.
- `TItem.cpp::GetPrice/GetPowerLevel/GetWeaponPowerLevel/GetShieldPowerLevel`
  supply the actual pricing rules: grade/gem attribute key and first unsigned
  WORD fallback, aggregate magic options, original weapon/armor rates, formula
  logs and BYTE narrowing, TLEVEL fallback, FLOAT product then double0.99.
  Template price or minimum level alone is not substituted for power level.
- `CSHandler.cpp:13883`, `TItem.cpp::GetRepairCost`, `TPlayer.cpp::UseMoney`,
  `CSSender.cpp:4858` and client repair send/readers establish DURATIONREP0x543C.
  Native `RepairNativeItems` implements quote, single/equipped/all selection,
  original errors, DWORD cost arithmetic and raw REPCOST112 percentage, optional
  NPC-call debit, purse/title changes and maximum durability restoration.
  Quote ACK0x543D is DWORD cost + BYTE discount; result0x543E is result/count
  BYTEs and reverse traversal entries BYTE bag/slot + DWORD max/current.
  Source `UseMoney(FALSE)` can award titles before a later NPC-call error; that
  side effect is committed too. Restoring broken equipment refreshes the native
  derived stat/skill cache without adding an unsolicited packet. Conditional
  territory/hero discounts and premium/PCBang admission remain blocked by their
  existing explicit authority gates, not replaced by guessed zero discounts.
- `DebitNativeItem` is shared by ITEMUSE, sale and NPC-call consumption. It
  reacquires the same instance after quest/reward changes, validates count and
  fingerprint and performs exact normalized UPDATE/DELETE, or changes the
  authoritative full graph after handoff. Sale/repair/box/payout currency, items,
  titles, effects and existing operation/checkpoint heads commit before replies.
  Equal request bytes are not a deduplication key. Stale heads/owners fail; no
  unknown-COMMIT write is retried. These guarantees still require Phase B proof.
- `72d6504` restores the original security prompt before body decoding for item
  use/sale/repair, including malformed requests while locked. The session lease
  protects the early read; the repository rechecks security within its ownership
  transaction. Soul Lottery retains its original absence of this security gate.

## PostgreSQL and explicit differences

Migration054 adds `special_boxes` to the existing catalog/publication audit and
publishes `game_compat.special_box_release` / `TSPECIALBOXCHART`. Earlier receipts
default to false, so they cannot authorize a chart they did not inspect.
`activate_quest_catalog.py --special-boxes` requires the complete source table,
original values/bytes and pinned import proof along with matching Map/character/
actor manifests and the existing explicit global alias. NULL bindings, ambiguous
one-character class conversions and groups exceeding the original BYTE loop are
rejected with a specific reason. Source rows remain in the archive. An old
profile cannot silently downgrade an existing extended publication.

The historical metadata reports five TSPECIALBOXCHART rows, but the inspected
private character exports do not contain those rows. No box contents were
invented; complete archive extraction/import/publication belongs to Phase B.
After migration, reapply the existing `deploy/sql/map-runtime-grants.sql` for
SELECT on the new views. No new sale/repair/lottery table is needed: existing
quest receipts, title/core/item/effect rows and full-graph recovery are reused.
All earlier migrations and original sources/backups remain unchanged.

- Randomness preserves the Windows 15-bit range/modulo bias, but independent
  OpenSSL draws replace predictable process-global CRT state. Its sequence is
  intentionally different; no uniform-over-N replacement changes the range.
- Special-box SQL has no ORDER BY. Native output uses archive row order for
  reproducibility; original inter-item delivery order is not proven.
- CTBLSpecialBoxChart does not bind TITEMQUERY.m_dEndTime. Zero-day native boxes
  use no expiry rather than reading uninitialized memory. Positive durations
  use UTC epoch + whole days, distinct from quest reward midnight rounding.
- Undefined float narrowing, zero modulo after DWORD wrap, malformed zero-count
  box items and counts too wide for the original response are rejected explicitly.
- Selling/repairing the last same NPC-call instance would leave a dangling source
  item pointer. Native operations reject that case with the original NPC-call
  error; valid shared stacks require enough survivors. Historical data is intact.
- Money and all durable mutations precede responses. Gold-title source quirks
  remain intact, including strict thresholds and category replacement.

## Compilation and deferred acceptance

Both commands returned0 for the final code, with `BUILD_TESTING=OFF`:

```sh
cmake --build build/phase-a-compile --target tloginsvr_asio tworldsvr_asio tmapsvr_asio tcontrolsvr_asio tpatchsvr_asio tlogsvr_asio -j2
cmake --build build/phase-a-quest-release --target tloginsvr_asio tworldsvr_asio tmapsvr_asio tcontrolsvr_asio tpatchsvr_asio tlogsvr_asio -j2
```

Final logs: `debug-security-build.log`, `release-security-build.log`. The full
economy builds also returned0 before this final handler fix. Earlier logs in
this directory are intermediate compile-only steps, not additional acceptance.
Warnings in existing compact control-flow expressions remain in the logs.
Python activation code was AST-parsed without importing/executing it;
`git diff --check` passed. [implementation.json](implementation.json) records
the commit, configuration, toolchain, hashes, source inputs and zero runtime runs.

Deferred until Phase B: exact independent wire cases and source distributions;
draw/settlement/zero-MP/expiry and relogin/handoff; money-item float boundaries;
every box class/stack/full-bag/source anomaly; sale power/price/NPC-call cases;
repair quotes/reverse ACK order, REPCOST and preflight title changes; catalog
upgrade/publication and old-profile retry; concurrent stale owners, rollback,
disconnect/process/DB failure, unknown commits and restart without duplication.
No tests, sanitizers, migrations, imports, servers, clients or containers ran.

Further Phase A work: full monster benefits/rewards/combat/AI, general item and
effect branches, premium/PCBang/pets, trade/store/tournament producers, guild/hero
territory discounts, exact durable effect sampling, other quest actions, complete
World/social/economy/mode/admin responsibilities and unresolved Patch procedures.
REFINE additionally needs CalcProb's authoritative event values and effect
consumption; its zero-material division and NPC-call alias lifetimes need explicit
safety handling. The whole runtime is **not yet PostgreSQL-only**.

Only original client source is available. Client executable, matching assets,
supported binary build/hash and real-client acceptance remain unavailable.
