# Native signed identity boundaries — 2026-10-15

Baseline: local `main` at `199fa5e`. Status: **INTEGRATED_UNVERIFIED**.
Only static source, declaration/caller, SQL-text, numeric-domain and diff review
was performed. No build, test, SQL, migration, import, backup extraction, server,
client, service, container or runtime acceptance ran. Local Git only; no push.
Original server/client/protocol/library sources and all existing migrations stay
unchanged. Applied001–043 and written/unapplied044–069 retain their status;
no070 is added. Private extracts and pre-existing local audits stay outside Git.

## Reconstructed contract and two key representations

Original Login `Server/TLoginSvr/DBAccess.h:15–32` binds user/character/session
identities as DWORD outputs. The exact source adapter
`Lib/Own/TNetLib/TNetLib/SqlBase.cpp:289` binds `unsigned long*` DWORD storage with
`SQL_C_SLONG` and `SQL_INTEGER`, proving the signed bit interpretation rather
than unsigned64 widening. `handlers.cpp` in modern Login already copies signed
account bits into the original DWORD login reply and keeps the signed account in
its authenticated connection registry. Native `008-native-login.sql` stores
`TACCOUNT_PW.dwUserID` and `TCURRENTUSER.dwKEY` as SQL `integer`; generated defaults
do not prohibit explicitly stored negative values. `015` handoffs, `016` claims,
`018` checkpoints, `020` transfers and `067` soulmate offers use signed integer
identity columns. Existing World social repositories already preserve these bits.

The previous Map locks widened unsigned DWORDs to positive `long long`. For
example, wire `0xffffffff` became4294967295, which does not match the stored
integer−1. Correct source INT binding preserves the bits as−1. A negative SQL
account is not a missing account: zero remains the absent-identity sentinel.

Operational tables have a distinct, explicit contract. The same wire key is
stored as a nonnegative BIGINT in skill casts035, finishes036, actions037,
purchases040, title selection041, secure code unlock/operations042, quest
operations045 and mail notification060. They must retain4294967295 for the same
example. Using a signed INT key there would violate CHECKs or stop receipt and
unlock matching. Table declarations, rather than the variable name `key`, select
the representation. Genuine BIGINT connection generations, authority epochs,
operation/item IDs, revisions, durations and positive IPv4 values also stay wide.

## Login: authenticated create/delete and actual START

`PostgreSQLCharService::LockSession` now refuses only account0/key0 and binds the
key's exact signed INT bits. This is the existing authenticated lobby authority
used by native character creation/deletion; password/name/slot/ownership checks,
account-before-session locks and transaction behavior stay intact.

`SociMapServerLocator::StartAuthorized` accepts a nonzero signed account rather
than rejecting every negative account. `StartNative` binds the session key as
the same signed32-bit value in current-user lookup, exact existing handoff retry,
handoff creation, current-user update and audit. Routing, endpoint choice,
deadlines and exact reservation reuse remain in the same transaction. No extra
success packet or field is added. Auth's signed SQL readback→DWORD reply and
Login termination's exact account/key cleanup already retain these bits.

The native character allocator is separate: `012-native-characters.sql:24`
requires GENERATED ALWAYS positive `dwCharID`. START's positive character check
remains. This increment does not introduce high-bit character allocation, import
historical characters, change a sequence or widen the character schema.

## Map: actual claim through mutation, confirmation and release

The native-only `services/native_identity.h` helper `SqlInt32Identity` explicitly
preserves DWORD bits for source SQL INTEGER identity columns. Map account/current
claim locks, current-session lookup, primary and replica claim/release, native
hydration, loaded/Ready updates, checkpoint receipt/write and transfer authority
use the matching signed representation. Downstream native character SQL bindings
use the same explicit boundary. [Map implementation inventory](map-identity.md)
lists the concrete files and mixed-domain call sites; the
[independent audit](analysis.md) records source/schema comparisons and review.

Gameplay functions which check `TCURRENTUSER` and write a BIGINT session receipt
keep separate signed-current-key and positive-wire-key locals. Secure unlock
state is compared and stored in its existing unsigned BIGINT domain. Cast charge
correlation, finish lookup, operation confirmation and mail notification metadata
retain that same operational representation. No receipt or schema is rewritten.

Existing exact account/key/character/owner/token/connection/epoch/role checks,
catalog and checkpoint fences, lock order, atomic mutation, SQL-aborted retry
rules and unknown-commit confirmation remain authoritative. No automatic replay
of unknown writes is introduced. Network DTOs and packet bytes are unchanged;
the corrected representation is confined to existing native database boundaries.

## Deferred verification and remaining scope

Phase B must exercise actual authentication→create/delete→START→Map lookup/claim,
load/Ready, replica admission/projection/release, checkpoint/restart and two-Map
transfer/logout using existing signed account and session representations at
`0x7fffffff`, `0x80000000` and `0xffffffff`; include account/key0 rejection and
ordinary positive character IDs. Confirm exact original DWORD packet bytes and
that owner/key/epoch replacement still rejects stale requests.

Exercise charged cast/action/finish, purchase/title/quest receipts and secure
unlock with high-bit keys: INTEGER current-user checks must match negative rows
while BIGINT receipts/unlock retain positive wire values. Include unknown commit
confirmation without replay, stale prior keys, late release, rollback and mail
notification publication. Genuine BIGINT generations above32bits and high IPv4
values must remain unchanged. No such execution is claimed here.

Full high-bit character allocation requires a later explicit source/schema and
sequence policy, separate from these binding fixes. Full source Map admission,
World/bootstrap, client roster, party HP/XP/loot/combat, guild/corps/modes and
other unported responsibilities remain in scope. Phase A is incomplete; full
functionality and compatibility with the original executable remain unverified.
Historical test results do not cover this increment.
