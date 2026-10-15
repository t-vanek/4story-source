# Map basic-party own-PC positive self-cure

Baseline `42451e1`, 2026-10-15. Status: **IMPLEMENTED /
INTEGRATED_UNVERIFIED**. Source, caller, declaration, packet, SQL-text and diff
review only. No build, test, SQL execution, migration, import, extraction, server,
client, service or runtime acceptance ran. No Map-agent commit or push. No new
migration; the written, unapplied071/072 prerequisites remain and next is073.

## Source and client contract

Original FINISHSKILL (`Server/TMapSvr/CSHandler.cpp:20197–20666`) parses a WORD
attack-party ID supplied by the client. The apparent assignment from the
attacker's GetPartyID is commented out. The source does not require zero or a
match to the current server party. Base Defend passes this value to OnDamage
at `TObjBase.cpp:950`, inside the IsDamageType branch. Every currently admitted
own-PC positive cure consists solely of SA_ONCE/SDT_CURE HP/MP rows and never
enters that branch (`TSkillTemp.cpp:118`). PerformSkill has no party argument.
The WORD is therefore parsed and recorded as inert request metadata in this
bounded path, not interpreted as membership or effect authority.

Original client `CSSender.cpp:2980` supplies
`TClientGame::GetPartyID(mainChar)`. That getter (`TClientGame.cpp:22821`)
returns the raw character party WORD. It does **not** apply the server's
PT_SOLO1 mask (`TObjBase.cpp:2240`). Source JOIN/ATTR can place masked0 in the
client; a later TYPE→SOLO changes only the obtain byte and sends no extra ATTR,
so a valid client can retain a nonzero raw party ID while the server's effective
party is0. No guessed raw/effective PID equality check is introduced, and the
request WORD never overwrites the actor's actual cluster.

The original own-HPMP sender (`CSSender.cpp:1315–1335`) enqueues the21-byte
client HPMP first, then sends PARTYMANSTAT only when the object's ID is the
owner and server GetPartyID is nonzero. The same actual HP/MP, maxima and level
are used. SOLO therefore emits no MANSTAT even if the finish request contains a
nonzero client party WORD.

## Native implementation and capability bounds

`PostgreSQLMapService::FinishSkill` removes the blanket `request.party!=0`
refusal. It still requires own primary PC attacker/host and only own-PC target
occurrences, with the existing positive instant-cure capability and single-use
accepted receipt. Ordinary, loop and ACT origins retain their separate
definition/rank/cost/route contracts. No foreign target, party heal, target
expansion, receipt field, opcode or DTO byte is added.

`ReadActItemSkill` also removes its raw-party refusal, allowing the supported
actual action-item definition and later FINISH to complete while in a basic
party. Actual-stat, guild/tactics/riding, companion/recall/local, special-mode,
kind21/flat-row/no-learned-trigger and hardcoded-skill bounds remain. The actual
commander is preserved and compared as context; its value does not prohibit
this independent own cure. The World-supplied cluster must be hydrated. No
client zero field is substituted for missing state. Complete corps membership
and its display fanout remain separate OPEN capabilities.

Stat derivation already supports basic parties: original pure CON/MEN/maxima
and the admitted effect formulas do not read party membership. Existing source
HP/MP row calculation, random contribution, representability bounds, maximum
clamp, applicable zero-MP/equipment-posture removal and Defend activity clock
are unchanged. Source pure-CURE aggro remains a proven no-op: damage inputs
stay0 and `CTMonster::SetAggro:141` returns before mutation. This does not port
damage, SoulHP or real monster aggro.

All durable owner/account/session/epoch, catalog/072 guild-skill absence,
physical-cell owner, inventory/graph/checkpoint and cast/finish-head checks
remain. The accepted occurrence still commits once, then output follows its
committed state. Unknown COMMIT or output is never retried. An empty filtered
target list still consumes its accepted finish without inventing a heal.

