# Original-client compatibility contract

The original client must remain unchanged. Database/domain migrations must pass
through the existing fixed-width packet boundary. A passing codec round trip is
not sufficient evidence of a particular original executable working.

Latest local increment based on `0b97c18` repairs direct Control–Map framing:
original linked CPacket header is16 bytes, metadata zero and no crypt for
SESSION_SERVER. Control selects SourcePlain16 for Map; the Map listener separates
that configured/current network role from game-client RC4/admission. Announcements
retain group3/sender0/source059 caption6/full NetString/protected1; kick is silent
and uses existing native save/release, with requested-intent audit. Monitor is
periodic DWORD tick followed by four DWORDs; no second echo or fabricated ACK.
DATACLEAR resets historical account membership, not gameplay caches/live users.
Other retained modern8 service/operator/Map–World profiles have not been proved
original framing; earlier source8 wording is superseded by this direct linked-
library audit. Their16-byte port, TLS, direct Map event conflict and full kick/
combat/transfer dependencies remain OPEN. No client/source library changed, and
no build/test/runtime/executable acceptance ran. [Source proof and exact boundaries](evidence/phase-a-control-map-20261014/README.md).

Previous code `0b97c18`, based on `c389976`, keeps original chat/TMS/ban/admin
opcodes, INT32 string lengths, BYTE audiences/types, DWORD IDs/keys and payload
order. Map fallback chat reaches native authority and original CS_CHAT_ACK; TMS
responses strip only the two MW identity DWORDs. Operator text uses published
message6, no-receiver message5/7 and uppercase eight-hex NetString lengths.
Left(1024) return values were ignored in source: full bounded strings remain.
ProtectedCHAT1/INVITE2, current peer/claim generations and admission projections
are enforced. Deliberate changes include durable World bans, commit-before-result,
canonical target names, TMS recipient-ID/key repair, bounded BYTE rosters and
stale/unsolicited response rejection. MW ban zero clears0; Control zero replaces
with now. Notices use sender0, targeted type7 or event groupWORLD3 and native
recipient protection. No extra event gameplay effect is inferred from its BYTE
metadata. Connected Relay, group/mode branches, native operator tools and locale/
DWORD high-bit parity remain OPEN. No build/test/runtime/client acceptance ran.
[Source contract and explicit differences](evidence/phase-a-chat-tms-20261014/README.md).

Previous code `c389976`, based on `75125a2`, connects original
SEARCH21532/READY21538/REG21534 through native Map/World offers. Existing MW
layouts are unchanged; receipt IDs/owner/key/connection/epoch metadata stay in
PostgreSQL. SEARCH/REG replies retain their BYTE result, DWORD target, original
string, DWORD region and three DWORD purse fields even on failure, as the
original server sends; the independent client consumes purse only on success.
READY remains BYTE/string/BYTE/BYTE. MONEY precedes economic success, and
source title/caller ordering and secure-before-decode guards are preserved.
No `MW_SOULMATE_ACK` body is invented. Cache polling emits no duplicate success.
Initial level-gap END now waits for ready-primary maintenance; source level0
guild lookup is restored. These timing/atomicity differences require subsequent
acceptance. World relay message entries now use the source DWORD key plus STRING
value; the native51-row catalog retains original bytes. TMS no-receiver text
retains the uppercase eight-hex-digit BuildNetString envelope, and event
announcements use the published operator name. Maintained effects now sample
exact milliseconds at checkpoint/logout/transfer without extra wire fields;
transfer expiry sends original SKILLEND. Full Map chat/TMS and general effect
simulation remain open. No build, service, test or client ran, and real executable compatibility
remains unverified. [Exact source comparisons and acceptance plan](evidence/phase-a-source-review-20261013/README.md).

Earlier code `1f1eb59` / `e709281` adds native World ownership and directed friend/group/
soulmate persistence (written migration066), original Map friend/group relays,
conditional soulmate FRIENDLIST fields and Map soulmate state/end handling.
Native social/mail transactions fence the current World; social writes also
validate primary Map/session claims. Commits precede cache/replies; uncertain
writes stop World without replay.
The full historical social importer/grants and offline World barriers are written
only. Production World now requires an explicit database, while native guild/war/
event bootstrap and complete soulmate Map economy/XP/load checks remain unfinished.
Map/World compile/link in Debug and Release; no tests, DB or runtime ran.
These scoped paths are **INTEGRATED_UNVERIFIED**, and Phase A/PG-only runtime are
still incomplete. [Source contracts, repairs and compile evidence](evidence/phase-a-native-social-20261013/README.md).

Phase A additions after `2fa2396` are **INTEGRATED_UNVERIFIED**: native quest list
and timers after CHARINFO; original NPCTALK/QUESTDROP/QUESTENDTIMER results;
QUESTEXEC branches for default talk, NPC talk, chapter, drop, mission/guild action,
give/delete item, give skill and routing item-list responses. These do not complete the quest system. Source
action prerequisites and counter/timer semantics replace the old generic accept
prototype; missing action/world integrations still reject rather than issue a
fabricated success. See [working status](full-rewrite-status.md) for explicit
changes to tie ordering, randomness, UTC expiration and transaction boundaries.

Earlier code `11e0df1` / `dd07164` repairs both mutual-friend acceptance
paths: two source directed edges commit together before cache/success responses,
with registered ready-primary checks before persistence and after await. Any
mutual persistence failure stops World without a write replay. World compiles
in Debug/Release; no tests or runtime ran. This preserves the existing schema
adapter and **does not complete native friend schemas/imports, World ownership
or the other best-effort social paths**. [Scope and remaining native work](evidence/phase-a-mutual-friends-20261013/README.md).

Code `e84ece4` / `65be6d1` integrates quest SENDPOST and native offline bill
returns. QUESTEXEC/child hooks select original rewards into the fenced reserved
queue; World returns due offline bills with authoritative item ownership and
notification/audit in one transaction. Written064/065 and `--quest-mail` are
unexecuted. Map/World compile/link in Debug/Release; no tests, DB or runtime ran.
The backup appearance omission is explicitly repaired from CTItem; source gem/
grade losses and distinct online/offline sender-ID rules are preserved. Full
Phase A, PostgreSQL-only World bootstrap and original-client acceptance remain
incomplete. [Contracts, differences and compilation evidence](evidence/phase-a-quest-mail-offline-bills-20261013/README.md).

Code `45d130d` adds the native World reserved-mail queue producer. It uses the
existing Map POSTRECV53A4 path, with recipient live time and no new protocol
fields. Database queue IDs, minute gates and delivery lineage are server-only.
This is INTEGRATED_UNVERIFIED; complete World bootstrap and reserved-mail source
producers remain unfinished. [Source and acceptance scope](evidence/phase-a-reserved-mail-20261013/README.md).

Code `85bf923` / `feea4a1` / `b4ff581` adds **INTEGRATED_UNVERIFIED** POSTSEND53A2:
three INT32-length raw strings, BYTE type, three DWORD funds and BYTE bag/slot;
POSTSEND53A3 returns the original BYTE result. INVEN_NULL is0xFC, while default
bag0xFF remains a valid attachment origin. PROTECTEDADD5377/ERASE5379 carry a
string; OPTION5472 carries DWORD/BYTE without a response. LIST5376 uses BYTE
count then DWORD/string/BYTE; ADD5378 uses BYTE/DWORD/string/BYTE; ERASE537A uses
BYTE/string. Source presence/client response order is retained. Notifications
use the receiving Map current second as in the original World relay. Immediate option durability,
commit-before-send and named security repairs are explicit differences. Raw
non-ASCII mail requires `native_map.client_text_encoding`; ASCII equality remains
the existing native name profile, not proof of full historical collation or a
particular executable. [Exact contracts and deferred scenarios](evidence/phase-a-mail-send-protected-20261013/README.md).

Code `d3904c5` adds **unverified** POSTRETURN53AC (DWORD postID/BYTE type).
Payment retains MONEY then POSTRETURN53AD BYTE0; return retains POSTDEL53A9
DWORD ID. Cross-Map notifications keep POSTRECV53A4. Receipt/watermark metadata
is server-only, outside original transfer bytes. Original SQL binding byte
profiles require explicit import configuration and later original-client proof;
no executable compatibility claim is made. [Source contracts and explicit repairs](evidence/phase-a-mail-returns-20261013/README.md).

Code `18ee496` adds **unverified** POSTLIST54A5 (WORD page), POSTVIEW53A6,
POSTGETITEM53AA and POSTDEL53A8 (DWORD post ID). It preserves source list/view
layouts, no-slot attachment descriptors, GETITEM result BYTE, DELETE DWORD and
four-byte POSTINFO54A6. Expiry uses original SYSTEMMSG5451 subtype17 with
DWORD scheduled seconds and BYTE bag/slot, DELITEM and INVENDEL responses, then
POSTRECV53A4 notifications. All native ownership changes commit before responses.
Alarms, mailbox selection and attachment FK metadata add no client fields.
Source unread counts, unusual pagination, frame bounds, native containment and
bill/delete safety changes are documented in [the exact contract](evidence/phase-a-item-mail-20261013/README.md).

Code `3bc3bd1` connects **unverified** NPCITEMLIST0x5302 (2 bytes),
ITEMBUY0x5304 (11 bytes) and MAGICITEMBUY0x54D6 (9 bytes). The buy replies remain
15/13 bytes; PvP spending emits the original13-byte total/useable/event/month
body. CHARINFO and handoff carry the existing three DWORD point fields. Trainer
and magic listing layouts follow the original sender/client. No new client
fields were introduced; exact scope, source quirks and explicit security changes
are in [NPC shop evidence](evidence/phase-a-npc-shops-20261011/README.md).

Code `0fe24d2` additionally connects **unverified** original GAMBLECHECK5424
(four bytes), GAMBLEOPEN5426 (six bytes) and ITEMUPGRADE5329 (ten bytes). Quote
errors retain zero default fields; OPEN has its original currency header and
optional descriptor without a repeated slot. Upgrade/magic/change-attribute
responses retain their separate source layouts and order. The existing secure
prompt precedes ITEMUPGRADE decoding; the source does not gate GAMBLE with it.
No new client fields or protocol opcodes were added. Owning-copy/UAF repairs,
archive-order policy, raw magic0 graph retention and other explicit differences
are recorded in the [source contract](evidence/phase-a-item-upgrades-20261011/README.md).

Code `ea7c811` additionally connects original completion/crafting result ordering,
item/magic reward packets, GOLD/QUEST title updates, skill rewards and ordinary
EXP/SOULUP/LEVEL/HPMP packets. Original MW_LEVELUP_ACK/REQ remains nine bytes;
new ownership checks add no client fields. Pure918/923 timer expiration emits
SKILLEND then private CHARSTATINFO; source AUTOEXP charge order is preserved.
All are **INTEGRATED_UNVERIFIED**, not executable compatibility evidence.
Code `fa4c282` adds level benefit creation, EXP-item903 restoration/expiry and the
original ITEMUSE layout (WORD template, BYTE bag/slot, WORD delay group, BYTE count,
then DWORD/BYTE target pairs). Source/client implementations override the stale
CSProtocol.h comment. Response order is forced status, item deletion/update,
ITEMUSE result, then quest events, all after the native transaction commits.
The owned item must match the requested template: this explicitly fixes original
cross-item quest-credit spoofing. Both player CHARINFO and ENTER serialize source
maintained fields and sample remaining time. LOADCHAR's discarded903 visual is
recreated at entry, without spurious removal packets. Exact durable sampling at
handoff/logout remains unfinished. These changes are **INTEGRATED_UNVERIFIED**.
See the [compile-only source contract](evidence/phase-a-during-items-20261011/README.md).

