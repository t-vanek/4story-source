# TWorldSvrAsio — modernized cluster coordinator

Wire-compatible replacement for `Server/TWorldSvr/` (38 851 LOC,
30 files) running on the `FourStoryCommon` infrastructure (SOCI
pool, spdlog audit, health endpoint) and the `boost::asio` reactor
that the four shipped Asio daemons already use.

> Cluster context: [main README](../../README.md#overall-progress) ·
> patch catalog vs legacy Araz sources:
> [`_rewrite/docs/PATCH_README.md` §6](../../_rewrite/docs/PATCH_README.md#6-tworldsvr)

## Status — W4-5 chat channel relay (CHAT)

| Phase | Scope | Status |
|---|---|---|
| W1 | Scaffold + transport + dispatch stub | ✅ |
| W2 | CharRegistry + MW_ADDCHAR_ACK + MW_CLOSECHAR_ACK | ✅ |
| W3a-1 | GuildRegistry + DM_GUILDLOAD_ACK + schema validator | ✅ |
| W3a-2 | PeerSession + PeerRegistry + RW_RELAYSVR_REQ + first senders | ✅ |
| W3a-3 | CharRegistry name index + OnMW_CHANGECHARBASE_ACK + OnRW_ENTERCHAR_REQ + OnRW_RELAYCONNECT_REQ | ✅ |
| W3a-4 | TChar.guild_id back-pointer + OnMW_GUILDLEAVE_ACK | ✅ |
| W3a-4b | guild_constants.h + write API + OnGuildDisorganizationReq + OnGuildDutyAck + OnGuildFameAck | ✅ |
| W3a-4c | guild_broadcast.h + OnMW_GUILDKICKOUT_ACK + OnMW_GUILDCONTRIBUTION_ACK + OnDM_GUILDMEMBERADD_REQ | ✅ |
| W3a-4d | CoOffloadVoidIf wiring (closes W-1) + GuildLevelCache mirror of TGUILDCHART | ✅ |
| W3a-5 | `services/guild_peerage.h` (CheckPeerage gate using guild_levels) + UpdateMemberPeer + UpdateMaxCabinet + OnMW_GUILDPEER_ACK + OnDM_GUILDCABINETMAX_REQ | ✅ |
| W3a-6 | Guild invite flow: INVITE + INVITEANSWER + JOIN_REQ 10-field sender | ✅ |
| W3a-7 | TGuildMember castle/camp/connected_date_unix + variable-length MEMBERLIST sender + OnMW_GUILDMEMBERLIST_ACK + W3a-6 JOIN_REQ wire-bug fix | ✅ |
| W3a-8 | Articles board (LIST/ADD/DEL/UPDATE) + TGuildArticle storage + caps | ✅ |
| W3a-9 | TGuild gains 6 fields + GuildInfoPayload (30-field POD) + SendMwGuildInfoReq + OnMW_GUILDINFO_ACK | ✅ |
| W3a-10 | IGuildRepository::DeleteGuild + OnMW_GUILDMONEYRECOVER_ACK + OnDM_GUILDEXTINCTION_REQ | ✅ |
| W3a-11 | GuildWantedRegistry + AddWanted/DeleteWanted + OnMW_GUILDWANTEDADD/DEL/LIST_ACK + 3 senders + 14-day expiry | ✅ |
| W3a-12 | TGuildWantedApp + GuildWantedRegistry::AddApp/DelApp/SnapshotAppsFor/FindAppByChar (with 5 legacy validation gates) + IGuildRepository::AddVolunteerApp/DelVolunteerApp + 4 handlers (VOLUNTEERING/DEL/LIST/REPLY) + 4 senders + accept-path member promotion via OnGuildInviteAnswer YES-branch parity | ✅ |
| W3a-13 | `TryPromoteIntoGuild` helper (dedupes W3a-6 InviteAnswer YES + W3a-12 VolunteerReply accept) + IGuildRepository::UpdatePvPoints + OnDM_GUILDPVPOINT_REQ | ✅ |
| W3a-14 | DB-side fan-in cohort: 5 thin handlers (OnDM_GUILDDUTY/PEER/CONTRIBUTION/LEVEL/POINTREWARD_REQ) + 2 new repo methods (UpdateLevel, LogPointReward) + 6 mut-handler test scenarios | ✅ |
| W3a-15 | Fame + article DB fan-in (OnDM_GUILDFAME_REQ + OnDM_GUILDARTICLEADD/DEL/UPDATE_REQ) — 4 handlers reusing existing repo methods | ✅ |
| W3a-16 | Wanted/volunteering DB fan-in (OnDM_GUILDWANTEDADD/DEL_REQ + OnDM_GUILDVOLUNTEERING/INGDEL_REQ) — 4 handlers + GuildWantedRegistry defensive mirror + bType filter on the volunteering pair | ✅ |
| W3a-17 | Leave/kickout DB fan-in (OnDM_GUILDLEAVE_REQ + OnDM_GUILDKICKOUT_REQ) — 2 handlers + shared ScrubMembershipInMemory helper completes the W3a-4c MEMBERADD pair | ✅ |
| W3a-18 | Guild establishment: OnMW_GUILDESTABLISH_ACK creates new guilds in one coroutine (vs. legacy 4-hop map↔world↔DB roundtrip) + IGuildRepository::CreateGuild | ✅ |
| W3a-19 | Wanted-board periodic expiry sweep: GuildWantedRegistry::PruneExpired + SweepExpiredWanted coroutine wired into RegistryRefresher (closes W3a-11 TODO) | ✅ |
| W3a-20 | Vestigial DB-server ACK echoes (OnDM_GUILDESTABLISH/DISORGANIZATION/EXTINCTION_ACK) — 3 log+drop stubs eliminate "unknown wID" warnings on hybrid legacy-DB deployments | ✅ |
| W3a-21 | PvP record audit log (OnDM_PVPRECORD_REQ) — batched per-row persistence via new IGuildRepository::LogPvPRecord + kPvPEventCount=8 constant | ✅ |
| W3a-22 | Full-row guild update fan-in (OnDM_GUILDUPDATE_REQ) — 8-column scalar overwrite via new IGuildRepository::UpdateGuildFull; alliance/enemy ID lists parsed for wire-compat then dropped (deferred to W5+ war system) | ✅ |
| W3a-23 | PvP record list reader (OnMW_GUILDPVPRECORD_ACK) — pairs with W3a-21 audit log; new TPvPRecord POD + TGuildMember.weekrecord + GuildPvPRecordRow sender | ✅ |
| W3a-24 | Per-period war-result fan-in (OnMW_LOCALRECORD_ACK) — accumulates kill/die/points deltas into TGuildMember.weekrecord so W3a-23 reader returns live data | ✅ |
| W3a-25 | Alliance + enemy state modelling — TGuild gains vector<uint32_t> fields populated by W3a-22 (drained-and-dropped lists become real in-memory state) | ✅ |
| W3a-26 | Cabinet LIST stub (OnGuildCabinetListAck) — wire-compat empty-list reply via SendMwGuildCabinetListReq; PUTIN/TAKEOUT + item codec still deferred | ✅ |
| W3a-27 | PvP point reward log reader (OnGuildPointLogAck) — pairs with W3a-14 writer; new TPointRewardEntry + TGuild.point_log in-memory mirror | ✅ |
| W3a-28 | Per-day vRecord history + CalcWeekRecord — replaces W3a-24's plain accumulator with proper week-trim semantics matching legacy CTGuild::CalcWeekRecord exactly | ✅ |
| W3a-29 | PvP-point gain/use fan-in (OnGainPvPointAck) — char relay + guild bank mutation (total/useable/month) + point_log newest-first/TOP-50 trim fix | ✅ |
| W3a-30 | Boot-time point_log load (SOCI LoadAll third pass from TGUILDPVPOINTREWARDTABLE) + FakeGuildRepository::Clone fidelity fix (was dropping alliance/enemy/point_log/month on round-trip) | ✅ |
| W3a-31 | Tactics subsystem part 1 — wanted board (OnGuildTacticsWantedAdd/Del/ListAck) + new GuildTacticsWantedRegistry (multi-posting-per-guild, globally-unique ids, reward fields) | ✅ |
| W3a-32 | Tactics subsystem part 2 — volunteer applicant flow (OnGuildTacticsVolunteering/Del/VolunteerListAck) + registry applicant API (AddApp 7-gate / DelApp / SnapshotAppsFor) + wanted-board already_applied wiring | ✅ |
| W3a-33 | Tactics subsystem part 3 — reply accept/reject (OnGuildTacticsReplyAck): hires applicant as a tactics member (TTacticsMember model + TChar.tactics_guild_id) charging PvP-points + money up front, with the 7 hire gates + dual broadcast | ✅ |
| W3a-34 | Tactics subsystem part 4 — kickout (chief-kick forfeit / self-leave refund via TGuild::RemoveTactics) + roster list (GetCurGuild priority) | ✅ |
| W3a-35 | Tactics subsystem part 5 — chief-initiated hire (OnGuildTacticsInvite/AnswerAck): invite-by-name dialog relayed to the target, accept runs the hire promotion + dual outcome echo. Tactics subsystem now feature-complete (wanted/volunteer/reply/kickout/list/invite/answer) | ✅ |
| W3a-36 | Tactics contract term-expiry sweep (SweepExpiredTactics on a RegistryRefresher tick) — ends contracts past end_time, clears char back-pointers; closes the legacy EXPIRED_GT path | ✅ |
| W3a-37 | Cabinet item codec — TGuildCabinetItem model + WrapItem/CreateItem-symmetric wire codec; OnGuildCabinetPutin/TakeoutAck (stack/decrement) + LIST upgraded from the W3a-26 stub to emit real items | ✅ |
| **W3a-38** | Disband + point-reward player actions (OnGuildDisorganization/PointRewardAck) — the map→world entry points pairing the W3a-4b/W3a-14 DB fan-ins; closes the last player-facing guild gaps | ✅ |
| W3a-39+ | DB persistence (tactics + cabinet) + W5 castle/skill guild handlers | ⏸ |
| W3b-1 | Party subsystem foundation — PartyRegistry + TParty + TChar party_id/party_waiter/HP-MP fields + OnMW_PARTYADD_ACK invite-relay gate + SendMwPartyAddReq | ✅ |
| W3b-2 | Party formation — OnMW_PARTYJOIN_ACK (create new party / join existing) + PartyRegistry::GenId + JoinParty pairwise PARTYJOIN_REQ fan-out + PARTYATTR HUD push + SendMwPartyJoinReq/AttrReq | ✅ |
| W3b-3 | Party leave/kick — OnMW_PARTYDEL_ACK + LeaveParty (chief succession, PARTYDEL fan-out, disband cascade on drop-below-two) + SendMwPartyDelReq | ✅ |
| W3b-4 | Party attribute changes — OnMW_PARTYMANSTAT_ACK (member-stat broadcast) + OnMW_CHGPARTYCHIEF_ACK (hand off leadership) + OnMW_CHGPARTYTYPE_ACK (loot mode) + 3 senders | ✅ |
| W3b-5 | Party member recall — OnMW_PARTYMEMBERRECALL_ACK (summon/move-to gate + RECALLANS_REQ forward) + OnMW_PARTYMEMBERRECALLANS_ACK (destination relay, same-map + meeting-room gates) + 2 senders | ✅ |
| W3b-6 | Party round-robin loot — OnMW_PARTYORDERTAKEITEM_ACK (turn-cursor next-looter selection + item forward via the cabinet codec; stale-party MIT_NOTFOUND) + TParty order rotation (GetNextOrder/SetNextOrder/GetOrderIndex) + 2 senders | ✅ |
| W3c-1 | Corps subsystem opener — CorpsRegistry + TCorps + corps_constants + OnMW_CORPSASK_ACK invite-relay gate (CheckCorpsJoin) + SendMwCorpsAskReq/ReplyReq | ✅ |
| W3c-2 | Corps formation — OnMW_CORPSREPLY_ACK (create new corps / join existing) + CorpsRegistry::GenId (shared party id pool) + NotifyCorpsJoin pairwise ADDSQUAD fan-out + CORPSJOIN_REQ + commander PARTYATTR + SendMwAddSquadReq/CorpsJoinReq | ✅ |
| W3c-3 | Corps leave/dissolve — OnMW_CORPSLEAVE_ACK + NotifyCorpsLeave (mutual DELSQUAD fan-out, commander succession, dissolve cascade on drop-to-one) + SendMwDelSquadReq | ✅ |
| W3c-4 | Change corps commander — OnMW_CHGCORPSCOMMANDER_ACK (general hands the commander role to another squad; reply + all-squad refresh) + SendMwChgCorpsCommanderReq | ✅ |
| W3c-5 | Corps squad reshuffle — OnMW_PARTYMOVE_ACK (general moves a member between squads / swaps two members) reusing the party Leave/Join machinery + SendMwPartyMoveReq | ✅ |
| W3c-6 | Corps command broadcast — OnMW_CORPSCMD_ACK (general's move/attack order relayed to every corps member, or own party when corps-less) + SendMwCorpsCmdReq | ✅ |
| W3c-7 | Corps enemy-list family + HP — OnMW_CORPSENEMYLIST/ADDCORPSENEMY/DELCORPSENEMY/MOVECORPSENEMY/MOVECORPSUNIT/CORPSHP_ACK (6 chief-to-chief opaque relays via a shared CorpsChiefRelay + SendMwCorpsChiefRelay) — **corps subsystem complete** | ✅ |
| W4-1 | Friend subsystem opener — TChar.friends/region + TFriend + friend_constants + OnMW_FRIENDASK_ACK invite gate (relay / mutual instant-add) + SendMwFriendAddReq/AskReq | ✅ |
| W4-2 | Friend reply + erase — OnMW_FRIENDREPLY_ACK (accept upserts mutual entries / reject relays code) + OnMW_FRIENDERASE_ACK (mutual demote / one-way remove) + SendMwFriendEraseReq | ✅ |
| W4-3 | Friend groups — OnMW_FRIENDGROUPMAKE/DELETE/CHANGE/NAME_ACK (per-char named buckets, TChar.friend_groups + TFriend.group) + 4 senders | ✅ |
| W4-4 | Friend list reader — OnMW_FRIENDLIST_ACK → MW_FRIENDLIST_REQ (groups + non-pending friends, per-friend level/class/connected/region resolved live) | ✅ |
| **W4-5** | Chat channel relay — OnMW_CHAT_ACK (GUILD/TACTICS/PARTY/FORCE/MAP/WORLD/SHOW/WHISPER) routed to the right audience via the guild/party/corps/peer registries + SendMwChatReq | ✅ |
| W4-6+ | Friend connection notifications (presence) + friend DB load + Soulmate | ⏸ |
| W4 | Friend + Chat + Soulmate | ⏸ |
| W5 | War + Castle + Tournament / TNMT | ⏸ |
| W6 | BR + Bow + Event + RPS + APEX / ARENA / BATTLEMODE | ⏸ |
| W7 | Item + Cash + MonthRank + CMGift + cutover hardening | ⏸ |

### W4-5 — what landed

The **chat channel relay** — a capstone that routes a message to
the right audience through every social registry ported so far.

Handler — `OnChatAck` (wID 0x903F), switching on the CHAT_GROUP:
- `GUILD` / `TACTICS` — every guild member (+ hired tactics
  members for TACTICS), via GuildRegistry.
- `PARTY` — the target party's members, via PartyRegistry.
- `FORCE` — the sender's corps: every member of every squad, via
  Party + CorpsRegistry.
- `MAP` / `WORLD` / `SHOW` — global: one `MW_CHAT_REQ` per map peer
  (char_id/key = 0), via PeerRegistry.
- `WHISPER` — a direct recipient (resolved by id, else by name),
  delivered + echoed back to the sender (the echo carries the
  recipient's name for the client's "To <name>" line), gated by
  war-country (waived when either side is in the peace country).

Each recipient is reached through `RelayToChar` (CharRegistry
lookup → its map peer). The sender's country/aid ride on every
`MW_CHAT_REQ`. Deferred: the operator-whisper sub-case ("/GM …"),
which needs the operator list + server-message table.

Sender — `SendMwChatReq` (11-field) in the new `senders_chat.cpp`.

Tests — `tests/test_chat_handlers.cpp` (5 channels, three-peer
loopback): GUILD (both members), PARTY (both members), WORLD
(every peer), WHISPER (recipient + sender echo), and the
cross-war-country whisper block (proven by a follow-up WORLD).

Build verified: cmake + ctest -R tworldsvr_asio (37/37 passed).

### W4-4 — what landed

Friend **list reader** — the reply when a client opens its friend
window.

Handler — `OnFriendListAck` (wID 0x912F): snapshots the char's
friend groups + every non-pending (`!= FT_TARGET`) friend, then
resolves each friend's `level` / `class` / `region` + the online
flag **live** from the CharRegistry (online → real values, offline
→ 0). Replies `MW_FRIENDLIST_REQ`.

This live resolution is cleaner than legacy's stored
`m_bLevel`/`m_bConnected`/`m_dwRegion` (which drift until a
presence update refreshes them) and keeps the reader self-contained
— it doesn't depend on the not-yet-ported presence-notification
flow. The soulmate slot is emitted as the "none" sentinel (DWORD 0)
until soulmate ports.

Sender — `SendMwFriendListReq` (variable-length: soulmate sentinel
+ group list + friend rows) via the new `FriendListRow` POD.

Tests — `tests/test_friend_list_handlers.cpp`: a list with one
group, an online friend (live level/class/region + connected=1),
an offline friend (zeroed + connected=0), and an `FT_TARGET`
pending entry correctly excluded.

Build verified: cmake + ctest -R tworldsvr_asio (36/36 passed).

### W4-3 — what landed

Friend **groups** — the per-char named buckets a client uses to
organise its friend list (legacy m_mapFRIENDGROUP, capped at
MAX_FRIENDGROUP=5, names ≤ MAX_GROUPNAME=20).

Model — `TChar.friend_groups` (vector of {byte id, name}); each
`TFriend.group` references one (0 = ungrouped).

Handlers (all per-char, single-lock; persistence deferred):
- `OnFriendGroupMakeAck` (0x905B): create — gates group-id≠0 +
  under cap (`kMax`), unique id + name (`kAlready`); over-long name
  is a silent drop.
- `OnFriendGroupDeleteAck` (0x905D): delete — only an *empty* group
  (no non-pending friend references it, else `kRefuse`); unknown
  group is a silent drop.
- `OnFriendGroupChangeAck` (0x905F): move a friend into a group
  (0 = ungroup); bad group / unknown friend silent-drop.
- `OnFriendGroupNameAck` (0x9061): rename — name-unique
  (`kRefuse`), group-exists (`kNotFound`).

Senders — `SendMwFriendGroupMake/Delete/Change/NameReq`.

Tests — `tests/test_friend_group_handlers.cpp` (11 checks,
single-peer): MAKE success/id-taken/group-0, CHANGE into a group,
DELETE occupied (REFUSE) → ungroup → DELETE empty (SUCCESS), and
NAME rename/name-taken/unknown-group.

Build verified: cmake + ctest -R tworldsvr_asio (35/35 passed).

### W4-2 — what landed

Friend **accept/reject + remove** — completes the add/remove core
of the friend subsystem.

Handlers
- `OnFriendReplyAck` (wID 0x9054): the invited char's answer. On
  ASK_YES both sides' entries are upserted to a connected
  `FT_FRIENDFRIEND` (each other's region copied in) and both get
  `MW_FRIENDADD_REQ` SUCCESS; on reject the inviter gets the answer
  code. Inviter-/answerer-offline branches relay `FRIEND_NOTFOUND`.
- `OnFriendEraseAck` (wID 0x9056): legacy round-trips the removal
  through the DB then runs EraseFriend; the SOCI-direct port does
  the in-memory removal inline (persistence deferred). A mutual
  (`FT_FRIENDFRIEND`) friend is *demoted* — self → `FT_TARGET`, the
  online other side → `FT_FRIEND`; a one-way (`FT_FRIEND`) friend is
  fully removed from both lists (when the other is online); a
  pending `FT_TARGET` is left as-is. Missing entry → `kNotFound`.

Lock discipline: the two affected chars' lists are read/mutated
under each char's own lock (sequential, never both held at once).

Sender — `SendMwFriendEraseReq` (4-field); the reply result reuses
the W4-1 `SendMwFriendAddReq`.

Tests — `tests/test_friend_reply_handlers.cpp` (6 scenarios,
three-peer loopback): accept (both SUCCESS + mutual entries +
region swap), reject (inviter gets the code), inviter-offline
NOTFOUND, mutual erase (demote to TARGET/FRIEND), erase-not-found,
and one-way erase (both entries removed).

Build verified: cmake + ctest -R tworldsvr_asio (34/34 passed).

### W4-1 — what landed

Opens the **friend** subsystem (the W4 social-graph phase). Unlike
the guild/party/corps cluster entities, a friend list is per-char
state, so this adds the model to `TChar` rather than a new
registry.

Model — `TChar` gains `friends` (the legacy m_mapTFRIEND list of
`TFriend{ id, name, type, connected, region, group }`) + `region`
(m_dwRegion, the last-seen zone shipped in presence updates).
`services/friend_constants.h` mirrors NetCode.h's `FRIEND_RESULT`,
`FRIEND_TYPE` (FT_FRIEND/TARGET/FRIENDFRIEND), `FRIEND_CONNECTION`,
and `MAX_FRIEND` (64).

Handler — `OnFriendAskAck` (wID 0x9052): a char requests to
befriend another by name. Gates (legacy order): target online +
same country (`kNotFound`), not already a non-pending friend
(`kAlready`), requester under MAX_FRIEND (`kMax`). Then:
- if both chars already hold a pending `FT_TARGET` for each other,
  the friendship completes immediately — both upgrade to
  `FT_FRIENDFRIEND` (connected, each other's region copied in) and
  the requester gets `MW_FRIENDADD_REQ` SUCCESS with the new
  friend's row;
- else the target's own cap is checked (`kRefuse`) and
  `MW_FRIENDASK_REQ` is forwarded to the target's map for the
  confirm dialog (→ FRIENDREPLY, W4-2).

Lock discipline: the requester's + target's friend lists are read,
then mutated, under each char's own lock (sequential, single io
thread — never both held at once).

Deferred: friend-row persistence (legacy DM_FRIENDINSERT_REQ) is
in-memory only — it lands with the friend repository slice.

Senders — `SendMwFriendAddReq` (9-field) + `SendMwFriendAskReq`
(4-field), in the new `senders/senders_friend.cpp`.

Tests — `tests/test_friend_handlers.cpp` (5 scenarios, three-peer
loopback): unknown-target NOTFOUND, a clean ask forwarding
FRIENDASK_REQ to the target, ALREADY, the mutual pending-target
instant-add (both entries upgraded + regions swapped + the SUCCESS
row), and a different-country NOTFOUND.

Build verified: cmake + ctest -R tworldsvr_asio (33/33 passed).

### W3c-7 — what landed

The **corps enemy-list family + HP sync** — six chief-to-chief
broadcasts that complete the corps subsystem. Each is an opaque
relay: the commander of a corps squad pushes a payload (the shared
enemy/target list — ENEMYLIST / ADD / DEL / MOVE-ENEMY; the unit
reorder — MOVE-UNIT; or a member-HP sync — CORPSHP) to every other
squad's chief.

Handlers — `OnCorpsEnemyListAck` / `OnAddCorpsEnemyAck` /
`OnDelCorpsEnemyAck` / `OnMoveCorpsEnemyAck` / `OnMoveCorpsUnitAck`
/ `OnCorpsHpAck` (wIDs 0x9095/9B/9D/97/99/9F). All six are one-line
wrappers over a shared `CorpsChiefRelay` (legacy
BroadcastCorps + RelayCorpsMsg): validate the sender (exists + key
+ chief of a party in a corps), then forward the inbound payload —
everything after the leading char_id + key — to each *other* squad
chief's map, with the recipient's char_id + key swapped into the
head. Non-chief / corps-less senders are dropped.

Sender — `SendMwCorpsChiefRelay` (generic: msg_id + recipient
head + opaque tail).

Tests — `tests/test_corps_enemy_handlers.cpp` (4 scenarios,
four-peer loopback): a chief's ENEMYLIST payload reaching both
other squad chiefs (tail intact, char/key swapped, own squad +
non-chief members skipped), a non-chief send dropped (proven by the
next valid relay carrying the right payload), and a CORPSHP send
mapping to the HP wID.

Build verified: cmake + ctest -R tworldsvr_asio (32/32 passed).

**The corps subsystem (W3c) is now feature-complete**: invite →
form → leave/dissolve → change-commander → squad-reshuffle →
command → enemy-list/HP. The only deferred corps item is the
per-member `m_command` cache that the W3c-2 ADDSQUAD payload stubs
(emitted as 0) and W3c-6 CORPSCMD would populate — it affects only
a late-joiner's initial command display.

### W3c-6 — what landed

Corps **command broadcast** — the general issues a movement/attack
order that fans out to the whole corps so every member's client
mirrors it.

Handler — `OnCorpsCmdAck` (wID 0x9093): validates the issuer
(exists + key match + in a party), then relays `MW_CORPSCMD_REQ`
to every member of every squad in the corps — or, when the issuer
is corps-less, just their own party's members. Silent drop on a
bad issuer (legacy parity).

Sender — `SendMwCorpsCmdReq` (10-field).

Deferred: legacy also caches the order on the issuing squad's +
the commander char's `m_command` so a late-joiner's ADDSQUAD shows
the current order. That per-member command state isn't modelled
yet (W3c-2's ADDSQUAD emits it as 0); the broadcast — the
player-facing effect — is byte-identical without it. The cache +
ADDSQUAD un-stub land together when the command model is ported.

Tests — `tests/test_corps_cmd_handlers.cpp` (2 scenarios,
three-peer loopback): a general's command reaching all three corps
members across two squads/peers (asserts the relayed cmd / target /
squad / commander / position fields), and a corps-less issuer
reaching only their own party.

Build verified: cmake + ctest -R tworldsvr_asio (31/31 passed).

### W3c-5 — what landed

Corps **squad reshuffle** (`PARTYMOVE`) — the handler that was the
original reason for opening the corps subsystem. A corps general
rebalances members across squads. It lives in `handlers_party.cpp`
because it reuses the party `LeaveParty` + `JoinPartyFanout`
machinery (W3b-2/3).

Handler — `OnPartyMoveAck` (wID 0x90A1):
- **Move mode** (empty dest name): the target leaves their party
  (`is_delete=true` → the source party dissolves if it drops below
  two) and joins party `target_party`.
- **Swap mode** (dest name set): the target and the named dest char
  trade parties; both parties must have ≥2 members (else
  `CORPS_WRONG_TARGET`), each leaves with `is_delete=false` (no
  dissolve) then cross-joins.

Gates mirror legacy: the general must exist with a matching key
(the map server pre-validates the commander authority), the target
must be in a party, you can't move to the target's own party, and
the swap size rule. Result via `MW_PARTYMOVE_REQ`
(`CORPS_SUCCESS` / `CORPS_NOT_COMMANDER` / `CORPS_WRONG_TARGET`).

Sender — `SendMwPartyMoveReq` (3-field). `handlers_party.cpp` now
includes `corps_constants.h` for the CORPS_* codes.

Tests — `tests/test_party_move_handlers.cpp` (5 scenarios,
two-peer loopback with the general isolated on peer1 so it sees
only the result): key-mismatch / no-party / own-party
`NOT_COMMANDER`, size-1-swap `WRONG_TARGET`, and a successful move
between squads (asserts the moved char's party_id, both squads'
membership, and the source party's chief succession).

Build verified: cmake + ctest -R tworldsvr_asio (30/30 passed).

### W3c-4 — what landed

Change corps commander — the corps-level analog of party
CHGPARTYCHIEF. Only the **general** (the chief of the current
commander party) may hand the commander role to another squad.

Handler — `OnChgCorpsCommanderAck` (wID 0x90A4): gates
`kNoParty` (requester party-less / corps-less) → `kNotCommander`
(not the commander party's general) → `kWrongTarget` (target is
already the commander) → `kTargetNoParty` (target not a squad of
this corps). On success moves `commander_party_id` +
`general_char_id` to the target squad (its chief), replies
`kChgCommander`, and refreshes every squad's HUD via the W3c-3
`CorpsJoinBroadcast` (CORPSJOIN + commander PARTYATTR).

Sender — `SendMwChgCorpsCommanderReq` (3-field).

Tests — `tests/test_corps_commander_handlers.cpp` (4 scenarios,
three-peer loopback): non-commander reject, current-commander
`kWrongTarget`, non-member `kTargetNoParty`, and the general's
successful reassign (reply + all-three-squad commander refresh +
registry commander/general update).

Build verified: cmake + ctest -R tworldsvr_asio (29/29 passed).

### W3c-3 — what landed

Corps **leave / dissolve** — the corps-level analog of party
PARTYDEL; completes the corps create→form→leave triangle.

Handler — `OnCorpsLeaveAck` (wID 0x9073): a party chief removes a
squad — their own (`squad_id == their party`) or, as the general,
any squad. Gates: requester is their party's chief, in a corps, the
target squad is a real party + a member of that corps, and the
authority check (own squad or general).

`NotifyCorpsLeave` (file-local self-recursive coroutine, mirrors
legacy NotifyCorpsLeave): mutual `MW_DELSQUAD_REQ` (each other
squad's members drop the leaver, the leaver's members drop each
other squad), `TCorps::RemoveParty` + the leaving party's cleared
`corps_id`, then —
- **dissolve** when the corps drops to one squad: pull the last
  squad out via a recursive `NotifyCorpsLeave` and drop the corps
  from the registry;
- **succession** when the leaver was the commander (and the corps
  survives): promote the first remaining squad (+ its chief as the
  new general) and refresh every survivor's HUD with a
  `CorpsJoinBroadcast` (CORPSJOIN + commander PARTYATTR);
- the leaver's members always get `CORPSJOIN_REQ(0, 0)` + a
  cleared PARTYATTR.

Sender — `SendMwDelSquadReq`. The CORPSJOIN/PARTYATTR refresh
reuses a new file-local `CorpsJoinBroadcast`. Lock chain unchanged
(char → party → corps, never two held at once).

Tests — `tests/test_corps_leave_handlers.cpp` (3 scenarios,
three-peer loopback on one corps): an unauthorized kick (rejected,
corps unchanged), commander succession (3→2 squads, asserts the
DELSQUAD fan-out + the survivors' new-commander CORPSJOIN/PARTYATTR
+ the registry commander/general flip), and the dissolve cascade
(2→0: both squads pulled out, corps removed).

Build verified: cmake + ctest -R tworldsvr_asio (28/28 passed).

### W3c-2 — what landed

Corps **formation** — the answer to the W3c-1 invite, the corps-
level analog of party PARTYJOIN. First handler that mutates the
CorpsRegistry.

Registry — `CorpsRegistry::GenId(parties)` allocates a free WORD id
skipping ids live in *either* the corps or party registry, matching
legacy `GenPartyID`'s shared pool (corps ids never collide with
party ids).

Handler — `OnCorpsReplyAck` (wID 0x906F): the invited chief's
answer. Relays the decline code to the inviter on a no; on
ASK_YES re-runs the gate (both party chiefs / same war-country /
non-arena / CheckCorpsJoin) then forms the corps — fresh TCorps
(commander = the inviter's party, general = inviter) when neither
side has one, else the corps-less party joins the existing corps.

`NotifyCorpsJoin` (file-local coroutine, mirrors legacy
NotifyCorpsJoin + CorpsJoin): snapshots the corps' squads, then for
each existing squad fires the pairwise `MW_ADDSQUAD_REQ` (each
existing-squad member learns the joining squad and vice versa),
commits `TCorps::AddParty` + the joining party's `corps_id`
back-link, and pushes each joining member `MW_CORPSJOIN_REQ` + a
`MW_PARTYATTR_REQ` carrying the commander party id. Squad members
are gathered through a `SnapshotSquad` helper.

The `MW_ADDSQUAD_REQ` per-member payload includes legacy
`m_command` real-time target/move state (target obj, target pos,
command, move type) that the world side doesn't model — emitted as
0 / `MOVE_NONE`; the corps-command handler (later W3c) will own it.

Lock discipline holds the char → party → corps order; no two locks
are ever held at once (snapshot-then-release).

Senders — `SendMwAddSquadReq` (squad + member list via the new
`SquadMemberInfo` POD) + `SendMwCorpsJoinReq`.

Tests — `tests/test_corps_reply_handlers.cpp` (3 scenarios,
three-peer loopback): decline relay, new-corps formation (asserts
the pairwise ADDSQUAD + CORPSJOIN + commander PARTYATTR ordering +
the registry's 2-squad/commander/general state), and a third party
joining the existing corps (grows to 3).

Build verified: cmake + ctest -R tworldsvr_asio (27/27 passed).

### W3c-1 — what landed

Opens the **corps** subsystem — the party subsystem's parent: a
corps is a set of parties (squads) under a general. Mirrors how
W3b-1 opened party (registry + back-pointer + first handler). The
`TParty.corps_id` back-link already existed since W3b-1; this adds
the registry it points into.

Infrastructure
- `services/corps_registry.{h,cpp}` — `CorpsRegistry` (16-shard,
  same actor model as Party/GuildRegistry) + `TCorps` (id,
  commander party id, general char id, ordered squad-id list +
  IsParty/AddParty/RemoveParty/Size helpers). Corps creation
  (Insert with a freshly-allocated id, sharing the party id pool)
  + commander succession land in W3c-2/3.
- `services/corps_constants.h` — `corps::` mirror of NetCode.h
  `CORPS_RESULT` + `kMaxCorpsParty` (7).
- `HandlerContext.corps`, wired in `main.cpp`.

Handler — `OnCorpsAskAck` (wID 0x906D): a party chief invites
another party's chief to ally. Validates both are party chiefs of
the same war-country, neither party is in an arena, and the legacy
`CheckCorpsJoin` gate (not both already in a corps → `kWrongTarget`;
neither corps at the `MAX_CORPS_PARTY` cap → `kMaxParty`), then
forwards `MW_CORPSASK_REQ` to the target chief's map. Failures relay
`MW_CORPSREPLY_REQ` to the inviter. No corps is created — formation
is the answer's job (CORPSREPLY, W3c-2).

Lock discipline extends the README §5 chain to char → party →
corps; never two held at once.

Senders — `SendMwCorpsAskReq` (4-field) + `SendMwCorpsReplyReq`
(4-field) in the new `senders/senders_corps.cpp`.

Tests
- `tests/test_corps_registry.cpp` (6 scenarios) — registry
  lifecycle + squad-set helpers + concurrent inserts.
- `tests/test_corps_handlers.cpp` (6 scenarios, three-peer
  loopback) — success forward + WRONG_TARGET / NO_PARTY (not a
  chief) / BUSY (arena) / MAX_PARTY (full corps) / both-in-corps
  WRONG_TARGET.

Build verified: cmake + ctest -R tworldsvr_asio (26/26 passed).

### W3b-6 — what landed

Party **round-robin loot** (PT_ORDER mode) — the last party-only
handler; the party subsystem is now complete on the player-facing
wire surface. A monster drop for a turn-based-loot party is handed
to the next member in turn-order.

Handler — `OnPartyOrderTakeItemAck` (wID 0x904A): reads the header
+ the eligible-member list (those in range of the drop) + the
dropped item, then picks the next looter via the party's turn
cursor and forwards `MW_PARTYORDERTAKEITEM_REQ` with the item to
that looter's map. A stale party id replies
`MW_ADDITEMRESULT_REQ(MIT_NOTFOUND)` to the reporting map.

The item is read with the W3a-37 cabinet codec's `ReadCabinetItem`
and re-emitted with `WriteCabinetItem` — that codec is exactly the
legacy `CreateItem` / `WrapItem` pair this handler uses, so it
round-trips byte-for-byte without a new item codec.

`TParty` order rotation (ported from legacy CTParty,
party_registry.cpp) — `GetOrderIndex` / `SetNextOrder` /
`GetNextOrder(eligible)`: the cursor honours the current member's
turn if eligible, else the first eligible member after the cursor
position, else wraps to the front. Runs under the party lock.

Senders — `SendMwPartyOrderTakeItemReq` (header + WrapItem) +
`SendMwAddItemResultReq` (generic item-pickup result).

Tests — `tests/test_party_order_handlers.cpp` (4 scenarios,
three-peer loopback): the cursor walking Alice→Bob across drops,
a single-eligible-member drop, the item round-trip through the
cabinet codec (incl. a variable magic entry), and the stale-party
MIT_NOTFOUND reply.

Build verified: cmake + ctest -R tworldsvr_asio (24/24 passed).

### W3b-5 — what landed

Party member **recall** — the recall-scroll teleport flow between
two party members (summon a member to me, or move me to a member).
Pure relay logic; no new party state. Corps-free.

Handlers
- `OnPartyMemberRecallAck` (wID 0x9105) — the initiator's request.
  If the wire `origin_name` equals the initiator's own name it's a
  summon (`TP_RECALL`): the target must be in the same party + on
  the same map. Otherwise it's a move-to (`TP_MOVETO`): the origin
  must be on the same map + same war-country. On a passing gate
  world forwards `MW_PARTYMEMBERRECALLANS_REQ` to the other party's
  map so their client confirms; on a failing gate it relays
  `MW_PARTYMEMBERRECALL_REQ(IU_TARGETBUSY)` back to the initiator
  (matching legacy: gate-pass-but-peer-offline is a silent drop,
  only gate-fail replies busy).
- `OnPartyMemberRecallAnsAck` (wID 0x9115) — the confirmation
  coming back with the destination channel/map/position. World
  re-checks the teleported char is still on the destination map
  and not inside a small meeting room (forcing `IU_TARGETBUSY`
  otherwise), then relays `MW_PARTYMEMBERRECALL_REQ` to that
  char's map.

Constants (party_constants.h) — `kTpRecall`/`kTpMoveTo`,
`kItemUseTargetBusy` (IU_TARGETBUSY), the meeting-room map range +
`IsSmallMeetingRoom`.

Senders — `SendMwPartyMemberRecallAnsReq` (6-field) +
`SendMwPartyMemberRecallReq` (12-field; failure replies zero the
trailing destination fields).

Tests — `tests/test_party_recall_handlers.cpp` (6 scenarios,
three-peer loopback): summon + move-to RECALLANS forwarding, the
non-party-member busy reject, and the RECALLANS relay (success +
map-mismatch + small-meeting-room IU_TARGETBUSY gates). Float wire
fields (position) are round-tripped.

Build verified: cmake + ctest -R tworldsvr_asio (23/23 passed).

### W3b-4 — what landed

Three small party-attribute mutations, each fanning a broadcast to
the roster. All read-mostly: no new state beyond the existing
`TParty.chief_char_id` / `obtain_type` and `TChar` combat stats.

Handlers
- `OnPartyManstatAck` (wID 0x9026) — a member's HP/MP/level
  changed on the map side. Updates the subject member's stored
  combat stats (legacy SetCharStatus — HP/MP only; level stays
  owned by LEVELUP) and re-broadcasts `MW_PARTYMANSTAT_REQ` to
  every member so their roster HUD refreshes.
- `OnChgPartyChiefAck` (wID 0x90A2) — chief hands leadership to
  another member. Gates (legacy order): requester online + key
  match → target online (`kNoUser`) → both in a party
  (`kNoParty`) → same party (`kNoUser`) → not self (`kAlready`)
  → requester is chief (`kNotChief`). On success sets the new
  chief, replies `kChgChief` to the requester, and re-broadcasts
  `MW_PARTYATTR_REQ` to every member with the new chief.
- `OnChgPartyTypeAck` (wID 0x90CC) — chief changes the loot-
  distribution mode. Gates: requester in a party + is chief
  (`kNotChief` reply otherwise). On success updates
  `TParty.obtain_type` + broadcasts `MW_CHGPARTYTYPE_REQ`
  (result=0) to every member.

Senders — `SendMwPartyManstatReq` (9-field), `SendMwChgPartyChiefReq`
(3-field), `SendMwChgPartyTypeReq` (4-field). The chief-change
roster refresh reuses the W3b-2/3 `SendAttr` (PARTYATTR) helper.

Lock discipline unchanged: member-set + chief/obtain snapshotted
under the party lock, released before any char lock or send.

Deferred (corps not ported): the MANSTAT corps-general relay, the
CHGCHIEF `ChgSquadChief` / `ReportEnemyList` / general-reassign
and the `RW_PARTYCHGCHIEF_ACK` relay-DB echo — all corps_id-gated
(always 0) or relay-channel-only.

Tests — `tests/test_party_attr_handlers.cpp` (6 scenarios,
three-peer loopback on one party): MANSTAT broadcast + stored-stat
update; CHGTYPE non-chief reject (obtain unchanged) then chief
broadcast (obtain updated); CHGCHIEF Alice→Bob (reply + roster
PARTYATTR with the new chief + registry chief flip), the
now-ex-chief's CHGCHIEF rejected `kNotChief`, and a self-target
`kAlready`.

Build verified: cmake + ctest -R tworldsvr_asio (22/22 passed).

### W3b-3 — what landed

Party **leave / kick** — completes the party lifecycle's
create→join→leave triangle. The map server already validated chief
authority before sending, so the handler is symmetric for both the
voluntary-leave (bKick=0, char removes self) and chief-kick
(bKick=1) cases.

Handler — `OnPartyDelAck` (wID 0x9024): finds the party, confirms
the named char is a member, then runs `LeaveParty`.

`LeaveParty` (file-local self-recursive coroutine, mirrors legacy
`CTWorldSvrModule::LeaveParty`):
- **Survives** when ≥2 members remain (party size > 2, or the
  recursive `is_delete=false` tail). If the leaver was chief,
  succession promotes the first other member (legacy
  `GetNextChief`) and every member gets a `MW_PARTYATTR_REQ`
  refresh with the new chief before the DEL fan-out.
- **Disbands** when the leave would drop it below two: the chief
  is zeroed and, after the leaver is removed, the last remaining
  member is pulled out via a recursive `LeaveParty(..., is_delete
  =false)` and the party is dropped from the registry.
- Fan-out: every member is sent `MW_PARTYDEL_REQ` — the leaver
  with chief_id/party_id = 0 (their client clears the HUD), the
  survivors with the surviving chief + party id. The leaver's
  `TChar.party_id` back-pointer is cleared and it gets a final
  cleared `MW_PARTYATTR_REQ`.

Lock discipline unchanged: party member-set + meta snapshotted
under the party lock and released before any char lock; a char
lock is never held across the party lock (README §5).

Deferred (corps not ported): `NotifyDelCorpsUnit` /
`NotifyCorpsLeave` / `ChgSquadChief` / `ReportEnemyList` and the
`RW_PARTYDEL_ACK` / `RW_PARTYCHGCHIEF_ACK` relay-DB echoes — all
corps_id-gated (always 0) or relay-channel-only.

Sender — `SendMwPartyDelReq` (7-field).

Tests — `tests/test_party_del_handlers.cpp` (3 scenarios,
four-peer loopback on one progressively-shrinking party):
chief-leave with succession to the next member (asserts the
new-chief PARTYATTR to all + the DEL fan-out + the leaver's
cleared PARTYATTR), a non-chief kick (flag propagation + survival),
and the disband cascade (2→0: both members pulled out, party
removed from the registry, both back-pointers cleared).

Build verified: cmake + ctest -R tworldsvr_asio (21/21 passed).

### W3b-2 — what landed

Party **formation** — the invitee's answer to the W3b-1 invite
dialog. Where W3b-1 only relayed the dialog, this is the first
handler that actually mutates the PartyRegistry.

Registry — `PartyRegistry::GenId()` allocates a free WORD party id
(rolling cursor over [1, 65535], skipping live ids; 0 reserved as
the TChar "no party" sentinel). Legacy pre-seeds a recycled-id
queue (`m_qGenPartyID`); the scan is the modern equivalent.

Handler — `OnPartyJoinAck` (wID 0x9022). Re-runs the gate the
invite passed (inviter+invitee online → `kNoReqUser`/`kNoUser`;
answer == ASK_YES else relay the decline code, which aligns
ASK_NO/ASK_BUSY ↔ PARTY_DENY/PARTY_BUSY; invitee still unpartied
→ `kNoUser`; same war-country → `kCountry`), stashes the
invitee's combat stats (SetCharStatus), then:
- inviter already in a party → invitee joins it, chief-gated
  (`kNotChief` / `kFull` / arena `kBusy`);
- otherwise a fresh `TParty` is created (GenId + Insert) with the
  inviter as chief and both chars joined.

`JoinParty` fan-out (file-local coroutine) mirrors legacy
`CTWorldSvrModule::JoinParty` + `AddPartyMember`: snapshot the
party's member ids + meta under the party lock, release it, then
for each online member fire the pairwise `MW_PARTYJOIN_REQ` (the
joiner learns the member, the member learns the joiner), commit
`TParty::AddMember` + set the joiner's `TChar.party_id`, and push
the joiner a `MW_PARTYATTR_REQ` HUD refresh. Member describe-
fields (name / level / HP-MP / race-sex-face-hair / class / guild
name via GuildRegistry) are snapshotted through a `SnapshotMember`
helper. Lock discipline: a char lock is never held across the
party lock (snapshot-then-release), per README §5.

Deferred (corps not ported): the `RW_PARTYADD_ACK` relay-DB
persistence echo, `MW_CORPSJOIN_REQ`, and `NotifyAddCorpsUnit` —
all guarded by `corps_id != 0`, which is always 0 until the corps
subsystem lands. The BOW/BR cross-server JoinParty guards are
W6-territory and skipped.

Senders — `SendMwPartyJoinReq` (19-field, via a `PartyMemberInfo`
POD) + `SendMwPartyAttrReq` (6-field).

Tests
- `tests/test_party_registry.cpp` scenario 7 — GenId sequential /
  occupied-slot-skip / non-zero.
- `tests/test_party_join_handlers.cpp` (4 scenarios, three-peer
  loopback) — decline relay, inviter-offline relay, new-party
  formation (asserts the pairwise JOIN_REQ describe-fields + both
  PARTYATTR pushes + registry 2-member state + chief + both
  back-pointers + the stashed stats), and a third char joining the
  existing party (grows to 3, member-order-deterministic fan-out).

Build verified: cmake + ctest -R tworldsvr_asio (20/20 passed).

### W3b-1 — what landed

Opens the **party** vertical — the W3a guild subsystem's sibling
and the first half of the W3b phase. Mirrors how W3a-1 opened the
guild work: the cluster-wide registry + back-pointer + the first
handler. The guild + tactics + cabinet vertical is functionally
complete on the player-facing surface, so this broadens scope to
the next subsystem rather than chasing the schema-uncertain DB
persistence backlog.

Infrastructure
- `services/party_registry.{h,cpp}` — `PartyRegistry` (16-shard,
  same partitioning + per-entry-mutex actor model as
  GuildRegistry; keyed by the WORD party id) + `TParty` (the
  subset of legacy `CTParty` the world side touches: id, loot
  `obtain_type`, optional `corps_id`, chief + loot-turn-order
  back-pointers, arena flag, ordered member-id list, with
  IsChief / IsMember / Size / AddMember / RemoveMember helpers).
  Party **creation** (Insert with a freshly-allocated id) +
  chief succession land with the W3b-2 PARTYJOIN / W3b-3 PARTYDEL
  flows; W3b-1 only needs the read path for the invite gate.
- `services/party_constants.h` — `party::` mirror of the
  NetCode.h `TPARTY_RESULT` (kAgree..kCountry) + `PARTY_TYPE`
  loot modes (kObtain*) + kMaxPartyMember=7 + the `WarCountry`
  helper (legacy GetWarCountry: aid_country unless neutral).
- `TChar` gains `party_id` + `party_waiter` (legacy m_pParty /
  m_bPartyWaiter) + `max_hp/hp/max_mp/mp` (legacy SetCharStatus
  stash for the later JOIN/MANSTAT broadcasts).
- `HandlerContext.parties`.

Handler — `OnPartyAddAck` (wID 0x901A, the chief/solo player
inviting another player). Runs the legacy SSHandler.cpp:2486
gate in order: requester online (else drop) → not inviting self
→ target online (`kNoUser`) → target not mid-invite
(`kWaiters`) → target unpartied (`kAlready`) → same war-country
(`kCountry`) → if the requester is already in a party they must
be its chief (`kNotChief`) of a non-full (`kFull`), non-arena
(silent drop) party. Failures relay back to the requester's map
(the originating peer); success stashes the requester's combat
stats and forwards `PARTY_AGREE` (keyed by the inviter id) to
the target's map peer so their client pops the join dialog, then
flags the target `party_waiter`. No party is created — formation
happens when the target answers (PARTYJOIN, W3b-2).

Sender — `SendMwPartyAddReq` (7-field: char_id, key,
request_name, target_name, obtain_type, result, request_char_id).

Tests
- `tests/test_party_registry.cpp` (6 scenarios) — registry
  lifecycle + the TParty member-set helpers, incl. concurrent
  inserts.
- `tests/test_party_handlers.cpp` (7 scenarios) — drives
  MW_PARTYADD_ACK over a two-peer loopback session, asserting
  each gate (NOUSER / AGREE-on-target-peer / WAITERS / ALREADY /
  COUNTRY / NOTCHIEF / FULL), the target waiter flip, and the
  inviter stat stash.

Build verified: cmake + ctest -R tworldsvr_asio (19/19 passed).

### W3a-38 — what landed

The two remaining map→world player-action entry points whose
DB-side fan-in already shipped earlier. A guild-handler audit
(legacy SSHandler `On*` set vs. the Asio dispatch) found these
were the only player-facing guild gaps left — everything else
unported is W5 castle/skill (`CT_CASTLEGUILDCHG_REQ`,
`MW_GUILDSKILLACTION_REQ`, `MW_UPDATEGUILDCOOLDOWN_ACK`),
DB-routed echoes (`DM_GUILDESTABLISH_REQ`, `DM_GUILDLOAD_REQ`,
the `DM_GUILDTACTICS*` / `DM_TACTICSPOINT_REQ` persistence
fan-ins), or timer-service messages (`SM_GUILDDISORGANIZATION_REQ`).

Handlers
- `OnGuildDisorganizationAck` (wID 0x902E) — player requests
  their guild disband / cancel. 3-field wire `{char_id, key,
  disorg}`; resolves guild_id from the char (vs. the W3a-4b
  `DM_GUILDDISORGANIZATION_REQ` 4-field fan-in). No-ops when the
  flag already matches (legacy gate), else flips
  `disorg`/`disorg_time` under the guild lock, persists via
  `repo->SetDisorg`, replies `MW_GUILDDISORGANIZATION_REQ`.
- `OnGuildPointRewardAck` (wID 0x9127) — a chief grants
  PvP-useable points to a member by name. Gates: sender must be
  the chief (`kGprNoMember` otherwise), guild useable ≥ point
  (`kGprNeedPoint`), target is a member (`kGprNoMember`). On
  success deducts the points, prepends a `point_log` entry
  (newest-first, capped at `kPointLogMaxEntries` — same as the
  W3a-29 fix), persists via `repo->LogPointReward`, replies
  `MW_GUILDPOINTREWARD_REQ` with `kGprSuccess`, and relays a
  `MW_GAINPVPPOINT_REQ` gain toast to the recipient's map peer.

Sender — `SendMwGuildPointRewardReq` (8-field result).
Constants — `kGprSuccess/NeedPoint/NoMember` (0/1/2, inferred
from the client switch order at TClient/CSHandler.cpp:16482).

Tests
- `tests/test_guild_mut_handlers.cpp` scenarios 64-65: disband
  flip 0→1 (verify reply + flag/time) + repeat-is-no-op (framer
  survives), and a chief point-reward grant (verify the result
  reply, the recipient gain-toast relay, the deducted bank +
  the newest point_log entry).

Build verified: cmake + ctest -R tworldsvr_asio (17/17 passed).

The guild + tactics + cabinet vertical is now functionally
complete on the player-facing wire surface; the remaining work
is cross-cutting DB persistence + the W5 castle/skill family.

Deferred to W3a-39+
- DB persistence for the in-memory-only state (tactics members
  + wanted board, cabinet items, alliance/enemy, guild money)
- W5 castle / guild-skill handlers

### W3a-37 — what landed

The guild cabinet (storage vault) item codec — the last big
guild-area subsystem. Replaces the W3a-26 empty-list LIST stub
with real item storage + the PUTIN / TAKEOUT operations.

State model
- `TGuildCabinetItem` on `TGuild` (subset of the legacy CTItem
  per-instance fields): slot_id (cabinet key, legacy
  m_dwItemID) + id (m_dlID instance id) + the 16 codec scalars
  (item ids, level, gem, mogg, count, glevel, dura max/cur,
  refine, end_time, grade_effect, 4 ext values) + a
  variable-length `magic` enchant list ({magic_id, value}).
- `TGuild.cabinet_items` + `PutInCabinet` (stack onto an
  existing slot or append), `TakeOutCabinet` (decrement, erase
  at zero), `FindCabinetItem`. Mirror legacy
  PutInItem/TakeOutItem/FindCabinetItem.

Codec — `services/guild_cabinet_codec.{h,cpp}`
- `ReadCabinetItem(Reader&, item)` / `WriteCabinetItem(body,
  item)`: symmetric with the legacy CreateItem (TWorldSvr.cpp:
  5498) / WrapItem (TServer.cpp:16). 17 fixed fields + the
  variable magic list. ReadCabinetItem fails cleanly on a short
  read or an overrunning magic_count; it leaves slot_id for the
  caller to set from the preceding DWORD.

Handlers
- `OnGuildCabinetListAck` (0x90D1) — upgraded from the W3a-26
  stub to snapshot `cabinet_items` under the guild lock and
  emit real items.
- `OnGuildCabinetPutinAck` (0x90D3) — reads `{char_id, key,
  slot_id, <WrapItem>}`, stores the item (stacking on a repeat
  slot), chases a CABINETLIST refresh (legacy parity).
- `OnGuildCabinetTakeoutAck` (0x90D5) — reads `{char_id, key,
  slot_id, count}`, decrements/erases, chases a refresh.

Sender — `SendMwGuildCabinetListReq` gained an `items` param;
emits `{char_id, key, max_cabinet, count, [slot_id + WrapItem]…}`.

Tests
- `tests/test_guild_mut_handlers.cpp` scenarios 61-63: PUTIN an
  item with 2 magic entries (verify the full round-trip codec
  in the LIST refresh), PUTIN the same slot again (verify the
  count stacks 3+4=7), TAKEOUT 7 (verify the slot erases at
  zero).

Build verified: cmake + ctest -R tworldsvr_asio (17/17 passed).

Notes
- The dead W3a-5 `SendMwGuildCabinetMaxReq` (declared but never
  called) still nominally emits on MW_GUILDCABINETPUTIN_ACK,
  which is now an inbound handler wID. Harmless (it's never
  invoked); a future cleanup can drop it.
- In-memory only — the cabinet DM fan-in
  (DM_GUILDCABINETPUTIN/TAKEOUT/ROLLBACK) + DB persistence
  (TGUILDCABINETTABLE) are W3a-38.

Deferred to W3a-38+
- Cabinet DM fan-in + rollback + DB persistence
- DB persistence for the tactics subsystem
- Guild-money repo flush (W3a-33 note)

### W3a-36 — what landed

Periodic prune of tactics-member contracts whose fixed term
(`TTacticsMember.end_time`) has elapsed — the in-process
replacement for the legacy EXPIRED_GT timer path
(`CTGuild::AddTactics` registered an `OnEventExpired` entry that
the timer service fired at end_time). Mirrors the W3a-19
wanted-board sweep.

New service — `services/guild_tactics_sweep.{h,cpp}`
- `SweepExpiredTactics(GuildRegistry&, CharRegistry*)`: samples
  now, walks every guild (via `GuildRegistry::SnapshotIds` →
  `Find`), erases tactics members with `end_time <= now` under
  each guild's lock collecting the freed char_ids, then clears
  each freed char's `tactics_guild_id` back-pointer. `end_time
  == 0` is treated as "no expiry". Logs a one-line summary when
  anything expired.
- Expiry is an end-of-term, **not** a refund event — the
  member served their contract, so no money/point is returned
  (matching legacy: `DelTactics` only refunds on the self-leave
  path, W3a-34).

Wiring
- `main.cpp` spins a second `RegistryRefresher` (alongside the
  W3a-19 wanted sweeper) with the tactics sweep registered as a
  coroutine hook. Stops cleanly on shutdown.
- `[guild] tactics_sweep_period_sec = N` config knob (default
  300s; 0 disables — same shape as `wanted_sweep_period_sec`).

Tests
- New `tests/test_tactics_sweep.cpp` (4 scenarios): empty
  roster no-op, mixed expired/live (only expired removed +
  back-pointer cleared, live kept), `end_time == 0` never
  expires, null CharRegistry still prunes the roster.

Build verified: cmake + ctest -R tworldsvr_asio (17/17 passed,
+1 new dedicated sweep test).

Deferred to W3a-37+
- DB persistence for tactics members + wanted board (and the
  guild-money repo flush noted in W3a-33)
- Cabinet PUTIN / TAKEOUT + item codec

### W3a-35 — what landed

Fifth and final core slice of the tactics subsystem — the
chief-initiated hire dialog (vs. the W3a-32/33 player-initiated
volunteer path). Mirrors legacy `OnMW_GUILDTACTICSINVITE_ACK` +
`OnMW_GUILDTACTICSANSWER_ACK`.

Handlers
- `OnGuildTacticsInviteAck` (wID 0x9136): chief offers a
  contract to a player by name. Validates char/key/guild + the
  hire gates (target found → else `kNotFound`; country match;
  guild useable points ≥ offer; guild money ≥ offer; target not
  a tactics member of a *different* guild → `kHaveGuild`; roster
  not full → `kMemberFull`; target not a full member of the
  chief's guild → `kSameGuildTactics`). On success the offer is
  relayed to the target's map peer as a dialog (carrying the
  chief's name so the ANSWER can find the origin); on failure
  the chief gets an ANSWER reply with the failure code.
- `OnGuildTacticsAnswerAck` (wID 0x9138): target accepts/declines.
  Decline → `kJoinDeny`. Accept → re-validates the same gates,
  then (with renewal support: an existing contract for the same
  guild is dropped no-refund first, preserving its end_time
  base) charges the guild's useable points + money, appends the
  `TTacticsMember`, wires the target's `tactics_guild_id`,
  persists the PvP banks. The outcome is echoed to BOTH the
  target's peer and the chief's peer.

Senders — `SendMwGuildTacticsInviteReq` (9-field dialog) +
`SendMwGuildTacticsAnswerReq` (10-field outcome). Shared
`FindMapPeer(ctx, msi)` helper for the relay lookups.

Scope notes
- Same in-memory-money / persisted-points split as W3a-33.
- guild_levels is optional (null → no roster cap, W3a-5
  relaxed-gate convention).

Tests
- `tests/test_guild_mut_handlers.cpp` scenarios 59-60: chief
  invites char 700 by name (verify the relayed dialog carries
  the chief's name + offer), char 700 accepts (verify the dual
  ANSWER echo + the tactics member lands + points charged).

Build verified: cmake + ctest -R tworldsvr_asio (16/16 passed).

The tactics subsystem is now feature-complete across W3a-31..35:
wanted board, volunteer flow, reply (volunteer accept), kickout,
list, and invite/answer (chief hire).

Deferred to W3a-36+
- Tactics term-expiry sweep (contracts ending at `end_time` —
  parallels the W3a-19 wanted sweep + the legacy EXPIRED_GT
  timer)
- DB persistence for tactics members / wanted board + a
  guild-money repo flush
- Cabinet PUTIN / TAKEOUT + item codec

### W3a-34 — what landed

Fourth slice of the tactics subsystem — kicking a tactics
member + listing the roster. Mirrors legacy
`OnMW_GUILDTACTICSKICKOUT_ACK` + `OnMW_GUILDTACTICSLIST_ACK`.

State model
- `TGuild::RemoveTactics(char_id, refund)` (out-of-line in
  guild_registry.cpp so it can use the CalcMoney/SplitMoney
  helpers). When `refund` is true the member's up-front
  PvP-points + money are returned to the guild's useable banks
  (legacy DelTactics bKick==0 GainPvPoint + GainMoney); a
  chief-kick forfeits them.

Handlers
- `OnGuildTacticsKickoutAck` (wID 0x9100). Wire `{char_id, key,
  target}`. `char != target` is a chief-kick (resolves the
  guild from the requester's `guild_id`, no refund); `char ==
  target` is a self-leave (resolves the guild from the target's
  `tactics_guild_id`, refunds). Removes the member, clears the
  target's `tactics_guild_id`, persists the refunded PvP banks
  on self-leave. Chief-kick replies KICKOUT + a roster refresh;
  self-leave is silent (legacy parity).
- `OnGuildTacticsListAck` (wID 0x913B). Wire `{char_id, key}`.
  Resolves the "current guild" with tactics-priority (legacy
  GetCurGuild: tactics guild if a member, else full guild) and
  sends the roster.

Sender — `SendMwGuildTacticsKickoutReq` (5-field result) +
`SendMwGuildTacticsListReq` (12-field-per-member variable list;
region/castle/camp emitted as 0 — not modelled / W5 castle war).

Tests
- `tests/test_guild_mut_handlers.cpp` scenarios 57-58: list the
  roster (verify char 700's contract fields from scenario 55's
  hire), then chief-kick char 700 (verify member gone +
  back-pointer cleared + no refund since chief-kick forfeits).

Build verified: cmake + ctest -R tworldsvr_asio (16/16 passed).

Deferred to W3a-35+
- Tactics INVITE / ANSWER (chief invites a player by name, who
  accepts/declines → hire via the same promotion path)
- Tactics term-expiry sweep (contracts ending at `end_time` —
  parallels the W3a-19 wanted sweep + the EXPIRED_GT timer)
- DB persistence for tactics members + a guild-money repo flush
- Cabinet PUTIN / TAKEOUT + item codec

### W3a-33 — what landed

Third slice of the tactics subsystem — the chief's accept/reject
reply, which on accept actually hires the applicant as a tactics
member. Mirrors legacy `OnMW_GUILDTACTICSREPLY_ACK` +
`ApplyGuildTacticsApp`.

State model
- `TTacticsMember` struct on `TGuild` (legacy TTACTICSMEMBER):
  id (char_id), name, level, klass, reward_point, reward_money,
  gain_point, day, end_time. Stored in
  `TGuild.tactics_members` (legacy m_mapTTactics) with a
  `FindTactics(char_id)` helper.
- `TChar.tactics_guild_id` back-pointer (legacy m_pTactics) —
  a char can be a full member of one guild AND a tactics
  mercenary of another, so this is separate from `guild_id`.
- Money helper in `guild_constants.h`: `CalcMoney(g,s,c)` /
  `SplitMoney(total, …)` with the 1 gold = 1000 silver = 1e6
  cooper ratio (inferred from the audit-log packing at
  TMapSvrAsio/legacy_src/UdpSocket.cpp:1799).

Handler — `OnGuildTacticsReplyAck` (wID 0x90FD)
- Wire: `{char_id (chief), key, target_char_id, reply}`.
- Reject (`reply=0`): `DelApp(target)` + chief volunteer-list
  refresh.
- Accept (`reply=1`): looks up the applicant's stored app
  (reward fields), then runs the 7 legacy hire gates:
  already-a-tactics-member-anywhere → `kHaveGuild`, target is
  vice-chief+ of their own guild → `kNoDuty`, already a tactics
  member of this guild → `kAlreadyMember`, full member of this
  guild → `kSameGuildTactics`, guild PvP-useable < reward point
  → `kNoPoint`, guild tactics roster full (level-chart
  `tactics_count`) → `kMemberFull`, guild money < reward money
  → `kNoMoney`. On success: charges the guild's useable points
  + money, appends the `TTacticsMember` with `end_time = now +
  day*kDaySec`, wires the target's `tactics_guild_id`, clears
  the app, fires the dual `TACTICSREPLY_REQ` (new member's map
  peer + chief), refreshes the chief's volunteer list.

Sender — `SendMwGuildTacticsReplyReq` (10-field hire result).

Registry — `GuildTacticsWantedRegistry::FindApp(char_id)` added
(full applicant record lookup for the accept path).

Scope notes
- PvP-point deduction is persisted (`repo->UpdatePvPoints`).
  Money deduction stays in-memory only — `IncrementContribution`
  is an additive delta (can't flush an absolute balance) and
  `UpdateGuildFull` needs every guild scalar; a dedicated
  `UpdateGuildMoney` repo method is a documented follow-up. A
  restart re-warms money from the canonical DB row.
- Tactics-member roster cap uses the level chart's
  `tactics_count`; a null level row (dev/test path) means no
  cap, matching the W3a-5 relaxed-gate convention.

Tests
- `tests/test_guild_mut_handlers.cpp` scenarios 55-56: seed the
  guild's banks, post a hire entry, have a guild-less char
  apply, then accept (verify the tactics member lands with the
  contract + the guild's points/gold were charged) and reject
  (verify no member + the app dropped).

Build verified: cmake + ctest -R tworldsvr_asio (16/16 passed).

Deferred to W3a-34+
- Tactics INVITE / ANSWER (chief-initiated hire by name) +
  KICKOUT + LIST flows
- DB persistence for tactics members (TGUILDTACTICSTABLE) +
  a dedicated guild-money repo flush
- The EXPIRED_GT term-expiry sweep (tactics contracts ending
  at end_time) — parallels the W3a-19 wanted sweep
- Cabinet PUTIN / TAKEOUT + item codec

### W3a-32 — what landed

Second slice of the tactics subsystem — the volunteer
(applicant) flow. Players apply to a tactics-wanted posting,
chiefs browse applicants. Parallels the W3a-12 guild volunteer
flow. The accept/reject REPLY handler is deferred to W3a-33
(accept needs the tactics-member promotion model that hasn't
ported yet).

Registry applicant API (`GuildTacticsWantedRegistry`)
- `TGuildTacticsWantedApp` struct: char_id, wanted_id,
  wanted_guild_id, region, level, klass, name + the posting's
  reward fields (day/point/gold/silver/cooper, copied at apply
  time so the chief's list shows what each applicant was
  promised).
- `AddApp(app, country, applicant_guild_id)` runs the 7 legacy
  gates: already-applied-same-guild → `kSame`,
  already-applied-elsewhere → `kAlreadyApply`, no-such-posting
  → `kFail`, country mismatch → `kFail`, posting expired →
  `kWantedEnd`, level out of `[min,max]` → `kMismatchLevel`,
  applicant in the posting's own guild → `kSameGuildTactics`,
  else `kSuccess`. One pending application per char (keyed by
  char_id).
- `DelApp(char_id)`, `SnapshotAppsFor(guild_id)` (all applicants
  across a guild's postings — the chief view),
  `FindAppGuildByChar(char_id)` (drives the wanted board's
  `already_applied` flag).

Handlers (`handlers_guild.cpp`)
- `OnGuildTacticsVolunteeringAck` (wID 0x90F7) — apply. Snapshots
  the applicant's char fields, calls AddApp, replies the 3-byte
  result, and on success chases a wanted-board refresh (so the
  `already_applied` flag flips).
- `OnGuildTacticsVolunteeringDelAck` (wID 0x90F9) — cancel +
  wanted-board refresh.
- `OnGuildTacticsVolunteerListAck` (wID 0x90FB) — chief browses
  applicants across all of their guild's postings. Empty list
  when guildless.

Senders — `SendMwGuildTacticsVolunteering/Del/VolunteerListReq`
- 3-byte result replies + the 10-field-per-applicant variable
  list.

W3a-31 follow-through
- `BuildTacticsWantedRows` now takes an `applied_guild_id` and
  sets `already_applied=1` on the matching posting. All three
  W3a-31 wanted-board handlers now thread
  `FindAppGuildByChar(char_id)` through (previously hardcoded 0).

Scope notes
- In-memory only (DB persistence deferred with the rest of the
  tactics subsystem). Applicant `region` left 0 — our TChar
  doesn't model the map-zone id the legacy refreshes here, and
  no ported handler needs it.

Tests
- `tests/test_guild_mut_handlers.cpp` scenarios 52-54: re-post a
  tactics-wanted entry, have a guild-less char apply (verify
  `kSuccess` + board refresh), chief lists applicants (verify
  the 10-field row carries the promised reward fields), then
  the applicant cancels (verify the chief's list empties).

Build verified: cmake + ctest -R tworldsvr_asio (16/16 passed).

Deferred to W3a-33+
- Tactics REPLY (accept → tactics-member promotion / reject) —
  the accept path needs the TTacticsMember model (TGuild's
  m_mapTTactics equivalent) which hasn't ported
- Tactics INVITE / ANSWER / KICKOUT / LIST flows
- Cabinet PUTIN / TAKEOUT + item codec
- DB persistence for the tactics wanted board + applicants

### W3a-31 — what landed

First slice of the tactics subsystem — the "we need tactics
members" recruitment board. Structurally parallels the W3a-11
guild wanted board but with two key differences: a guild may
hold MULTIPLE postings (vs. one), and each posting carries a
globally-unique id + reward fields (point / gold / silver /
cooper / day).

New registry — `services/guild_tactics_wanted_registry.{h,cpp}`
- `TGuildTacticsWanted` struct: id, guild_id, country, name,
  title, text, day, min/max level, point, gold, silver, cooper,
  end_time.
- Storage: `unordered_map<guild_id, vector<entry>>` + a reverse
  index `id -> guild_id` for O(1) `Find(id)`. `shared_mutex`.
- `NextId()` — monotonic global counter (legacy
  `++m_dwTacticsIndex`).
- `AddOrUpdate(entry)` — upsert; in-place update when the id
  matches an existing posting, otherwise insert subject to the
  `kMaxTacticsWantedPerGuild` cap (returns `kSuccess` /
  `kMaxWanted`).
- `Remove(guild_id, id)`, `Find(id)`,
  `SnapshotByCountry(country)`, `Size()`.

Handlers (`handlers_guild.cpp`)
- `OnGuildTacticsWantedAddAck` (wID 0x90F1) — chief posts an
  entry. `id=0` auto-allocates via `NextId()`; non-zero updates
  in place. Validates char/key/guild/disorg gates. Replies the
  3-byte ADD result + a LIST refresh on success.
- `OnGuildTacticsWantedDelAck` (wID 0x90F3) — removes by
  `(guild_id, id)`. ADD/DEL both chase success with a LIST
  refresh (legacy `NotifyGuildTacticsWantedList`).
- `OnGuildTacticsWantedListAck` (wID 0x90F5) — country-filtered
  snapshot.

Senders — `SendMwGuildTacticsWantedAdd/Del/ListReq`
- 3-byte result replies + the variable-length list (14 fields
  per row, DWORD count prefix). `already_applied` always 0
  until the tactics-volunteer subsystem ports.

Constant
- `kMaxTacticsWantedPerGuild = 5` (legacy MAX_TACTICSWANTED;
  exact value not in available headers — documented assumption).

Scope notes
- In-memory only. DB persistence (legacy
  TGUILDTACTICSWANTEDTABLE) deferred, mirroring the W3a-25
  alliance/enemy approach — the gameplay surface works, the
  postings just don't survive a restart yet.
- `end_time = now + kGuildWantedPeriodSec` (legacy uses
  GUILDWANTED_PERIOD for the tactics board too; `day` is stored
  for display but doesn't shorten the expiry on this path).

Tests
- `tests/test_guild_mut_handlers.cpp` fixture gains a
  `GuildTacticsWantedRegistry`; scenarios 49-51 cover
  add→list→del round-trips: post auto-id entry + verify the
  14-field LIST row, standalone LIST, then DEL + verify the
  registry empties.

Build verified: cmake + ctest -R tworldsvr_asio (16/16 passed).

Deferred to W3a-32+
- Tactics volunteer / invite / answer / reply / kickout flows
  (the rest of the ~17-handler tactics subsystem)
- DB persistence for tactics wanted (TGUILDTACTICSWANTEDTABLE)
- Cabinet PUTIN / TAKEOUT handlers + item codec
- War-bonus award for B-country tactics-guilds

### W3a-30 — what landed

Two related durability/correctness items: boot-time loading of
the PvP point reward log + a latent `Clone` fidelity bug fix.

Boot-time point_log load
- `SociGuildRepository::LoadAll` gains a third pass after the
  guild + member passes: `SELECT dwGuildID, szName, dwPoint,
  dlDate FROM TGUILDPVPOINTREWARDTABLE ORDER BY dlDate DESC`,
  dispatched into each guild's `point_log` (newest-first,
  capped at `kPointLogMaxEntries` = 50 per guild — matching the
  legacy `CTBLGuildPvPointReward` SELECT-TOP-50 + the
  `CTGuild::PointLog` in-memory bound). After this, the W3a-27
  log reader returns rewards granted before the latest process
  restart, not just ones fanned in during the current run.
- `dlDate` (DB timestamp) is read as `std::tm` and converted to
  Unix epoch seconds via `std::mktime`.
- Wrapped in its own try/catch — a missing
  TGUILDPVPOINTREWARDTABLE (the schema validator already warns
  at boot) just leaves `point_log` empty rather than aborting
  the whole `LoadAll`.

Clone fidelity fix
- `FakeGuildRepository`'s internal `Clone` (used by `LoadAll` /
  `FindById` to hand out deep copies) was field-by-field and
  silently dropped everything added after W3a-1:
  `pvp_month_point`, `rank_total` / `rank_month`, `stat_*`,
  `alliance_ids`, `enemy_ids`, and `point_log`. Any test that
  round-tripped a guild through the fake lost those fields.
  Now copies all of them. (Production
  `SociGuildRepository` builds fresh TGuild from DB rows so it
  was never affected — this is a test-fidelity fix.)

Tests
- `tests/test_fake_guild_repository.cpp` gains a Gamma guild
  seeded with `pvp_month_point` + alliance/enemy lists +
  a point_log entry; verifies `FindById` round-trips all of
  them.

Build verified: cmake + ctest -R tworldsvr_asio (16/16 passed).

Deferred to W3a-31+
- Boot-time vRecord load from TGUILDPVPRECORDTABLE (the
  point_log half landed here; the per-member per-day vRecord
  half is more involved — needs a per-member dispatch + a
  CalcWeekRecord pass after load)
- Cabinet PUTIN / TAKEOUT handlers + DM fan-in + item codec
- Tactics subsystem (~17 handlers)
- War-bonus award for B-country tactics-guilds
- SOCI persistence for alliance / enemy

### W3a-29 — what landed

Per-event PvP-point delta fan-in from the map server, plus a
follow-up fix to W3a-27's point_log ordering / trim. The map
reports a gain or use against either a character or a guild.

Handler — `OnGainPvPointAck` (wID `MW_GAINPVPPOINT_ACK` = 0x9121)
- Wire: `{owner_type, owner_id, point, event, type, gain, name,
  klass, level}`.
- `TOWNER_CHAR` (=0): pure relay. Looks up the char's
  `main_server_id`, finds the matching map peer in PeerRegistry,
  forwards `MW_GAINPVPPOINT_REQ` verbatim so the map shows the
  gain/loss toast. Drops if char missing or main map offline.
- `TOWNER_GUILD` (=1): applies the delta to the guild's PvP
  banks under the guild lock, mirroring legacy
  `GainPvPoint`/`UsePvPoint` (TGuild.cpp:564/585):
  - gain + `PVP_TOTAL` → `total += point` AND `month += point`
  - gain + `PVP_USEABLE` → `useable += point`
  - use + `PVP_TOTAL` → `total = max(0, total - point)`
  - use + `PVP_USEABLE` → `useable = max(0, useable - point)`
  - **use never decrements month** (legacy quirk preserved)
  - then persists all three banks via `repo->UpdatePvPoints`
    (legacy fires `SendDM_GUILDPVPOINT_REQ` from inside
    Gain/UsePvPoint).

New sender — `SendMwGainPvPointReq`
- 8-field relay packet matching SSSender.cpp:3117.

Constants (`services/guild_constants.h`)
- `kPvPOwnerChar = 0` / `kPvPOwnerGuild = 1` (NetCode.h
  TOWNER_*).
- `kPvPMaskTotal = 1` / `kPvPMaskUseable = 2` (NetCode.h
  PVP_TOTAL / PVP_USEABLE).
- `kPointLogMaxEntries = 50`.

Follow-up fix — W3a-27 point_log ordering
- The W3a-14 `OnGuildPointRewardReq` mirror now inserts
  newest-first (`point_log.insert(begin(), …)`) and `pop_back`s
  once size exceeds `kPointLogMaxEntries` — exactly matching
  legacy `CTGuild::PointLog` (TGuild.cpp:603). Previously it
  `push_back`ed unbounded in oldest-first order, which both
  grew without limit and emitted the log in the wrong order to
  the W3a-27 reader. Closes the "TOP-50 trim semantics"
  deferred item from W3a-27.

Tests
- `tests/test_guild_mut_handlers.cpp` scenarios 47-48:
  - 47: guild gain with `PVP_TOTAL | PVP_USEABLE` — verifies
    all three banks bumped by the delta + the
    `UpdatePvPoints` persistence call landed with the new
    totals.
  - 48: guild use with `PVP_USEABLE` only — verifies useable
    shrinks while total + month stay put (the never-decrement-
    month quirk).

Build verified: cmake + ctest -R tworldsvr_asio (16/16 passed).

Deferred to W3a-30+
- Boot-time vRecord / point_log load from DB so the in-memory
  history survives process restart
- Cabinet PUTIN / TAKEOUT handlers + DM fan-in + item codec
- Tactics subsystem (~17 handlers)
- War-bonus award for B-country tactics-guilds
- SOCI persistence for alliance / enemy

### W3a-28 — what landed

Replaces W3a-24's "plain accumulator" approach to weekrecord
with the proper per-day history + week-trim semantics matching
legacy `CTGuild::CalcWeekRecord` (TGuild.cpp:615) exactly.
weekrecord now stays a true rolling 7-day aggregate instead of
growing unbounded for the process lifetime.

State model
- `TPvPDayRecord` POD added to `services/guild_registry.h` —
  same payload as `TPvPRecord` (kill/die/points[8]) plus a
  `day_index: i64` tag (Unix-epoch-seconds / 86400, legacy
  `dwDate = m_timeCurrent / DAY_ONE`).
- `TGuildMember.vRecord` field of `vector<TPvPDayRecord>` —
  one row per day, appended (or merged into today's row) on
  every war-result fan-in. Stays bounded at ~7 entries by
  `CalcWeekRecord`'s inline trim.

New helper — `services/pvp_aggregate.{h,cpp}`
- `CalcWeekRecord(member, today_day_index)` walks `vRecord`
  once: drops rows where `day_index + kPvPRecordWindowDays <=
  today_day_index` (legacy `+7 <= today` predicate), sums the
  kept rows into the zeroed `weekrecord`. O(N) per call;
  vRecord size N is bounded at ~7 so it's trivial.
- `CalcWeekRecord(member)` overload samples
  `std::time(nullptr) / kDaySec` for today's index.
- Two new constants in `services/guild_constants.h`:
  `kDaySec = 86 400` and `kPvPRecordWindowDays = 7`.

W3a-24 handler update — `OnLocalRecordAck`
- Instead of accumulating deltas directly into `weekrecord`,
  finds (or appends) today's `vRecord` row keyed by
  `day_index`, increments kill/die/points there.
- Then calls `CalcWeekRecord(m, today)` to re-derive
  `weekrecord` from the (now-updated) `vRecord` AND trim
  stale rows in one pass.
- Net effect: `weekrecord` reads now reflect true 7-day
  rolling totals, identical to legacy semantics.

Tests
- New dedicated `tests/test_pvp_aggregate.cpp` (6 scenarios):
  empty / single fresh / mixed fresh+stale / all-stale /
  boundary entry (`day_index + 7 == today` dropped per legacy)
  / idempotent re-call.
- `tests/test_guild_mut_handlers.cpp` scenario 43 updated to
  seed `vRecord` instead of writing `weekrecord` directly (so
  the next CalcWeekRecord from scenario 44's fan-in doesn't
  wipe the seed). Scenario 44 expectations unchanged — the
  arithmetic ends at the same per-member values.

Build verified: cmake + ctest -R tworldsvr_asio (16/16 passed,
+1 new dedicated pvp_aggregate test).

Deferred to W3a-29+
- Boot-time vRecord load from TGUILDPVPRECORDTABLE so the
  in-memory history survives process restart (the W3a-21
  audit-log writes have a paired read path now but it
  doesn't auto-warm-up on boot yet)
- Cabinet PUTIN / TAKEOUT handlers + DM fan-in + item codec
- Tactics subsystem (~17 handlers)
- War-bonus award for B-country tactics-guilds
- SOCI persistence for alliance / enemy + point_log

### W3a-27 — what landed

Read-side counterpart to W3a-14's PvP point reward writer.
Player opens the guild "point reward log" UI panel; map server
forwards via `MW_GUILDPOINTLOG_ACK`; world replies with the
guild's full rolling point-reward audit log. Pairs with the
W3a-14 writer (`OnGuildPointRewardReq`) which now also appends
to `TGuild.point_log` in addition to persisting to
`TGUILDPVPOINTREWARDTABLE` — so the reader returns live data
after at least one reward fan-in.

State model
- `TPointRewardEntry` POD added to `services/guild_registry.h`
  — `{date_unix: i64, recipient_name: string, point: u32}`.
  Mirrors legacy `m_vPointReward` entries.
- `TGuild.point_log` field of `vector<TPointRewardEntry>` —
  unbounded per-process growth; the legacy `m_vPointReward`
  doesn't trim either, and the read-side `CTBLGuildPvPointReward`
  SELECT-TOP-50 trims on read in the legacy DB layer. We
  don't currently trim in-memory or wire — boot-time load-
  from-DB wiring lives in a future batch.

W3a-14 handler update
- `OnGuildPointRewardReq` now also appends a `TPointRewardEntry`
  to `TGuild.point_log` under the guild lock, alongside the
  existing total/useable defensive mirror.
- `date_unix = std::time(nullptr)` (the DB SP stamps its own
  `dlDate` column with `CURRENT_TIMESTAMP`; we sample world's
  wall-clock so the reader's reply carries an accurate
  date even before a DB roundtrip).

Sender — `SendMwGuildPointLogReq`
- New `GuildPointLogEntry` POD on the wire-friendly side.
- Loop emits per entry: `{INT64 date_unix, STRING recipient_name,
  DWORD point}`.

Handler — `OnGuildPointLogAck` (wID `MW_GUILDPOINTLOG_ACK` =
0x9125)
- Wire: `{char_id, key}`.
- Validates char + key + non-zero `guild_id` + guild present in
  registry (legacy parity SSHandler.cpp:10298).
- Snapshots `guild->point_log` under the guild lock, fires the
  reply.
- Tactics-branch short-circuit deferred along with the rest of
  the tactics subsystem.

Tests
- `tests/test_guild_mut_handlers.cpp` scenario 46: builds on
  scenario 22 (which fired a `DM_GUILDPOINTREWARD_REQ` for
  "Bravo2" recipient + 250 points). Verifies the log reader
  reply contains an entry with the expected recipient name +
  point value + non-zero date.

Build verified: cmake + ctest -R tworldsvr_asio (15/15 passed).

Deferred to W3a-28+
- Boot-time `point_log` load from `TGUILDPVPOINTREWARDTABLE` so
  the in-memory log survives process restart and the reader
  returns rows from before the latest boot
- TOP-50 trim semantics (legacy
  `CTBLGuildPvPointReward` trims on read)
- Cabinet PUTIN / TAKEOUT handlers + DM fan-in + item codec
- Tactics subsystem (~17 handlers)
- Per-day vRecord history (replaces W3a-24's plain accumulator)
- War-bonus award for B-country tactics-guilds
- SOCI persistence for alliance / enemy

### W3a-26 — what landed

Wire-compat stub for the guild-storage UI open path. Player
opens the cabinet panel; map server forwards the open request
to world via `MW_GUILDCABINETLIST_ACK`; world replies with the
guild's `max_cabinet` cap + an empty item list. Clients see a
"no items" view, which is semantically truthful for our port
since nothing else populates the cabinet anyway.

Why a stub
- Legacy `SendMW_GUILDCABINETLIST_REQ` (SSSender.cpp:1087)
  iterates `pGuild->m_mapTCabinet` and emits `WrapItem` per
  entry — that's the full TItem codec (18 scalar fields +
  variable-length magic array, see `TWorldSvr.cpp:5498`). The
  codec is substantial and gates PUTIN/TAKEOUT as well, so all
  three cabinet handlers wait for one future W3a-* batch that
  ports TItem + the wrap/unwrap codec together.
- Until then, sending `count=0` on every reply is wire-compat
  and matches what the world actually knows (nothing populates
  the cabinet without the PUTIN handler). No data loss, no
  client UI hang.

Sender — `SendMwGuildCabinetListReq`
- Wire: `{char_id, key, max_cabinet, item_count=0}`. Always
  emits zero items.

Handler — `OnGuildCabinetListAck` (wID
`MW_GUILDCABINETLIST_ACK` = 0x90D1)
- Wire: `{char_id, key}`.
- Validates char + key + non-zero `guild_id` + guild present in
  registry. Drops silently on any mismatch (legacy parity
  SSHandler.cpp:3953).
- Snapshots `guild->max_cabinet` under the guild lock, fires
  the empty-list reply.
- Tactics-branch short-circuit (`pTactics != null`) deferred
  along with the rest of the tactics subsystem.

Tests
- `tests/test_guild_mut_handlers.cpp` scenario 45: chief opens
  cabinet, verifies reply carries `max_cabinet=10` (set by
  scenario 1's `GuildLoadBody`) and `count=0`.

Build verified: cmake + ctest -R tworldsvr_asio (15/15 passed).

Deferred to W3a-27+
- Cabinet PUTIN / TAKEOUT handlers + DM fan-in + item codec
  (would replace the empty-list stub with a real cabinet view)
- Tactics subsystem (~17 handlers): TACTICSADD/DEL/ANSWER/
  INVITE/KICKOUT/LIST/REPLY + tactics-side WANTED/VOLUNTEER
  + the tactics-branch short-circuit in OnGuildCabinetListAck
- Per-day vRecord history (replaces W3a-24's plain accumulator
  with proper week-trim semantics)
- War-bonus award for B-country tactics-guilds
- SOCI persistence for alliance / enemy (W3a-25's deferred
  schema-migration follow-up)

### W3a-25 — what landed

Completes the W3a-22 story. The full-row guild update handler
previously parsed the alliance + enemy DWORD ID lists for
wire-compat then dropped them; W3a-25 promotes them to proper
in-memory state on `TGuild` so future war-system code can read
guild relationships without waiting for a registry reload.

State model
- `TGuild` gains two `std::vector<std::uint32_t>` fields:
  `alliance_ids` and `enemy_ids`. Both default-construct empty.
  The legacy `TGUILDTABLE.szAllience` / `.szEnemy` columns
  are comma-separated DWORD strings (and yes, "szAllience" is
  the legacy spelling); we don't model the persistence path yet
  (deferred to a W5+ schema migration alongside the rest of the
  war system) so the in-memory state is rebuilt from
  `OnGuildUpdateReq` calls and lasts until process restart.

Repo signature change
- `IGuildRepository::UpdateGuildFull` gains two
  `const std::vector<std::uint32_t>&` parameters at the tail
  (alliance, enemy). Only the W3a-22 handler calls this method,
  so no other call sites needed updating.
- Fake records the lists in test-only members
  (`LastAllianceIds()` / `LastEnemyIds()` accessors) and also
  mirrors them onto the in-memory `TGuild` so subsequent
  `FindById` returns see them.
- SOCI logs a warning when the lists are non-empty and skips
  the persistence (the legacy CSV columns aren't in our
  portable schema yet).

Handler update — W3a-22 `OnGuildUpdateReq`
- Wire parse for the alliance / enemy lists now collects into
  `std::vector<std::uint32_t>` instead of draining + dropping.
- In-memory mirror under `guild->lock` copies the vectors onto
  `TGuild.alliance_ids` / `.enemy_ids` alongside the existing
  scalar fields.
- Repo call passes the lists through; log line counts allies +
  enemies instead of saying "dropped".

Tests
- `tests/test_guild_mut_handlers.cpp` scenario 42 extended:
  same packet (2 allies, 1 enemy) now verifies the
  `TGuild.alliance_ids` / `.enemy_ids` vectors landed AND the
  fake repo's `LastAllianceIds` / `LastEnemyIds` accessors
  return the same lists.

Build verified: cmake + ctest -R tworldsvr_asio (15/15 passed).

Deferred to W3a-26+
- SOCI persistence for alliance / enemy (`szAllience` /
  `szEnemy` CSV column round-trip OR a new relational join
  table — either way needs a schema migration). Currently the
  in-memory state is lost on process restart until a fresh
  `DM_GUILDUPDATE_REQ` arrives.
- Tactics subsystem (~17 handlers): TACTICSADD/DEL/ANSWER/
  INVITE/KICKOUT/LIST/REPLY + tactics-side WANTED/VOLUNTEER
- Cabinet item subsystem (CABINETLIST/PUTIN/TAKEOUT + item
  codec)
- Per-day vRecord history (replaces W3a-24's plain
  accumulator with proper week-trim semantics)
- War-bonus award for B-country tactics-guilds (W3a-24's
  win_guild_id + guild_point fields)

### W3a-24 — what landed

The missing link between W3a-21 (audit-log writes) and W3a-23
(reader). Map server flushes a batch of per-period PvP outcomes
(one or more members per guild, optionally several guilds per
batch); world accumulates the kill/die/points deltas into each
matched `TGuildMember.weekrecord` so the reader returns live
data on the next call.

Wire format (no reply):
```
DWORD win_guild_id, DWORD guild_point, WORD guild_count,
  [guild_count times]:
    DWORD guild_id, WORD record_count,
      [record_count times]:
        DWORD char_id, WORD kill_count, WORD die_count,
        DWORD points[kPvPEventCount=8]
```

Simplifications vs legacy (SSHandler.cpp:10139)
- Tactics-member branch is skipped — the legacy second-chance
  path for char→tactics→tactics->parent-guild lookup waits for
  the tactics subsystem to port.
- `win_guild_id` + `guild_point` (war-bonus award path for
  winning B-country tactics-guilds) read and logged but not
  acted on. Also tactics-dependent.
- No per-day `vRecord` history. `weekrecord` is a plain
  accumulator until the per-day store + `CalcWeekRecord`
  7-day-trim ports. Production may want a periodic weekly
  clear (analogous to the W3a-19 wanted-board sweep) but it
  doesn't ship today — operators can issue a registry-wide
  reset out-of-band if it matters before per-day vRecord
  lands.

Handler — `OnLocalRecordAck` (wID `MW_LOCALRECORD_ACK` = 0x9123)
- Reads header, then nested loop per guild → per record.
- For each record: short-body protection on header + point
  array; on missing guild OR missing member silently drops
  the row (legacy parity); on match accumulates under the
  guild lock.
- Logs `applied` + `dropped` counts per call.

Tests
- `tests/test_guild_mut_handlers.cpp` scenario 44: builds on
  scenario 43's pre-populated chief weekrecord, fires a fan-in
  with `kill+=2, die+=1, points[0]+=50`, verifies in-memory
  values accumulated (9/4/{550, 300, 100}), then round-trips
  through W3a-23 reader to confirm the accumulated state lands
  on the wire.

Build verified: cmake + ctest -R tworldsvr_asio (15/15 passed).

Deferred to W3a-25+
- Tactics subsystem (~17 handlers): TACTICSADD/DEL/ANSWER/
  INVITE/KICKOUT/LIST/REPLY + tactics-side WANTED/VOLUNTEER
  + the tactics second-chance branches that W3a-22, W3a-23,
  and W3a-24 all skip
- Cabinet item subsystem (CABINETLIST/PUTIN/TAKEOUT + item
  codec)
- Per-day vRecord history (production weekly-trim semantics
  for weekrecord)
- War-bonus award for B-country tactics-guilds (W3a-24's
  win_guild_id + guild_point fields)
- Alliance / enemy relationship modelling (TGuild gains two
  vector<uint32_t> fields)

### W3a-23 — what landed

Read-side counterpart to W3a-21's audit log. Player opens the
guild PvP-statistics panel; the map server forwards the open
request to world via `MW_GUILDPVPRECORD_ACK`; world replies
with a per-member roll-up of the rolling weekly kill / die /
points record.

State model
- `TPvPRecord` POD added to `services/guild_registry.h` —
  `{ kill_count: u16, die_count: u16, points: array<u32, 8> }`.
  Mirrors legacy `TENTRYRECORD`. Storage uses all 8 buckets for
  parity with W3a-21's audit row; the wire only emits the first
  6 (PVPE_KILL_H..PVPE_WIN-1).
- `TGuildMember.weekrecord` field of that type — zero-init on
  construction. Legacy `CalcWeekRecord` sums the last 7 days of
  per-day `vRecord` entries to populate this; the per-day fan-in
  path (war-result handler at `SSHandler.cpp:10155`) hasn't
  ported yet, so for now `weekrecord` stays zeros until
  something explicitly updates it. Reader is still wire-compat
  — clients just see empty record tables.

Sender — `SendMwGuildPvPRecordReq`
- New `GuildPvPRecordRow` POD: 6-bucket point array (slice of
  storage to match wire).
- Loop emits per member: `{char_id, weekrecord.kill_count,
  weekrecord.die_count, weekrecord.points[0..5]}` then a
  per-member "last record" slot that's currently always zeros
  (the legacy slot would carry the latest per-day vRecord row
  when its `dwDate >= dwRecentRecordDate`; deferred along with
  the per-day fan-in).

Handler — `OnGuildPvPRecordAck` (wID `MW_GUILDPVPRECORD_ACK` =
0x9129)
- Wire: `{char_id, key}`.
- Validates char + key + non-zero `guild_id` + guild present in
  registry. Drops silently on any mismatch (legacy parity:
  `SSHandler.cpp:10391`).
- Snapshots `guild->members` under the guild lock, builds the
  6-bucket-sliced row list, fires the reply.
- Legacy SSHandler.cpp:10391 ALSO short-circuits when the char
  has tactics-only membership; we drop the tactics branch
  (deferred until the tactics subsystem ports — same as W3a-22).

Tests
- `tests/test_guild_mut_handlers.cpp` scenario 43: pre-populates
  Bob's (char 200) weekrecord with non-zero kill/die/points,
  fires the request, reads back the reply, verifies the chief's
  row carries the populated values + every "last record" slot is
  all zeros.

Build verified: cmake + ctest -R tworldsvr_asio (15/15 passed).

Deferred to W3a-24+
- Per-day vRecord fan-in (`SSHandler.cpp:10155` war-result
  handler) — would actually populate `weekrecord` via
  `CalcWeekRecord` so reads see live data
- Tactics subsystem (~17 handlers): TACTICSADD/DEL/ANSWER/
  INVITE/KICKOUT/LIST/REPLY + tactics-side WANTED/VOLUNTEER
  + the tactics-branch short-circuit in OnGuildPvPRecordAck
- Cabinet item subsystem (CABINETLIST/PUTIN/TAKEOUT + item
  codec)
- Alliance / enemy relationship modelling (TGuild gains two
  vector<uint32_t> fields; the W3a-22 handler's
  drained-and-dropped lists become a real write)

### W3a-22 — what landed

Full-row guild update from the admin / bulk-load path. Mirrors
legacy `CSPGuildUpdate` (SSHandler.cpp:2979 + DBAccess.h:1601):
the wire packet carries the 8 scalar TGUILDTABLE columns +
variable-length alliance / enemy DWORD ID lists. We persist the
scalars + defensively mirror them into the registry; the lists
are parsed (so the framer length counter agrees with the packet
size) then dropped with a log note — TGuild doesn't yet model
inter-guild alliance / enemy relationships (deferred to the
W5+ war system).

Wire format (no reply):
```
DWORD dwID, BYTE bFame, BYTE bGPoint, BYTE bLevel, BYTE bStatus,
DWORD dwChief, DWORD dwExp, DWORD dwGI, DWORD dwTime,
BYTE allyCount, DWORD allies[allyCount],
BYTE enemyCount, DWORD enemies[enemyCount]
```

Legacy quirk: `bFame` is BYTE here but DWORD in
`OnDM_GUILDLOAD_ACK` — admin-path updates can only set fame in
0-255 range. We honor the wire as-is.

Repo — new `IGuildRepository::UpdateGuildFull`
- Signature takes the 8 scalars (post-truncation). Fake records a
  `Call::kUpdateGuildFull` entry with chief_id in the char_id
  slot and fame/gpoint/level/status/time in the a/b/c/d/e
  payload (gi + exp dropped from Call record; SOCI persists
  all 8 columns).
- SOCI impl: single `UPDATE TGUILDTABLE SET dwFame=:f, bGPoint=:gp,
  bLevel=:l, bStatus=:s, dwChief=:c, dwGI=:gi, dwExp=:e,
  dwTime=:t WHERE dwID=:g`.

Handler — `OnGuildUpdateReq` (wID `DM_GUILDUPDATE_REQ` = 0x589B)
- Reads scalar block, drains both variable-length lists with
  short-body protection per row.
- Mirrors the 8 scalar fields into the registry entry under the
  guild's lock.
- Queues `repo->UpdateGuildFull` via `CoOffloadVoidIf`.
- Logs ally / enemy counts so an operator can spot when a
  legacy admin tool tries to push them.

Tests
- `tests/test_guild_mut_handlers.cpp` scenario 42: sends a
  packet with 2 allies + 1 enemy + non-zero scalars; verifies
  the in-memory fields landed AND the repo recorded the call.

Build verified: cmake + ctest -R tworldsvr_asio (15/15 passed).

Deferred to W3a-23+
- MW_GUILDPVPRECORD_ACK read-side handler — needs TGuildMember
  weekrecord state-model expansion
- Tactics subsystem (~17 handlers)
- Cabinet item subsystem (CABINETLIST/PUTIN/TAKEOUT + item
  codec)
- Alliance / enemy relationship modelling (TGuild gains two
  vector<uint32_t> fields + repo + persistence; the W3a-22
  handler's drained-and-dropped lists become a real write)

### W3a-21 — what landed

Pure DB-write fan-in for batched PvP record persistence. Map
servers accumulate per-member PvP outcomes (kills / deaths +
per-event-bucket points) between flushes; periodically they
shoot the batch at TWorld in one packet, which loops + persists
each row.

Wire format (SSHandler.cpp:10456) variable-length, no reply:
```
DWORD guild_id, DWORD member_id, WORD row_count,
  [row_count times]:
    DWORD date, WORD kill_count, WORD die_count,
    DWORD points[kPvPEventCount]   // 8 DWORDs
```

Repo — new `IGuildRepository::LogPvPRecord`
- Signature: `(guild_id, member_id, date, kill_count, die_count,
  std::array<uint32_t, 8> points) → bool`. Per-row to match the
  legacy SP `TSaveGuildPvPRecord`.
- Fake impl: records a `Call::kLogPvPRecord` entry. Layout:
  `guild_id, char_id=member_id, a=date, b=kill_count, c=die_count,
  d=points[0], e=points[1]` — the other 6 points are dropped from
  the Call record (tests verify dispatch + per-row routing only;
  the SOCI impl persists all 8).
- SOCI impl: single INSERT into `TGUILDPVPRECORDTABLE` with all
  13 columns (guild_id, char_id, date, wKillCount, wDieCount,
  dwPoint_1..dwPoint_8). Schema validator gets a new optional
  warning for the table.

Constants
- `services/guild_constants.h` gains `kPvPEventCount = 8` —
  matches legacy PVPE_COUNT and the dwPoint_1..dwPoint_8
  schema columns.

Handler — `OnPvPRecordReq` (wID `DM_PVPRECORD_REQ` = 0x5917)
- Reads header `{guild_id, member_id, count}`.
- Loops count times, parsing `{date, kill_count, die_count,
  points[8]}` per row.
- Queues `repo->LogPvPRecord` per row via `CoOffloadVoidIf`.
- No in-memory mirror: the weekrecord aggregate that the matching
  `MW_GUILDPVPRECORD_ACK` reader would consume lives in
  TGuildMember state not yet ported. Audit-log only.
- Short-body protection: bails on truncated header / row / point
  array (with a log line indicating which row failed).

Tests
- `tests/test_guild_mut_handlers.cpp` scenario 41: sends a
  2-row batch and verifies both `LogPvPRecord` calls landed
  with the right per-row date/kill/die/points[0,1] fields.

Build verified: cmake + ctest -R tworldsvr_asio (15/15 passed).

Deferred to W3a-22+
- MW_GUILDPVPRECORD_ACK read-side handler — needs TGuildMember
  weekrecord state-model expansion
- Tactics subsystem (~17 handlers): TACTICSADD/DEL/ANSWER/
  INVITE/KICKOUT/LIST/REPLY + tactics-side WANTED/VOLUNTEER
- Cabinet item subsystem (CABINETLIST/PUTIN/TAKEOUT + item
  codec)
- `OnDM_GUILDUPDATE_REQ` (variable-length alliance/enemy CSV
  columns)

### W3a-20 — what landed

Three vestigial-echo handlers that accept legacy BATCH-server
broadcast confirmations and drop them at info-level. Wire-compat
plumbing for hybrid deployments where the legacy DB+BATCH
processes are still running alongside this TWorldSvrAsio.

Context
- Legacy TWorldSvr runs in a 3-process cluster: TWorld + DB +
  BATCH. Every guild-mutating `DM_*_REQ` that TWorld sends to
  DB triggers a `DM_*_ACK` that BATCH fans back to every world
  shard so they can refresh in-memory state.
- Our SOCI-direct port collapses each REQ+ACK pair into a
  single coroutine — W3a-4b for DISORGANIZATION, W3a-10 for
  EXTINCTION, W3a-18 for ESTABLISH — so the ACK side is
  redundant in single-shard deployments.
- A hybrid deployment (legacy BATCH still alive) would
  otherwise spam the log with `unknown wID=0x589E/0x58A0/0x58CE`
  warnings on every guild mutation. These stubs convert those
  to deliberate info-level "vestigial echo" logs.

Handlers (all in `handlers/handlers_guild.cpp` after the W3a-18
block)
- `OnGuildEstablishAckEcho` (wID `DM_GUILDESTABLISH_ACK` =
  0x589E). Wire: `{bRet, guild_id, name, time_es, char_id, key}`.
- `OnGuildDisorganizationAckEcho` (wID
  `DM_GUILDDISORGANIZATION_ACK` = 0x58A0). Wire:
  `{char_id, key, guild_id, disorg, disorg_time}`.
- `OnGuildExtinctionAckEcho` (wID `DM_GUILDEXTINCTION_ACK` =
  0x58CE). Wire: `{guild_id, result}`.

Each handler is ~15 LOC: read wire fields → log a one-liner with
"vestigial echo for X (W3a-N already handled the Y
synchronously)" → `co_return`. No state mutation, no reply.

Tests
- `tests/test_guild_mut_handlers.cpp` scenarios 38-40 drive each
  echo through dispatch then send a follow-up
  `RW_ENTERCHAR_REQ` to verify the framer survived (handler
  didn't crash, dispatch loop kept running).

Build verified: cmake + ctest -R tworldsvr_asio (15/15 passed).

Deferred to W3a-21+
- Tactics subsystem (~17 handlers): TACTICSADD/DEL/ANSWER/
  INVITE/KICKOUT/LIST/REPLY + tactics-side WANTED/VOLUNTEER
- PvP record listing (`CTBLGuildPvPointReward` TOP 50 +
  per-member weekrecord/vRecord state-model expansion)
- Cabinet item subsystem (CABINETLIST/PUTIN/TAKEOUT + DM
  fan-in + item codec)
- `OnDM_GUILDUPDATE_REQ` (variable-length alliance/enemy CSV
  columns — non-trivial wire codec)

### W3a-19 — what landed

Closes the W3a-11 TODO `// scheduler: SM_EVENTEXPIRED_ACK fan-out
for entries whose end_time + DAY_ONE elapsed`. Until now the
LIST handler returned every entry in the registry and let the
client filter on `end_time`; a stale entry with an expired
end_time would still leak into responses and (more importantly)
linger in the registry forever, taking memory + blocking new
posts from the same guild via the AddOrUpdate one-per-guild
rule.

Component split
- `GuildWantedRegistry::PruneExpired(now_unix)` — the surgical
  in-memory prune. Walks `m_entries` under unique_lock,
  removes anything where `end_time < now_unix`, drops the
  pruned entries' applicant reverse-index pointers from
  `m_app_by_char` (so freed char_ids can re-apply to a
  different wanted without tripping the `kAlreadyApply`
  gate). Returns the list of removed guild_ids.
- `services/guild_wanted_sweep.{h,cpp}` — `SweepExpiredWanted`
  coroutine. Samples `std::time(nullptr)`, calls
  `PruneExpired`, queues one `repo->DeleteWanted` per pruned
  id via `CoOffloadVoidIf`, logs a one-line summary.
  `nullptr`-tolerant on both `repo` and `db_pool` (defensive;
  production wires both).
- `main.cpp` wiring — when `cfg.wanted_sweep_period_sec != 0`
  spins a `fourstory::ops::RegistryRefresher` with the sweep
  registered as a coroutine hook. Stops cleanly on shutdown.

Config
- `[guild] wanted_sweep_period_sec = N` in the TOML config.
  Default 300s (5 min) — tighter than the 14-day entry
  lifetime so a stale entry never lingers more than one sweep
  tick past expiry. `0` disables (test-only).

Tests
- `tests/test_guild_wanted_sweep.cpp` — new dedicated unit
  test (5 scenarios):
  - PruneExpired only removes entries past `end_time`
  - PruneExpired drops pruned entries' applicant reverse
    index — a freed char can re-apply to a surviving wanted
  - SweepExpiredWanted persists one `DeleteWanted` per pruned
    id
  - SweepExpiredWanted with null repo still does the
    in-memory prune
  - No-op when nothing's expired (no repo calls)

Build verified: cmake + ctest -R tworldsvr_asio (15/15 passed,
+1 new dedicated sweep test).

Deferred to W3a-20+
- Tactics subsystem (~17 handlers): TACTICSADD/DEL/ANSWER/
  INVITE/KICKOUT/LIST/REPLY + tactics-side WANTED/VOLUNTEER
- PvP record listing (`CTBLGuildPvPointReward` TOP 50)
- Cabinet item codec
- `OnDM_GUILDUPDATE_REQ` (variable-length alliance/enemy CSV
  columns — non-trivial wire codec)
- Vestigial DM_*_ACK no-op handlers
  (`OnDM_GUILDESTABLISH_ACK`,
  `OnDM_GUILDDISORGANIZATION_ACK`,
  `OnDM_GUILDEXTINCTION_ACK`)

### W3a-18 — what landed

The "create new guild" gameplay flow — the handler player
clients fire when a chief-eligible char opens the guild-create
UI. Fills the last major gap in the guild lifecycle (load /
establish / disorganize / extinction now all have handlers).

Legacy splits this across 4 packets because the DB lives in a
separate process: `map → MW_GUILDESTABLISH_ACK → world →
DM_GUILDESTABLISH_REQ → DB → DM_GUILDESTABLISH_ACK → world →
MW_GUILDESTABLISH_REQ → map`. Our SOCI-direct architecture
collapses it into a single coroutine — validate, persist,
build registry state, reply. (The corresponding
`OnDM_GUILDESTABLISH_*` legacy handlers are vestigial in our
arch and stay deferred.)

Handler — `OnGuildEstablishAck` (wID `MW_GUILDESTABLISH_ACK`)
- Wire: `{char_id, key, guild_name}`.
- Gates (in order):
  - char missing / key mismatch → silent drop (legacy parity)
  - char already in a guild → `kHaveGuild` reply with empty
    meta + `bEstablish=1`
  - name empty or > `kGuildMaxNameLen` (50 bytes) → `kFail`
  - `repo->CreateGuild` returns nullopt (dup name or DB
    failure) → `kAlreadyGuildName`
  - registry insert race → `kEstablishErr`
- On success: builds `TGuild` with `level=1`, country copied
  from chief's TChar, chief added as the first
  `TGuildMember` with `kDutyChief`. `TChar.guild_id`
  back-pointer wired. `repo->AddMember` queued for the chief
  membership row. Replies `kSuccess` + new guild_id +
  `bEstablish=1`.

Repo — new `IGuildRepository::CreateGuild`
- Signature: `(name, chief_id, country, establish_time_unix)
  → optional<uint32_t>`. Returns the freshly-assigned guild_id
  on success, nullopt on dup-name / other failure.
- Fake impl: scans m_guilds for an existing matching name
  (nullopt if found), otherwise picks `max(id) + 1` and
  inserts a fresh TGuild. Records a `Call::kCreateGuild`
  entry with `guild_id = new_id, a = chief_id, b = country`.
- SOCI impl: single transaction. SELECT COUNT on szName for
  the dup check, COALESCE(MAX(dwID), 0) + 1 for the next id
  (portable — production schemas with IDENTITY get the same
  effective result), then INSERT. Falls back to nullopt on
  any SQL exception.

Constants
- `services/guild_constants.h` gains `kGuildMaxNameLen = 50`
  (matches legacy MAX_NAME for the ANSI build the original
  server runs).

Tests
- `tests/test_guild_mut_handlers.cpp` scenarios 35-37:
  - 35: fresh char (Foxtrot, char 600) creates "FoxtrotGuild"
    end-to-end → registry has the new guild + chief member +
    back-pointer; repo records both CreateGuild and AddMember
    calls
  - 36: char already in a guild (Bob, char 200, chief of
    guild 8) tries to create another → `kHaveGuild` reply
  - 37: char Golf (700) tries to create with the same name
    "FoxtrotGuild" → `kAlreadyGuildName` reply; char stays
    guild-less

Build verified: cmake + ctest -R tworldsvr_asio (14/14 passed).

Deferred to W3a-19+
- Tactics subsystem (~17 handlers): TACTICSADD/DEL/ANSWER/
  INVITE/KICKOUT/LIST/REPLY + tactics-side WANTED/VOLUNTEER
- PvP record listing (`CTBLGuildPvPointReward` TOP 50)
- Cabinet item codec
- Scheduler-driven wanted-entry expiry sweep
- `OnDM_GUILDUPDATE_REQ` (variable-length alliance/enemy CSV
  columns — non-trivial wire codec)
- Vestigial DM_*_ACK no-op handlers
  (`OnDM_GUILDESTABLISH_ACK`,
  `OnDM_GUILDDISORGANIZATION_ACK`,
  `OnDM_GUILDEXTINCTION_ACK`) — accept and drop for wire
  compatibility with a hybrid legacy DB server, no real work
  needed since our SOCI-direct arch doesn't have a separate
  DB process to ACK from

### W3a-17 — what landed

Two more DB-side fan-in handlers completing the membership-
lifecycle pair that W3a-4c opened with `OnDM_GUILDMEMBERADD_REQ`.
The cluster topology had `MEMBERADD` going DB→World but no
counterpart for LEAVE / KICKOUT — only the player-action
`OnGuildLeaveAck` / `OnGuildKickoutAck` paths existed. W3a-17
closes that gap.

- `OnDM_GUILDLEAVE_REQ` (wID=0x58CC) → `repo->RemoveMember`.
  Wire carries 4 fields: `{guild_id, char_id, leave_kind,
  leave_time}`. Defensive in-memory cleanup via
  `ScrubMembershipInMemory` (see below). `leave_kind` and
  `leave_time` are logged for audit but not persisted
  separately — the modern repo has no leave-log table; the
  legacy SP `TGuildLeave` may write one on production
  schemas (deferred).
- `OnDM_GUILDKICKOUT_REQ` (wID=0x58CF) → `repo->RemoveMember`.
  Wire carries 2 fields: `{guild_id, char_id}`. Same
  defensive cleanup. Legacy `CSPGuildKickout` is also a
  thin DELETE wrapper, so for our SOCI impl `RemoveMember`
  is semantically equivalent.

Shared helper
- `ScrubMembershipInMemory(ctx, guild_id, char_id)` —
  file-local helper that drops the member from
  `guild->members` under the guild lock + clears
  `TChar.guild_id` under the char lock. Returns `true` if
  either side actually mutated state. Both handlers call it
  before queueing the repo write. Designed for idempotency:
  re-runs on already-cleaned state are benign (a flapping DB
  push or a player+DB race both end up calling the same
  cleanup twice).

Tests
- `tests/test_guild_mut_handlers.cpp` scenarios 32-34:
  - 32: LEAVE drops Carol (char 400, joined in scenario 14)
    from guild 8 + clears back-pointer + repo recorded the
    RemoveMember call
  - 33: KICKOUT seeds Echo (char 500) into guild 8 manually,
    then exercises the fan-in and verifies the same cleanup
    (Bravo2 from earlier scenarios was already kicked in
    scenario 10 so we couldn't reuse him)
  - 34: idempotent re-LEAVE on already-cleaned Carol — repo
    still gets the call (DB-authoritative), in-memory state
    stays at guild_id=0 (no-op cleanup)

Build verified: cmake + ctest -R tworldsvr_asio (14/14 passed).

Deferred to W3a-18+
- Tactics subsystem (~17 handlers): TACTICSADD/DEL/ANSWER/
  INVITE/KICKOUT/LIST/REPLY + tactics-side WANTED/VOLUNTEER
- PvP record listing (`CTBLGuildPvPointReward` TOP 50)
- Cabinet item codec
- Scheduler-driven wanted-entry expiry sweep
- `OnDM_GUILDUPDATE_REQ` (variable-length alliance/enemy CSV
  columns — non-trivial wire codec)
- `OnDM_GUILDESTABLISH_ACK` / `OnDM_GUILDEXTINCTION_ACK` /
  `OnDM_GUILDDISORGANIZATION_ACK` (DB→World ACKs confirming
  prior World→DB writes landed — currently we fire-and-forget)

### W3a-16 — what landed

Four more DB-side fan-in handlers extending the W3a-14/15
cohort into the recruitment subsystem. All wrap existing
`IGuildRepository` methods (no new ones needed). Defensive
in-memory mirror to `GuildWantedRegistry` keeps the next
LIST handler + "already applied" indicator coherent.

- `OnDM_GUILDWANTEDADD_REQ` (wID=0x5927) → `repo->AddWanted`.
  Mirror path looks up guild_id in the registry to pick
  country + name (legacy parity — both fields come from the
  guild row, not the wire). Computes `end_time = now +
  kGuildWantedPeriodSec` matching the player path's
  `OnGuildWantedAddAck` clock so cross-server entries expire
  together.
- `OnDM_GUILDWANTEDDEL_REQ` (wID=0x5928) → `repo->DeleteWanted`.
  Mirror path calls `GuildWantedRegistry::Remove(guild_id)`.
- `OnDM_GUILDVOLUNTEERING_REQ` (wID=0x5929) →
  `repo->AddVolunteerApp`. Wire prefix `bType` filters the
  applicant kind: `kVolunteerKindMember` (=0) flows through,
  `kVolunteerKindTactics` (=1) gets logged + dropped (the
  tactics subsystem ships in a later W3a-* phase). The mirror
  path bypasses `AddApp`'s validation gates — the DB is
  authoritative for fan-in, so if our local registry would
  reject the row (already-applied / wanted-expired /
  level-mismatch) we still persist + log the divergence. Next
  full reload reconciles.
- `OnDM_GUILDVOLUNTEERINGDEL_REQ` (wID=0x592A) →
  `repo->DelVolunteerApp`. Same `bType` filter. Mirror path
  calls `GuildWantedRegistry::DelApp(char_id)` (idempotent).

Constants
- `services/guild_constants.h` gains
  `kVolunteerKindMember = 0` + `kVolunteerKindTactics = 1`.
  Legacy uses raw 0/1 without an exported enum; we name them
  so the bType-filter branch reads clearly.

Tests
- `tests/test_guild_mut_handlers.cpp` test fixture gains a
  `GuildWantedRegistry` member (previously only the player-path
  scenarios for W3a-11/12 needed it via a different test file).
- Scenarios 27-31:
  - 27: WANTED ADD fan-in lands in registry + repo
  - 28: WANTED DEL clears the registry entry
  - 29: VOLUNTEERING ADD (kMember) persists; note the
    in-memory AddApp may reject if applicant char is already
    in a guild — handler logs and continues (DB-authoritative)
  - 30: VOLUNTEERING ADD (kTactics) gets dropped, no
    persistence call lands
  - 31: VOLUNTEERINGDEL (kMember) clears + persists

Build verified: cmake + ctest -R tworldsvr_asio (14/14 passed).

Deferred to W3a-17+
- DB fan-in: `OnDM_GUILDLEAVE_REQ` + `OnDM_GUILDKICKOUT_REQ`
  (~2 handlers; need defensive in-memory cleanup of
  `guild->members` + `TChar.guild_id` back-pointer + the
  W3a-4c leave/kickout broadcast tail)
- Tactics subsystem (~17 handlers): TACTICSADD/DEL/ANSWER/
  INVITE/KICKOUT/LIST/REPLY + tactics-side WANTED/VOLUNTEER
- PvP record listing (`CTBLGuildPvPointReward` TOP 50)
- Cabinet item codec
- Scheduler-driven wanted-entry expiry sweep
- `OnDM_GUILDUPDATE_REQ` (variable-length alliance/enemy CSV
  columns — non-trivial wire codec)

### W3a-15 — what landed

Four more DB-side fan-in handlers extending the W3a-14 cohort.
All wrap existing `IGuildRepository` methods (no new ones
needed). No wire replies — DB is authoritative for these
fields.

- `OnDM_GUILDFAME_REQ` (wID=0x58DB) → `repo->UpdateFame`.
  Defensively mirrors fame + fame_color into the registry
  because they're broadcast in `GuildInfoAck` and the
  Establish-broadcast tail — keeping them stale would
  cause subtle UI drift on the next refresh.
- `OnDM_GUILDARTICLEADD_REQ` (wID=0x58D9) →
  `repo->AddArticle`. **No** in-memory mirror:
  `TGuild.articles` is owned by the `article_index` counter
  incremented on `OnGuildArticleAddAck` (the player-action
  path). DB-pushed `article_id` chooses its own value
  that might collide with the local counter — we let the
  next `OnGuildArticleListAck` refresh reconcile. Same
  behavior as legacy SSHandler.cpp:4201.
- `OnDM_GUILDARTICLEDEL_REQ` (wID=0x58DA) →
  `repo->DelArticle`. No in-memory mirror, same rationale.
- `OnDM_GUILDARTICLEUPDATE_REQ` (wID=0x58E7) →
  `repo->UpdateArticle`. No in-memory mirror.

Tests
- `tests/test_guild_mut_handlers.cpp` gains scenarios 23-26
  covering the 4 new handlers end-to-end through dispatch.

Why so small
- Each handler is 15-20 LOC: read wire → CoOffloadVoidIf →
  log. The interesting work was the FAME vs. ARTICLES
  decision split — fame needs the defensive mirror, articles
  shouldn't have one. The legacy SSHandler.cpp counterparts
  (lines 4201/4264/4323/4412) are equally tiny.

Build verified: cmake + ctest -R tworldsvr_asio (14/14 passed).

Deferred to W3a-16+
- DB fan-in: `OnDM_GUILDWANTEDADD/DEL_REQ` +
  `OnDM_GUILDVOLUNTEERING/INGDEL_REQ` (~4 handlers; need
  defensive in-memory updates to `GuildWantedRegistry`)
- DB fan-in: `OnDM_GUILDLEAVE_REQ` + `OnDM_GUILDKICKOUT_REQ`
  (~2 handlers; need defensive in-memory cleanup of
  `guild->members` + `TChar.guild_id` back-pointer)
- Tactics subsystem (~17 handlers): TACTICSADD/DEL/ANSWER/
  INVITE/KICKOUT/LIST/REPLY + tactics-side WANTED/VOLUNTEER
- PvP record listing (`CTBLGuildPvPointReward` TOP 50)
- Cabinet item codec
- Scheduler-driven wanted-entry expiry sweep
- `OnDM_GUILDUPDATE_REQ` (variable-length alliance/enemy CSV
  columns — non-trivial wire codec)

### W3a-14 — what landed

The "DB-side fan-in" cohort. Five new handlers covering the
DB→World direction of guild state updates. Each is a thin
wrapper around an existing (or newly-added)
`IGuildRepository` method — none of them reply on the wire
because the DB is authoritative for the fields they touch.

Handlers
- `OnDM_GUILDDUTY_REQ` (wID=0x58D6) → `repo->UpdateMemberDuty`
- `OnDM_GUILDPEER_REQ` (wID=0x58D7) → `repo->UpdateMemberPeer`
- `OnDM_GUILDCONTRIBUTION_REQ` (wID=0x58DD) →
  `repo->IncrementContribution`. The legacy wire carries 6 fields
  (no pvp_point); the repo signature gained that 7th param in
  W3a-13 for forward parity with the castle-war flow, so the
  fan-in path passes `pvp_point=0`.
- `OnDM_GUILDLEVEL_REQ` (wID=0x58DC) → new
  `repo->UpdateLevel`. Defensively updates in-memory
  `TGuild.level` too — the peerage-gate member cap derives from
  `GuildLevelCache::Find(level)->max_count`, so keeping that
  stale would silently mis-gate kickout / peerage decisions.
- `OnDM_GUILDPOINTREWARD_REQ` (wID=0x5916) → new
  `repo->LogPointReward`. Legacy `CSPSaveGuildPointReward`
  is a single SP that fans out to INSERT into
  `TGUILDPVPOINTREWARDTABLE` + UPDATE TGUILDTABLE total/useable.
  SOCI impl wraps both in one transaction. Also defensively
  mirrors total/useable into the registry (same pattern as
  W3a-13 `UpdatePvPoints`).

Repo additions
- `IGuildRepository::UpdateLevel(guild_id, level)` — single
  `UPDATE TGUILDTABLE SET bLevel`.
- `IGuildRepository::LogPointReward(guild_id, point, name,
  total, useable)` — INSERT + UPDATE in one SOCI transaction.
- `Call::Kind::{kUpdateLevel, kLogPointReward}` on the fake.
  The recipient_name string is dropped from the Call record
  (same pattern as `kAddArticle`); tests verify numeric fields
  + arrival order.

Tests
- `tests/test_guild_mut_handlers.cpp` gains scenarios 17-22:
  scenario 17 covers W3a-13 (`DM_GUILDPVPOINT_REQ` round-trip
  through dispatch), scenarios 18-22 cover the five W3a-14
  handlers. All exercise the full
  socket→dispatch→handler→repo→registry path.

Why these are tiny
- Each handler is 10-20 LOC: read wire → CoOffloadVoidIf →
  log. They're "DB fan-in" plumbing that closes the
  bidirectional loop for state we already write outbound via
  the MW_* handlers. The legacy SSHandler.cpp counterparts
  (lines 3480, 3551, 4069, 4103, 10429) are equally tiny;
  the work was identifying which existing repo methods to
  reuse vs. add new ones.

Build verified: cmake + ctest -R tworldsvr_asio (14/14 passed).

Deferred to W3a-15+
- Tactics subsystem (~17 handlers): TACTICSADD/DEL/ANSWER/
  INVITE/KICKOUT/LIST/REPLY + tactics-side WANTED/VOLUNTEER
  parity with the W3a-11/12 guild-wanted/volunteer flows
- PvP record listing (`CTBLGuildPvPointReward` TOP 50)
- Cabinet item codec
- Scheduler-driven wanted-entry expiry sweep
- `OnDM_GUILDLEAVE_REQ` + `OnDM_GUILDKICKOUT_REQ` +
  `OnDM_GUILDUPDATE_REQ` fan-ins (the leave/kickout pair
  needs the leave-log SP wired separately from RemoveMember;
  UPDATE has variable-length alliance/enemy CSV columns)

### W3a-13 — what landed

Two unrelated cleanups bundled into a single small patch:

1. **Promote-into-guild helper.** The W3a-6 InviteAnswer YES branch
   and the W3a-12 VolunteerReply accept branch ran the same
   ~60 LOC sequence: snapshot guild meta under `guild.lock`,
   re-validate disorg / have-guild / full gates, push into
   `guild->members`, set `TChar.guild_id`, persist via
   `repo->AddMember`. The duplication was flagged as a deferred
   refactor in W3a-12. W3a-13 lifts it to `TryPromoteIntoGuild`
   in the new W3a-13 anon namespace at the top of
   `handlers_guild.cpp`. Both call sites now reduce to a single
   `co_await TryPromoteIntoGuild(...)` plus result-code
   dispatch — failure replies and JOIN_REQ broadcasts stay at
   the call sites because the reply targets and packet types
   differ (dual JOIN_REQ for invite, single VOLUNTEERREPLY_REQ
   for volunteer reject).

2. **`OnDM_GUILDPVPOINT_REQ` (wID=0x5915).** Legacy
   SSHandler.cpp:10405 — the DB server pushes new PvP point
   counters (total / useable / month) for a guild after a
   castle-war point reward run. Handler updates the in-memory
   `TGuild.pvp_*_point` fields under `guild.lock` and persists
   via `IGuildRepository::UpdatePvPoints` (single UPDATE
   `TGUILDTABLE` SET dwPvPTotalPoint / dwPvPUseablePoint /
   dwPvPMonthPoint). If the guild isn't in the registry the
   write still hits the DB (legacy parity — same call path used
   by `CTGuildPvPointReward` to backfill guilds that are
   currently offline).

Adds
- handlers/handlers_guild.cpp: W3a-13 anon namespace at the
  top of the file with `PromotionResult` struct and
  `TryPromoteIntoGuild` helper coroutine + new
  `OnGuildPvPointReq` handler. Call sites at
  `OnGuildInviteAnswerAck` (W3a-6) and `OnGuildVolunteerReplyAck`
  (W3a-12) shrunk by ~60 LOC each.
- handlers/handlers.h: `OnGuildPvPointReq` declaration.
- handlers/dispatch.cpp: `DM_GUILDPVPOINT_REQ` case routes
  to the new handler.
- services/guild_repository.h: `UpdatePvPoints` virtual.
- services/fake_guild_repository.{h,cpp}:
  `Call::Kind::kUpdatePvPoints` + impl mutates in-memory
  TGuild fields.
- services/soci_guild_repository.{h,cpp}: SOCI impl runs a
  single UPDATE.

Helper contract notes
- The helper takes the guild lock once, snapshots meta + runs
  the validation gates + does the member push in a single
  critical region. Both call sites still snapshot their own
  char fields (key / name / msi / level) under `char->lock`
  beforehand — char locks never overlap with the guild lock.
- Failure return values are the wire result codes
  (`kNotFound` / `kHaveGuild` / `kMemberFull`) so call sites
  can forward them directly to clients. `max_member` is only
  populated on `kMemberFull` (used by `SendJoinError` for the
  cap-reached toast).
- Persistence happens inside the helper via
  `CoOffloadVoidIf` — call sites no longer queue their own
  `AddMember` call. The W3a-12 path still queues its own
  `DelVolunteerApp` afterward (that's the volunteer-flow
  cleanup, not the promotion).

Build verified: cmake + ctest -R tworldsvr_asio (14/14 passed).

Deferred to W3a-14+
- Tactics subsystem (~7 handlers): TACTICSADD/DEL/ANSWER/
  INVITE/KICKOUT/LIST/REPLY + tactics-side wanted/volunteer
- Cabinet item codec
- Scheduler-driven wanted-entry expiry sweep
- PvP point reward listing (CTBLGuildPvPointReward — TOP 50
  query, not a request/reply handler)

### W3a-12 — what landed

The volunteer/applicant complement to W3a-11's wanted board:
players apply to a guild's recruitment posting, chiefs browse +
accept/reject applicants. Four handlers, four senders, the
applicant-tracking extension to GuildWantedRegistry, and the
guild-member promotion path lifted from W3a-6 invite YES-branch.

Adds (state)
- services/guild_wanted_registry.h
  - `TGuildWantedApp` POD (char_id, wanted_id, region, level,
    klass, name).
  - `TGuildWanted` gains `applicants` vector — populated by
    `OnGuildVolunteeringAck`; cleared on entry removal or
    member acceptance.
- services/guild_wanted_registry.{h,cpp}
  - `AddApp`: 5 legacy gates (already-applied-same /
    already-applied-elsewhere / no-such-wanted / country
    mismatch / wanted expired / level out of range);
    returns one of `kSame` / `kAlreadyApply` / `kFail` /
    `kWantedEnd` / `kMismatchLevel` / `kSuccess`.
  - `DelApp`: removes by char_id from both the per-entry
    applicant list and the reverse-index map.
  - `SnapshotAppsFor(guild_id)`: copy out applicant list
    under shared lock (chief's VOLUNTEERLIST path).
  - `FindAppByChar`: O(1) "which wanted did this char apply
    to?" — drives future "already_applied" hints in the
    wanted board.
  - Internal `m_app_by_char` reverse index keeps `DelApp` +
    `FindAppByChar` off the per-entry scan.

Adds (repo)
- services/guild_repository.h: `AddVolunteerApp` /
  `DelVolunteerApp`.
- services/fake_guild_repository.{h,cpp}: impls + Call::Kind
  ::{kAddVolunteerApp, kDelVolunteerApp}.
- services/soci_guild_repository.{h,cpp}:
  - AddVolunteerApp: DELETE-then-INSERT against
    TGUILDVOLUNTEERTABLE (portable upsert; bType column
    hardcoded to GUILDAPP_MEMBER = 0).
  - DelVolunteerApp: single DELETE WHERE dwCharID.

Adds (senders)
- senders/senders.h + senders_guild.cpp
  - `GuildVolunteerRow` POD (5 fields per applicant).
  - SendMwGuildVolunteeringReq / DelReq / ReplyReq — 3-byte
    result replies.
  - SendMwGuildVolunteerListReq — variable-length DWORD count
    + per-row tuple matching SSSender.cpp:1336.

Adds (handlers)
- handlers/handlers_guild.cpp (W3a-12 block inserted
  between W3a-5 and W3a-11; forward-declares the
  ResolveRequesterGuild + SendWantedList helpers that live
  in the W3a-8 + W3a-11 blocks downstream)
  - GuildHandle struct lifted to file-scope anonymous
    namespace so multiple W3a-* blocks share the type via
    forward decl.
  - `OnGuildVolunteeringAck` (MW_GUILDVOLUNTEERING_ACK,
    wID=0x90EA): legacy SSHandler.cpp:4547 port. Player
    must not be in a guild; AddApp gates the 5 conditions;
    persist on success + wanted-board refresh.
  - `OnGuildVolunteeringDelAck` (MW_GUILDVOLUNTEERINGDEL_ACK,
    wID=0x90EC): DelApp + persist + wanted-board refresh.
  - `OnGuildVolunteerListAck` (MW_GUILDVOLUNTEERLIST_ACK,
    wID=0x90EE): chief lists their guild's applicants;
    SnapshotAppsFor → reply.
  - `OnGuildVolunteerReplyAck` (MW_GUILDVOLUNTEERREPLY_ACK,
    wID=0x90F0): legacy SSHandler.cpp:4629 port.
    - Reject (bReply=0): DelApp + persist + applicant-list
      refresh; no reply to rejected char (legacy parity).
    - Accept (bReply=1): re-validate gates (state may have
      changed during dialog), AddMember under guild.lock,
      set TChar.guild_id, persist via CoOffloadVoidIf
      (DelVolunteerApp + AddMember), fire dual JOIN_REQ
      (kJoinSuccess) to new member + chief. Mirror of the
      W3a-6 invite YES-branch — the shared promotion path
      was extracted to `TryPromoteIntoGuild` in W3a-13.

Build verified: cmake + ctest -R tworldsvr_asio (14/14 passed).

Deferred to W3a-13+
- Tactics subsystem (~7 handlers): TACTICSADD/DEL/ANSWER/
  INVITE/KICKOUT/LIST/REPLY + tactics-side wanted/volunteer
- PvP record / point reward (~3)
- Cabinet item codec
- Scheduler-driven wanted-entry expiry sweep

### W3a-11 — what landed

Guild recruitment board: chiefs post a "we are recruiting"
entry, players browse the list filtered by country, chiefs
delete or re-post (upsert) at will. Entries auto-expire after
14 days. Three handlers (ADD / DEL / LIST), three senders, one
new registry + one new repository write API pair.

Adds (state)
- services/guild_wanted_registry.{h,cpp}
  - TGuildWanted POD (guild_id, country, min/max_level,
    end_time, name, title, text).
  - GuildWantedRegistry: shared_mutex + unordered_map keyed
    by guild_id. Single map shared across all guilds because
    the cardinality is low (~1k entries at peak in legacy)
    and the LIST handler always scans cross-country anyway.
  - AddOrUpdate (upsert; second post overwrites the first
    per legacy semantics), Remove (by guild_id), Find,
    SnapshotByCountry (LIST handler's input), Size.

Adds (constants)
- services/guild_constants.h — kGuildWantedPeriodSec =
  14 days (TWorldType.h::GUILDWANTED_PERIOD).

Adds (repo)
- services/guild_repository.h — AddWanted / DeleteWanted.
- services/fake_guild_repository.{h,cpp} — impls + Call::
  Kind::{kAddWanted, kDeleteWanted}.
- services/soci_guild_repository.{h,cpp}:
  - AddWanted: DELETE-then-INSERT against TGUILDWANTEDTABLE
    (portable upsert without backend-specific MERGE syntax).
  - DeleteWanted: single DELETE WHERE dwGuildID.

Adds (senders)
- senders/senders.h + senders_guild.cpp
  - GuildWantedRow POD (8 fields per entry).
  - SendMwGuildWantedAddReq / DelReq — 3-byte result replies.
  - SendMwGuildWantedListReq — variable-length DWORD count
    + per-row tuple. Matches legacy SSSender.cpp:1269.

Adds (handlers)
- handlers/handlers_guild.cpp
  - Private BuildWantedRows + SendWantedList helpers shared
    by ADD/DEL/LIST. The "already_applied" byte stays 0
    until the volunteer/applicant subsystem ports (W3a-12+).
  - OnGuildWantedAddAck (MW_GUILDWANTEDADD_ACK, wID=0x90E4):
    legacy SSHandler.cpp:4432 port. Title/text caps +
    not-disorg + member-of-guild gates. Upsert into
    GuildWantedRegistry, persist via CoOffloadVoidIf, ACK
    + LIST refresh.
  - OnGuildWantedDelAck (MW_GUILDWANTEDDEL_ACK, wID=0x90E6):
    Remove from registry; ACK + LIST refresh.
  - OnGuildWantedListAck (MW_GUILDWANTEDLIST_ACK,
    wID=0x90E8): pure read — SnapshotByCountry → reply. The
    legacy SM_EVENTEXPIRED_ACK fan-out for entries past
    end_time + DAY_ONE is a TODO (W3a-12+ scheduler).

Plumbing
- handlers/handlers.h: HandlerContext gains
  `GuildWantedRegistry* guild_wanted`.
- main.cpp: instantiates GuildWantedRegistry, wires into ctx.
- CMakeLists: adds guild_wanted_registry.cpp.

Build verified: cmake + ctest -R tworldsvr_asio (14/14 passed).

Deferred to W3a-12+
- Volunteer / applicant subsystem (~8 handlers):
  GUILDVOLUNTEERING_ACK + GUILDVOLUNTEERINGDEL_ACK
  + GUILDVOLUNTEERLIST_ACK + GUILDVOLUNTEERREPLY_ACK +
  matching DM_REQ persistence
- Tactics subsystem (~7 handlers)
- PvP record / point reward (~3)
- Cabinet item codec
- Scheduler-driven wanted-entry expiry sweep

### W3a-10 — what landed

Two more handlers that close out the guild lifecycle: simple
money recover (chief sells a cash-shop item priced in cooper)
and the full cluster-wide extinction flow (disorg-timer fires →
DB delete + member fan-out + registry drop). Both are
representative of the "DM handler that mutates registry +
notifies peers" pattern future handlers will follow.

Adds (repo)
- services/guild_repository.h — `DeleteGuild(guild_id)`.
- services/fake_guild_repository.{h,cpp} — impl + Call::Kind
  ::kDeleteGuild record so tests can assert on the persistence
  side.
- services/soci_guild_repository.{h,cpp} — explicit DELETEs in
  dependency order: TGUILDARTICLETABLE + TGUILDMEMBERTABLE +
  TGUILDTABLE. Legacy CSPGuildDelete is a single DELETE on
  TGUILDTABLE that assumes the production schema has FK
  CASCADE on the children. Our explicit version sweeps the
  children regardless, so dev / test schemas without the FK
  cascade clause stay consistent. Production schemas pay two
  extra cold-path round-trips — negligible vs. the safety win.

Adds (handlers)
- handlers/handlers_guild.cpp
  - OnGuildMoneyRecoverAck (MW_GUILDMONEYRECOVER_ACK,
    wID=0x912D): legacy SSHandler.cpp:10539 port. Validates
    guild exists, bumps `cooper` under the guild lock, persists
    via IncrementContribution (cooper delta only) on the worker
    pool. No reply (cluster sees the change via the next
    OnGuildInfoAck refresh).
  - OnGuildExtinctionReq (DM_GUILDEXTINCTION_REQ,
    wID=0x58CD): legacy SSHandler.cpp:3283-3315 port,
    collapsed (REQ + ACK round-trip → single coroutine because
    CoOffloadVoidIf already serialises the DB write).
    Sequence:
      1. snapshot member_ids under guild.lock + read name
         for log line
      2. ctx.guilds->Remove(guild_id) (registry-level drop;
         cached shared_ptrs still work)
      3. CoOffloadVoidIf → repo->DeleteGuild
      4. per-member: clear TChar.guild_id, look up main map
         peer via PeerRegistry, fire MW_GUILDLEAVE_REQ
         (kLeaveDisorganization) — the same sender from W3a-4
         + the kLeaveKick sender path from W3a-4c.

Build verified: cmake + ctest -R tworldsvr_asio (14/14 passed)
on GCC 13.3 Ubuntu noble.

Deferred to W3a-11+
- Tactics subsystem (~7 handlers): TACTICSADD / TACTICSDEL /
  TACTICSANSWER / TACTICSINVITE / TACTICSKICKOUT /
  TACTICSLIST / TACTICSREPLY + tactics-side wanted/volunteer
- Volunteers / Wanted (~11): GUILDWANTED + GUILDVOLUNTEER
  flow for guild recruitment board
- PvP record / point reward (~3)
- Cabinet item codec (Lib/Own/TProtocol/ITEM struct port)

### W3a-9 — what landed

Single-guild info refresh — the densest sender + handler in the
guild family. One handler (OnGuildInfoAck), one big composite
sender (SendMwGuildInfoReq with 27 wire fields plus 2 vice-chief
slot strings padded to NAME_NULL).

While writing this, found the legacy `NotifyAddGuildMember`
(TWorldSvr.cpp:5099) only fans out to the new member + chief —
not to *all* existing members as the W3a-7 README's "deferred
TODO" implied. Our W3a-6 implementation already matches that
shape, so the deferred item was a phantom. Removed from the
backlog.

Adds (state)
- `services/guild_registry.h` — TGuild gains six fields for
  the info-pane payload (all default-zero until owning
  subsystems port):
    pvp_month_point   — monthly PvP point reset (W5+)
    rank_total        — guild ranking (W5+)
    rank_month        — monthly ranking
    stat_level        — guild stats subsystem
    stat_point        — stats subsystem
    stat_exp          — stats subsystem

Adds (sender)
- `senders/senders.h` + `senders_guild.cpp`
  - `GuildInfoPayload` POD struct — 30 fields, including a
    `std::array<std::string, 2>` for vice-chief slot names
    (legacy emits 2 slots always; empty strings = NAME_NULL).
  - `SendMwGuildInfoReq(peer, char_id, key, result, payload)`
    — 27 wire fields on success (header + payload), 9-byte
    error reply. Pulls max_member + level_exp from the
    W3a-4d guild_levels cache. Matches SSSender.cpp:1015
    field-for-field including the vice-chief padding loop.
  - `<array>` added to senders.h includes for the fixed-2
    vice-chief slot.

Adds (handler)
- `handlers/handlers_guild.cpp`
  - `OnGuildInfoAck` (MW_GUILDINFO_ACK, wID=0x903A)
  - Legacy SSHandler.cpp:3866 port. Builds the payload under
    guild.lock (snapshot + release), then runs the
    guild_levels lookup outside the lock. Fail paths:
    - No guild_id on the requesting char → kNotFound.
    - Guild not in registry (unloaded) → kNotFound.
    - Chief not in members (corrupt state) → kNotFound.
  - Collects up to 2 vice-chief names by iterating members;
    the sender pads slots 0..1 with empty strings as needed.
  - Most-recent article title is `guild->articles.back().title`
    (legacy keeps a separate m_strArticleTitle field; we
    derive on read since the article vector is the source
    of truth).

Build verified: cmake + ctest -R tworldsvr_asio (14/14 passed).

Deferred to W3a-10+
- Tactics / volunteers / PvP record / point reward (~15)
- Cabinet item codec (Lib/Own/TProtocol/ITEM struct port)
- DM_GUILDEXTINCTION delete flow (paired with W3a-4b disorg
  — triggers via the disorg countdown timer in W7+).

### W3a-8 — what landed

Guild bulletin board: members post short articles to a shared
buffer. Four handlers (LIST / ADD / DEL / UPDATE) plus their
matching senders + the IGuildRepository write API for
TGUILDARTICLETABLE persistence. Handlers cap the title at 256
chars + body at 2048 + total articles per guild at 100 — legacy
parity (silent-drop oversized payloads, kFail on guild full).

Adds (state)
- `services/guild_registry.h` — `TGuildArticle` POD struct
  (id, duty, writer, title, body, time_unix). TGuild grows
  `articles` vector + monotonic `article_index` counter
  (the legacy `m_dwArticleIndex` — IDs never reused even
  after deletion).
- `services/guild_constants.h` — `kMaxBoardTitle = 256`,
  `kMaxBoardText = 2048`, `kMaxGuildArticleCount = 100`
  (NetCode.h:27/29/75).

Adds (repo)
- `services/guild_repository.h` — AddArticle / DelArticle /
  UpdateArticle.
- `services/fake_guild_repository.{h,cpp}` — impls + Call
  records (Kind::kAddArticle / kDelArticle / kUpdateArticle).
- `services/soci_guild_repository.{h,cpp}` — INSERT / DELETE
  / UPDATE against TGUILDARTICLETABLE (legacy CSPGuildArticle*).

Adds (senders)
- `senders/senders.h` + `senders_guild.cpp`
  - `GuildArticleRow` POD (6 fields per article).
  - `SendMwGuildArticleListReq` — variable-length tail:
    1-byte count + per-article tuple. Sent on every
    successful add/del/update (legacy chases each ACK
    with a LIST refresh so the chief's UI re-renders).
  - `SendMwGuildArticleAddReq` / `DelReq` / `UpdateReq`
    — 3-field result replies, all share the same wire
    shape; factored behind a private `SendArticleResultReply`
    helper that takes the wID + result byte.

Adds (handlers)
- `handlers/handlers_guild.cpp`
  - `ResolveRequesterGuild` private helper: locates the
    requesting char's guild + validates char/key/guild
    gates in one place. The 4 article handlers share this.
  - `BuildArticleRows` private helper: builds wire-shaped
    rows under the guild lock, formats Unix timestamps to
    "YYYY-MM-DD" strings (legacy CTime::Format).
  - `OnGuildArticleListAck` (MW_GUILDARTICLELIST_ACK,
    wID=0x90DA): snapshot + send.
  - `OnGuildArticleAddAck` (wID=0x90DC): cap checks +
    member lookup + bump article_index + persist via
    CoOffloadVoidIf + ACK + LIST refresh.
  - `OnGuildArticleDelAck` (wID=0x90DE): linear scan for
    matching ID + erase + persist + ACK + LIST refresh.
    Returns kFail when ID isn't found.
  - `OnGuildArticleUpdateAck` (wID=0x90E2): linear scan +
    title/body update + persist + ACK + LIST refresh.

Build verified: cmake + ctest -R tworldsvr_asio (14/14 passed)
on GCC 13.3 Ubuntu noble.

Deferred to W3a-9+
- `NotifyAddGuildMember` fan-out to *all* existing members on
  join (currently just chief + new member). The W3a-4c
  broadcast helper is the right tool.
- Tactics / volunteers / PvP record / point reward (~15)
- Cabinet item codec (Lib/Own/TProtocol/ITEM struct port).

### W3a-7 — what landed

Guild member-list refresh handler plus a silent wire-bug fix
from W3a-6 the round-trip test didn't catch.

**Wire-bug fix** — `OnGuildInviteAnswerAck`'s YES path used
`kSuccess` (=0) as the result byte on the dual MW_GUILDJOIN_REQ
reply. The legacy `NotifyAddGuildMember` (TWorldSvr.cpp:5109)
sends `GUILD_JOIN_SUCCESS` (=15, NetCode.h:451) — a different
enum entry. Round-trip tests echoed the byte and passed; a real
legacy client would render the wrong notification ("regular ack"
vs. "you joined the guild"). Fixed to `kJoinSuccess` on both
replies.

Adds (extension to TGuildMember)
- `services/guild_registry.h` — `TGuildMember` gains three
  fields: `castle` (m_wCastle), `camp` (m_bCamp), and
  `connected_date_unix` (m_dlConnectedDate). All default-zero
  until the owning subsystems port (castle = W5+, connected_
  date = TGUILDMEMBERTABLE column read on member load).

Adds (sender)
- `senders/senders.h` + `senders_guild.cpp`
  - `GuildMemberListRow` POD — 13 fields, one per member entry
    in the tail.
  - `SendMwGuildMemberListReq(peer, char_id, key, result,
                              guild_id, guild_name, members)`
    — header + variable-length tail. The error branch (result
    != kSuccess) emits only the 9-byte header (matching legacy
    SSSender.cpp:984: only fills the tail when pGuild != null).

Adds (handler)
- `handlers/handlers_guild.cpp`
  - `OnGuildMemberListAck` (MW_GUILDMEMBERLIST_ACK, wID=0x903D)
  - Legacy SSHandler.cpp:3830 port. Snapshots the member list
    under guild.lock (copy out + release), then walks each row
    fetching the live TChar via CharRegistry — sets `online=1`
    + uses the live TChar.level instead of the cached
    member.level (legacy SSSender.cpp:1000 prefers the live
    value over the stale cache). Reply ID matches legacy
    MW_GUILDMEMBERLIST_REQ.

Test changes
- `test_guild_mut_handlers` extended 14 → 15 scenarios:
  * MEMBERLIST refresh returns chief + Carol with the
    expected 13 fields + chief's duty = kDutyChief = 2 (W3a-5
    bug-fix value).
  * Carol's reply in scenario 14 now asserts `kJoinSuccess`
    (15) instead of `kSuccess` (0) — locks in the W3a-7
    wire-bug fix.

Build verified: cmake + ctest -R tworldsvr_asio (14/14 passed).

Deferred to W3a-8+
- `NotifyAddGuildMember`-style fan-out to *all* existing
  guild members on join (not just chief + new member). Now
  unblocked by the broadcast helper from W3a-4c.
- Articles board (5+ handlers): add / del / update / list /
  notify
- Tactics / volunteers / PvP record / point reward (~15)
- Cabinet item codec (Lib/Own/TProtocol/ITEM struct port)

### W3a-6 — what landed

The two-handler guild-invite flow plus its 10-field "join
result" sender — the longest sender in the guild family. Closes
the in-memory member-add path the W3a-4c DM_GUILDMEMBERADD_REQ
already persists (it was the DB half; this is the in-memory
mirror).

* `services/guild_constants.h` — gained `kAskYes` (0) and
  `kAskNo` (1) for the answer-byte enum (NetCode.h:222).
* `senders/senders.h` + `senders_guild.cpp`
  - `SendMwGuildInviteReq` (5-field; forwards from chief's
    peer to target's main map peer).
  - `SendMwGuildJoinReq` (10-field; result + full guild meta
    + new-member id/name + max-member cap byte).
* `handlers/handlers_guild.cpp`
  - `OnGuildInviteAck` (MW_GUILDINVITE_ACK, wID=0x9030):
    chief invites target by name. Gates: requester has a
    guild, guild not disorg, member cap not reached
    (`max_member` from guild_levels), target online, target
    same country as chief, target not in another guild.
    Each failure path replies MW_GUILDJOIN_REQ with the
    matching kNotFound/kMemberFull/kFail/kHaveGuild error
    + zero meta. Success path forwards MW_GUILDINVITE_REQ to
    the target's main map peer.
  - `OnGuildInviteAnswerAck` (MW_GUILDINVITEANSWER_ACK,
    wID=0x9031): target accepts (kAskYes) or declines.
    Decline → JOIN_REQ to chief with the answer code as
    result. Accept → re-validate every gate (state may have
    changed during the dialog), add member under guild.lock,
    flip target's TChar.guild_id, persist via
    `IGuildRepository::AddMember` through CoOffloadVoidIf,
    send two JOIN_REQ replies (one to each side) with the
    full guild meta. The post-join `NotifyAddGuildMember`
    broadcast (announce new member to all existing members)
    defers to W3a-7 — needs a new sender family.

Tests
- `test_guild_mut_handlers` extended 12 → 14 scenarios:
  * INVITEANSWER ASK_NO → chief gets MW_GUILDJOIN_REQ with
    result=1
  * INVITEANSWER ASK_YES → both Carol + chief get
    MW_GUILDJOIN_REQ kSuccess + Carol's TChar.guild_id=8 +
    guild members contains 400 + fake repo recorded
    kAddMember(400, 8, …)

Build verified: cmake + ctest -R tworldsvr_asio (14/14 passed).

Deferred to W3a-7+
- `NotifyAddGuildMember` broadcast — needs a new sender (the
  legacy uses a TGUILDMEMBERADD_REQ shape that the W3a-5+
  member-list-refresh handler will share).
- Articles board (5+ handlers)
- Tactics / volunteers / PvP record / point reward (~15)
- Cabinet item codec (Lib/Own/TProtocol/ITEM struct port)

### W3a-5 — what landed

Two more mutating guild handlers + the CheckPeerage gate that
uses the guild-level cache from W3a-4d. Total handlers ported:
14 → 16. Repo write API gains 2 methods (UpdateMemberPeer,
UpdateMaxCabinet). New unit test exercises every legacy branch
of CheckPeerage on synthetic guild + level rows.

* `services/guild_peerage.{h,cpp}` — `CheckPeerage(level_row,
  requester_duty, new_peer, guild)` mirrors `CTGuild::CheckPeerage`
  (TGuild.cpp:205) field-for-field:
  - `new_peer == 0` → always allowed (degrade is free)
  - `new_peer > MAX_GUILD_PEER_COUNT` → refused
  - Slot cap from `level_row->peer_slots[new_peer-1]`
  - Chief-only band per legacy switch (level 3-4 BARON, 5-6
    VISCOUNT, 7-8 COUNT, 9 MARQUIS, 10 DUKE)
  - **Null level_row** → relaxed gate (only the global
    MAX_GUILD_PEER_COUNT cap applies). Matches W3a-4d's
    "missing TGUILDCHART = empty cache" dev-friendly behavior.
* `services/guild_repository.h` — `UpdateMemberPeer` +
  `UpdateMaxCabinet` write API additions. SOCI impls map to
  CSPGuildPeer / CSPGuildMaxCabinet semantics.
* `services/fake_guild_repository` — new `Call::Kind::
  {kUpdateMemberPeer, kUpdateMaxCabinet}` so tests can assert
  on the persistence side.
* `handlers/handlers_guild.cpp`
  - `OnGuildPeerAck` (MW_GUILDPEER_ACK, wID=0x9038):
    validates requester is in the guild (snapshots their
    duty), gates the change through CheckPeerage with
    `ctx.guild_levels->Find(guild->level)`. Success path:
    update target's peer under guild.lock, persist via
    CoOffloadVoidIf, reply to requester, broadcast to
    target's main map peer via the W3a-4c BroadcastToGuildMembers
    helper. Gate-fail: reply with `guild::kFail` carrying the
    old + new peer (clients use these for the chat log).
  - `OnGuildCabinetMaxReq` (DM_GUILDCABINETMAX_REQ,
    wID=0x58E8): pure DB persistence + in-memory mirror.
    Clamps the inbound `bMaxCabinet` against the per-level
    cap from `guild_levels->Find(level)->cabinet_count`
    (operator-visible via log warning) — legacy trusted the
    inbound value, we add a soft check.
* `senders/senders.h` + `senders_guild.cpp`
  - `SendMwGuildPeerReq` (6-field reply per SSSender.cpp:953)
  - `SendMwGuildCabinetMaxReq` (3-field reply; piggybacks on
    `MW_GUILDCABINETPUTIN_ACK` channel — legacy has no
    dedicated wID for the cap-change notification).

Tests
- `test_guild_peerage` — 7 scenarios covering every branch of
  CheckPeerage: zero-peer always OK, out-of-range refused,
  slot-cap full, slot-cap available + level 1/2 (any duty),
  level 3-4 BARON chief-only refused / chief-allowed, level
  5-6 VISCOUNT chief-only + BARON any-duty, level 10 DUKE
  chief-only, null level_row relaxed gate.

Deferred to W3a-6+
- `OnMW_GUILDINVITEANSWER_ACK` — in-memory member-add side
  (W3a-4c's DM_MEMBERADD_REQ is just the DB write). Needs
  `SendMwGuildJoinReq` sender + `NotifyAddGuildMember`
  broadcast.
- Articles / cabinet items / tactics / volunteers / PvP
  record / point-reward handlers.
- Cabinet item codec (Lib/Own/TProtocol/ITEM struct port).

### W3a-4d — what landed

Pure infrastructure PR. **Closes the last remaining item on
PATCH_README §6 W-1** (async DB + per-shard write queue): every
SOCI write the guild handlers issue now runs on the
`fourstory::db::thread_pool` worker — the io_context coroutine
suspends across the DB roundtrip instead of blocking the reactor.
Also introduces `GuildLevelCache` so the W3a-5+ peerage / member-
cap gates have the legacy `m_pTLEVEL` lookup they need.

* `services/guild_level_cache.{h,cpp}` — small read-only mirror
  of `TGUILDCHART` (10 rows max, indexed by `bLevel`). `LoadFrom`
  drops out-of-range rows with a debug log; `Find(level)` is a
  lock-free direct-index lookup (the table is immutable post-load).
  Mirrors `tagTGUILDLEVEL` field-for-field including the 5-slot
  `bPeer[]` array.
* `services/guild_level_repository.h` — `IGuildLevelRepository`
  read-only interface. The chart is operator-tuned + offline-
  reloaded, so no write API.
* `services/fake_guild_level_repository.{h,cpp}` — in-memory
  impl with `AddRow` seed API. Used by tests + the no-DB dev
  path.
* `services/soci_guild_level_repository.{h,cpp}` — SOCI
  `SELECT * FROM TGUILDCHART` once at boot.
* `db/schema_validator.cpp` — required-column check for
  TGUILDCHART (bLevel / dwEXP / bMaxCnt / bCabinetCnt /
  bPeer1 / bPeer5). Missing chart aborts boot when `[database]`
  is configured.
* `main.cpp` — instantiates GuildLevelCache, calls
  `LoadFrom(guild_level_repo->LoadAll())` before the listener
  spawns, wires `ctx.guild_levels`.
* `handlers/handlers.h` — HandlerContext gains
  `const GuildLevelCache* guild_levels`.

**CoOffloadVoidIf wiring** — every `ctx.guild_repo->*` write
call in `handlers/handlers_guild.cpp` now reads:

```cpp
co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
    [repo = ctx.guild_repo, ...] {
        repo->XxxYyy(...);
    });
```

Covered handlers: `OnGuildLeaveAck` (RemoveMember),
`OnGuildDisorganizationReq` (SetDisorg), `OnGuildDutyAck`
(UpdateMemberDuty × 1 or 2 for chief promotion),
`OnGuildFameAck` (UpdateFame), `OnGuildKickoutAck`
(RemoveMember), `OnGuildContributionAck`
(IncrementContribution), `OnGuildMemberAddReq` (AddMember).

When `ctx.db_pool == nullptr` (no `[database]` configured + no
`worker_threads` set) the lambda runs in-line on the current
coroutine thread — `CoOffloadVoidIf` is the nullptr-tolerant
overload. Tests rely on this fall-through.

Build verified: cmake + ctest -R tworldsvr_asio (13/13 passed)
on GCC 13.3 Ubuntu noble. The existing wire tests pass
unchanged — the offload is observationally transparent.

Test added
- `test_guild_level_cache` — 4 scenarios: empty cache, LoadFrom
  filters out-of-range rows (level 0 + level > kMaxGuildLevel
  dropped), second LoadFrom replaces, FakeGuildLevelRepository
  round-trips.

Deferred to W3a-5+ (now unblocked by this PR)
- OnMW_GUILDPEER_ACK with full `CheckPeerage` validation
  (reads `guild_levels->Find(level)->peer_slots[bPeer-1]`)
- OnDM_GUILDCABINETMAX_REQ (reads `cabinet_count`)
- Member-cap gate on the W3a-5 OnMW_GUILDINVITEANSWER_ACK path
  (reads `max_count`)

### W3a-4c — what landed

Three more mutating-guild handlers + the reusable broadcast
helper that handlers can compose into multi-peer fan-outs
without rewriting the LOBYTE-of-wID lookup every time.

* `services/guild_broadcast.{h,cpp}` — `BroadcastToGuildMembers`
  walks a list of member char_ids, finds each TChar, resolves
  `main_server_id` → PeerSession, and `co_await`s a caller-
  supplied `GuildPacketBuilder` per recipient. Returns the
  number of recipients actually reached. Used by
  `OnGuildFameAck` (broadcast new fame to every online member)
  and `OnGuildKickoutAck` (1-element broadcast to the kicked
  char's main peer). Future Article / Peer / Announcement
  handlers ride the same path.
* `services/guild_repository.h` — write API gains `AddMember`
  and `IncrementContribution`. Mirrors CSPGuildMemberAdd +
  CSPGuildContribution.
* `services/fake_guild_repository.{h,cpp}` — impls plus
  `Call::Kind::{kAddMember, kIncrementContribution}` records
  so tests assert "the right CSP fired with the right args".
  The `Call` struct grew from 5 → 8 fields (`c/d/e` cover the
  contribution's silver/cooper/pvp); existing W3a-4b
  push_back call sites updated to zero-fill the new fields.
* `services/soci_guild_repository.cpp`
  - `AddMember`: INSERT into TGUILDMEMBERTABLE, fall back to
    UPDATE on PK conflict (legacy CSPGuildMemberAdd has
    upsert semantics via the SP). `bLevel` is documented as
    unused — legacy stores it on TCHARTABLE, not the member
    row.
  - `IncrementContribution`: two UPDATEs — one against
    TGUILDTABLE for the totals delta (gold/silver/cooper/exp/
    pvp_total_point/pvp_useable_point), one against
    TGUILDMEMBERTABLE for the member's `dwService`.
* `handlers/handlers_guild.cpp`
  - `OnGuildKickoutAck` (MW_GUILDKICKOUT_ACK, wID=0x903B):
    enforces the "officer kick only by chief" gate from
    SSHandler.cpp:3364, removes the target from the guild +
    clears their `TChar.guild_id`, persists via
    `RemoveMember`, then double-broadcasts MW_GUILDLEAVE_REQ
    (reason=kLeaveKick) — one reply to the requesting peer,
    one routed to the kicked char's main peer via
    `BroadcastToGuildMembers` (single-element list).
  - `OnGuildContributionAck` (MW_GUILDCONTRIBUTION_ACK,
    wID=0x90D8): validates not-disorg, rejects exp on
    max-level guild with `kFail`+zeros (legacy
    SSHandler.cpp:4052-4058), bumps guild totals + member
    service score in-memory, persists via
    `IncrementContribution`, replies with
    MW_GUILDCONTRIBUTION_REQ.
  - `OnGuildMemberAddReq` (DM_GUILDMEMBERADD_REQ, wID=0x58D8):
    pure DB write via `AddMember` — no reply. Legacy fires
    CSPGuildMemberAdd from the BatchThread without an ACK
    path; in-memory member insertion happens on the
    OnMW_GUILDINVITEANSWER_ACK side (W3a-5+).
  - `OnGuildFameAck` refactored from an inline member-loop to
    a single `BroadcastToGuildMembers` call.
* `senders/senders.h` + `senders_guild.cpp`
  - `SendMwGuildContributionReq` (8-field reply).
* Dispatcher routes the 3 new wIDs.

Test changes
- `test_guild_mut_handlers` extended 9 → 12 scenarios:
  cancel-disorg + KICKOUT producing 2 MW_GUILDLEAVE_REQ
  replies (chief + kicked target) with `kLeaveKick`
  reasons; CONTRIBUTION applying deltas + reply + repo call
  recorded; DM_MEMBERADD persisting + no reply + framer
  stays alive.

Deferred to W3a-4d (focused infrastructure PR)
- Wire all SOCI writes through
  `fourstory::db::CoOffloadIf(ctx.db_pool, …)` so DB I/O
  doesn't block the io_context thread (closes the W-1
  remaining work from PATCH_README §6).
- Guild-level table cache (`FindGuildLevel(bLevel)`) — the
  legacy `m_pTLEVEL` lookup that backs `CheckPeerage` and
  `MAX_GUILD_LEVEL` gates. Needed before OnMW_GUILDPEER_ACK
  can ship its full validation.

Deferred to W3a-5+
- OnMW_GUILDPEER_ACK (peerage change with CheckPeerage gate)
- OnMW_GUILDINVITE / INVITEANSWER (in-memory member-add side)
- OnMW_GUILDARTICLE* (guild board)
- OnMW_GUILDCABINET* (guild storage) + cabinet item codec
- Tactics / volunteers / PvP record / point reward (~20)

### W3a-4b — what landed

Three more guild mutating handlers (`DM_GUILDDISORGANIZATION_REQ`,
`MW_GUILDDUTY_ACK`, `MW_GUILDFAME_ACK`) plus the IGuildRepository
write API and the single-source-of-truth enum header that the
remaining ~20 guild handlers in W3a-4c will rely on.

Pre-W3a-4b bug fixes
- `senders::kGuildLeaveSelf = 1` was wrong — NetCode.h:448 puts
  `GUILD_LEAVE_SELF` at offset 12. Wire-incompatible against
  legacy peers; not caught by W3a-4's round-trip test.
- `kGuildDutyChief = 1` was wrong — NetCode.h:1983 puts
  `GUILD_DUTY_CHIEF` at offset 2 (offset 1 is VICECHIEF). Same
  silent round-trip bypass. Fixed in `services/guild_constants.h`
  as a single source of truth; old per-file ad-hoc constants
  removed.

Adds
- `services/guild_constants.h` — every GUILD_RESULT + GUILD_DUTY
  value mirrored from `Lib/Own/TProtocol/include/NetCode.h` with
  the line refs kept in sync. Imported by all handler / sender
  files that need a guild enum.
- `services/guild_repository.h` — write API: `SetDisorg`,
  `UpdateMemberDuty`, `UpdateFame`, `RemoveMember`. Mirrors the
  legacy CSP* set 1:1.
- `services/fake_guild_repository.{h,cpp}` — in-memory impls
  + a `Calls()` snapshot of all mutating calls so handler tests
  can assert "the right CSP-equivalent ran".
- `services/soci_guild_repository.{h,cpp}` — UPDATE / DELETE
  against TGUILDTABLE / TGUILDMEMBERTABLE; soft-fail on driver
  errors (logs + returns false rather than aborting the handler).
- `handlers/handlers_guild.cpp`
  - `OnGuildDisorganizationReq` (DM_GUILDDISORGANIZATION_REQ,
    wID=0x589F): persists via `SetDisorg`, flips
    `TGuild.disorg`+`disorg_time` under the guild lock, ACKs
    the requesting peer with `MW_GUILDDISORGANIZATION_REQ`.
  - `OnGuildDutyAck` (MW_GUILDDUTY_ACK, wID=0x9036): validates
    requester is in-guild, gates against "same duty / self /
    vice-cap reached / target has tactics", does the chief-handover
    dance (legacy SSHandler.cpp:3457-3465: outgoing chief →
    DUTY_NONE before promoting target), persists, ACKs the
    requesting peer.
  - `OnGuildFameAck` (MW_GUILDFAME_ACK, wID=0x90E0): checks the
    PvP-point budget (`kPvPointCostFameChange = 30 000`), debits
    points, updates `TGuild.fame`/`fame_color`, broadcasts
    `MW_GUILDFAME_REQ` to **every online guild member's main map
    peer** (legacy SSHandler.cpp:4391-4407). `kNoPoint` reply
    branch for insufficient budget.
  - `OnGuildLeaveAck` now wires the persistence call
    (`RemoveMember`); the in-memory remove and reply paths from
    W3a-4 are unchanged.
- `senders/senders.h` + `senders_guild.cpp`
  - `SendMwGuildDisorganizationReq` (3-field reply)
  - `SendMwGuildDutyReq` (4-field reply)
  - `SendMwGuildFameReq` (6-field reply; broadcast-shaped)

Test changes
- `test_guild_mut_handlers` extended from 4 → 9 scenarios.
  Added: load second guild with 2 members + 50 000 PvP points,
  duty promotion + fake-repo call recorded, fame change + 2-member
  broadcast + PvP points debited, disorg flip + persistence,
  disorg-rejected duty change (legacy gate).
- `test_guild_handlers`: chief member's `duty` assertion updated
  to the correct `GUILD_DUTY_CHIEF = 2`.

Deferred to W3a-4c
- Cross-peer broadcast helpers (RW_CHANGENAME_ACK fan-out,
  DM_FRIENDERASE_REQ on rename, MW_GUILDPEER routing)
- Wire OnGuild* handlers through `fourstory::db::CoOffloadIf`
  so SOCI writes don't block the io_context thread
- The remaining ~20 mutating guild handlers (Establish ACK /
  Member add / Invite answer / Kickout / Peer / Contribution /
  Article / Cabinet)
- Cabinet item codec (`Lib/Own/TProtocol/ITEM` struct port)
- Guild-level table cache

### W3a-4 — what landed

The first mutating-guild handler — `OnMW_GUILDLEAVE_ACK` —
exercises the full stack the remaining ~24 guild handlers will
use: locate the char's guild via the new `TChar.guild_id`
back-pointer, mutate `TGuild.members` under the per-guild lock,
clear the back-pointer atomically with the member removal, and
fire the matching MW sender back to the originating peer.

* **TChar.guild_id** — back-pointer matching legacy
  `pTCHAR->m_pGuild->m_dwID`. 0 = no guild. The registry-owned
  `TGuild` holds the strong reference; this is a non-owning id
  so there's no shared_ptr cycle between TChar and TGuild.
* **TGuild::FindMember / RemoveMember** — in-place helpers that
  expect `TGuild.lock` to be held by the caller. Linear over
  `members` (typical size < 200); a name-keyed secondary index
  arrives in W3a-4b once benchmarks justify it.
* **OnGuildLoadAck update** — sets `TChar.guild_id = guild_id`
  on the founder when the guild is inserted, closing a W3a-1
  TODO. The char→guild link now survives across handler calls
  without requiring a `FindByName` scan.
* **OnEnterCharReq update** — now resolves the requesting
  char's guild fields (`guild_id`, `guild_chief`, `duty`) from
  the `GuildRegistry` instead of always returning zeros. Disorg
  guilds short-circuit to `guild_id=0` (legacy parity). Stale
  back-pointers (guild unloaded but `TChar.guild_id` still set)
  log a warning and reply with zeros — defensive, since W3a-4b
  may add a Disorg sweep that clears these on the next pass.
* **OnGuildLeaveAck** (MW_GUILDLEAVE_ACK, wID=0x9034) — full
  port of the legacy `SSHandler.cpp:3571` semantics: char +
  key validation, member removal under the guild lock,
  `TChar.guild_id = 0`, `MW_GUILDLEAVE_REQ` reply with
  `GUILD_LEAVE_SELF`. The legacy `SendDM_GUILDLEAVE_REQ` DB
  persistence + cross-peer broadcast defer to W3a-4b once the
  `IGuildRepository::RemoveMember` write API lands.
* **SendMwGuildLeaveReq** — second sender in the MW-guild
  family (after SendMwGuildEstablishReq from W3a-2). Wire
  layout matches `SSSender.cpp` exactly.

New test: `test_guild_mut_handlers` — 4 wire scenarios across a
real loopback peer: load → leave → ENTERCHAR after leave returns
zero. Verifies the `TChar.guild_id` ↔ `TGuild.members` invariant
is preserved across mutation.

### W3a-3 — what landed

Three handlers that turn the relay-map ↔ world handshake from a
stub into a real round trip, plus the TChar identity expansion
that all subsequent guild / party / chat handlers need to reach
beyond `dwCharID`.

* **CharRegistry name index** — sharded secondary index
  (16 shards × `std::shared_mutex`) keyed by `ToUpper(name)` so
  `FindByName` is case-insensitive (matches legacy
  `m_mapTCHARNAME` semantics). `Rename(char_id, new_name)` is
  atomic: insert under new name → store on TChar → drop old
  entry, rejecting cluster-wide name collisions. `Rename(id, "")`
  drops only the index entry — used by future CloseChar prep
  paths.
* **TChar identity fields** — `name`, `country`, `aid_country`,
  `klass`, `level`, `race`, `sex`, `face`, `hair`, `map_id`,
  `pos_x/y/z`. Each is the modern mirror of one legacy
  `tagTCHARACTER` member; OnRW_ENTERCHAR_REQ now answers with the
  real country / map id instead of zeros.
* **OnChangeCharBaseAck** (MW_CHANGECHARBASE_ACK, wID=0x911B) —
  branches on `bType` for FACE / HAIR / RACE / SEX / COUNTRY /
  AIDCOUNTRY / NAME. NAME drives `CharRegistry::Rename` and
  refuses on collision; the friend / soulmate / guild-app
  notification fan-out defers to W4 / W3a-4 (they need the
  matching registries).
* **OnEnterCharReq** (RW_ENTERCHAR_REQ, wID=0x999C) — looks up by
  name, validates `dwCharID`, replies with `RW_ENTERCHAR_ACK`
  carrying the char's cluster state. Guild / party / corps /
  tactics ids stay zero-default in W3a-3 (those registries don't
  hold member back-pointers yet); W3a-4 fills them.
* **OnRelayConnectReq** (RW_RELAYCONNECT_REQ, wID=0x99A5) —
  routes `MW_RELAYCONNECT_REQ` to the peer matching the char's
  `main_server_id` (LOBYTE of the peer's wID, set at
  RELAYSVR_REQ time). Legacy parity for `FindMapSvr(bMainID)`.
* **PeerRegistry::SnapshotExcept** — efficient
  "every other peer" iterator. Used by `OnRelaysvrReq` to fan
  out the legacy `(*it).second->SendMW_RELAYCONNECT_REQ(0)`
  broadcast when a new relay registers.
* **Sender batch** — `SendRwEntercharAck` (16-field reply,
  legacy RWSender.cpp:34) + `SendMwRelayconnectReq` (legacy
  SSSender.cpp:3062).

New tests:
* `test_char_name_index` — 7 scenarios: insert+rename round trip,
  case insensitivity, rename clears the old entry, drop-only
  rename (empty new_name), collision refusal, Remove drops both
  indices, concurrent renames on disjoint chars × 4096 entries.
* `test_char_base_handlers` — 5 wire scenarios driving every
  `bType` branch on a real socket + a follow-up valid packet
  proves the framer stays alive after the unknown-bType drop.
* `test_rw_lookup_handlers` — 4 wire scenarios across two real
  peer connections: ENTERCHAR hit + miss, RELAYCONNECT routing
  by main_server_id, and the cluster broadcast triggered by a
  second RELAYSVR registration.

### W3a-2 — what landed

PeerSession wraps every accepted WorldSession so handlers see
map-server identity (wID + nation flag) without going through
PeerRegistry on every packet. PeerRegistry indexes the live
peers by wID; OnRelaysvrReq registers the peer on RW_RELAYSVR_REQ
and WorldServer's HandleConnection exit-path unregisters cleanly.
The dispatch signature changes to `shared_ptr<PeerSession>` —
all char/guild handlers updated.

The first batch of SSSender lands under `senders/senders_*.cpp`:
* `SendRwRelaysvrAck` — reply to RW_RELAYSVR_REQ (nation + empty
  operator/msg lists; full lists arrive in W5 castle-war).
* `SendMwGuildEstablishReq` — completes the OnGuildLoadAck round
  trip (was a TODO in W3a-1; now actually fires the ACK back to
  the originating map server).

OnAddCharAck now stamps the real `server_id` (LOBYTE of peer wID)
on the inserted `TCharCon` — closes a W2 TODO. OnGuildLoadAck
now sends its long-pending MW_GUILDESTABLISH_REQ reply, completing
the legacy `SSHandler.cpp:9019` round-trip.

New tests: `test_peer_registry` (6 scenarios — empty, Register,
sentinel rejection, duplicate retains original, idempotent
Unregister, Snapshot) and `test_relay_handlers` (5 wire scenarios
— register+ACK with nation echo, sentinel reject, duplicate-wID
on a second socket keeps original entry, disconnect unregisters).

### W3a-1 — what was done

The 76 guild handlers split into three PRs because the W3a-1
**infrastructure** alone (registry + repo + schema validator + 1
handler) needs to land first so W3a-2/-3 can focus on handler
wiring without infrastructure churn.

**Done in W3a-1:**
* `services/guild_registry.{h,cpp}` — same 16-shard partitioning
  as CharRegistry; `TGuild` POD with the fields OnGuildLoadAck
  and (W3a-2) OnGuildInfoAck read. Per-guild mutex for the
  actor-model writes guild handlers need.
* `services/guild_repository.h` — `IGuildRepository` interface
  with the read path (`LoadAll`, `FindById`). The write path
  (`Save`, `Disorg`, member CRUD) lands in W3a-2 alongside the
  matching handlers.
* `services/soci_guild_repository.{h,cpp}` — SOCI implementation
  against `TGUILDTABLE` + `TGUILDMEMBERTABLE`. Two batched
  queries on boot warmup (one for guilds, one for all members
  joined back in-memory by `dwGuildID`) — beats per-guild
  fan-out at the legacy population sizes.
* `services/fake_guild_repository.{h,cpp}` — in-memory test impl
  with `AddGuild` seed API; deep-copies on `LoadAll` /
  `FindById` so test mutation doesn't bleed into the seed.
* `db/schema_validator.{h,cpp}` — fail-fast on missing
  `TGUILDTABLE` / `TGUILDMEMBERTABLE` columns. Optional warns
  for `TGUILDARTICLETABLE`, `TGUILDCABINETTABLE`,
  `TGUILDTACTICSTABLE` (the tables W3a-2/-3 will demand).
* `main.cpp` wires the guild registry, runs the schema validator
  when `[database]` is set, and warms the cache from
  `IGuildRepository::LoadAll`.
* `handlers/handlers_guild.cpp` — `OnGuildLoadAck` (DM_GUILDLOAD_ACK,
  wID=0x58FA). Parses the 21-field guild row + chief char back-link
  + cabinet count (items themselves are W3a-2 — discarded for now).
  Inserts a `TGuild` into the registry with the founder as the
  first member; gates on char-registered + key-match.
* Tests: `test_guild_registry` (5 scenarios), `test_guild_handlers`
  (4 wire scenarios + framer-survives-truncated-body), and
  `test_fake_guild_repo` (deep-copy isolation + nullopt on miss).

**Deferred to W3a-3 (each gated on a piece that doesn't exist yet):**
* **OnRW_ENTERCHAR_REQ** — needs `CharRegistry::FindByName`
  (secondary name index) + the per-char `country` / `name` fields
  that arrive with the W2 OnMW_CHANGECHARBASE_ACK port.
* **OnRW_RELAYCONNECT_REQ** — needs `PeerRegistry::Find` (already
  shipped in W3a-2) plus `SendMW_RELAYCONNECT_REQ` (one-line
  sender, lands with the next batch).
* **Cabinet item codec** — OnGuildLoadAck still skips items.
  The legacy `Lib/Own/TProtocol/ITEM` struct is non-trivial; W3a-3
  ports both the codec and the storage container.
* **Guild-level table cache** (`FindGuildLevel(bLevel)`) — a small
  separate read-only table; lands with the first mutating handler
  that needs it (probably `MW_GUILDLEAVE_ACK`).
* **~25 mutating guild handlers** — Establish / Update / Disorg /
  Member add+leave+duty / Kickout / Fame / Contribution / Peer.
  Each adds 1–2 senders to the sender table.
* **Cluster-wide RELAYCONNECT broadcast** — when a relay registers,
  legacy fans an MW_RELAYCONNECT_REQ to every other peer. Drops
  in once the broadcast helper lands (it's also needed by the
  W3a-3 announcement/chat-broadcast handlers).

### W2 — what's done, what's deferred

A grep of the legacy code showed that **TWorldSvr does NOT load
characters from the database** — it's a cluster-wide in-memory
index of chars that map servers have already loaded. The legacy
`DBAccess.h` has 78 `CSP*` stored-procedure wrappers but none of
them are char-load (the char-load CSPs live in TLoginSvr +
TMapSvr). So "char persistence" in TWorld really means **char
registry** — the in-memory `m_mapTCHAR` + `m_mapACTIVEUSER` —
which is exactly the global-lock target named in PATCH_README §6
W-2.

**Done in W2:**
* `services/char_registry.{h,cpp}` — 16-shard hash map with
  per-shard `std::shared_mutex` (fast read path under the W3
  guild lookups) + per-char `std::mutex` for field-level mutation
  ("per-char actor model" from W-2). Active-user index is a
  separate sharded set.
* DB infrastructure in `main.cpp` — when `[database]` is set, a
  `fourstory::db::SessionPool` + `boost::asio::thread_pool` come
  up. W2 doesn't issue any queries; W3a is the first phase that
  exercises this.
* `handlers/handlers_char.cpp` — `OnAddCharAck` (MW_ADDCHAR_ACK)
  inserts into the registry, marks user active, and handles the
  "additional connection" branch (TCharCon push). `OnCloseCharAck`
  (MW_CLOSECHAR_ACK) removes the entry, deactivates the user
  when no other char of theirs is online.
* Tests: `test_char_registry` (6 scenarios — insert/find/remove,
  duplicate-insert rejection, snapshot consistency, active-user
  index, concurrent inserts × 16k chars, shared_ptr lifetime),
  `test_char_handlers` (5 wire scenarios — happy path, additional
  connection, wrong key, close + user deactivation, stale close).

**Deferred to W3+ (each gated on a piece that doesn't exist yet):**
* **Peer-server registry** — `OnAddCharAck` records
  `TCharCon::server_id = 0` because the WorldSession doesn't yet
  carry the map-server's `wID`. W3 introduces a `PeerSession`
  wrapper (parallel to `Server/TControlSvrAsio/peer_session.h`)
  that knows which map server is on the other end.
* **`SendMW_ENTERSVR_REQ`** — the ACK back to the map server
  saying "yes, the char is registered". Requires a sender layer
  (TWorldSvr's `SSSender.cpp` counterpart) which is 4046 LOC; the
  first batch lands with the W3a guild work because the guild
  acks need it too.
* **`SendMW_INVALIDCHAR_REQ`** — fired on wrong-key collision
  ("possible session hijack" branch in the legacy module). Same
  blocker as ENTERSVR_REQ.
* **Cross-map `MW_DELCHAR_REQ` cleanup** — when CLOSECHAR_ACK
  arrives for an unknown char (stale close), the legacy server
  fires DELCHAR back to the map so the map cleans up its half.
  W2 logs it and skips; the map server's own close-loop converges
  the state without our reply.
* **Guild/party/BR/BoW side-effects on ADDCHAR_ACK** — the
  legacy module touches `m_pBOWModule`, `m_pBRModule`, and looks
  up guild membership when a char enters. Each of those modules
  is its own W3+/W6 PR.

The W2..W7 split was sized against a real `grep` of
`Server/TWorldSvr/SSHandler.cpp` (287 unique handlers across 4
families: OnMW=160, OnDM=88, OnCT=23, OnSM=16) plus 3 handlers in
`RWHandler.cpp`. See §4 for the per-feature handler counts that
drove the split.

> **Gating clause** (from PATCH_README §6): the legacy World↔Map
> wire contract is the bigger constraint for this port. W1 ships a
> transport-only scaffold that is safe to land now because the
> packet IDs aren't dispatched yet. **Per-feature handler work in
> W2+ should wait until [`TMapSvrAsio`](../TMapSvrAsio/README.md)'s
> MW wire shape stabilises**, otherwise W2 handlers may need to be
> reshaped when TMap's side lands. Holding the scaffold is OK; the
> infrastructure work in W2 (Soci char repo, dispatch family-file
> split) is independent of the wire shape.

## 1. Directory layout

```
Server/TWorldSvrAsio/
├── CMakeLists.txt                  — wired into root CMakeLists.txt
├── README.md                       — this file
├── tworldsvr.example.toml          — annotated reference TOML
├── main.cpp                        — boot, signals, SessionPool +
│                                     worker pool + CharRegistry
├── config.{h,cpp}                  — toml++ → AppConfig POD
├── world_server.{h,cpp}            — accept loop on DEF_WORLDPORT,
│                                     max_connections gate
├── world_session.{h,cpp}           — CPacket framing per peer
│                                     (plain TCP, no RC4 on SS link)
├── peer_session.h                  — wraps WorldSession with wID +
│                                     nation (per-map-server identity)
├── db/
│   ├── schema_validator.h          — boot-time TGUILD* column check
│   └── schema_validator.cpp        — required + optional probes
├── services/
│   ├── char_registry.{h,cpp}       — 16-shard partitioned char
│   │                                 index + active-user set
│   ├── guild_registry.{h,cpp}      — 16-shard partitioned guild
│   │                                 index, per-guild mutex
│   ├── guild_repository.h          — IGuildRepository interface
│   ├── fake_guild_repository.{h,cpp}
│   │                               — in-memory test impl w/ seed
│   ├── soci_guild_repository.{h,cpp}
│   │                               — SOCI impl, batched LoadAll
│   └── peer_registry.{h,cpp}       — by-wID hash index of live
│                                     PeerSession objects
├── handlers/
│   ├── handlers.h                  — HandlerContext + decls
│   ├── dispatch.cpp                — switch on wID, drops unknown
│   ├── handlers_char.cpp           — MW_ADDCHAR_ACK +
│   │                                 MW_CLOSECHAR_ACK
│   ├── handlers_guild.cpp          — DM_GUILDLOAD_ACK
│   └── handlers_relay.cpp          — RW_RELAYSVR_REQ (W3a-2)
├── senders/
│   ├── senders.h                   — outbound packet builders;
│   │                                 family-file split inside
│   ├── senders_relay.cpp           — SendRwRelaysvrAck
│   └── senders_guild.cpp           — SendMwGuildEstablishReq
├── wire_codec.h                    — POD reader/writer + length-
│                                     prefixed string (CPacket layout)
└── tests/
    ├── test_dispatch.cpp           — wire framing + checksum
    ├── test_char_registry.cpp      — 6 scenarios (incl. 16k-char
    │                                 concurrent insert)
    ├── test_char_handlers.cpp      — 5 wire scenarios for ADD/CLOSE
    ├── test_guild_registry.cpp     — 5 scenarios (incl. 16k-guild
    │                                 concurrent insert)
    ├── test_guild_handlers.cpp     — 4 wire scenarios + framer-
    │                                 survives-truncated-body
    ├── test_fake_guild_repository.cpp
    │                               — deep-copy isolation + nullopt
    ├── test_peer_registry.cpp      — 6 scenarios for Register /
    │                                 Find / Unregister (W3a-2)
    └── test_relay_handlers.cpp     — 5 wire scenarios for
                                      RW_RELAYSVR_REQ / ACK (W3a-2)
```

W3a-3 lands OnRW_ENTERCHAR_REQ + OnRW_RELAYCONNECT_REQ + ~25
mutating guild handlers (Establish / Update / Disorg / Member
add+leave+duty / Kickout / Fame / Peer / Contribution) plus the
cabinet item codec and the guild-level table cache. The remaining
~50 guild handlers (tactics / volunteers / articles / pvp record /
point reward) ship in W3a-4.

## 2. Build

The scaffold is wired into the root `CMakeLists.txt` next to the
four shipped Asio daemons:

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target tworldsvr_asio
ctest --test-dir build -R tworldsvr_asio --output-on-failure
```

W1 has one test — `tworldsvr_asio_dispatch` — that stands the
server up on an ephemeral port, frames a known wID, an unknown
wID, and a corrupt-checksum frame, and verifies the framer accepts
the first two and closes on the third.

## 3. Running

```bash
./build/Server/TWorldSvrAsio/tworldsvr_asio --config Server/TWorldSvrAsio/tworldsvr.example.toml
```

The W1 binary accepts SS connections on `DEF_WORLDPORT` (3815),
logs every inbound packet through the dispatch stub, and exposes
a `/healthz` HTTP endpoint on port 18087. It does NOT touch the
database — `[database]` in the TOML is parsed but unused until W2.

## 4. Phasing — how the SSHandler.cpp surface splits

Real counts from `grep` of the 287 unique `On(MW|DM|CT|SM)_*` names
in `Server/TWorldSvr/SSHandler.cpp` (the legacy monolithic switch),
plus 3 `OnRW_*` in `RWHandler.cpp`. The per-feature totals drive
the W2..W7 PR sizing — guild is its own PR (W3a) because at 76
handlers it is bigger than every other phase combined.

| Phase | Buckets | Handler count |
|---|---|---|
| W2 | CHAR (4) + USER (2) + OnRW (3) + ACTIVECHARUPDATE + GETCHARINFO + ADDCHAR + CHANGECHARBASE + CHECKCONNECT | ~15 |
| W3a | GUILD | 76 |
| W3b | PARTY (8) + CORPS (6) | 14 |
| W4 | FRIEND (18) + CHAT (3) + SOULMATE (8) | 29 |
| W5 | WAR (2) + CASTLE (5) + TOURNAMENT (15) + TNMT (3) | 25 |
| W6 | BR (3) + BOW (1) + EVENT (11) + RPSGAME (3) + APEX / ARENA / BATTLEMODE | ~30 |
| W7 | ITEM (5) + CASH (5) + MONTHRANK (3) + CMGIFT (7) + HELPMESSAGE + PVPRECORD + RESERVEDPOST + cutover hardening | ~30 |

Each ported handler in W2..W7 also gets its matching sender —
`Server/TWorldSvr/SSSender.cpp` is 4046 LOC / ~196 sender funcs,
so realistic effort per phase is **2× the handler count** (handler
+ sender + DB wrapper where applicable).

## 5. Architectural patches that land alongside the handler work

From PATCH_README §6:

| ID | Severity | Patch | Status |
|---|---|---|---|
| W-1 | 🟡 | Async DB + per-shard write queue (legacy `m_hDB` is a single DB thread serving all TMapSvr instances) | 🟡 W2 wired `SessionPool` + `boost::asio::thread_pool`; **W3a-1 ships the first SOCI consumer** (`SociGuildRepository::LoadAll` at boot). Per-shard write queue arrives in W3a-2 with mutating guild handlers. |
| W-2 | 🟡 | Partition the global `m_mapTCHAR` / `m_mapTGuild` locks (per-char actor model, per-guild grain) | ✅ **W3a-1 closes the in-memory half.** Char (W2) + guild (W3a-1) both run on 16-shard registries with per-entry mutex. The DB-side per-shard write queue is the remaining piece (W3a-2). |
| W-3 | ✅ | TWorldSvrAsio binary doesn't exist yet | ✅ closed by W1 scaffold |

Other related concerns from `_rewrite/docs/MODERNIZATION_PLAN.md`:

* **`SSHandler.cpp` 14 615 LOC monolithic switch** — replaced by
  one `handlers/handlers_<feature>.cpp` per W2..W7 phase, dispatched
  through the W1 `Dispatch` stub which grows one `case` per ported
  handler. Same shape as `Server/TControlSvrAsio/handlers/`.
* **`DMSender.cpp` raw-pointer handoff** — 48 `new` / 1 visible
  `delete` for outbound DB write packets. The Asio port owns
  outbound bodies through `std::vector<std::byte>` + move semantics
  (see `WorldSession::SendPacket`), so the lifecycle hazard never
  reappears.
* **Threading model** — legacy `_ControlThread` + `_WorkThread` +
  `_BatchThread` (plus a `MAX_THREAD` worker pool) collapse onto a
  single `io_context`. CPU-bound batch work that needs off-reactor
  execution rides on the `db_pool` introduced in W2.
