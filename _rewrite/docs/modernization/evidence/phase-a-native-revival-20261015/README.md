# Native revival and aftermath — 2026-10-15

Baseline: local `main` at `1c83b5f`. Status: **INTEGRATED_UNVERIFIED**, Phase A;
implementation and integration are reviewed statically. No build, test, SQL,
migration, import, backup extraction, service, server, client, container or runtime
acceptance is authorized in this phase. Local Git only; no push or deployment.
Original source/client/protocol libraries and existing migrations remain unchanged.
Applied001–043 and written/unapplied044–069 retain their previous status; next070.
Private extracts and pre-existing `_rewrite/docs/audits/` stay outside Git.

## Original request, client consumers and actual gap

`Server/TMapSvr/CSHandler.cpp:1067–1098` reads three FLOAT positions and a BYTE
type. It wakes the actor and moves it on its existing map; the primary then calls
`Revival(type==REVIVAL_NPC ? AFTERMATH_ATONCE : AFTERMATH_GHOST, NULL, 0)`.
This request does not select a server spawn from TSPAWNPOSCHART. NPC0 maps to
aftermath20 and all other source BYTE values to ghost10; HELP in this request
is not the skill-assisted revival branch.

`TPlayer.cpp:3610–3833` adds aftermath where applicable, emits updated statistics,
changes to normal mode, fills actual maxima at double-derived30% or40%, applies
the source minimum1 HP, broadcasts REVIVAL then HPMP to the actual neighbors,
and calls `ForceMaintain(800,...)`. Companions, tournament and BOW/BR variants
have additional responsibilities. A blanket full heal does not implement these
ordinary source paths.

Original senders and independent client consumers agree:

| Message | Body | Source / client |
| --- | --- | --- |
| REVIVAL request | three FLOAT + BYTE,13bytes | CSHandler1067 |
| REVIVAL acknowledgement | actor DWORD + three FLOAT,16bytes | CSSender1404; client CSHandler2011 wakes only a dead/ghost PC |
| AFTERMATH acknowledgement | actor DWORD + step BYTE,5bytes | CSSender5810; client CSHandler17685 installs the actual step |
| CHGMODE acknowledgement | actor DWORD + object BYTE + mode BYTE,6bytes | CSSender1172; client CSHandler1952 |
| HPMP acknowledgement | actor DWORD + object BYTE + four DWORD bars | CSSender1315; existing native sender/party-vitals boundary |

The baseline native `handlers/combat.cpp::OnRevivalReq` entered the older
in-memory prototype. It accepted client coordinates, filled both bars to100%,
mutated the cached character/presence and replied without a PostgreSQL claim,
checkpoint or mutation receipt. This is a concrete production gap, despite the
other native combat paths already rejecting that prototype.

## Aftermath source lifecycle

`TPlayer.cpp:3444–3540` defines Set/ResetAftermath. At level10 and above, adding a
step caps the ordinary result at100, derives stat reduction and reuse increase,
and starts the source interval. `OnTimer:955` orders maintained-skill handling,
recovery and aftermath decay. The deadline comparison is strict; item magic
`MTYPE_AFTERMATH` contributes through actual powered equipment
(`TObjBase.cpp:1624/1780`). The BYTE decrement has a source underflow edge which
must not be hidden by an invented bonus or an unannounced clamp.

Only the step travels in the original RELEASEMAIN/ENTERSVR body
(`SSSender.cpp:2082`). The constructor zeroes transient state, and common
LOADCHAR completion calls `SetAftermath(now, received_step)` at SSHandler5674.
The apparent zero at SSHandler3365 is initialization **before** query Open/Fetch;
CTBLChar SELECT/COLUMN_ENTRY (`DBAccess.h:192/235`) reads the stored step, and
the DM_LOADCHAR reply transmits it at3435. It is not a fresh-login penalty reset.
Native hydration must preserve the authoritative step and reconstruct the local
deadline; checkpoint sampling must not restart it on every write. No new wire
timer field or SQL timer column is inferred from the source.

## Native integration boundaries

The production repair enters existing native gameplay authority and the
PostgreSQL mutation/checkpoint pipeline before publishing a cached result or any
success response. Actual source statistics and effects determine maxima and bars.
`OnRevivalReq` selects native hydrated actors before the older prototype;
`OnNativeQuestReq` routes the13-byte request to `ChangeQuest` and `ReviveNative`.
Core, supported maintained effects, quest operation receipt and checkpoint/graph
commit together in the existing transaction. Local presence moves only after the
commit and before neighbor publication. The replica path cannot use the full
heal prototype; its distinct status/movement producer remains OPEN.
The existing current-user/key, owner token, connection/epoch, catalog, graph/head
and checkpoint checks remain required. Unknown commit outcomes must retain
ordinary close/recovery handling and must not replay the revival.

