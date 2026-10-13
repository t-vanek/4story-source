# Phase A — soulmate economy, World catalogs and maintained timers

Status: **INTEGRATED_UNVERIFIED**, except written SQL/publication tooling, which
is **IMPLEMENTED_UNVERIFIED**. Local `main`, based on `75125a2`. No build,
test, migration, grant, importer, server, client, container or runtime verification
was executed in this increment. Previous compilation/test evidence does not
apply to this code. The complete implementation gate remains **incomplete**.

The analytical and development agents used disjoint files; the primary agent
integrated World, source declarations, CMake, grants and documentation. The
pre-existing untracked `_rewrite/docs/audits/` remains outside this work.

## Original source → actual production paths

| Responsibility | Authoritative source | Modern integration |
|---|---|---|
| SEARCH / REGREADY / REG | Map CSHandler13034/13125/13198; SSHandler14230/14307; World SSHandler9355 onward | Map dispatcher → existing native quest operation → original MW request → World selection/preview → exact prepared offer → Map native finalizer → original client ACK |
| Source prices and optional caller | TLEVELCHART via Map DBAccess1047; FindTLevel6420; UseItem9520 | Published character chart DWORD prices, source first-level fallback, caller kind55/count/map gates; native item debit and purse/title checkpoint |
| Directed registration | Backup `TSoulmateReg.sql`; World RegSoulmate4303 | One Map transaction consumes offer, caller, money, directed native row, social audit and quest/checkpoint receipt; no reverse database pairing |
| Delayed/replaced replies | Original MW packet layouts, modern native ownership contracts | Full request/response bytes plus Map token/key/connection/epoch and World token; target current-user/ready claim/name/class/level/country checks; no new packet fields |
| Level-gap end at entry | World CheckSoulmateEnd4288 and SSHandler9129 onward | World ready-primary maintenance and existing level handler → exact-target native END → original MW/CS END; incoming owners also checked |
| Guild-level chart | World DBAccess6–38, TWorldSvr906–929/3976–3987, original15-column schema | Explicit offline publication → schema-qualified native loader → existing GuildLevelCache → existing guild cap/peer/member/cabinet consumers |
| World server messages | World DBAccess665–675; TWorldSvr875–883/5481–5495; RWSender1–31; SSHandler7463/7556/8671 | Existing059 full pinned byte publication → fenced51-row World bootstrap → original DWORD-key relay entries; actual event operator name and TMS no-receiver envelope |
| Durable maintained timers | TSkill100–113; Map SSSender1110–1148/2197–2243; TPlayer955–1005; TObjBase2527–2559 | Actual periodic checkpoint/logout/RELEASEMAIN → explicit exact-ms sample → native normalized/full-graph validation and receipt; original transfer SKILLEND after preparation |

The selected native service implementations are in the actual Map/World CMake
libraries and actual main/dispatch/timer paths. Existing adapters are retained;
their presence is not native PostgreSQL coverage for the remaining features.

## Atomic soulmate boundary and source packet details

Written067 creates server-only `soulmate_offers`. A fenced Map preflight creates
one pending invocation before sending the original World request. World locks
its process generation, sorted participant Map owners and accounts, then current
claims; it prepares the exact original result without changing the relationship.
The Map finalizer fences the current World, both Map owners and sorted accounts,
locks the exact prepared receipt, rechecks target eligibility, and atomically
commits purse/caller/link/audit/checkpoint plus its terminal offer status. Client
results are emitted only after commit. Same bytes after completion are a new
invocation; receipt matching never replays a completed purchase.

SEARCH charges even the original World NOTFOUND response. REG charges only
success; REGREADY consumes neither money nor caller. `UseMoney(FALSE)` retains
the original gold-title side effect. NPC caller debit has no invented USEITEM
quest hook. SEARCH has strict level distance less than3; explicit REG allows3.
Source silence, self/already, secure-code-before-decode, tutorial and ordinary
BOW no-op branches remain separately represented.