Shared FINISH now refuses nonempty actual GOHOME2 hits before effect mutation.
Complete `GetAtkHitType` (`TObjBase.cpp:2266–2348`) returns MISS for mode2 before
premium/kind21/guaranteed-hit checks. Source MISS has an additional Avoid
maintained-effect lifecycle outside this cure capability. The existing unsupported-finish
handler closes the requester with an uncertain cache; it does not insert a finish
operation, refund accepted costs or claim source MISS parity. Ordinary/loop had a
latent partyless gap here; ACT already refused this mode. Empty ordinary/loop
completion never calls that hit test and retains its existing receipt consumption
and equipment cleanup; ACT keeps its earlier wider mode2 bound. This increment fixes
that gap without inventing a successful heal or partial MISS reply. A mode/scope
change after an accepted cast may therefore leave costs already committed and
make its finish unavailable; no refund or cast/finish replay is introduced.

## Current output and independent membership authority

`domain/party.h::PartyProjection` is a process-local copy of hydrated raw
party/type/chief/commander. It is neither a PostgreSQL party row nor proof of
World registry membership. `DeliverNativeGridProjection` accepts it optionally;
existing callers without this proof keep their prior contract. ACTION and
FINISH capture the actual committed tuple and compare it to the live Map
snapshot before and after awaited recipient SQL authorization. Exact
actor/recipient Ready identity, user/key/channel/connection/epoch/role, open
session and live presence are still checked before enqueue. Other recipients
only acquire nonblocking operations; no cross-actor waiting or packet replay.

The final native ACTION ACK now uses that helper as well as its already fenced
effect END/STAT output. A captured `request.skill!=0` selects source64-radius
nearby scope; skill0 retains3×3-cell neighbors. FINISH preserves the separate
END/STAT neighboring-cell and DEFEND/HPMP nearby audiences and exact output
order. Its post-await recipient scope is checked against actual current
presence. These changes do not fabricate application delivery acknowledgement.

`PublishPartyVitals` runs only after the owner's actual HPMP queue attempt. It
checks the current live hydrated party tuple against the output snapshot,
requires the real owner PC packet, and uses **current effective local party**
for MANSTAT. It never uses the finish request's WORD. No extra HPMP or MANSTAT
is invented for unchanged resources, a suppressed owner enqueue or SOLO.

The caller's gameplay operation blocks local JOIN/ATTR/TYPE updates because
`BeginPartyProjection` (`session_registry.h:210`) requires operations0. World
can already have changed its registry while the corresponding Map projection
waits. The Map SQL `AuthorizeParty` proves live owner/current actor claim; it
does **not** query or prove process-local World membership. This distinction is
preserved explicitly.

Existing World `OnPartyManstatAck` (`handlers_party.cpp:904–992`) owns the actual
fanout decision. Its party-operation gate serializes membership/type/handoff
changes; current typed primary and PostgreSQL actor authority are revalidated.
The actual registry party/member backlink and corps0 are checked independently.
A stale reported PID causes an actual ATTR refresh and drops the stats; matched
stats reach only current authorized members. This increment changes no World
handler and does not claim a distributed transaction between the heal and
World membership. Local tuple checks complement that existing authority.

## Deferred Phase B and OPEN

Exercise ordinary, loop and ACT self-cure in a real basic party; nonzero,
zero, stale and forged-but-inert finish WORDs; JOIN→SOLO and TYPE→SOLO raw-client
histories; owner21-byte HPMP followed by actual MANSTAT; no publication on
unchanged bars, missing owner enqueue or effective SOLO0; current chief/type
changes; World change before blocked Map ATTR; transfer/close/owner loss around
every await; actor/recipient role or cell movement during ACTION/finish output;
first versus repeated identical outstanding finishes; empty targets; nonzero
raw commander without a fabricated zero, and World corps-fanout refusal.
GOHOME2 with a nonempty ordinary/loop target list, including mode changes after
cast acceptance, must refuse without a fake heal/MISS or replay; empty
ordinary/loop completion retains receipt consumption and equipment cleanup.
ACT keeps its earlier wider mode2 refusal.
Existing071/072 unavailable,
catalog drift, rollback and unknown-COMMIT cases remain required. None was run.

Other targets, group cures, corps, general/negative/maintained/linked effects,
SoulHP, complete AI/combat, full bootstrap/AOI/roster recovery and broader
original-client acceptance remain OPEN. Original maintained database-save
projection and request-nonce ambiguities are unchanged. Successful local heal
does not establish remote party display delivery or SQL membership parity.