The supported runtime is ordinary PvE with a Ready primary and an actual supported
stat/effect graph. The supported position scope is the current map, same source64-unit spatial cell
and actual physical Map owner. It preserves the current neighbor set; cross-cell
movement, visibility transitions and transfer decisions are separate source
responsibilities. The committed position must update local presence before its
neighbor packets are published. Replica, unsupported mode, companion and effect
graphs must not enter the older cache-only path.

Fanout reuses the existing native owner/claim projection proof without invitation
or protected-list gates. It pins the current actor/account/key/channel/connection/
epoch/role and checks open Ready identity and current3×3-cell presence after SQL
waits. The caller already owns its gameplay operation; other recipients use a
short nonblocking operation, avoiding a wait cycle between two actors. A busy or
stale recipient may miss an event: no delivery acknowledgement or network replay
is invented. Client application and complete reconciliation remain unverified.

Skill800 must use actual published TSKILLCHART/TSKILLDATA and maintained-effect
state, including duration, expiry, relevant erase/cancel behavior and the source
negative-skill protection (`TObjBase.cpp:800`). No guessed template, cosmetic-only
buff or unconditional absence may substitute for it. Source missing-template
behavior is distinct from an existing template requiring unsupported semantics.

The implemented profile admits actual positive, nonhidden, nonposture
SA_BUFF/STATUS_NONE marker rows without object radius, with actual duration, priority and validated
erase flags. All ability-changing or other gameplay rows remain unsupported.
Native positive self-cure is the currently supported FINISH path; external
negative damage is still refused, so no fake MISS/damage producer is added.
Supported ACTION and ordinary SKILLUSE erase attack-marked800 before their source
reply; LOOPSKILL preserves it, matching its separate original handler. The other
source erase call at CSHandler20069 belongs to ACTITEMSUSE, whose native producer
remains OPEN. Expiry and own-PC cancellation use the existing effect pipeline.
Effect removal uses3×3-cell neighbors separately from ACTION's64-unit audience.

Explicit differences/bounds include refusing an already-alive revival request,
preserving exact13-byte/finite/source-range request validation, excluding lounge/
tournament maps500–532, and rejecting zero maxima, unsupported loaded penalty
normalization, BYTE decay underflow and dead nonstatic800 hydration. The latter
is not silently deleted from its durable receipt. The original own-PC SKILLEND
handler ignores bCanCancel; no new cancellation permission is invented. Existing
persistence recovery and ordinary physical routing are retained, without claiming complete
instance admission or instance lifecycle merely from the routing owner.

`RebaseAftermath` reconstructs only the local DWORD deadline from the actual
stored step. `AdvanceAftermath` uses actual powered item magic113, one strict
deadline decay per periodic transaction, and emits AFTERMATH→HPMP→own STAT.
The periodic producer selects actors with a nonzero aftermath step or finite800
even when no other progression effect is active; it does not add timer-based
zero-MP erasure for eternal800. Source zero-MP erasure belongs to ordinary
SKILLUSE/Defend, while LOOPSKILL and OnTimer do not call CheckEternalBuff.
No server cooldown multiplier is invented: the original m_fReuseInc field has
assignments but no source server reader in this audited path.

[Map implementation and exact supported branches](map-revival.md) and
[independent source/caller review](analysis.md) record final integration details,
explicit differences and remaining branches.

## Deferred verification and remaining dependencies

Phase B must exercise actual dead-character Login/Ready→REVIVAL→PostgreSQL
checkpoint→client acknowledgement, NPC versus ghost request bytes, levels9/10,
existing/capped aftermath, actual equipped decrement bonus and deadline boundary,
repeated requests, rollback/unknown commit and owner/key/epoch replacement.
Inspect exact packet order, statistic values,30/40% rounding and minimum HP,
own versus neighbor HPMP/party MANSTAT, normal mode and actual800 start/end.
Verify local timer reconstruction after fresh load, graph recovery and transfer
without dropping the stored step or adding timer bytes to the source protocol.

Same-cell and refused cross-cell/mode/graph/replica cases need independent tests;
then finish full source movement/AOI and mode-specific revival branches. Source
positive-player-damage SoulHP transfer, monster/party XP, full death/penalty and
other combat/reward producers remain OPEN. No fake SoulHP transfer is attached to
revival: its original trigger is positive, nonlethal player damage.

Phase A remains incomplete. Full functionality and compatibility with the
original executable are unverified; historical test results do not cover this
changed code. No migration or runtime acceptance has been executed.