Code `ff50e58` extends the same ITEMUSE request to original HP26/MP27/MAXHP42/
MAXMP43/GAINEXP81. Result3 is IU_FULL; a full resource consumes/rearms nothing.
Source HPMP/EXP packets precede item mutation ACK, ITEMUSE success and USEITEM
quest hooks. CHARINFO now emits BYTE count plus ordered WORD/DWORD item-reuse
entries, including expired live entries as zero; SAVE/RELEASEMAIN emit active
entries only. LOADCHAR preserves the tutorial and >=3600000ms filters. Shared
cooldowns use a monotonic 64-bit local observation; avoiding early reuse at the
original DWORD tick wrap is an explicit modern repair. No client fields changed.
Cast reconstruction now restores quest/item clock origins after its relative
calculation. No runtime or packet acceptance was executed.
`6a91e4a` confirms an uncertain transfer using only the exact consumed receipt;
a prepared/absent receipt cannot trigger another promotion attempt.
See [scoped implementation and deferred acceptance](evidence/phase-a-item-use-20261011/README.md).

Code `c4385af` adds the following source-derived packets through existing native
Map transactions. Follow-up `72d6504` restores the original security prompt before
item use/sale/repair body decoding, including malformed locked requests.
All are **INTEGRATED_UNVERIFIED**, with no original executable
or protocol runtime acceptance:

| Operation | Original request | Original response and ordering |
|---|---|---|
| Soul Lottery | 0x54FB / 0x54FD; empty bodies | 0x54FC: five DWORDs; settlement titles → MONEY → 0x54FE: DWORD roll/award + BYTE win → forced820 effect |
| Money item51 | Existing ITEMUSE0x52CA | Titles → MONEY → OPENMONEY0x5453 DWORD amount → common item/reuse/result/quest path; AP28 preserves its original no-stat-change branch |
| Special box115 | Existing ITEMUSE0x52CA | Original ADD/UPDATE item descriptors then box debit/result/USEITEM hook; IU_FULL leaves the box intact |
| Item sale | 0x5306; five BYTEs (bag/slot/count/caller bag/slot) | Optional caller debit → gold titles → sold item debit → 0x5307 BYTE result + three DWORD purse fields → GETITEM count0 update; no separate MONEY |
| Durability repair | 0x543C; BYTE quote/type/bag/slot + WORD NPC + BYTE caller bag/slot | Quote0x543D: DWORD cost + BYTE discount. Execution preserves preflight titles, caller debit, charge/title/MONEY then0x543E result/count BYTEs + reversed BYTE bag/slot and DWORD max/current entries |

Sources are the original Map handlers/senders, TItem calculations and independent
client send/readers, identified in the
[source contract and differences](evidence/phase-a-lottery-boxes-20261011/README.md).
Source lottery state resets with player-object admission; no client transfer
field was added. Skill820's pinned status73 is not the source GAINEXP69 reader,
so no EXP multiplier is inferred from its display name. The template zero-MP
rule still applies to820 with a finite forced duration. Secure 15-bit draws,
archive ordering for unordered box SQL, defined zero-day expiry and rejection
of undefined numeric/caller-pointer cases are explicit modern differences.
Shop/deal/tournament producers, conditional territory discounts and premium
admission remain incomplete; these packets do not certify those game systems.

Code `0457d3d` / `750c797` adds these **INTEGRATED_UNVERIFIED** source paths:

| Operation | Preserved source wire / order | Explicit bounds |
|---|---|---|
| REFINE0x543F | BYTE quote/bag/slot/count + WORD NPC + BYTE caller bag/slot + material BYTE pairs;0x5440 DWORD cost/BYTE discount;0x5441 result/bag and successful item descriptor | Secure prompt precedes decoding; full material stacks are spent before chance; conditional territory authority remains missing |
| WASTEREFINE0x5479 | BYTE bag/slot; debit → ADDITEM →0x547A BYTE result | Requires a blank before debit; constructor values and no merge; no invented secure/dead/use-item hooks |
| ITEMUSE912/913 | Source self-Defend host/self IDs and attack level; zero-MP END/stat first, then DEFEND/stat, common debit/result/quest hooks | Only the pinned pure statuses; general effects/combat remain incomplete |
| EVENTINFO | Full original fixed-width body/opaque text bytes; World overrides inner value, live uses outer event ID and replay inner ID in unsigned index order | No client metadata;055 stores server-only admission context; original ACK precedes replay with no end marker |
| Control transport | Original8-byte framing/checksum; role frame then complete running-event replay; ordered whole plain/TLS writes | New256-frame/4MiB limit, exact-connection cleanup and authorization repairs require Phase B acceptance |

World requires an explicit Control IPv4/CIDR allowlist and refuses role
reassignment. Direct Control event/cash pushes now require GMLevel3 or All.
These are documented security changes; IP trust does not establish cryptographic
identity. Zero-material division, dangling caller references and one-past event
indices are rejected. Conflicting raw values in unordered source skill rows
remain an explicit contract blocker. No source-client or packet test was run.
[Exact source references and differences](evidence/phase-a-refine-events-20261011/README.md).

Native Patch metadata sends the original varchar byte sequences, not a UTF-8
re-encoding of decoded catalog text (`TPatchSvr.vcxproj` is MultiByte). Backup
`dbo.TMinBetaVer` sets OUT parameter `@dwMinVer=2`; `CSPMinBetaVer` instead binds a
return value without supplying that argument. The native path implements the
backup OUT contract; its effect on a real launcher is unverified. Missing
`TPreCompleteAdd` blocks native promotion. No executable/assets were supplied,
and no client session, synthetic peer or protocol test was run for these changes.

Control Phase A restores the source event IDs (cash2, spawn12, regen13,
lottery14, gift15), byte strings and source event-value parsing quirks. Native
operator passwords use salted PBKDF2 and exact case instead of plaintext SQL CI
comparison; this is an explicit security change. Event calendar interpretation
requires a source timezone chosen at import and matching process TZ. Unknown
commits stop the Control writer. TUpdatePreVersion, TBetaToVersion and
TDeletePreVersion also lack authoritative SQL definitions. These paths remain
blocked, including original operator-tool acceptance; native schema existence is
not compatibility evidence.

| Contract | Evidence and current scope | Status / remaining proof |
|---|---|---|
| Exact client executable | Client source exists; shared `ProtocolBase.h` reports `TVERSION=0x2918`; source headers and backups can be fingerprinted independently | BLOCKED for a named binary build: owner confirms source only, no additional client files; executable identity, matching data and runnable environment are absent ([gate](client-acceptance.md)) |
| Opcode registry | `tools/modernization_inventory.py` independently evaluates all 1,542 legacy base-plus-offset expressions against `MessageId.h`; nine collisions preserved | VERIFIED for numeric definitions only |
| Packet framing and crypto | Portable codec, crypto and Asio tests plus independent Python RC4/MD5+XOR peer; queued final ACK flushes before close; uint64 modular checksum golden vectors avoid undefined signed overflow | INTEGRATED; independent captures and client-build validation pending |
| Login and session ordering | `TLoginSvrAsio/handlers.cpp`, auth/connection registry, existing handshake/control-gate tests; original `TLoginSvr` and `Client/TClient` are references | VERIFIED synthetic native Login transaction/duplicate/abandoned-login subset plus process failover, sequential pipelining and shutdown drain via login-lifecycle.json; original executable, non-ASCII wire credentials and direct-login 2FA/world ownership remain pending; login-security.json verifies source non-direct 0x2918 confirmation → client LOGIN retry without unsolicited LOGIN_ACK |
| Characters and enter-world | Native Login create/list/delete; source CN codes and CONNECT checksum; pending/loaded/admitted/ready Map phases; keyed World responses; real encrypted Login/World/Map TCP | VERIFIED native lobby, cell routing, handoff, fresh single-owner Map claim/load/core save/reconnect; normal-character primary transfer and recovery verified by native-primary-transfer-contract.json; full ancillary synchronization and original-client proof pending |
| Content → domain → packet widths | Native test compares all 3,495 mapped attribute keys/HP/AP/DP with current C++ narrowing; 14 unchanged chart SELECT contracts use compatibility views | VERIFIED for tested catalog conversion scope; not every gameplay serialization field |
| Historical strings and timestamps | Original CP1252 bytes retained; derived text uses PostgreSQL; SQL timestamps preserved without assumed UTC | ANALYZED; client text encoding and time policies need independent round-trip wire fixtures before mutable migration |
| Original-client session | No real client connected, authenticated, entered a map, persisted state and reconnected in this run | PENDING; no compatibility claim for any exact client build |

All packet definitions and production/test references, including unreviewed
packets, remain in `evidence/source-inventory.json`. `behavior_status=NOT_ANALYZED`
there means a per-packet behavior review has not been completed; it does not mean
the referenced modern handler is absent. Add reviewed evidence as each slice is
completed rather than converting references into coverage percentages.

## Native account secure-code lifecycle (2026-10-11)

The existing Map dispatcher now handles original CREATE, CHANGE, DISABLE, ENABLE,
UNLOCK, CHECK and GETLOCKTIME requests. Migration **042** adds account-owned code,
attempts, deadline and unlock state, plus an append-only receipt without request
credentials. Additive migration **043** removes the character-lifetime FK so
original low-level character deletion preserves the account code and audit.
Each mutation commits with the current primary's fresh/full-graph
checkpoint before replying. Account locks and revision/owner/epoch fences reject
concurrent stale writes; uncertain outcomes close the connection without retry.

CHARINFO now sends the original created/unlocked/disabled bytes. Native carried
inventory/equipment and skill purchases return the source unlock prompt while
locked, then use their existing repositories after unlocking. A blocked MOVEITEM
also sends the original MI_SAMEPOS (4) ACK after the security prompt to reset drag state. Code comparison is
case-sensitive on original bytes. The tenth UNLOCK failure redirects to CHECK
with result 10; CHANGE/DISABLE retain their own result 7 until a subsequent locked
request. Correct CHECK does not unlock, ENABLE does not clear attempts or unlock,
and an already-unlocked UNLOCK counts as a wrong attempt, matching the source.

**Documented modern changes:** create initializes enabled state instead of reading
an uninitialized legacy byte; PostgreSQL preserves the disabled flag omitted from
legacy Map transfer; a persistent database-clock deadline replaces reboot-sensitive
DWORD expiry. Only GETLOCKTIME clears an expired lock, as in the original handler.
The original transfer field widths remain unchanged. Input exceeding the backup's
55-byte column or containing NUL is rejected instead of truncated. The source
client enters 8–12 bytes. Commit-before-response and strict ownership replace
asynchronous legacy saves. Codes remain restricted raw bytes, also present in the
original private transfer graph; security transaction errors redact SQL diagnostics.

The backup contains **zero TSECURECODE rows**. Its column is VARCHAR(55), while
TSaveSecureCode accepts VARCHAR(50); no historical credentials or lock times are
invented or silently imported. Native unlock is scoped to the current non-recycled
Login key and survives same-login Map transfer; a new Login starts locked.

