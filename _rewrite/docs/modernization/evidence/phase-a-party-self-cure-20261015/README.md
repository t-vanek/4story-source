# Native party self cure and current ACTION output — 2026-10-15

Baseline: local `main` at `42451e1`. **INTEGRATED_UNVERIFIED**. This increment
extends existing actual positive own-PC HP/MP cure completion to native party
actors and repairs ordinary ACTION publication after awaited effect output.
The original client, protocol, item debit and one-use receipt policy stay intact.

Only static source/API/SQL-text/packet/diff/CSV inspection was performed. No
build, test, SQL, migration, import, extraction, server, service, client,
container or runtime verification ran. Local Git only; no push or deployment.
No new migration: applied001–043 and written/unapplied044–072 are unchanged;
next073. Private audit output and extracts remain outside Git. Full Phase A,
full gameplay and acceptance by an actual original executable remain unfinished.

## Source and implementation

- [Whole source/client and authority analysis](analysis.md).
- [Map implementation and deferred acceptance](map-self-cure.md).
- [Accepted-cast correlation and actual FINISHSKILL effect](../../../../../Server/TMapSvrAsio/services/postgresql_map_checkpoint.cpp).
- [ACT item profile and actual debit caller](../../../../../Server/TMapSvrAsio/services/postgresql_map_actitems.cpp).
- [Ordinary ACTION and FINISHSKILL publication](../../../../../Server/TMapSvrAsio/handlers/maintained_effect.cpp).
- [Current native grid/nearby projection](../../../../../Server/TMapSvrAsio/services/native_projection.h).
- [Actual local party vitals relay](../../../../../Server/TMapSvrAsio/handlers/social.cpp).

Original OnCS_FINISHSKILL_ACK (`Server/TMapSvr/CSHandler.cpp:20197–20666`)
reads the client's party WORD and forwards it to Defend. The apparent override
from actual server GetPartyID is commented out. Complete Base Defend
(`TObjBase.cpp:767–1306`) uses this argument only for OnDamage inside the
IsDamageType block. Actual admitted ordinary/loop and ACT profiles contain
only SA_ONCE/SDT_CURE HP/MP rows; they cannot enter this block. PerformSkill
does not take a party argument. Source cure, random increment, clamping, activity,
zero-MP cleanup, equipment-posture cleanup and zero-aggro behavior are unchanged.

The original client writes this field from GetPartyID (`TClientGame.cpp:22821`),
which returns the PC's raw m_wPartyID. Server GetPartyID (`TObjBase.cpp:2240`)
instead masks PT_SOLO1 to0. A type-only acknowledgement can leave the original
client with a nonzero raw ID while server effective membership is0; JOIN/ATTR
can instead supply0. Matching the request to a guessed effective party ID would
reject legitimate original-client finishes. Pure self cure therefore retains
the supplied WORD in the exact durable request but does not use it as authority.
No actor cluster field, target, guild role or recipient is selected from it.

Previously the shared native finish rejected every nonzero request party WORD,
and ACT initiation/consumption rejected every actual party actor. Those broad
barriers are removed for the existing admitted own-cure profile. All actual
chart/trigger/rank/target/resource/catalog/physical-owner checks remain. No
other-player heal, invented party effect or broadened negative attack is added.
Complete source GetAtkHitType (`TObjBase.cpp:2266–2348`) returns MISS for
actual GOHOME2 before premium/kind21 or guaranteed-hit data. ACT already refused
this mode; the shared ordinary/loop finish now also refuses nonempty hit lists
before mutation or accepted-receipt consumption. An empty ordinary/loop target
list does not call the hit test: its existing receipt consumption and equipment
cleanup remain available. ACT retains its earlier wider mode2 capability bound.
Producing the source MISS would require its
unported avoid-maintain branch, so no heal or guessed MISS response is emitted.
This also fixes the earlier partyless finish gap. The existing unsupported-finish
error path closes the requester and marks its cache uncertain, without inserting
a finish operation or changing the earlier accepted cast. This is an explicit
modern capability refusal, not source MISS parity. A scope change after cast
acceptance can leave finish unavailable; no refund/replay is invented.

Current guild-skill nonmembership still requires written072's complete verified
publication; all new cast writers still require071. Neither is applied here.

## Local projection and World membership

The local native party projection comparison uses hydrated raw party ID,
obtain type, chief and commander. ACTION/FINISH output compares the committed
actor context against current cache state before and after awaited recipient
SQL authority. Existing exact actor/recipient identity, Ready role/epoch,
socket, presence, map/channel, grid and nearby64 checks still apply. The optional
context check does not change callers that do not provide it and does not
pretend that PostgreSQL stores World transient membership.

Ordinary ACTION previously sent its final reply directly after awaited END/STAT
projections. It now uses the same current native projection helper. Skill actions
retain source3×3 cells plus distance<=64; an action without a skill retains the
source neighboring3×3 cells. Errors and successes keep their original ACTION17
body. Busy or stale recipients may miss output; no retry or delivery ACK is added.

After the owner's actual HPMP enqueue, PublishPartyVitals requires the live
actor's local party context to match the supplied committed context and requires
own-PC HPMP type1. It relays the packet's actual bars with the live effective
party ID, so SOLO yields0 and no party MANSTAT. It never uses the finish WORD.
This is a local projection check, not a claim of current World membership.

Native gameplay operations exclude simultaneous local party projections through
BeginPartyProjection. TransferOut can start during a gameplay operation; handoff
drains it before capturing state. A committed cure can therefore survive handoff
even when its old Ready output is suppressed. Do not claim that gameplay prevents
handoff from starting or that a queued packet is delivered to the application.

World OnPartyManstatAck retains its existing serialized party operation, current
typed peer and SQL actor authority, actual character party ID and registry
backlink. A stale supplied relay ID triggers actual ATTR repair and drops the
stale statistics. Only matching current membership selects real member recipients
under current pair authority. Finish input cannot fabricate this membership.
Concurrent membership transitions can still suppress a statistics update; the
existing reconciliation/no-replay boundary remains explicit.

## Deferred acceptance and OPEN

Phase B must exercise ordinary/loop and ACT self cures with actual supported
charts in each party obtain type; SOLO after JOIN/ATTR and after type-only ACK;
zero, old, mismatched and nonzero unused finish WORDs; unchanged packet bytes,
effect rank, ordered random rows/clamping and empty filtered finish; actual
GOHOME2 hit refusal before finish mutation, ordinary/loop empty cleanup and mode
change after acceptance; exact one-use
consumption and mixed origins; no initial ACT debit/rearm/quest credit; current
World MANSTAT membership, actual owner bars and SOLO silence; concurrent
join/leave/type/chief changes, TransferOut/drain and reconnect; owner/recipient
loss or movement around every projection await; near64/grid boundaries and
unknown COMMIT/send without replay. None has been executed.

Full other-target or group healing, guild/corps membership and group effects,
learned-trigger ACT profiles, repeated/linked/maintained effects, negative
combat/aggro/SoulHP, special modes, full bootstrap/AOI, source maintained DB-only
save filters/collapse and actual original-client acceptance remain OPEN. Existing
nonce-less FIFO finish ambiguity and modern one-use receipt policy retain their
previous scope. This increment does not claim full party or combat parity.