SEARCH/REG client replies retain BYTE result, DWORD target, original string,
DWORD region and **three DWORD purse values even on failure**. The independent
client reads the purse only on success. READY retains BYTE/string/BYTE/BYTE;
MONEY precedes final economic success, with source item/title ordering. Shared
World encoders supply both prepared bytes and socket bytes. No invented
`MW_SOULMATE_ACK` body or new client opcode is used.

World polls committed receipts in cyclic batches128 because IDs can commit out
of order. Current active directed rows and same-login ready claims authorize
cache-only projections; END/newer registrations filter obsolete receipts.
Projection sends no second purchase/state ACK. Old World-token receipts are not
replayed after process replacement; fresh entry hydrates the authoritative
social database. Existing World-loss/reconnect lifecycle still requires Phase B
acceptance. Map standalone state/END projections additionally confirm native
SQL authority under an exclusive projection lease before touching the cache.

## Explicit modern differences and unresolved responsibilities

- Relation creation now commits with the price/caller rather than before the
  separate legacy Map response. A stale target or uncertain commit closes the
  connection instead of accepting a fabricated success or retrying a write.
  REG additionally rechecks security and affordability at finalization: the
  original successful response ignores UseMoney(TRUE)'s insufficient-funds return.
  The new guard rejects that stale purchase atomically, while preserving the
  source caller-error precedence. This is an explicit modern safety difference.
- One outstanding economic invocation is allowed per current character claim.
  A15-second unresolved deadline retires the client without replay; a successor
  abandons old receipts while retaining their evidence. Original packets have
  no offer ID. Exact current peer/object and raw-byte checks are required; stale
  identical replies across topology replacement remain a Phase B scenario.
- Initial-load level-gap END waits for the authoritative primary to become
  ready and the World maintenance tick, rather than queueing a DB END while the
  legacy list is loading. Exact-target updates avoid ending a later different
  pairing. Its client timing impact has not been verified.
- World cache projection is asynchronous. A snapshot can become stale before
  projection if another Map finalizer commits; cyclic reconciliation is cache
  only. Native rows remain authority and client-economic replies are not replayed.
  Atomic cross-client cache/packet acceptance remains required.
- The original source loads/looks up guild level0. The previous modern cache
  discarded it; this is repaired. Duplicate cache rows no longer inflate Size.
  Existing cache assertions now retain level0 and cover duplicate/reload counts
  by static source alignment only; they have not been run.
- Native guild-level publication does **not** port guild lifecycle/storage,
  World guild/war/event/rank/service repositories or Map guild hydration. Actual
  Map admission still rejects an unsupported nonempty native guild membership.
- Party soulmate XP and emergency HP transfer are **not implemented** here.
  Their original producers depend on full party monster-reward/combat and
  multi-character transactions. No quest/item/automatic-XP substitute was added.
- Existing generic Map account/claim bindings use positive long long DWORDs
  against signed native INTEGER identities. Own high-bit account/key/character
  admission can fail closed before a write. The new participant lock uses signed
  bits and unsigned ordering, but does not close that broader identity gap.
- World message publication/relay/event/TMS text consumers are integrated;
  operator-list/whisper completion and Map chat/TMS client paths remain OPEN.
  Raw archive text is never replaced by guessed names or UTF-8 wire conversion.
  The chosen ANSI profile and real executable still need acceptance.
- Maintained-timer sampling preserves the eight original durable fields, finite
  expiry omission and indefinite zero. An ephemeral before/clock proof permits
  only identity-preserving duration reductions; core save cannot add/reorder/rank
  effects. Periodic checkpoint waits for the existing progression producer if an
  effect has already expired; it publishes sampled clocks after commit only.
  Logout/transfer do not grant automatic EXP or renew903 merely for saving.
  Frozen/uncertain transfer receipts remain authoritative. General effect
  simulation, save-only TSTORE/localzone filters and the missing full party/combat
  producers remain OPEN beyond the currently admitted pinned effect subset.

## Written guild-level publication and unavailable source material