**Verification:** final Debug, ASan/UBSan and installed Release each pass
**1,455** focused native checks (1,148 Map, 29 pool/TLS, 151 outer TCP, 73
learning/title and 54 secure-code wire checks). A broader Debug regression passed
**3,692** checks before the final MOVEITEM second-ACK correction and positive
secured-mutation fixture expansion; the final focused suites verify those deltas.
These suites overlap and must not be added. Debug CTest has 190 actual passes,
eight internal legacy skips and seven explicit fixture skips; all 43 sanitizer
suites pass. All 120 additive upgrade checks and six-service health/DNS/SIGTERM
checks pass. Forty-one prior migrations, both database backups and both client
source backup files retain their hashes. All five owned test labs were removed.

Exact test evidence is recorded in
[evidence/security-contract.json](evidence/security-contract.json). Local reproduction
is in [deploy/README](../../../deploy/README.md#native-account-security).
Full BOW entry/reset behavior, other protected economy operations and administrator
code recovery still need integration. Clock-step policy and real-client UI/timing
acceptance remain unverified. No supported client EXE/hash is claimed: only source
is available. Applied migrations **001–043** and original backups are immutable;
next migration **044**.

The older increments below retain their original test counts and then-open gaps;
the current security scope and schema requirement above supersede those notes.

## Previous increment: native title list, selection and gold-title transitions (2026-10-11)

The existing Map dispatch handles `CS_TITLELIST_REQ` (0x54f4) and
`CS_CHANGETITLE_REQ` (0x54f6). Selection commits owned-title flags and a new
migration **041** receipt with the fresh/full-graph checkpoint, then follows the
original `MW_CHANGECHARBASE_ACK` → World → all Maps → primary neighborhood
`CS_CHANGECHARBASE_ACK` (0x544a) path. The list uses BYTE count and WORD/BYTE
entries; the change ACK retains the source name string, title WORD and final
DWORD. Bow/list tutorial gates and silent same/unowned selection remain intact.

A separate selection head rejects stale snapshots, including later skill
purchases. Concurrent selections have one winner; late SQL failures roll back
flags and receipts. Replicas only refresh their projection. Exact-connection
FIFO echo correlation, bound to the ownership epoch, publishes ordered notifications without applying an old
World reply over a newer committed primary snapshot. No write or notification
is automatically replayed after a lost connection or uncertain commit.

The former selected-gold-title learning gate is removed. Original `GetTitle`
can promote the live ID **before** returning on an already owned eligible title,
leaving the row flags unchanged; it can also mark several gained titles selected.
Both behaviors are retained. Original reload/transfer derives the active title
from the flags; native loads now accept that state and use ascending unsigned
IDs, with the last selected ID winning. The earlier single-selected-row rejection
was incompatible with this source behavior.

**Explicit modern changes:** persistence commits before World/client publication
(the legacy code used separate asynchronous saves); World accepts selection only
from the registered ready primary; malformed bodies are rejected; SQL row order
is made deterministic. This provides a durable selection linearization point,
including pipelined client requests, while preserving existing wire layouts.
Actual-client timing/visual acceptance remains blocked. A lost World connection
uses existing drain/reconnect recovery rather than replaying a selection.

**Verification:** final Debug, ASan/UBSan and installed Release focused runs each
pass **1,243** checks: 1,065 Map, 29 pool/TLS, 76 outer TCP and 73 learning/title
wire checks. A broader Debug native regression passed **3,496** checks before the
final notification-epoch refinement; the final focused/CTest runs cover that
refinement. These overlapping suites must not be added as unique coverage.
Debug CTest has 189 actual passes, eight internal legacy skips and seven explicit
fixture skips; all 42 sanitizer suites pass with no skips. The 97 additive upgrade
checks, three runtime role permission checks, 13 trainer publication checks per
run and six-service health/DNS/SIGTERM smoke pass. Both backups and migrations
001–040 (42 files) are unchanged. The final local runtime image is
`localhost/fourstory:postgresql-titles`, running as `10001:10001`.

The scope is existing native character title ownership, list/selection, learning
side effects and their recovery. Other title awards/resets, complete achievement
conditions and all related gameplay systems remain unfinished. The original
client executable/assets are absent, so **no exact supported binary build or
real-client acceptance is established**. See [bounded evidence](evidence/titles-contract.json)
and [local reproduction](../../../deploy/README.md#native-title-lifecycle).
At this earlier title increment, migrations 001–041 were applied; current requirements are above.

## Previous increment: native skill learning and rank upgrades (2026-10-11)

The existing Map dispatcher now handles `CS_SKILLBUY_REQ` (0x52b2). Its four-byte
NPC/skill request reaches a fenced PostgreSQL transaction and returns the original
30-byte `CS_SKILLBUY_ACK` (0x52b3), including the otherwise undocumented Tick DWORD.
The original NPC offer, country, level, class, parent rank, same-kind spent points,
FLOAT price and currency normalization determine the result. An existing rank-zero
starter entry uses the source upgrade branch. Upgrades deliberately do not repeat
the new-skill trainer-offer or class gates. Existing cooldowns are retained.

Migration 039 publishes all 3,364 `TNPCITEMCHART` rows and 111 `TTITLECHART` rows
from the unchanged backups, independently of the existing character/actor releases.
The publisher verifies values and original text bytes, pins both releases and
blocks changed publication while a Map owner is alive. Migration 040 atomically
journals learning, coins, points, titles, the raw request/response and checkpoint.
A separate purchase head fences stale saves and simultaneous purchases. Transferred
characters update their complete graph; their old normalized skill rows are retained.
Unknown commit outcomes close the session and are never automatically retried.

`UseMoney` also calls `GetTitle(GOLD_TITLE, ..., TRUE)` even during its affordability
check. The implementation preserves its strict threshold, unusual early return
when any eligible title is owned, and ordered TITLEGAIN responses before SKILLBUY.
These changes do not certify the complete title or trainer UI lifecycle.

**Explicit remaining gates:** territory-owned neutral NPCs, nonzero guild/hero
trainer discounts, disguise/general effects, active party/recall dependencies,
active trading, NPC quest
trigger selection and complete skill effects. These branches require further
integration; no replacement success or guessed discount is supplied. Persistent
purchase receipts provide the local audit; original TLog skill events still need
integration. The original missing-level dereference and nonrepresentable FLOAT
price are rejected safely, rather than invoking undefined behavior.

**Verification:** all three configurations (Debug, ASan/UBSan and installed
Release) pass **3,418** checks in the full PostgreSQL/network matrix. Separate
focused runs pass **1,177** checks each, including 1,033 Map checks and the 51-check
SKILLBUY wire oracle. The focused and full suites overlap and are not additive.
The focused suite adds fresh/full-graph relogin and a payable rank-change rollback
followed by success after the known rollback. Thirteen publication checks verify
catalog identity, drift, original text bytes and active-owner exclusion. Debug
CTest has 189 actual passes, eight internal skips and seven explicit skips; all
41 sanitizer suites, 85 schema-upgrade checks and six service health/DNS/SIGTERM
checks pass. The original backups and all 38 prior migrations are unchanged.
See [evidence and limits](evidence/skill-learning-contract.json) and
[local reproduction](../../../deploy/README.md#native-skill-learning).

No actual client
binary or assets exist here: source protocol 0x2918 is identified, but **no exact
supported executable build or complete client acceptance is established**.
That increment applied migrations 039/040; current schema requirements are above.

## Native self-cure with permanent postures (2026-10-11)

Migration 038 extends the existing finish receipt; there are no new wire fields.
Original `TObjBase.cpp:4663–4697` and backup 131/132 flags preserve those postures
during ACTION. `TObjBase::Defend` calls `CheckEternalBuff` after effects and before
its response. A zero-MP self-cure emits END, private STAT, DEFEND, then changed
HPMP; the DEFEND instance powers were calculated before posture removal.
`TPlayer::CheckEquipSkill` runs at the end of FINISHSKILL even with no targets;
`IsEquipSkillItem` includes broken equipped weapons. Dependent-posture effects
remain outside the supported two-posture domain.

`EraseMaintainSkill` uses the 3×3 neighboring cells; instantaneous Defend uses
`GetNeerPlayer` radius CELL_SIZE=64. The owner alone receives CHARSTATINFO.
The controlled seven-socket test checks these recipients and exact radius/cell
boundaries; the separate native TCP oracle checks original bytes, equipment,
charge, healing, empty targets, late rollback and crash recovery. No original
client binary was run. General AOI/replica synchronization, cure aggro/party relay,
other effects/targets and full gameplay remain pending. See
[verification and explicit limits](evidence/finish-effects-contract.json).

## Native ACTION and charged First Aid (2026-10-11)

`CSSenderAll.cpp:319–343` sends the original 19-byte ACTION request to every
connection; `CSProtocol.h:793–812` defines opcodes 0x52ae/0x52af. The 17-byte ACK
is result BYTE, object DWORD, type/action BYTE, act/animation DWORD and skill WORD.
Native own-PC processing preserves the source gate order and return codes from
`CSHandler.cpp:1248–1436`. ACTION does not debit resources or arm reuse. The
accepted cast remains the cost boundary; FINISHSKILL remains the effect boundary.

Migration 037 binds one successful same-login/same-route ACTION to at most one
ordinary accepted cast. First Aid 240 then requires its source 300ms minimum.
Rejected actions preserve a pending charge, and primary handoff retains it. A
new login cannot inherit it. This provenance requirement and zero-skill ACTION
cancellation are deliberate stricter repairs; the original conditional last-skill
check allowed missing ACTION to bypass charge validation. A bound cast retains
its origin after later actions. No new field or guessed expiry is introduced.
Shared PostgreSQL time replaces process-local uptime for cross-owner timing;
forward clock jumps, movement/damage interruption and complete charge-speed
semantics still require verification. Successful ACTION with general active
effects beyond 131/132 or recall/trap side effects remains explicitly unsupported.

Source audit also corrected the prior native percentage-cost base. ACTION,
SKILLUSE and LOOPSKILL now use `GetPureMaxHP/MP` semantics: race/class/level and
FLOAT growth, excluding aftermath, equipment and all effects
(`TObjBase.cpp:1450–1468,1816–1879,1991–2009`). The original server uses those
pure values at `CSHandler.cpp:1302,2726,3218`; using visible maxima was a rewrite
bug. The TCP oracle derives skill34's five-percent MP cost independently from
backup formulas and creates different pure/visible maxima with aftermath100.
No imported source values are changed.

The self-cure selector accepts source targets SELF/ALLIANCE, covering First Aid240
and Cure509. Aid629 targets MAINRECALL, so its cure rows do not authorize own-PC
healing. Companion ownership, general effects and actual client acceptance remain
pending. See [source and executed evidence](evidence/skill-actions-contract.json).

## Previous primary FINISHSKILL and bounded self-cure (2026-10-11)

Original `TClientGame.cpp:14050–14108,28759–28830` routes ordinary PC skills through
`FinishSkill`; `CSSender.cpp:2980–3019` sends 0x55f7 on the primary session with
host DWORD, attacker DWORD, type BYTE, ground FLOAT×3, skill WORD, linked BOOL,
fake BOOL, party WORD, target count BYTE and `(DWORD,BYTE)` targets. Windows BOOL
is **four bytes**, giving a 34-byte fixed header. The client removes missing or
ineligible targets without reordering survivors. Linked/exception skills and
non-PC actors use additional branches; this does not replace their contracts.

Original `CSHandler.cpp:20197–20690` recomputes present-attacker powers, iterates
targets and invokes `Defend` with zero animation/action IDs. The bounded native
509 own-PC instant Cure follows that path, including source rank, magic AL/CP,
positive guaranteed-hit calculation, per-row `rand()%16` cure variance and
clamping after effect application. Original `TObjBase.cpp:3555` and `1160` specify
the arithmetic and DEFEND-before-HPMP ordering. The 84-byte DEFEND response and
21-byte HPMP are unchanged. Empty target lists produce no new success opcode.
Local audience uses the original `GetNeerPlayer` 64-unit radius; complete remote
visibility and party HPMP relay are still unverified/unimplemented.

Modern repairs: exact lengths/finite ground/own-PC authority are mandatory;
unsupported effects and native DEFEND's old untrusted-power prototype fail closed.
Migration 036 makes accepted-cast consumption, healing, exact response receipt
and checkpoint atomic. No automatic retry occurs after uncertain commit. A
server-only finish head fences old snapshots, including no-delta finishes. Its
hydration is separate from the unchanged original SS graph.

FIFO matching requires same login key, route, skill, ground and an ordered target
subsequence. The entire accepted cast finishes once; filtered-out occurrences
cannot be claimed later. No expiry is invented. Same-body later casts remain
distinct. This bounds effects but cannot distinguish every replay from a later
identical outstanding finish without additional source/client evidence. Multiple
actors, damage, repeats, linked/charged skills, monsters, buffs and general Defend
remain in scope and are not certified by this slice. Only backup 509 matches the
current learned instant-cure selector. Original binary/assets remain unavailable.

## Behavior change review: missing monster attributes

Original essential spawn setup in `Server/TMapSvr/TMap.cpp:629` looks up
`FindMonAttr(MAKELONG(wMonAttr, level))`, deletes the candidate and returns when
attributes are absent. `Server/TMapSvr/TAICmdRegen.cpp:76` also refuses missing
attributes. The backup has 39 absent combinations; none were repaired.

The portable spawn manager formerly fabricated HP from level for these cases.
It now skips the invalid candidate and logs the count, with no changes to opcode,
field order, widths or error packets. It also rejects zero-HP live candidates;
this is an explicit validation policy, not proven original zero-HP behavior.
The pinned catalog yielded zero such candidates.

Remaining differences are explicit: the portable manager distributes candidates
round-robin, treats zero `bCount` as one, allocates sequential runtime IDs and
continues after an invalid candidate. Original code separates ordinary/essential
spawns, uses probability/leader rules and packs spawn/channel/index into IDs; an
essential attribute failure exits the whole spawn setup. Those rules and regen
events need a dedicated behavior slice before claiming full monster parity.
The current verified result establishes authentic combat attributes, not exact
original monster population or client-visible spawn order.

Acceptance for the next gameplay change: derive ordinary/essential selection and
ID contracts from original initialization + regen + client consumers, add
independent expected packet bytes/order, test missing/zero/weighted/leader cases,
then run real PG and the supported client when available. Do not invent missing
stats to get through initialization.

## Login result-code correction

`Lib/Own/TProtocol/include/NetCode.h:267` and restored `TLogin` agree on internal
error 5 and agreement 8; security is 10 in the original enum. The previous modern
auth enum incorrectly used 8/9/5 respectively. These values were corrected;
internal account-ban classification maps to backed-up return 7, and `TCheckIP`
restriction maps to 6. The TCP fixture independently expects original numeric
codes and a 41-byte ACK. This is a correction toward original source semantics,
not proof that a named client build accepts the entire server.

Local duplicate handling now closes both Login connections after flushing reply 3.
Map-owned remote duplicate termination is not implemented by this change. New
session cleanup is account-and-key specific; delayed old cleanup is intentionally
safer than the source procedure's account-only deletion. Protocol failure after
DB commit is tested using a delayed synthetic audit and TCP disconnect.

Login now awaits each packet handler before decoding the next packet on the same
connection. The encrypted fixture sends `CS_GROUPLIST_REQ` before receiving the
login reply and independently expects `CS_LOGIN_ACK` then `CS_GROUPLIST_ACK`.
Shutdown closes actual sockets and awaits their cleanup, preserving `MapHandoff`
for completed `CS_START_REQ`; the synthetic socket test also covers an unlimited
connection cap. PostgreSQL process ownership is operational metadata, with no new
client packet, field, opcode or credential encoding.

An additional ordering conflict remains recorded in `auth-source-contract.json`:
the original C++ wrapper calls `CSPCheckIP` before credential validation and can
return 6 immediately, while the backed-up `TLogin` procedure checks `bIPCheck`
after credentials. The current native service follows the backed-up procedure
order. Its synthetic test proves that chosen behavior, not complete parity with
the original wrapper on a restricted IP with invalid credentials.

## Native character lobby increment (2026-10-09)

Original NetCode.h defines DELCHAR results success=0, invalid password=1,
no group=2, internal=3, guild=4. The rewrite enum and handler now preserve these
values. CREATECHAR's final count is the backed-up global live-character count;
the level and all 13 trailing BYTE fields preserve the original layout. The
source-derived encrypted peer checks real native create/list/delete and process
restart. Equipment remains 12 bytes, including the explicit modern empty custom
texture extension. The source client executable was not run. Native supported
names are US ASCII; full encoding/collation acceptance remains pending. The later
Map section covers native fresh lifecycle. See `evidence/characters*.json` and the
character implementation contract.

## Map admission boundary (2026-10-09)

`domain/connect.h` uses CN_SUCCESS=0, CN_NOCHANNEL=1, CN_NOCHAR=2,
CN_ALREADYEXIST=3, CN_INVALIDVER=4, CN_INTERNAL=5 from the original NetCode.h.
The CONNECT accumulator starts with wrapped DWORD products/sum before 64-bit
mixing. Fixed vectors cover every mixing count and overflow. Wrong checksum and
malformed bodies close silently; wrong version sends code 4 and closes after flush.
A terminal queued response suppresses further packet dispatch on that connection.

One client ACK follows the matching World verdict, after identity load. Duplicate
verdicts and old-key close/enter messages cannot affect a newer reservation.
A deliberate modern policy rejects a competing socket with code 3 while the prior
reservation is live; the original code instead closed/suspended competing sockets.
The earlier controlled-service admission tests do not certify complete client
entry. The native increment below adds actual PostgreSQL ownership, inventory and
skill lists, cell checks and core save. A failed ready save still blocks reload;
durable crash recovery and full ancillary client synchronization remain unfinished.

## Native Login START boundary

The production handler requires exactly six body bytes (group/channel/DWORD char).
It carries the authenticated session key into PostgreSQL rather than trusting the
wire character ID alone. Original result bytes 0/1/2/3 and the 8-byte ACK layout
are preserved: result, four IPv4 octets, little-endian port, server byte. Successful
ACK is terminal: flush, then close the Login socket, suppressing buffered later
requests. These behaviors pass an independently encoded encrypted TCP test.

Logical/physical channels and map-unit ownership come from the pinned routing
release; the original spawn fallback updates X/Z and sets Y to zero. Cell indices
use truncation of coordinate/1024 rather than PostgreSQL float-to-integer rounding.
No missing cell is synthesized. Earlier selected-character stamping, all-or-nothing
rollback, repeated-request idempotence and 60-second unclaimed reservation expiry
are explicit modern repairs. The database and handler support normal application
worlds; no claim is made for BR/BOW or unimplemented TFindServerID side effects.
See `evidence/map-handoff-contract.json` for exact source and remaining Map work.

## Native Map fresh-entry verification (2026-10-09)

The actual Login, World and native PostgreSQL Map processes now complete the
0x2918 fresh single-owner admission sequence with source-derived encrypted TCP.
CHARINFO includes real bags/items (including magic), skills and all twelve keys
per hotkey inventory. Maximum HP/MP are separate from current values; zero current
HP means dead. Aid country defaults to the source neutral sentinel 3. World
MW_CHARINFO_REQ now emits its original four-byte BOOL trailer. The native Map
parses the complete ENTERCHAR composite before acknowledging it, checks routing
ownership and notifies World on disconnect, allowing reconnect on the same World.

This is protocol conformance for the exercised flow, not original-client execution
or full world/gameplay parity. Mail/cabinet/pet client synchronization, maintained
effects, neighboring Map transfers and persisted social graphs are unfinished.
See `evidence/native-map-runtime*.json` and `map-runtime-contract.json`.


## Checkpoint/reconnect boundary (2026-10-09)

Client opcodes, encrypted framing and packet field layouts are unchanged. The
actual peer can reconnect through the same World after Map SIGKILL, reading the
last committed core and unchanged inventory. This uses an independently encoded
Python peer, not the original executable. The native checkpoint scope remains
fresh single-owner characters; secondary connections and social hydration are
unfinished. Map's inter-server ID now follows original
`MAKEWORD(server_id, SVRGRP_MAPSVR=4)` (`TMapSvr/SSSender.cpp:SendMW_CONNECT_ACK` and
`TProtocol/include/CTProtocol.h`). The existing modern peer registration envelope
remains `RW_RELAYSVR_REQ`; this is not proof of interoperability with an original
World binary. Unexpected Map TCP loss executes character cleanup while native
PostgreSQL ownership and checkpoint recovery retain responsibility for the account.

## World-link lifecycle boundary (2026-10-09)

Native admission requires the existing modern `RW_RELAYSVR_ACK` envelope: BYTE
nation, WORD operator count plus DWORD IDs, WORD message count plus pairs of
length-prefixed strings. The complete body must parse with no trailing bytes.
The ACK changes local readiness only; no original client packet is added or changed.
A missing ACK expires after five seconds. World loss closes client sockets and
awaits durable save/release before replacement registration; reconnect requires
fresh Login/START as in the supported workflow. Failed persistence keeps the prior
account reserved. The source Map instead queued `SM_QUITSERVICE_REQ` on World loss
(`TMapSvr.cpp::OnCloseSession`) and saved/logged out characters on exit. Surviving
Map-process reconnect is an explicit modern recovery policy, not legacy restart
parity or an assertion that an original World binary speaks this registration.

`test_world_reconnect.cpp` uses a synthetic TCP peer for missing/malformed ACKs,
teardown ordering and simultaneous Map→World frame writes. Real PostgreSQL and
Login/World/Map process evidence is tracked separately in `native-world-link*.json`.
Neither provides original-client execution evidence. Silent TCP blackholes still
need a heartbeat/timeout contract; the five-second deadline applies to registration.

## World writer and expected secondary connections (2026-10-09)

`WorldSession::SendPacket` now owns its frame and serializes complete writes.
The 8-byte SS header, checksum and opcode/body bytes stay unchanged. A stalled
peer exceeding 256 outstanding frames or 4 MiB (headers included) is disconnected;
queued writes are not replayed. Read EOF, malformed frames and write failure close
that writer and wake waiting senders. This is a modern resource/lifetime policy.

Original `Server/TWorldSvr/SSHandler.cpp::OnMW_ADDCHAR_ACK` accepts an existing
character only through a preplanned connection with the matching server/IP/port,
key and live main. It marks that connection valid/not-ready, then requests CHARDATA
from the main only when all expected connections are valid. Modern World now
follows this success path through the real dispatch loop. It additionally checks
the account and exact registered Map peer. On bad secondary input it sends the
original INVALIDCHAR body with release-main=0 while preserving the valid main;
the original called CloseChar too. This deliberate failure-policy change prevents
a stale or foreign connection from evicting the active character.

Tests cover absent routes, wrong key/account/address/port, partial completion,
duplicates and final CHARDATA, with independent SS bytes also exercised against
the actual daemon. Empty rank/tournament registration replays are checked exactly,
not discarded as arbitrary noise. See `evidence/world-writes-contract.json`.
These checks do not prove original-client execution or original World binary
interoperability. The later native-secondary increment below implements fresh
replica admission and summary synchronization; primary handoff remains pending.

## Map retirement and unaccepted connections (2026-10-09)

Legacy `Server/TMapSvr/SSHandler.cpp:1212` DELCHAR reads DWORD char/key and BYTE
logout/save, then closes the exact player without a client reply. At line 1249,
INVALIDCHAR reads DWORD char/key and BYTE release-main, sends empty
CS_INVALIDCHAR_ACK, and closes. CLOSECHAR at line 2196 sends empty CS_SHUTDOWN_ACK
with close-all disabled. Modern Map now handles all three via its actual World
socket, rejecting truncated, trailing and non-boolean flag bodies and stale keys.

A separate local World-presence state records unannounced, announced or retired.
A failed native database claim sends neither ADDCHAR nor CLOSECHAR. World-driven
retirement suppresses CLOSECHAR echo, including instructions received while an
existing final save is pending. Local phase is preserved so a ready native primary
still saves/releases atomically; a wire save=0 cannot discard its durable ownership.
This is an explicit modern persistence policy. The later native-secondary
increment below supplies a separate role-specific replica release path.

World CLOSECHAR additionally requires the exact registered typed Map peer and an
already valid character connection. A stale/unaccepted sender receives DELCHAR
without evicting the valid World character/account. This guard goes beyond the
legacy key-only lookup; it closes a verified path where a rejected second native
Map could otherwise evict the primary. Client packet layouts are unchanged.
See `evidence/map-retirement-contract.json`. Original-client execution and primary
transfer completion remain unverified.

## Native fresh secondary admission and client hydration order (2026-10-09)

Original `Client/TClient/CSHandler.cpp:176` opens each ADDCONNECT endpoint.
At line 93, CONNECT success selects the main session, sends empty CONREADY to
every listed Map ID and activates the game frame with `OnRegionChanged`. Original
`Server/TMapSvr/SSHandler.cpp:2019` CHARINFO processing sends CHGCHANNEL followed
by full client CHARINFO before that final CONNECT. The rewrite had deferred
CHARINFO until CONREADY; an independent native TCP regression now rejects that
order. Native Map sends hydration at World CHARINFO, ignores a repeated initial
metadata message and does not send a duplicate full character reset on CONREADY.
Original auxiliary quest/stat/pet/post/social packets remain separate unfinished
contracts; fixing the full-character order does not certify the complete client.

The fresh neighbor path now runs through actual primary ROUTE, World ADDCONNECT,
secondary CONNECT, primary CHARDATA and World ENTERCHAR. Migration 019 binds a
60-second one-use grant to the primary token/generation, target token and exact
published IPv4/port, plus all three source catalog releases. Unconsumed repeated
grant requests retain the original deadline. An explicit new route decision may
replace an expired/unconsumed or retired grant. Successful consumption records
the secondary generation separately from the unique mutable primary.

Original `Server/TMapSvr/SSHandler.cpp:1447` supplies a partial secondary summary
and recalls, rather than loading inventory from durable tables. The supported
native no-recall path now validates that summary against the granted location,
records a loaded replica and confirms ENTERCHAR. World sends CONRESULT only to
the main; the replica accepts the client's CONREADY without a separate verdict.
It sends no invented CONNECT/CHARINFO. Original `TCell.cpp:56` exposes primary
actors to observing replicas; `CSHandler.cpp:599` broadcasts movement only from
the primary. Native readiness/visibility/movement now preserve that distinction.
Other secondary gameplay dispatch remains unported and cannot mutate a fabricated
full character graph.

Ready replicas never run primary load/checkpoint/save or release current-user
ownership. Exact-generation retirement affects only the replica row; primary
close/recovery cascades all replica grants. A target restart cleans only its
old-token replicas. Actual accepted secondary disconnect still requests the source
close-all through World and lets the primary finish its save. These process and
connection fences are modern persistence policies, with unchanged client fields.

The pinned backup topology separates the three normal channels across Map IDs.
Two-Map tests explicitly split one cell via a temporary compatibility view in an
owned disposable PostgreSQL database and restore its exact definition in cleanup.
Imported historical rows and original backups remain unchanged. This is a
synthetic deployment test, not a claim that this split was present in the backup.
Full distributed actor ownership/AOI, movement-driven connection-set changes,
primary role transfer and original-client execution remain pending. See
`evidence/map-replica-contract.json` for scope and actual-process evidence.


## World main-handoff coordination (2026-10-09)

The original `Server/TWorldSvr/SSHandler.cpp:1095` CHECKMAIN handler chooses the
responding owner, requests RELEASEMAIN from the old main and points the World
character at the candidate. `:2284` forwards the source RELEASEMAIN body verbatim
as ENTERSVR; the ENTERSVR acknowledgment then requests the Map server list before
final CHECKMAIN/CONRESULT. The Map source at `TMapSvr/SSSender.cpp:1999` contains
the full transient transfer graph. World must not rebuild it from stale SQL rows.

A regression against the previous actual Release daemon showed that a registered
but unaccepted Map could send unsolicited CHECKMAIN, reassign the primary and
trigger RELEASEMAIN. The coordinator now requires a current typed Map peer, an
accepted ready character connection and an outstanding CHECKMAIN request to that
exact socket. One response consumes the round. A transfer pins both source and
target sockets before sending RELEASEMAIN, advances through release/load/final
confirmation, and rejects competing, repeated, stale or out-of-phase replies.
Only the source may forward the opaque state and only the target may finish load
and confirmation. The source state bytes remain unchanged on the wire.

Fresh ENTERSVR is also consumed once from the actual registered main. Exact body
length, Boolean fields and finite coordinates are checked before metadata changes.
ROUTE/CHARDATA remain primary decisions; ENTERCHAR cannot make an unaccepted
connection ready or repeatedly re-arm CHECKMAIN. Connection-list reconciliation
cannot disturb release/load stages. During final confirmation it accepts only
the target's list and may still add newly required secondary connections.

A modern five-second total deadline covers release, target load and final
confirmation. Expiry or an explicit target failure closes the World character
through the existing per-Map retirement path; this is coordination cleanup, not a
claim that a native database transfer committed. Losing either pinned peer also
closes the pending exchange, including when the source has already moved out of
the live connection list. Close is idempotent and cancels the timer. Closure and
dead-connection notifications use the original Map server type byte 4.

No client opcode, field or server-to-server header was added. The original
CHECKMAIN ACK has only character/key, so it cannot distinguish an arbitrarily
late response from an earlier round on the same still-current socket if another
round has since asked that socket again. This is an explicit source-wire limit;
the new checks establish expected phase and connection identity, not a new wire
epoch. Future native PostgreSQL transfer must independently fence its transaction
with durable owner/connection/transfer identity.

See `evidence/world-handoff-contract.json` for before/after evidence. The independent
World peer fixture uses a labelled opaque transport payload; it does not certify
a complete Map graph encoder/decoder. Native Map primary transfer, dynamic border
routing, full gameplay and actual original-client execution remain unfinished.

The same verification exposed a transport defect in `tnetlib::AsioSession`:
concurrent `Close` calls touched the same socket while composed reads were still
running, producing a UBSan null descriptor access. Packet producers also used a
channel without thread-safe shared access. All internal TCP read/write/channel
operations now execute on one per-session strand, including composed-operation
continuations. Public `Close` publishes a closed flag immediately and schedules
socket cancellation on that strand; native Login/Map liveness checks use that
atomic flag. Setup/raw socket access still requires external synchronization,
one read loop is allowed, and raw writes must be awaited serially.

A backpressure regression additionally showed that `channel.close()` alone left
pending producers waiting. Shutdown now cancels waiters before closing the
channel. Tests run packet producers on two executor threads and race four
external closers against partial reads, writes and a capacity-one queue for 24
rounds; every producer and reader must complete before executor shutdown. This
changes internal concurrency and cleanup, not packet bytes. See the retained
sanitizer/backpressure failures and passing transport reports in the handoff
contract. The strand requirement follows the
[Boost.Asio composed-operation contract](https://www.boost.org/doc/libs/1_83_0/doc/html/boost_asio/overview/core/strands.html)
and [channel thread-safety contract](https://www.boost.org/doc/libs/master/doc/html/boost_asio/reference/experimental__basic_channel.html).


## Complete source Map transfer representation and cooldown lifecycle (2026-10-09)

`TMapSvr/SSSender.cpp:1999–2638`, `TItem.cpp:468` and the non-login
`SSHandler.cpp:4424` branch define the complete RELEASEMAIN/ENTERSVR body.
`TMapSvrAsio/domain/main_transfer.h` and `services/main_transfer_codec.cpp`
now represent and encode/decode its full ordered graph: core and security/aid/mail
metadata; inventory and cabinet descriptors; raw server items; skills, effects,
quests, hotkeys, cooldowns, pets, recalls, protected characters, PvP/duel records,
auctions, titles, companions, interleaved companion items and the final counters.
Strings preserve source bytes; integers are explicit little endian, including
signed 64-bit time sentinels and unsigned high bits. The saddle's misleadingly
named `m_wItemID` is actually a DWORD. Server item magic is raw and extended item
attributes are DWORDs; the client projection is insufficient to reconstruct them.
Native PostgreSQL load now retains both representations without changing client
packet widths or historical database rows.

The independent Python fixture contains 861 bytes and 319 named fields, with every
variable section populated. It does not import the C++ layout. All bytes must
round-trip, and all truncated prefixes must fail. Parsing is bounded by the source
WORD frame length (65527-byte body), validates structural selectors/finite core
positions and exact EOF, and allocates entries only after decoding them. This is
wire verification, not proof that every represented subsystem has working gameplay.

Original skill load installs the transmitted remaining duration as the local
reuse delay (`SSHandler.cpp:4880–4915`, `TSkill.cpp:73–117`). Fresh native admission
now restores that gate, including when the optional gameplay catalog is absent.
A changed template cannot shorten an outstanding imported cooldown. Duration/origin
storage avoids deadline overflow and permits tick zero; a backward clock cannot
bypass a live gate. Snapshot/export is deterministic and does not rearm timers.
Successful teardown forgets only that character's timers. Full rank/attack-speed
reuse rules, effect timers and persistence of newly used cooldowns remain pending;
current checkpoints save only core fields. The initial CHARINFO still carries the
loaded duration; refreshing it at the exact send instant is a follow-up.

The original client has not been executed. The new codec does not enable native
primary transfer: embedded ENTERSVR remains refused until live graph restoration,
transactional ownership fencing and fault recovery are implemented. Synthetic TCP
checks prove restored cooldown rejection in actual native Map daemons. See
`evidence/main-transfer-state-contract.json` and `next-steps.md`.


## Native PostgreSQL primary handoff (migration 020)

The preceding codec/coordinator prerequisites are now connected to the actual
Map runtime. Original movement-driven CHECKCONNECT/CHECKMAIN/RELEASEMAIN/ENTERSVR
packets carry the handoff. A ready replica answers cell ownership, then consumes
an exact prepared body and changes database ownership before publishing primary
state. A monotonically increasing SQL authority epoch prevents stale same-socket
writes across a round trip; no epoch is added to the original wire format.

The source freezes mutation and exports current core/raw inventory/skill timers.
Gameplay operations drain for at most three seconds and client close interrupts
that drain. The target reconstructs supported state from the body and pinned
catalogs, restores timers and retains every typed extra section. World retains
its existing expected-peer/phase checks and total five-second coordination deadline.
CONNECT makes the original client choose the new main; empty CONREADY activates
it and demotes the retained source. No duplicate CHARINFO is sent during handoff.

Version-2 checkpoints preserve the transferred graph with a database-checked
SHA-256 and catalog/core identities. Source cancellation/replacement saves the
prepared graph; target replacement recovers committed loaded/ready checkpoints.
Fresh relogin uses the saved graph rather than stale child rows. Same-revision
confirmation checks graph and core. A client close during delayed target COMMIT
waits for local ownership publication before teardown; World sends original
INVALIDCHAR to the old source before closing it in that interrupted-handoff case.

Actual Debug/sanitizer/installed tests exercise two native Maps with an explicitly
synthetic routing partition, source-sized encrypted client packets and unchanged
historical rows. The prior installed daemon fails the new movement handoff.
See `evidence/native-primary-transfer-contract.json`. Original-client execution,
full AOI/effects/quests/summons/secondary gameplay and complete native social
persistence remain pending. Typed preservation does not certify active gameplay;
fresh non-transfer characters now use the separate contract-3 skill checkpoints below.


## Fresh-primary skill durability (migration 021)

The original sender captures `GetReuseRemainTick(m_dwSaveTick)` as a DWORD;
`TSaveSkill` from the authoritative backup inserts that value with the learned
rank into `TTEMPSKILLTABLE`. The backup `TLogout` replaces `TSKILLTABLE` from
that staging table. The corresponding skill copy is commented out in the backup
`TSaveCharDataEnd`; modern periodic durability is an explicit improvement, not
a claim that the old periodic procedure persisted skills. Fresh source load
starts remaining durations on the local clock, with no offline decay here.

Fresh native periodic/final snapshots now sample the live cooldown tracker even
without a transfer graph. The PostgreSQL transaction changes only existing skill
remaining values, preserving the full learned set and ranks. Contract-3 receipts
contain an unsigned, ID-sorted skill array alongside the core. Changed same-revision
timers fail; exact repeats confirm the commit. Core/skill drift blocks save,
transfer and owner recovery. A normal fresh relogin reads the updated database
rows, so mail/account loading is not replaced with transfer-wire counters.

No packet field or opcode changes. Native encrypted TCP checks cover countdown
while online, logout/relogin, SPEEDYUSE enforcement and SIGKILL recovery. Backend
fault tests cover rollback after receipt/audit failure, expiry to zero, new use,
full DWORD durations, malformed skill sets and restricted SQL grants. Full graph
contract 2 remains unchanged in meaning. Skill learning, full effect/quest timers
and original-client execution remain pending. See [contract and evidence](evidence/skill-checkpoints-contract.json).

Native CHARINFO now samples `GetReuseRemainTick`-equivalent values from the live
tracker immediately before encoding, after the channel notification. CONREADY
samples again for revision-zero durability; neither sample updates loaded values
or resets timer origins. Expired values become zero. Missing trackers and active
unknown skills refuse admission. This preserves the source BYTE skill count and
WORD/BYTE/DWORD skill rows without adding an opcode or changing packet order.
See [admission timer evidence](evidence/admission-timers-contract.json).


## Native learned-skill resource gates (2026-10-09)

`CS_SKILLUSE_REQ` keeps its original header and defender list. Native casts must
match the admitted PC, channel and current map and reference a learned skill.
Missing ownership returns the original `SKILL_NOTFOUND` fixed ACK. Truncated
lists, trailing bytes and non-finite positions are dropped before mutation.
Static definitions come from pinned `character_compat.TSKILLCHART` and formula 34,
rebuilt during both fresh and transferred character hydration. Type-1 costs use
the original FLOAT-rounded exponent at the learned rank; type-2 percentages retain
DWORD wrap before division. Invalid/out-of-range arithmetic closes the session
through the existing dispatch exception path.

Under the character-state lock, MP is checked before HP, HP must exceed its cost,
and the live cooldown gate precedes exact deduction. These resource rejections
leave resources and timers unchanged; the later ordinary weapon gate below has
a source-defined timer side effect. Success echoes the actual learned rank in the existing ACK
and broadcasts the original HPMP layout when charged. Existing checkpoint/final
save paths persist those core values; learned ranks remain unchanged.

This increment established resource/ownership acceptance. Later sections describe
native new-use timers and previous-action gates. Full target validation, effects,
damage, learning and the original client executable remain pending. Source trace and
executed evidence: [skill resource contract](evidence/skill-costs-contract.json).


## Native normal-cast cooldown generation (2026-10-09)

The later timing increment replaces the preceding native zero-delay placeholder
for normal casts. `CTSkill::GetReuseDelay(SDELAY_SKILL)` contributes base delay plus
`(learned rank - 1) * signed rank increment`, adds the character's attack delay,
and multiplies by its rate before `/100`. DWORD wrapping is retained; arithmetic
outside supported source INT/FLOAT conversions is refused. Source formulae 4/16
use base class/race DEX/WIS, without current-level scaling. Equipped primary and
secondary slots supply physical **and magic** delay, the ranged slot supplies
ranged delay, and broken items supply neither delay nor magic. Passive rate deltas
apply before item speed reductions, which cap at 100 percent.

A successful cast arms its own timer and extends all learned same-kind timers
under one cooldown lock while still holding the character-state lock. Kind delay
is used directly, including kind zero, and cannot shorten a longer running timer.
Resource/reuse rejection changes neither peer timers nor HP/MP; the later ordinary
weapon gate retains the source timer side effect described below. Existing snapshots, database
receipts and transfer restoration now carry these generated timers as well.

Pinned definitions and timing are rebuilt at native fresh/transfer hydration;
they add no wire fields or persistent columns. An unported buff-bearing transfer
retains its full graph but leaves speed-dependent timing unavailable. Such casts
fail before mutation, rather than calculate guessed buff/disarm/disguise effects.
No-speed casts retain the original zero attack delay and 100 percent rate. Active
effect expiry, cancellation, full target validation and original-client
execution remain pending. See [timing contract](evidence/skill-timing-contract.json).


## Native LOOPSKILL branch

`CS_LOOPSKILL_REQ` (0x5372) retains its 23-byte header/count and six-byte target
rows. The response 0x5373 retains the 45-byte body followed by five-byte targets;
normal skill animation, transform and back-skill fields are absent. The same ACK
layout serves rejection and success. Native identity/channel/map and learned rank
are checked before any mutation. Cooldown precedes MP then HP (`HP < cost`),
followed by the active-effect prerequisite and non-consuming weapon requirements.
Only then does the character lock enclose timer arming and exact resource costs.
Loop duration uses `(dwLoopDelay + attackSpeed) * rate / 100`, preserving DWORD
wrap and omitting normal rank increment and same-kind extension. Ordinary and
loop packets share timer state. Migration 022 below adds single-reagent support.
Ammunition/cash bypasses and active buff-dependent branches remain unsupported. Full target/action/peace-zone checks,
combat powers/damage, multi-hit expansion and actual-client acceptance remain
pending. See [source contract and evidence](evidence/skill-loop-contract.json).


## Native ordinary cast prerequisites

`CS_SKILLUSE_REQ` retains the existing packet layout and ready-primary/owned-PC
fences. Native hydration reads source `wMapID` and `wPrevActiveID`, including
signed database `-1` as the original unsigned WORD sentinel `0xffff`.
The supported checks run in source order: map restriction (WRONGREGION=17),
MP (7), HP (8), maintained previous effect (10), reuse (6), and weapon (9).
The normal prerequisite uses `wPrevActiveID`; loops use `wTargetActiveID` and do
not inherit the ordinary map-template restriction. A learned prerequisite is
insufficient without a maintained effect.

Original normal `SkillUse` precedes `UseSkillItem`: supported unsuitable weapon
rejection therefore retains own and same-kind timers without resource deduction.
Loop weapon rejection leaves timers unchanged. At that increment graph-backed reagents,
ammunition/cash exceptions and buff-backed prerequisites closed before any timer/resource
mutation; their unfinished side effects are not acknowledged as success.
The 62-byte ordinary reject and 45-byte loop reject formats remain unchanged.
See [source contract and verification](evidence/skill-gates-contract.json).


## Native single-reagent consumption

The later reagent increment implements source `wItemID` requirements with no
weapon mask for fresh owned PCs. Select the first matching item in unsigned
bag/slot order, including default bag255. Ordinary failure keeps already-armed
own/shared-kind timers; loop failure arms nothing. A depleted stack returns the
original UNSUITWEAPON result and charges no resources.

After a confirmed PostgreSQL commit, send private `CS_UPDATEITEM_ACK` (0x52AA:
bag BYTE plus the original item descriptor) or `CS_DELITEM_ACK` (0x52AC: bag and
slot BYTEs), followed by `CS_MOVEITEM_ACK` (0x52A9: zero result BYTE), then the
existing cast ACK and HPMP if charged. No item or success ACK precedes commit.
Unknown transaction outcomes close the session and retain the reservation for
recovery; stale final saves are refused. Checkpoints and immediate writes share
an operation fence. See [source contract](evidence/skill-reagents-contract.json).

Full transferred graphs were outside the initial migration-022 scope; migration
023 below extends it. Equipped reagents, ammunition/cash branches and active
trade/store/effect lifecycles remain pending. This is source-derived
wire verification; original-client execution and complete combat are pending.


## Reagents after primary transfer

Migration 023 extends the same original inventory packet sequence to contract-2
characters on the current primary epoch, including relogin at epoch zero. The
server hashes the original encoded item record, validates the full persisted
graph, and permits exactly one count decrement/deletion plus current character
core and sampled learned cooldowns. Learned IDs/ranks and other graph collections
must remain unchanged. Raw item magic, DWORD extensions and unsigned 64-bit IDs
retain their original representation; no packet fields were added.

The exact before/after graph hashes and item receipt commit with the new graph,
core and timers. Stale normalized item/skill rows are neither consumed nor reloaded
by this branch. Encrypted two-Map tests verify ordinary consumption on the promoted
primary, loop consumption after returning, count/deletion across relogin, exhausted
rejection and private ACK isolation from the former primary. A delayed database
trigger proves that no item/success response is emitted before commit.
See [source contract and acceptance boundaries](evidence/graph-reagents-contract.json).


## Arrow and bolt consumption across targets and stacks

Migrations 024–025 retain the original request DWORD/BYTE/BYTE target tuples.
Only flagged entries, capped at 16, participate in the supported non-expanded
cast. Success uses DWORD/BYTE tuples: 62 + 5×targets bytes for ordinary use and
45 + 5×targets for loop. Reject bodies remain 62/45 bytes; no new wire fields exist.

A powered compatible weapon with base use count one charges one unit per flagged
target. Source `UseSkillItem` scans inventories and slots in unsigned order,
accumulates BYTE counts within each bag, chooses its first sufficient bag and
consumes matching-kind stacks until the charge is met. Different templates of
one kind may combine within that bag. Cross-bag totals never authorize a cast.
Runtime selection and locked PostgreSQL validation use the same source arithmetic.

Every selected stack commits with core, timers and grouped audit receipts before
private UPDATEITEM/DELITEM responses in slot order, one MOVEITEM and cast success.
Fresh storage validates owned rows and the loaded weapon hash; graph storage
validates the entire checkpoint and preserves raw fields and unrelated state.
Original timer side effects still distinguish ordinary and loop missing-item
rejections. Unknown commit outcomes close the client and retain the reservation;
a stale final save must not replace possibly committed state.

Zero-target ammunition remains unsupported. The following increment adds
source-generated multi-attack expansion.
Cash templates 25020–25022 are absent from the pinned backup. Full damage/target
rules, equipment mutation and actual original-client execution remain pending.
See [batch source contract and evidence](evidence/ammo-batch-contract.json).

## Native multi-attack defender expansion

Migration 026 accompanies native `IsMultiAttack` / `GetCountMultiAttack` support.
The pinned definition sees every SDT_ABILITY/MTYPE_EFC row as a multi-attack, but
only SA_ONCE rows contribute to the learned-rank hit sum. Original INT deltas and
BYTE narrowing are retained; invalid INT arithmetic is refused. All valid ranks
of recovered skills 324, 412 and 1407 need at most seven final hits. Budgets above
16 remain unsupported because the original inner random loop can exceed MAX_TARGET.

For each flagged target while budget remains, append it once, draw from
`0..bTargetHit` and duplicate while the local count is below that draw. Remaining
hits repeat the first defender. Unflagged entries never become final defenders;
empty input never fabricates one. The runtime uses a thread-local random engine;
the original Windows process seed sequence is not reproduced. For the recovered
limits 1 and 3, the possible outcomes and distribution match the original modulo
range. Ordinary and loop response layouts are unchanged: 62/45 fixed bytes plus
five bytes per final defender, including original DWORD ID and BYTE type.

Ammo charges the final hit count, validated against the canonical learned rank
inside the same transaction. Resources and cooldowns apply once per cast. Fresh
storage locks the learned row; complete graphs use their own learned skills and
ignore stale normalized ranks. The existing ordered stack ACKs precede one
MOVEITEM, cast success and HPMP when charged. Migration 026 records expanded
ammo separately with `hit_mode`; older receipts remain direct.

See [source contract and verification](evidence/multi-attack-contract.json).
Full combat powers/damage, authoritative target/AOI checks, active effect timing
and original-client executable acceptance are separate unfinished contracts.


## Native whole-stack inventory movement

CS_MOVEITEM_REQ remains opcode 0x52A8 with exactly five BYTEs: source bag, source
slot, destination bag, destination slot and requested count. Source missing-bag
and missing-item errors retain values 2/1 and 3; zero count returns 3, same position
returns 4. Dead PCs and replicas do not mutate inventory.

A whole stack moving to an empty carried slot sends DELITEM(0x52AC, old bag/slot),
ADDITEM(0x52AB, new bag plus original complete descriptor), then MOVEITEM(0x52A9,
result0). Different templates swap entire stacks even for a one-unit request:
UPDATEITEM(0x52AA, source at destination), UPDATEITEM(destination at source), then
one MOVEITEM result0. IDs and counts remain unchanged; these ACKs are private.
Every response follows confirmed commit of all item positions and recovery state.

The original handler does not bound the requested position by bag capacity. The
modern handler deliberately refuses out-of-range slots using pinned bSlotCount,
rather than creating an invisible item. Whole-stack moves clamp oversized counts
as the source does. Splits, merges and same-template swaps, dropping, equipment,
timed bags and secured inventories are not yet accepted; unsupported requests
close before mutation. Unknown database outcomes close without success and keep
the reservation until recovery. Source-derived encrypted peers verify fresh-state
packets; graph transfer/recovery is verified at the native service boundary.
See [inventory contract](evidence/inventory-moves-contract.json). Original-client
executable acceptance remains pending.


## Native stack splitting, merging and exact item equality

The later 028 increment extends the same five-byte MOVEITEM request. For an empty
carried slot, a partial quantity clones the full original item fields and allocates
one new identity from the transactionally locked world counter. The source keeps
its ID and remaining count. Response order is source UPDATE, destination ADD, then
MOVEITEM success. A whole-stack move still retains its identity and DEL/ADD order.

For occupied carried slots, original CTItem equality compares template, level, gem,
appearance, grade, max/current durability, refine, expiration, all six full DWORD
extensions and the raw magic ID/value map. Equal template/level/gem resolve the
same reconstructed source attribute pointer. Equality intentionally ignores identity,
quantity and grade effect. Unequal items swap whole stacks even when templates match
or the requested count is one. Equal items merge min(request,source count,free stack
capacity), preserving destination identity and its attributes. The source emits
UPDATE or DEL, then destination UPDATE, then MOVEITEM. A full destination retains
both unchanged UPDATEs and successful MOVEITEM, exactly as the source does.

Pinned bStack is loaded on fresh and transfer hydration and revalidated in the
transaction. A corrupt destination already above capacity is explicitly refused
instead of reproducing source signed subtraction followed by BYTE narrowing.
Equipment, drops, timed bags, secure-code state and active trade/store rules remain
outside the supported mutation scope. Packet widths, framing and encryption do
not change. No result here is original-client executable acceptance.

Fresh rows or the complete graph, allocator, core, timers and both ordered stack
receipts commit atomically. Unknown commit outcomes retain the existing reservation
and do not retry or save a stale snapshot. Actual two-Map packet tests split and
move on the promoted server, transfer back and merge on the new primary while
stale normalized rows remain untouched. See [stack contract](evidence/inventory-stacks-contract.json)
and the [confirmed client acceptance gate](client-acceptance.md).

## Log write outcomes and binary payloads (2026-10-09)

`LogPacket.h` and `CUdpSocket.cpp:212,514` establish the original LP_LOG layout
and connectivity-based requeue behavior. This increment changes no opcode, size,
field order or receive decoder. An independent Python fixture uses the original
Win32 offsets and reaches the actual installed UDP daemon and PostgreSQL.

The modern correction is explicit transaction outcome handling: failed COMMIT or
unconfirmed rollback is never automatically replayed. Those records are counted
as unknown, without a success count or acknowledgement. No acknowledgement or
idempotency field was added to the original UDP protocol. A received duplicate
UDP event remains indistinguishable from a separate identical event; no global
exactly-once or lossless delivery claim is made. Empty payload retains the existing
modern NULL behavior; nonempty payload retains exactly the transmitted bytes.
The original writer always wrote its 512-byte array, so short payload equivalence
to a released sender still needs capture evidence. Timestamp fractions and legacy
code-page handling remain outside this test's scope.

PostgreSQL binding now uses hexadecimal bytea input for single and bulk writes;
raw zero bytes no longer pass through a text parameter. Parameterized batches
replace interpolated text, and DWORD/WORD values are not narrowed to signed
32/16-bit values. The fixture schema can store their full width; production schema
and read-side acceptance remain pending. Tests use loopback plaintext PostgreSQL
to inject reply loss deliberately; they do not replace the separate pool TLS tests.
See `evidence/log-outcomes-contract.json` for the exact guarantees and limitations.

## Native LP_LOG storage and read contract (migration 029)

The backup-derived schema inventory has no `TLOG_AUDIT` or `ITEMLOGTL*` table in
either supplied database. New empty `app_audit` storage follows the original
`LogPacket.h` field types and `CUdpSocket::LogDBSave`/`SQLQUERY_DGINSERT` column
order, not an invented audit-history import. `lt_id` and `received_at` are modern
metadata. Original event time remains timestamp without time zone at the seconds
precision used by the source writer. No automatic history deletion is introduced.

Raw bounded CHAR values become bytea, preserving the parser's bytes through
PostgreSQL even when they are not valid UTF-8. No code page or collation is guessed;
character interpretation and search by localized text remain separate acceptance
work. The native query repository returns the same raw strings and uses hex only
for startup display. The actual UDP test sends a complete 50-byte high-bit search
key, not just an ASCII approximation. Original DWORD/WORD maxima, signed keys,
nonempty payload bytes and the existing modern empty-payload NULL contract survive.
Native read ordering is descending generated identity, with bound user and LIMIT.

The original UDP decoder and packet fields are unchanged. This increment's
PostgreSQL queries and schema are production paths tested using synthetic events
and a dedicated append-only role; original clients and historical audit content
remain unavailable. Committed records survive graceful restart and SIGKILL; the
RAM-only queues and uncertain outcomes do not acquire a durability promise.
See `evidence/native-audit-contract.json` for source hashes and reproducible tests.


## Character-statistics inspection (migration 030)

`CS_CHARSTATINFO_REQ` (0x5323) contains one DWORD target ID. The original
`CSSender.cpp:3241` and `Client/TClient/CSHandler.cpp:6620` agree on an 87-byte
`CS_CHARSTATINFO_ACK` (0x5324). The new encoder retains all widths/order/endian
behavior, including three delays before three rates, WORD attack levels and BYTE
critical/charge fields. World messages prepend only the original requester DWORD.
No protocol field or client change was introduced.

Equipment attribute selection uses `wAttrID + grade[bLevel] + bGem` narrowed to
WORD; missing attributes select the first unsigned-WORD key, exactly as original
`SetItemAttr`. This fallback is evidenced behavior, not an estimated substitute.
Zero-initialized source grade entries are retained. Broken equipment contributes
no statistics; shield attributes belong to the separate blocking calculation.
The normal sheet follows original formula truncation, primary minimum floors and
min-attack clamping. Source backup data contains no SA_CONTINUE/SA_PASSIVE ability
rows (`bType=1`); supported passive arithmetic callbacks are unit-tested, but no
native populated passive-ability fixture is claimed.

Modern validation differences: exact body sizes are required; World requests and
answers must come from the relevant current primary. Map serves ready primaries
only, refuses sheets after level/aftermath drift, and withholds unsupported active
buff/companion/recall/guild/local-battle sheets. Undefined floating conversions and
signed overflow are rejected. These are explicit restrictions pending the full
systems, not successful fallback answers. The source request carries no epoch or
request token, so same-character delayed read-only replies across reconnect are
not claimed to be uniquely correlated.

Actual PostgreSQL/TCP verifies normal self-inspection and both transfer directions;
World relay loopback tests verify routing and wrong-peer rejection. Complete
remote-player inspection over two native Maps and actual unchanged client UI
acceptance remain pending. See `evidence/character-statistics-contract.json`.


## Offline actor-catalog transition (migration 031)

This is a documented modern operational extension. No client opcode, framing,
checksum, field, reply code or message order changes. Existing checkpoint and
primary-transfer packet bodies remain byte-for-byte unchanged during publication.
Only an explicit directional certificate for identical old item-magic/skill-point
charts plus the new source-derived statistics tables permits an old actor hash
when restoring a logged-out or recovered graph. Current character/routing hashes,
core fingerprints, typed graph decode and owner fencing remain mandatory.

The publisher checks pinned backups, metadata, both verified imports and original
text-byte/value hashes. It refuses live Map owners and blocks catalog changes that
would strand existing graph checkpoints or prepared transfers. Offline token
retirement prevents detached workers from writing after publication; ordinary
process replacement handles their original session/receipt identities. There is
no implicit retry after an uncertain commit and no transitive compatibility rule.

Actual old installed Map TCP creates logout, ready-target-crash and prepared-source
states. Current Map TCP verifies recovery, graph-only inventory and cooldowns,
source-frozen movement, current stat inspection and repeat login. The cell split
and accounts are explicit isolated fixtures. This proves the recorded runtime
upgrade scenarios, not all legacy graph variants or an original client session.
See [contract and reproducible evidence](evidence/actor-transition-contract.json).


## Native equipment boundary (migration 032)

`CS_MOVEITEM_REQ` remains exactly five BYTE fields at `0x52A8`. The existing
handler now accepts ordinary equipment positions, using `CanEquip` precedence:
full-DWORD raw wrap → learned SDT_EQUIP kind → slot mask → class mask → level.
Original `GetEquipLevel` compares default level against the full DWORD ELD before
subtracting a BYTE; an ELD at or above the required level does not reduce it.
Database TITEMCHART.bLevel is the original query's m_bDefaultLevel. The source
forces count one for equipment, remaps a two-hand secondary request to primary,
normalizes equipment-to-occupied-carried swaps, and plans displacement before the
incoming carried slot becomes free. Default-bag merge, other-bag merge,
default-bag blank and other-bag blank priority is retained. Equal equipped
items produce no item diff but still run the original equipment response sequence.

After the complete PostgreSQL commit: original DEL/ADD/UPDATE side effects;
`CS_EQUIP_ACK` (`0x52AD`, DWORD character, BYTE count, complete source item
descriptors ordered by slot) to self and local neighboring cells; MOVEITEM success;
87-byte CHARSTATINFO; 21-byte HPMP (DWORD ID, BYTE PC type, four DWORD values);
final MOVEITEM success. Both MOVEITEM successes are intentional source behavior.
Statistics, weapon gates and attack timing are rederived from the pinned charts.
HP/MP above the new maxima are clamped in the same transaction as the items;
reequipping raises maxima without healing. Original item bytes/opaque raw values
are retained and new split IDs use the existing world allocator.

Modern documented corrections: reply-after-commit and transactional allocation;
reject malformed aliasing after two-hand remapping and overlapping displacement
instead of reproducing original pointer alias/double-free paths; retain the
existing refusal of positions outside pinned bag capacity. No changes to framing,
crypto, checksums, opcodes, widths, endianity or client code. Native equal-item
receipts may contain zero changes, without inventing item updates.

The original 032 scope excluded active effects and warrior auto-postures; 033
adds the bounded permanent-posture contract below. It still excludes active recalls/
companions, party/guild/local battle rules, race-costume changes and special-item
combat consequences. Those implementations remain required. Local 3×3-cell EQUIP
broadcast is integrated; complete cross-Map visibility/replica updates and original
client appearance/UI acceptance remain open. Timed bags, trade/store and secure
code integration are also unfinished. No successful fallback represents these
missing contracts. See [equipment evidence](evidence/equipment-contract.json).

## Permanent warrior postures (migration 033)

The unchanged five-byte `CS_MOVEITEM_REQ` now drives source `ForceMaintain` for
131 (shield) or 132 (two-hand/axe), retaining the original pointer-lifetime branch.
No learned posture or client change is required. Both templates and all ability
values come from the pinned backup. `SA_BUFF` contributions use the original base,
clamp before `SA_CONTINUE`/`SA_PASSIVE`, and preserve separate shield-defense rules.

All replies follow the item/effect/core/checkpoint transaction. Item displacement
and main-move packets come first. Posture replacement sends old `CS_SKILLEND_ACK`
(7 bytes), the post-erase `CS_CHARSTATINFO_ACK` (87 bytes), then new
`CS_DEFEND_ACK` (84 bytes). Ordinary EQUIP → MOVEITEM → intermediate CHARSTATINFO →
HPMP follows. `CheckEquipSkill` then ends unsupported self-cast postures and sends
the final stat sheet before the last MOVEITEM. A broken but equipped matching
weapon still meets original `IsEquipSkillItem`; this differs from powered attack
equipment eligibility. Local neighboring sessions receive DEFEND/SKILLEND;
complete remote and cross-Map actor visibility is still unverified.

CHARINFO now emits each source 51-byte maintained entry rather than dropping the
collection. The original eight persisted fields remain unchanged in transfer and
storage: skill ID, rank, remaining, attacker type/ID, host type/ID, country. A new
ForceMaintain instance has hit/attack-level/attacker-level zero and current position.
After original LOADCHAR reconstruction these values become CTSkill constructor
defaults (1/1/1, position zero). DEFEND itself uses host 0/type 0, attack level 1
and character level, as the source sender does. These differences are intentional.

Zero remaining is permanent for these zero-duration templates, not expired.
The 033 baseline did not implement `CS_SKILLEND_REQ`; 034 adds the bounded path
below. `CheckEternalBuff` at zero MP,
timed expiry, generic status effects, dependent posture skills and death/remote
simulation remain acceptance gaps. Only synthetic encrypted TCP
and original source/backup oracles are available; actual client build/assets and
client UI acceptance remain blocked. See [posture evidence](evidence/postures-contract.json).

## Client-requested maintained-effect cancellation (migration 034)

`CS_SKILLEND_REQ` stays opcode `0x52b6` with exactly 19 bytes:
`DWORD object, BYTE objectType, DWORD host, DWORD attacker, BYTE attackType,
WORD skill, WORD map, BYTE channel`, little-endian. The original client sends it to
all Map connections (`CSSenderAll.cpp:965–989`). The primary own-PC path ignores
host/map/channel and removes only the first matching attacker/type/skill, regardless
of learned skill membership or template `bCanCancel`. Pinned posture 131 has
`bCanCancel=0`; this UI flag does not prohibit the original server operation.

Both a successful removal and an absent match send the original seven-byte
`CS_SKILLEND_ACK` (`0x52b7`, object/type/skill). Removal then sends the original
87-byte own `CHARSTATINFO`; an absent match does not. Replica connections answer
only their own socket and never invoke a database mutation. These semantics come
from `CSHandler.cpp:10384–10491` and `TObjBase::EraseMaintainSkill`, not modern mocks.
The removable collection must contain only permanent rank-one postures 131/132.
They do not change HP/MP maxima; generic and mixed effect erase side effects remain
unimplemented. Neighbor delivery uses existing local same-channel/map cells; full
cross-Map and multi-character visibility acceptance is still pending.

Modern corrections are explicit: exact-length decoding rejects trailing/truncated
bodies; own-PC object ID must equal the authenticated character (the original PC
branch ignored it and could broadcast a forged target on no match). Non-PC targets
and unsupported matching effects close without fabricated success until their
contracts are ported. No valid own-PC wire field changes. Replies follow the
PostgreSQL commit of effects, derived core, sampled timers and recovery receipt;
this strengthens original deferred persistence. A transaction error closes the
socket, suppresses stale logout and requires process recovery, with no automatic
retry of a potentially committed write.

Migration 034 retains exact request bytes and ordered before/after effects with
owner, connection, epoch and graph/core fingerprints. Fresh cancellation uses native
effect rows; graph cancellation updates the authoritative transfer receipt and
leaves stale normalized children untouched. The executed tests and explicit limits
are in [effect cancellation evidence](evidence/effect-end-contract.json).

`CheckEternalBuff` at zero MP is still pending. Correction to the previous audit:
it is called in `Defend` (TObjBase.cpp:1102), ordinary SKILLUSE (CSHandler.cpp:2972,
after costs, instance-power computation and `EraseBuffByAttack`) and ACTITEMSUSE
(CSHandler.cpp:20070). LOOPSKILL and `TPlayer::OnTimer` have no such call.
Outgoing ordinary powers must retain the pre-removal modifiers.
General timer expiry, death/status/recall hooks and original-client acceptance
remain required before claiming a complete effect lifecycle.


## Native outgoing cast attack profiles

The existing native SKILLUSE/LOOPSKILL handler now obtains attack powers, attack
level and critical probability from the selected learned skill's instance
projection. `DeriveStats` reads pinned `TSKILLDATA`, evaluates original
`CTSkillTemp::GetValue` at the learned rank, and applies every matching
`SDT_ABILITY` row regardless of action, as `CalcValue` does. It combines instance
and supported maintained-effect changes before the active clamp, then applies
passives against the original base. Ordinary inspection and maximum HP/MP retain
the projection without an instance skill. Equipment/cancellation refresh and graph
hydration rebuild the profiles; missing or mismatched level/aftermath/rank closes
the native request before costs or timers change.

Source authority: `TSkillTemp.cpp:49-101,172-209`, `TObjBase.cpp:1409-1547,1800-2265`
and `CSHandler.cpp:2943-2972,3300-3368`. Ordinary use chooses physical AL only for
`SAT_PHYSIC`; every other type, including `SAT_NONE`, sends magic AL. LOOPSKILL
always sends physical AL. Physical power uses ranged values when `IsLongAttack`
is true; magic power is populated in both cases. `GetCritical` selects magic
critical only for `SAT_MAGIC`. Ordinary use now carries the actual aid country,
which was previously left at the placeholder zero. The 62-byte and 45-byte fixed
ACK bodies, optional targets, opcodes, framing and encryption are unchanged.

**Explicit ordering correction:** original `CTBLSkillData` has no SQL `ORDER BY`.
The backup has a clustered key `(wSkillID,bAction,bType,bAttr,bExec)`. Four skills
contain both physical/long and MAGICNO attributes, making early-return attack type
sensitive to row order: 637, 640, 814 and 1343. The native read explicitly orders
by that recovered key. Twelve original parameterized reads on the restored backup
agree: the first three select physical, 1343 magic. This observation does not prove
that every historical SQL Server plan/version returned the same order. See
[original query observation](evidence/cast-powers-source-order.json).

The independent Python oracle uses original formulas and packet offsets, not the
modern projection or serializer. It checks ordinary/loop casts, rank modifiers,
magic/physical/ranged/no-attribute selection, equipment/aftermath/broken weapons,
stat isolation, cancellation refresh, restart and two-Map handoff. These are
synthetic TCP acceptance checks; original executable/assets remain unavailable.
Complete hit authorization, Defend, transfer HP/MP costs, durability, generic
effects, zero-MP removal and combat persistence are separate unfinished work.
At that previous cast-profile baseline, no stronger crash guarantee for cost-only
casts was claimed: they used periodic checkpoints, while item costs retained
the existing atomic transaction. Migration 035 below closes this accepted-cast
persistence gap. See [bounded cast-profile results](evidence/cast-powers-contract.json).

## Accepted native casts and ordinary zero-MP removal (2026-10-09)

The same original SKILLUSE (32-byte request header including count) and LOOPSKILL
(23-byte header) handlers now commit accepted state before publishing success.
The 62/45-byte ACK headers and five-byte ordered target occurrences are unchanged.
No cast ID is appended to any client or SS packet. PostgreSQL migration 035 stores
raw request/ACK and a server-only accepted-cast identity; hydration reads the
per-world/character head independently of source transfer bytes.

The backend rederives rank-based costs, timing, inventory selection and outgoing
powers from pinned source catalogs. It checks a possible original random target
expansion without drawing again. Valid later identical zero-cooldown requests
are separate casts. The ledger's predecessor uniqueness and account/claim locks
reject concurrent same-state writes, including free casts. All accepted resources,
timers, item receipts, supported effect removals and recovery writes share one
transaction. Unknown outcomes close without retry or saving the previous snapshot.

Original `CSHandler.cpp:2943–2972` computes instance powers, clears the instance,
calls EraseBuffByAttack and then CheckEternalBuff before the cast ACK.
`TObjBase.cpp:2562` erases is-use/zero-duration PC effects when MP is zero.
The supported pinned 131/132 templates have is-use=1, duration=0 and erase-act=0;
ordinary casts remove them in stored order with END, conditional HPMP and own STAT
before cast ACK, retaining the pre-removal powers. LOOPSKILL has no such call.
Generic attack cancellation, transHP/MP, durability, other effects and Defend
remain unfinished. The ledger records acceptance; target-hit authorization and
one-use consumption still require integration and verification.

See [verification scope and reproduction](evidence/accepted-casts-contract.json).
No real-client binary/assets exist in the available environment. Synthetic peers
and the source version 0x2918 do not establish a supported executable build.