Written068 introduces `world_compat.TGUILDCHART`, verified release/activation
receipts and an explicit publication gate. `activate_world_guild_levels.py`
requires the complete pinned historical manifest, a verified archival import,
actual value/checkpoint reconciliation and offline Map/World owner barriers.
It changes no archive/player data and is not executed here. Runtime validates
all15 fields, nonnull BYTE widths, DWORD EXP bits and the complete supported
keyset0..10; an unexpected actual keyset is an explicit reconciliation error.

Public backup metadata proves11 rows; it does not prove their actual keyset or
values. No table value hash, private extraction, cap or EXP value is fabricated.
That full private source manifest remains a required input for later publication.
Native World startup selects this loader, but still requires its unported guild/
war/event dependencies; no existing schema validator was broadly bypassed.
Apply the written soulmate and guild-level grant scripts only in the later
isolated migration/acceptance phase.

World messages reuse existing059 and the complete historical
`activate_quest_catalog.py --server-messages` gate, including original text byte
hashes. The new loader requires current verified Map/character/actor dependency
receipts, the51-row TSVRMSG checkpoint and complete unique raw byte projection.
It retains DWORD signed storage bits and validates original256-byte buffers,
no NUL and a lossless configured ANSI profile for non-ASCII content. Required
operator text6 follows the existing publication requirement. Absent text5 uses
the original GetSvrMsg NAME_NULL fallback and its exact empty NetString envelope;
no unavailable keyset is invented. `world-message-runtime-grants.sql` is written only;
no new migration or duplicate publication was added for this chart.

## Subsequent verification — deferred, not performed

1. Build all six services in Debug/Release and sanitizer configurations after the
   implementation gate permits it. Check the changed declarations, CMake units
   and adapter consumers; compilation evidence is currently absent.
2. On an isolated PostgreSQL instance, upgrade immutable001–066 with067/068
   and exercise least-privilege grants. Publish the full pinned guild chart;
   cover missing/bad/ambiguous manifests, scalar/key/hash drift, level0, signed
   EXP bits and live-owner refusal. Never modify original backups/old migrations.
3. Replay independent original-client SOURCE fixtures for every SEARCH/READY/REG
   success/error, MONEY/title/caller ordering, failure purse tails, raw names,
   source price fallback, strict/inclusive level windows and secure/tutorial/BOW
   gates. The real unchanged executable/assets remain unavailable.
4. Exercise simultaneous opposite selections across Maps, same/high-bit account
   identities, level/country/name changes, account security changes, caller/fund
   mutation, target logout/handoff, process takeover and stale duplicate replies.
5. Force failures before/after offer preparation, at every SQL mutation and at
   COMMIT/client-send loss. Prove one debit/link/audit, rollback, no replay and
   authoritative recovery after Map/World replacement or new Login.
6. Check cache-only projection races against newer REG/END, batches over128,
   out-of-order commits, initial own/incoming level-gap END, standalone state
   confirmation and initial-admission/projection lease ordering.
7. Verify all51 message bytes and unsigned DWORD keys against independent RW
   parsers; cover missing publication/byte rows/keys, wrong dependency releases,
   ANSI profiles, TMS envelopes and operator announcement routing. Full client
   chat/TMS and operator behavior require the remaining source paths first.
8. Verify fresh/full-graph effect sampling below/at/above expiry, repeated saves,
   eternal zero,903 charge provenance, transfer preparation/abort/takeover and
   rejected core additions/rank/reorder/time increases or unproved omission.
   Check simultaneous periodic/progression/logout ownership and original transfer
   SKILLEND ordering. Complete general expiry producers and source save filters.
9. Complete the still-open source inventory before claiming full Phase A:
   guild/party/social XP/HP, full combat/AI/spawn/death/rewards, remaining quest
   actions/producers, storage/trade/shop/crafting variants, premium/PCBang,
   companions/mounts, events/ranks/wars/tournaments/BOW/BR and administration.
   Missing pre-version SQL procedures and real-client assets remain explicit
   external contract blockers; ordinary unimplemented code remains OPEN work.

See [the analytical source audit](analysis.md), [full rewrite status](../../full-rewrite-status.md)
and [the complete source index](../source-inventory.json). No original function
is removed from scope by this increment.
