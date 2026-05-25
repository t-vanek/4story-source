# TWorldSvrAsio — modernized cluster coordinator

Wire-compatible replacement for `Server/TWorldSvr/` (38 851 LOC,
30 files) running on the `FourStoryCommon` infrastructure (SOCI
pool, spdlog audit, health endpoint) and the `boost::asio` reactor
that the four shipped Asio daemons already use.

> Cluster context: [main README](../../README.md#overall-progress) ·
> patch catalog vs legacy Araz sources:
> [`_rewrite/docs/PATCH_README.md` §6](../../_rewrite/docs/PATCH_README.md#6-tworldsvr)

## Status — W6-35 Ctrl-svr identification (closes W6-34 admin path)

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
| W4-5 | Chat channel relay — OnMW_CHAT_ACK (GUILD/TACTICS/PARTY/FORCE/MAP/WORLD/SHOW/WHISPER) routed to the right audience via the guild/party/corps/peer registries + SendMwChatReq | ✅ |
| W4-6 | Soulmate — OnMW_SOULMATESEARCH/REG/END_ACK (matchmaking + mutual pairing / register-preview / dissolve) + TChar.real_sex/soulmate + 3 senders | ✅ |
| W4-7 | Social presence on logout — OnCloseCharAck fans FRIENDCONNECTION(DISCONNECTION) to friends + marks reverse friend/soulmate entries offline (NotifyFriends/SoulmateOnLogout) | ✅ |
| W4-8 | Region update — OnMW_REGION_ACK stores TChar.region + mirrors it into the soulmate + mutual-friend reverse entries (makes presence region live) | ✅ |
| W4-9 | Level update — OnMW_LEVELUP_ACK stores TChar.level + multi-connection LEVELUP_REQ fan-out + soulmate level-sync / auto-dissolve on gap + SendMwLevelUpReq | ✅ |
| W4-10 | Inspect-player stat relay — OnMW_CHARSTATINFO_ACK + OnMW_CHARSTATINFOANS_ACK (request routed to the target's map, the gathered stat block forwarded verbatim to the requester) | ✅ |
| W4-11 | TMS conference channels — TmsRegistry + TTms + TChar.tms id-set + OnMW_TMSSEND/INVITEASK/INVITE/OUT_ACK (open / fan-out / re-pair / tear-down a multi-party group chat) + 4 senders | ✅ |
| W4-12 | TMS presence on logout — NotifyTmsOnLogout (legacy LeaveTMS) wired into OnCloseCharAck: drops a logging-out char from every conference + tells the survivors via TMSOUT_REQ | ✅ |
| W4-13 | Mail delivery relay — OnMW_POSTRECV_ACK (player mail) + OnDM_RESERVEDPOSTRECV_ACK (system mail) forward MW_POSTRECV_REQ verbatim to the recipient's map (routed by target name) + SendMwPostRecvReq | ✅ |
| W4-14 | Per-character visual state sync — OnMW_PETRIDING_ACK (mount fan-out to the char's other map sessions) + OnMW_HELMETHIDE_ACK (helmet-visibility store + confirm) + TChar.riding/helmet_hide + 2 senders | ✅ |
| W4-15 | Friend/soulmate load-at-login — IFriendRepository (Soci + Fake) + OnAddCharAck hydrates TChar.friends/friend_groups/soulmate via CoOffloadIf with forward/reverse type derivation (FT_FRIEND / FT_FRIENDFRIEND / FT_TARGET) | ✅ |
| W4-16 | Friend-group write-back — IFriendRepository MakeGroup/DeleteGroup/RenameGroup/ChangeFriendGroup wired into the W4-3 group handlers via CoOffloadVoidIf (persist alongside the in-memory mutation) | ✅ |
| W4-17 | Friend-edge write-back — IFriendRepository InsertFriend/EraseFriend wired into the accept paths (both directed edges) + erase path (forward edge) via CoOffloadVoidIf | ✅ |
| W4-18 | Soulmate write-back — IFriendRepository RegSoulmate/DelSoulmate wired into SEARCH-pair / REG / END + the W4-9 level-gap auto-dissolve (both mutual rows) via CoOffloadVoidIf | ✅ |
| W4-19 | GM chat ban — OnMW_CHATBAN_ACK sets/extends/clears TChar.chat_ban_time, enforces on the target's map + echoes to the issuing GM (MW_CHATBAN_REQ) | ✅ |
| W4-20 | Login finalization — OnMW_ENTERSVR_ACK indexes the char's name + bulk-sets identity/position/region, then fires NotifyFriendsOnLogin (connect-presence fan-out — now unblocked) | ✅ |
| W4-21 | Friend-protected presence sync — OnMW_PROTECTEDCHECK_ACK is the symmetric partner to W6-9. The map updates the connect/disconnect status of a protected friend; world mirrors the transition across both directed edges (when both still hold each other), syncs regions, and relays MW_FRIENDCONNECTION_REQ to the target's map (skipping FT_TARGET — legacy parity). On disconnect, also fires the W4-17 IFriendRepository::EraseFriend write-back. Missing char/friend/edge → silent drop | ✅ |
| **W4-22** | Fresh-login ENTERSVR completion — OnEnterSvrAck now does the legacy fresh-login chain after the W4-20 identity load: build the CHARINFO_REQ composite from in-memory guild + tactics + party state (FindMember / FindTactics for the per-char castle/duty/peer) and send it back to the responder; ROUTE_REQ so the main can resolve any additional connections (answered by W6-20's MW_ROUTE_ACK); MW_FRIENDLIST_REQ (groups + non-pending friends, online state resolved live like W4-4); MW_CHATBAN_REQ when the char's chat_ban_time is still active. 2 senders (CharInfoPayload + CHARINFO_REQ, ROUTE_REQ); reuses FRIENDLIST/CHATBAN. Deferred: BR/Bow bow_release flag, RW_CHANGEMAP relay-server hop, APEX notify (TW), cluster-wide chat-ban list | ✅ |
| W6-23 | CHARDATA_ACK drift fan-out — closes the W6-20 deferral. When CHARDATA_ACK arrives but some cons haven't ENTERCHAR_ACKed yet, world fans MW_ENTERCHAR_REQ (33-field composite + opaque recall-mon tail lifted verbatim from the inbound body) to each non-ready con. Each map loads the char and replies ENTERCHAR_ACK → CheckMainCon completes the loop. 1 new sender (EnterCharReqPayload + SendMwEnterCharReq); reuses GuildRegistry / PartyRegistry / CorpsRegistry for the composite | ✅ |
| W4-23 | Fresh-login fidelity polish — closes the two W4-22 placeholder-zero defaults. (1) SendMwFriendListReq now takes a soulmate_target DWORD that both W4-4 (OnFriendListAck) and W4-22 (OnEnterSvrAck) populate from TChar.soulmate.target — clients render the soulmate slot live. (2) CharInfoPayload.bow_release is set to 1 when chg_main_id arrives as BOW_SERVER_ID or BR_SERVER_ID, matching legacy SSHandler.cpp:1456 (the chg_main_id stays after the fresh-login emit — legacy quirk preserved) | ✅ |
| W6-24 | Bow battleground opener — first W6 🚧 content subsystem ported. OnAddToBowQueueReq derives effective_country (primary, falling back to aid_country when primary > TCONTRY_C — legacy gate), picks tactics_id > guild_id > 0 for the queue-grouping hint, calls BowRegistry::AddPlayer, replies MW_ADDTOBOWQUEUE_ACK(result, char_id, key, tick). OnCancelBowQueueReq runs RemovePlayer + emits MW_CANCELBOWQUEUE_ACK + BR fall-through (W6-25). OnBowPointsUpdateReq bumps the per-country scoreboard. New BowRegistry + bow_constants.h. Deferred: BS_PEACE/BS_ALARM status gating, match creation + team balancing, teleportation, per-guild queue grouping | ✅ |
| W6-25 | Battle Royale opener — second W6 🚧 content subsystem. 5 handlers covering the player-driven queue + premade team flow + map/mode votes: OnAddToBrQueueReq (enqueue OR ready-signal — chief→FlagTeamReady, mate→FlagPlayerReady), OnBrTeamMateAddReq (chief invites mate by name → forwarded SUCCESS dialog on mate's map / NOTFOUND to chief on self/unknown), OnBrTeamMateDelReq (chief drops mate or mate self-leaves), OnBrTeamMateAddResultAck (mate's accept/refuse — SUCCESS path runs the duplicate + cap gates and JoinPremadeTeam + UPDATEBRTEAM broadcast to every team member's map), OnVoteForBrMapReq (per-user first-vote-wins map and mode tallies). New BrRegistry + br_constants.h (TEAMADD_* + BR_TEAMMATE_MAX_COUNT(BR_3V3)=3). Wired W6-24's OnCancelBowQueueReq BR fall-through (legacy SSHandler.cpp:14078). Deferred: BS_PEACE/BS_ALARM gating, UpdatePlayerQueue / CreateMatch / team balancing, BR_SOLO vs BR_TEAM switch | ✅ |
| W6-26 | LEAVEBATTLEFIELD cleanup — OnLeaveBattlefieldReq routes by location (channel == BR_SERVER_ID → BrRegistry::ReleaseSinglePlayer; else map_id == BOW_MAP_ID → BowRegistry::ReleaseSinglePlayer). Both registry methods do opportunistic queue + premade drops (legacy teleport-home from active-match state is deferred — we don't model the active match yet). Off-battlefield chars are a silent no-op (legacy parity). Closes the W6-25 deferred "ReleaseSinglePlayer on logout" note | ✅ |
| W6-27 | BattleMode status + CM teleport — `OnBattleModeStatusReq` replies `MW_BATTLEMODESTATUS_ACK` on the char's main map carrying the quiescent payload (zeros + TCONTRY_N for bow_winner — same shape the legacy emits when neither subsystem is running). `OnCmTeleportBattleModeReq` routes by `system_type`: SYSTEM_BOW → `BowRegistry::AddPlayer(country=TCONTRY_C)` (admin force-add; our registry doesn't model the BS_ALARM/Admin gate so it just accepts), SYSTEM_BR → no-op (legacy body is empty — a TODO in the original). 1 new sender (`SendMwBattleModeStatusAck`). Closes 2 of the Arena/BattleMode trio | ✅ |
| W6-28 | ARENAJOIN — closes the Arena trio. `OnArenaJoinAck` flips `TParty.arena` to the inbound `join` flag; on the join path: if the party was in a corps, runs `NotifyCorpsLeave` so the corps unwinds (arena parties must be standalone); for each party member NOT in the inbound keep-list, runs `LeaveParty` (kick=1 so survivors get the right PARTYDEL flags). Refactor: `NotifyCorpsLeave` extracted from handlers_corps.cpp's anonymous namespace + declared in handlers.h (`LeaveParty` stays file-local — `OnArenaJoinAck` lives in handlers_party.cpp). No reply | ✅ |
| W6-29 | RPS event — small dedicated subsystem (rock-paper-scissors event game). `OnRpsGameAck` runs the win-keep cap gate via `RpsRegistry::RecordWin` (prunes >30-day-old entries, counts entries within win_period, denies on cap hit) + replies `MW_RPSGAME_REQ(result, player_rps)`. `OnRpsGameDataReq` snapshots every config row for the `CT_RPSGAMEDATA_ACK` reply. `OnRpsGameChangeReq` applies admin updates (silent-drop on unknown key, legacy parity), then ACKs the requester + broadcasts the verbatim `MW_RPSGAMECHANGE_REQ` body to every map peer. `OnRpsGameRecordReq` is a logged stub (DB persistence to TRPSGAMERECORDTABLE deferred — no IRpsRepository yet). New RpsRegistry + 3 senders | ✅ |
| W6-30 | Event subsystem opener — `OnCtEventMsgReq` fans the operator event-message line (event_id + msg_type + msg) verbatim to every map peer as `MW_EVENTMSG_REQ`. Pure broadcast — no per-event state. 1 new sender (`SendMwEventMsgReq`). Companion to W6-1's `SM_EVENTQUARTER_REQ`/`SM_EVENTQUARTERNOTIFY_REQ` broadcasts. Deferred to follow-ups: `CT_EVENTUPDATE_REQ` (full EVENTINFO codec + replay-on-connect + LOTTERY/GIFTTIME special-cases — heavy slice), `CT_EVENTQUARTERLIST/UPDATE_REQ` (DataSvr forwarding — no DataSvr peer in our infra), `SM_EVENTEXPIRED_REQ/_ACK` (W3a-19/W3a-36 sweepers already supersede the legacy timer-fed expiry path), `DM_EVENTQUARTER*` (heavy SOCI — no IEventQuarterRepository) | ✅ |
| W6-31 | Event update — `OnCtEventUpdateReq` adds the W6-30 deferred piece. New `EventRegistry` stores active events keyed by `dw_index` with the EVENTINFO body kept opaque (only the routing fields `dw_index` + `b_id` are surfaced); value==0 erases (legacy "deactivate"), value!=0 inserts. Then fans `MW_EVENTUPDATE_REQ(event_id, value, <opaque body>)` to every map peer — same shape legacy emits via `pEvent->WrapPacketIn` (SSSender.cpp:3270), avoiding a full WrapPacketOut/WrapPacketIn parse round-trip. `event_id > kCount` is dropped (SSHandler.cpp:276). LOTTERY/GIFTTIME body-ids short-circuit to log+drop (legacy runs `LotteryItem`/`GiftTime` reward subsystems on the world server — random char pick + in-game mail via SendPost + `MW_EVENTMSGLOTTERY_REQ`; helpers not ported yet). New `event_constants.h` (kLottery=14, kGiftTime=15, kCount=16) + `event_registry.h/.cpp` + 1 sender (`SendMwEventUpdateReq`). Replay-on-connect (legacy SSHandler.cpp:664 walks `m_mapEVENT` on a new map peer's connect) deferred — `EventRegistry::Snapshot()` is the hook | ✅ |
| W6-32 | Event replay-on-connect — wires `EventRegistry::Snapshot()` into `OnRelaysvrReq`. After the joining peer is registered + RELAYSVR_ACK'd and the cluster gets its RELAYCONNECT broadcast, the handler walks the active-event snapshot and re-fires `SendMwEventUpdateReq` (W6-31's verbatim relay) on this peer only. Closes legacy SSHandler.cpp:662-664 ("for each event in m_mapEVENT, re-send to this server"). The other legacy replays at the same site (CASHITEMSALE, castle applicant counts) stay deferred — they touch state we haven't ported yet | ✅ |
| W6-33 | Cash-shop sale — `OnCtCashItemSaleReq` ports the admin-driven cash-shop sale campaign (SSHandler.cpp:342). value!=0 stores a new (dw_index → items[]) row; value==0 deactivates an existing row in-place (zero `sale_value` on every item, keep the entry so replay-on-connect still shows it — legacy parity SSHandler.cpp:372-385); deactivate-miss is silently dropped (no broadcast — legacy SSHandler.cpp:393-397 logs an error and returns). Then fans `MW_CASHITEMSALE_REQ(dw_index, value, count, items[])` to every map peer. `OnCtCashShopStopReq` is the operator emergency-stop relay (SSHandler.cpp:328) — pure broadcast of `MW_CASHSHOPSTOP_REQ(type, send_player=1)`. Replay-on-connect: `OnRelaysvrReq` extension walks `CashItemSaleRegistry::Snapshot()` (mirrors W6-32 for events) and re-fires `SendMwCashItemSaleReq` per row — closes legacy SSHandler.cpp:666-668. New `cash_item_sale_registry.h/.cpp` + `handlers_cashshop.cpp` + `senders_cashshop.cpp` (2 senders). Castle-applicant replay (SSHandler.cpp:670-680) and expired-buffer init (:682+) at the same site stay deferred | ✅ |
| W6-34 | CMGift result relay — `OnCmGiftResultAck` ports the in-game GM-issued cash-gift completion handler (SSHandler.cpp:13988). Map server reports `(result, tool, gm_id)` after firing the gift transaction; world routes `MW_CMGIFTRESULT_REQ(result, gm_id)` to the GM's main map so the client renders the success/failure dialog. The tool=1 admin path was deferred in W6-34 (closed in W6-35). Missing GM char / `main_server_id=0` / target peer offline are silent drops (legacy SSHandler.cpp:13769-13783). Wire ID quirk: `MW_CMGIFTRESULT_REQ` and `MW_CMGIFTRESULT_ACK` share `0x9178` (MWProtocol.h:522-523) — dispatcher keys on the `_ACK` enum, sender targets the `_REQ` enum, both resolve to the same uint16. 1 new sender (`SendMwCmGiftResultReq`). The rest of the CMGift family (`CT_CMGIFT_REQ/LIST/CHARTUPDATE` + `DM_CMGIFT*` + `CMGiftRegistry` + the SOCI repo) stays deferred — see README §C | ✅ |
| **W6-35** | Ctrl-svr peer identification — `OnCtCtrlsvrReq` ports the legacy ctrl-svr handshake (SSHandler.cpp:207). Pure single-cell store: the connecting peer fires an empty `CT_CTRLSVR_REQ`, world stashes the inbound `peer` in a new `CtrlSvrSlot` (weak_ptr-backed so a dropped session naturally expires the slot — better than legacy's dangling-pointer behaviour). Unlocks the W6-34 tool=1 admin path: `OnCmGiftResultAck` now consults `ctx.ctrl_svr->Get()` and fires `SendCtCmGiftAck(result, gm_id)` when the slot is live; empty slot or expired weak_ptr both silently drop (matches legacy `if(m_pCtrlSvr) ...`). New `services/ctrl_svr_slot.h/.cpp` + `handlers/handlers_ctrlsvr.cpp` + 1 sender (`SendCtCmGiftAck`). Same hook will land the deferred CT-bound replies in `OnCT_ITEMSTATE_ACK`, `OnCT_CMGIFTLIST_ACK`, `OnCT_CASHITEMSALE_ACK`, etc. when those subsystems get ported | ✅ |
| W4-24+ | Relay CHANGEMAP + failure replies; cluster-wide chat-ban list; APEX | ⏸ |
| W5-1 | Territory occupation broadcasts — OnMW_CASTLEOCCUPY/LOCALOCCUPY/MISSIONOCCUPY_ACK fan the new owner+flag to every map peer (+ LOCAL B-country display flip) + 3 senders; guild stat-exp + castle-apply reset deferred (absent constants/model) | ✅ |
| W5-2 | Castle-war apply — OnMW_CASTLEAPPLY_ACK (chief assigns a member/tactics to a castle, 49-cap via CanApplyWar, toggle-cancel) + dual reply + applicant-count broadcast (NotifyCastleApply); TGuildMember/TTacticsMember castle/camp + 2 senders. DB persist deferred | ✅ |
| W5-3 | Castle-occupy application reset — OnMW_CASTLEOCCUPY_ACK now runs ResetCastleApply for the winning + losing guild (clears each applicant's castle/camp + tells their map), closing W5-1's deferred reset; the guild stat-exp award stays deferred (absent constants) | ✅ |
| W5-4 | War-window enable broadcast — OnSM_BATTLESTATUS_REQ fans the LOCAL/CASTLE/MISSION enable packet to every map peer (the trigger that starts the sieges) + 3 senders; BS_PEACE record bookkeeping + SKYGARDEN deferred | ✅ |
| W5 | War + Castle + Tournament / TNMT | 🚧 |
| W6-1 | Timed-event broadcast — OnSM_EVENTQUARTER_REQ (present event, single server-chosen bucket) + OnSM_EVENTQUARTERNOTIFY_REQ (world-chat announcement via the chat sender) fan to every map peer + SendMwEventQuarterReq | ✅ |
| W6-2 | Combat / taming cross-server relays — OnMW_MAGICMIRROR_ACK (verbatim) + OnMW_MONTEMPT_ACK + OnMW_MONTEMPTEVO_ACK route the effect to the attacker char's map + 3 senders; GETBLOOD deferred (OT_PC absent) | ✅ |
| W6-3 | Global announcement broadcasts — OnMW_FAMERANKUPDATE_ACK (verbatim) + OnMW_HEROSELECT_ACK fan to every map peer + 2 senders | ✅ |
| W6-4 | Recall-mon (summoned creature) sync — OnMW_CREATERECALLMON_ACK (assigns the recall id, mirrors to the char's connections) + OnMW_RECALLMONDATA/DEL_ACK (verbatim) + 3 passthrough senders; id-counter DB-seed deferred | ✅ |
| W6-5 | Companion-mon (spolecnik) sync — OnMW_CREATESPOLECNIKMON_ACK + OnMW_SPOLECNIKMONDEL_ACK (recall-mon's sibling; shares the recall-id counter + connection fan-out) + 2 passthrough senders | ✅ |
| W6-6 | Monster-result relays — OnMW_MONSTERDIE_ACK + OnMW_TAKEMONMONEY_ACK route verbatim to the char's main map (id+key) + 2 senders; MONSTERBUY deferred (guild-money + MSB_* absent) | ✅ |
| W6-7 | Solo-instance party lifecycle — OnMW_ENTERSOLOMAP_ACK (spins up a 1-member PT_SOLO party, mirrors to the char's connections) + OnMW_LEAVESOLOMAP_ACK (tears it down) + SendMwEnterSoloMapReq; uses PartyRegistry | ✅ |
| W6-8 | GM char message relay — OnCT_CHARMSG_ACK routes a control-server system/GM message (≤1 KiB) to the named char's main map (MW_CHARMSG_REQ) + sender | ✅ |
| W6-9 | Friend-protected refuse relay — OnMW_FRIENDPROTECTEDASK_ACK relays an auto-refuse (FRIEND_REFUSE + requester name) to a protection-enabled target's map; completes the friend-ask protection sub-case | ✅ |
| W6-10 | Item-result relays — OnMW_ADDITEMRESULT_ACK (route to the requesting map server) + OnMW_DEALITEMERROR_ACK (route to the target char's map) + SendMwDealItemErrorReq (reuses the W3b SendMwAddItemResultReq) | ✅ |
| W6-11 | Day-change guild ranking — OnSM_CHANGEDAY_REQ recomputes every guild's PvP total/month rank over GuildRegistry (legacy CalcGuildRanking); read back by GuildInfoAck | ✅ |
| W6-12 | GM user-tracking relays — OnCT_USERPOSITION_ACK (locate, → MW_USERPOSITION_REQ) + OnCT_USERMOVE_ACK (force-move, → CT_USERMOVE_ACK) route to the target's map + 2 senders | ✅ |
| W6-13 | Connection-list reconcile — OnMW_CONLIST_ACK + OnMW_MAPSVRLIST_ACK (byte-identical) reconcile `cons` vs the reported set: drop stale cons to `dead_cons`, ROUTELIST new servers via the main map, else CHECKMAIN every remaining connection; DELCHAR/INVALIDCHAR error replies + 4 senders. First slice of the connection/teleport cluster | ✅ |
| W6-14 | Main-session confirmation — OnMW_CHECKMAIN_ACK: responder-is-main → drain `dead_cons` via CLOSECHAR (ClearDeadCON) + CONRESULT (CN_SUCCESS) the live set; responder-is-other → RELEASEMAIN the old main + re-point main; DELCHAR/INVALIDCHAR errors + 3 senders. CloseChar (logout teardown) + cession queue (PopConCess) deferred | ✅ |
| W6-15 | Main-session handoff forward — OnMW_RELEASEMAIN_ACK: the old main releases → forward the released char verbatim to the new main (re-tagged MW_ENTERSVR_REQ) + record the old main in `chg_main_id`; new main offline → INVALIDCHAR(release_main=1); unknown char → DELCHAR. Opaque-passthrough sender | ✅ |
| W6-16 | Handoff completion — OnMW_ENTERSVR_ACK now branches on `chg_main_id`: a normal-map handoff clears it + asks the new main for the server list (MAPSVRLIST_REQ → re-enters the W6-13 reconcile), skipping the fresh-login fan-out; BR/Bow battleground ids (50/30) excluded. Closes the handoff loop (reconcile→checkmain→release→entersvr→reconcile) | ✅ |
| W6-17 | Teleport begin + cession queue — OnMW_BEGINTELEPORT_ACK: same-channel fast path records the channel; else PushConCess serialises against any in-flight handoff and (if first) BeginTeleport broadcasts MW_STARTTELEPORT_REQ to every con. Deferred entries replay via PopConCess, now wired into CHECKMAIN_ACK. `TChar::con_cess` queue (legacy m_qConCess) + 1 sender | ✅ |
| W6-18 | Connect-check reconcile — OnMW_CHECKCONNECT_ACK (the other cession producer): updates the char's position then reconciles cons (count=0 → CHECKMAIN sweep; else drop stale → dead_cons + ROUTELIST new servers via main, else CHECKMAIN); replays via PopConCess. Reuses the W6-17 cession queue; no new senders | ✅ |
| W6-19 | CloseChar teardown — OnMW_CLOSECHAR_ACK now does the full legacy CloseChar: chg_main_id → INVALIDCHAR(release_main) on the would-be new main, DELCHAR every con (dead first, then live; logout/save flags only on the main), registry+name-index removal, friend/soulmate/TMS offline fan-out. Shared helper wired into the BeginTeleport/CheckConnect main-offline paths. Party-leave + guild/tactics DB persistence still deferred | ✅ |
| W6-20 | Connection-completion sub-flow — closes the W6-13/W6-18 reconcile loop. OnMW_ROUTE_ACK (main's reply to ROUTELIST): count==0 → SendMwCharDataReq; count>0 → register each (ip/port/server_id) as a *pending* TCharCon (valid=false, ready=false; existing entry's valid bit preserved) + SendMwAddConnectReq to the reporter. OnMW_ENTERCHAR_ACK flips the reporter's con.ready; all-ready → CheckMainCon broadcast. OnMW_CHARDATA_ACK refreshes level/HP/MP; all-ready → CheckMainCon. (Not-all-ready ENTERCHAR_REQ fan-out shipped in W6-23.) DELCHAR/INVALIDCHAR errors + 2 senders (ADDCONNECT_REQ / CHARDATA_REQ). Drive-by: add `<cstddef>` to guild_constants.h (MSVC doesn't pull it via `<cstdint>`) | ✅ |
| **W6-21** | Teleport confirm — OnMW_TELEPORT_ACK: happy path clears `party_waiter`, replies MW_TELEPORT_REQ(TPR_SUCCESS) to the responder + MW_CONLIST_REQ to the destination map (which re-enters the W6-13 reconcile to join the char's con set). Destination map offline → MW_TELEPORT_REQ(TPR_NODESTINATION) + CloseChar (the W6-19 helper). Unknown char / key mismatch → MW_DELCHAR_REQ. Closes the W6-17 BEGINTELEPORT chain end-to-end. 2 senders (TELEPORT_REQ / CONLIST_REQ) | ✅ |
| W6 | BR + Bow + Event + RPS + APEX / ARENA / BATTLEMODE | 🚧 |
| W7 | Item + Cash + MonthRank + CMGift + cutover hardening | ⏸ |

## Gaps audit — not yet ported / deferred (as of W6-35)

Legacy `Server/TWorldSvr/` declares **290** message handlers
(`CTWorldSvrModule::On*` — 160 MW + 88 DM + 23 CT + 16 SM + 3 RW, the same
breakdown the W2 sizing note records); **183** are ported in
`handlers/dispatch.cpp` (138 MW + 28 DM + 10 CT + 4 SM + 3 RW), leaving
**107** with no port. A portion of those are `DM_*` DB-thread round-trips
replaced by the repository pattern (§D) rather than wire handlers we still
owe; netting those out, the *owed* wire surface is ~266. Raw handler
coverage is **≈ 63 %** (183/290); against the owed surface it is ~69 %.
The unported remainder is the deferred subsystems in §C plus a number of
sub-branches deferred *inside* handlers that did land. (Note: the legacy
source is CP949 — grep it with `-a`, or whole handlers appear "missing"
when they are not.) This section is the authoritative checklist of what
is still open.

### A. Connection/teleport cluster — complete (W6-13 … W6-21)

The full cluster is ported. The `reconcile → ROUTELIST → ROUTE_ACK →
ADDCONNECT → ENTERCHAR_ACK / CHARDATA_ACK → CheckMainCON` chain (W6-13
… W6-20) closes the reconcile loop end-to-end; W6-21 ports the
`BEGINTELEPORT_ACK → … → TELEPORT_ACK → TELEPORT_REQ + CONLIST_REQ`
finish that closes the W6-17 teleport handshake. The sole sub-branch
deferred inside the cluster is the `CHARDATA_ACK` non-ready
`ENTERCHAR_REQ` fan-out — the fat composite is the same one
"Fresh-login ENTERSVR completion" (§B) owns, so it lands with that.

Intentionally not ported:

- **`OnMW_CONNECT_ACK`** (SSHandler.cpp:592, map-server registration)
  — replaced by `RW_RELAYSVR_REQ` (W3a-2).
- **`OnMW_TERMINATE_ACK`** (SSHandler.cpp:12818) — commented out in the
  legacy. The dead-code body broadcasts a `MW_TERMINATE_REQ` to every
  peer gated by a hard-coded magic key (`720809425`); a dev-time
  kill-switch backdoor. If a cluster-wide shutdown is ever needed it
  should go through proper control-server admin auth, not this
  pattern.

### B. Sub-branches deferred inside ported handlers

- **ENTERSVR fresh-login** (W4-22 + W4-23 + W6-16): the legacy
  fresh-login chain is now ported (CHARINFO_REQ + ROUTE_REQ +
  FRIENDLIST_REQ + CHATBAN_REQ when banned). The BR/Bow
  `bow_release` flag (W4-23) and the soulmate target in
  FRIENDLIST_REQ (W4-23) now carry live values too. Still
  deferred:
  - the `RW_CHANGEMAP_ACK` hop to the relay server (handed by
    the legacy `m_pRelay` — the asio relay wiring is a separate
    slice);
  - the cluster-wide `m_mapBanChar` ban list (W4-19 already
    syncs the per-char `chat_ban_time`, but the cluster-wide
    list isn't a thing here yet);
  - the APEX (Taiwan) notify.
  (The W6-20 `CHARDATA_ACK` non-ready `ENTERCHAR_REQ` fan-out
  shipped separately in W6-23 — it's a different 33-field
  composite, distinct from `CHARINFO_REQ`.)
- **CloseChar** (W6-19): party-leave on logout; guild/tactics DB
  persistence (`PVPRECORD`/`TACTICSPOINT`/`SaveGuildStats`). The sketchy
  legacy `if(!m_bSave) CloseChar` at the top of CHECKMAIN_ACK (a
  use-after-free in the original) is **intentionally** not ported.
- **Guild**: stat-exp award (absent constants), duty/peer/contribution
  caps, member-online-date, some broadcast fan-outs (GUILDDUTY to the
  target, chat re-broadcast).
- **Chat**: operator-whisper "/GM …" sub-case; cluster-wide ban list
  (`AddChatBan`).
- **Occupy/War** (W5): guild stat-exp award, `BS_PEACE` bookkeeping,
  SKYGARDEN, castle/camp in the member-load query, castle-apply DB persist.
- **Combat** (W6-2/6): `GETBLOOD` (absent `OT_PC`), `MONSTERBUY`
  (absent `MSB_*`).
- **Char-base**: ChangeCountry/ChangeName cluster fan-out
  (`RW_CHANGENAME_ACK`), region tracking.
- **Friend/Soulmate**: soulmate write-back is in-memory only (no soulmate
  repository); some friend write-backs.
- **Corps**: the `m_command` late-joiner ADDSQUAD cache; the MANSTAT
  corps-general relay.
- **Recall-mon**: id-counter DB-seed at boot.

### C. Whole subsystems with no port (the 112), grouped

**Roadmap W6 🚧 (battle / event content):**
- Battle Royale: queue + invite/del + accept + ready signal + map/mode
  vote landed in W6-25; the scheduler / match creation / BR_SOLO vs
  BR_TEAM switch / teleportation are deferred (see W6-25 row)
- Bow battleground: queue + cancel + points landed in W6-24; the
  scheduler / match creation / teleportation / per-guild grouping
  are deferred (see W6-24 row)
- Arena / BattleMode: all three handlers landed (W6-27 status +
  CM teleport; W6-28 ARENAJOIN). The Arena/BattleMode trio is
  complete.
- APEX (Taiwan): `MW_APEXDATA/APEXSTART`, `SM_APEXDATA/APEXKILLUSER`
- Tournament: `MW_TOURNAMENT/ENTERGATE/RESULT`, `DM_TOURNAMENT*` (6),
  `DM_TNMTEVENT*` (3), `SM_TOURNAMENT*` (3), `CT_TOURNAMENTEVENT`
  (blocked on `TNMTSTEP_*`)
- RPS event: all four handlers landed in W6-29; DB persistence
  (TRPSGAMERECORDTABLE via the legacy CSPRPSGameRecord SP) is
  deferred — no IRpsRepository yet (logged stub)
- Event subsystem (broader): W6-1 ported the `SM_EVENTQUARTER*` broadcasts;
  W6-30 ports `CT_EVENTMSG_REQ` (operator event-message line); W6-31 ports
  `CT_EVENTUPDATE_REQ` via an opaque-tail `EventRegistry` + broadcast;
  W6-32 closes the replay-on-connect loop (`OnRelaysvrReq` re-emits every
  active event to a joining peer — legacy SSHandler.cpp:662-664). Still
  deferred: LOTTERY/GIFTTIME reward subsystems (random char pick + in-game
  mail via SendPost + `MW_EVENTMSGLOTTERY_REQ`), `CT/DM_EVENTQUARTERLIST/UPDATE`
  (DataSvr forwarding), `SM_EVENTEXPIRED_REQ/_ACK` (W3a-19/W3a-36 sweepers
  already cover the wanted/tactics expiry paths)

**Roadmap W7 ⏸ (cash / item / rank):**
- CMGift: W6-34 ports `OnMW_CMGIFTRESULT_ACK` in-game GM path (relays
  `MW_CMGIFTRESULT_REQ` to the GM's main map); W6-35 wires the
  tool=1 admin path via the new `CtrlSvrSlot` (now fires
  `SendCtCmGiftAck` back to the ctrl-svr). Still deferred:
  `OnCT_CMGIFT_REQ`/`OnCT_CMGIFTLIST_REQ` (need
  `CMGiftRegistry` + `m_mapCMGift` boot-load),
  `OnCT_CMGIFTCHARTUPDATE_REQ` (DataSvr forwarder),
  `OnDM_CMGIFT_REQ`/`_ACK` and
  `OnDM_CMGIFTCHARTUPDATE_REQ`/`_ACK` (SOCI repository — no
  `ICmGiftRepository` yet)
- Cash-item sale: W6-33 ports `CT_CASHITEMSALE` (admin sale activation/deactivation) +
  `CT_CASHSHOPSTOP` (operator emergency-stop) + replay-on-connect. Still deferred:
  `MW_CASHITEMSALE_ACK` (the map's reply confirming a campaign landed) and
  `DM_CASHITEMSALE` (DB persistence of campaign rows — no IcashSaleRepository yet)
- MonthRank: `MW_MONTHRANKUPDATE/RESETCHAR`, `DM/SM_MONTHRANKSAVE`
- GM item tools: `MW_ADDITEM`, `CT/DM_ITEMFIND`, `CT/DM_ITEMSTATE`

**War/Castle extras (W5+):** `MW_CASTLEWARINFO`, `MW_ENDWAR`,
`MW_WARCOUNTRYBALANCE`, `MW_WARLORDSAY`, `MW_SKYGARDENOCCUPY`,
`CT_CASTLEGUILDCHG`, `DM_CASTLEAPPLY`

**Guild extras (blocked on absent constants/model):** `MW_GUILDSKILLACTION`,
`MW_MEETINGROOM`, `MW_UPDATEGUILDCOOLDOWN`,
`DM_GUILDTACTICSADD/DEL/WANTEDADD/WANTEDDEL`, `SM_GUILDDISORGANIZATION`

**Service / control plane:** `CT_CTRLSVR`, `CT_SERVICEDATACLEAR`,
`CT_SERVICEMONITOR`, `CT/DM_HELPMESSAGE`, `SM_DELSESSION`, `SM_QUITSERVICE`,
`DM_CLEARDATA`, `DM_CLEARMAPCURRENTUSER`, `DM_ACTIVECHARUPDATE`,
`DM_GETCHARINFO`

### D. "Missing" only on paper — replaced by a different mechanism

The legacy DB-thread round-trips are replaced by the repository pattern,
so these are not wire handlers we owe: `DM_FRIENDLIST/INSERT/ERASE/GROUP*`,
`DM_SOULMATELIST/REG/DEL/END` (soulmate persistence still in-memory),
`DM_TACTICSPOINT`, `DM_RESERVEDPOSTSEND` (generator poll — deferred).
`DM_GUILDLOAD` and `DM_PVPRECORD` *are* ported (as `_ACK`/`_REQ`).

### Suggested next slices (by value / self-containedness)

1. **CT_ITEMSTATE_ACK / CT_USERPROTECTED_REQ family** — small ops
   relays that the W6-35 CtrlSvrSlot now unlocks. SSHandler.cpp:
   10079 `OnDM_ITEMSTATE_ACK` forwards `CT_ITEMSTATE_ACK` to
   ctrl-svr verbatim; SSHandler.cpp:10662 `OnDM_CASHITEMSALE_ACK`
   sends `CT_CASHITEMSALE_ACK` on the wValue==0 deactivate path.
   Both are 1-2 line handlers now that the slot is live.
2. **APEX (Taiwan)** — small notify hook from W4-22 fresh-login.
3. **`TChar.soul_silence`** — trivial field add for the W6-23
   composite.
4. Larger roadmap subsystems (Tournament / MonthRank / GM item
   tools).

### W6-35 — what landed

**Ctrl-svr peer identification** — `OnCtCtrlsvrReq` ports the
legacy ctrl-svr handshake (SSHandler.cpp:207-215). The connecting
peer fires an empty `CT_CTRLSVR_REQ` after the TCP handshake;
world stashes the inbound `peer` in a new `CtrlSvrSlot`.
Subsequent handlers needing to route a reply back to the operator
console call `ctx.ctrl_svr->Get()`.

- `OnCtCtrlsvrReq` — empty-body handshake; stores the inbound
  `peer` in `ctx.ctrl_svr`. The legacy `OnCT_CTRLSVR_REQ` (which
  consists of `m_pCtrlSvr = pSERVER; return;`) does the same.
- `CtrlSvrSlot` — single-cell `weak_ptr<PeerSession>` store under
  a shared_mutex. `Set` overwrites (matches legacy's no-duplicate-
  detection — a fresh ctrl-svr reconnect just takes the slot).
  `Get()` upgrades the weak_ptr; returns nullptr if the slot was
  never set or if the previously-registered peer has disconnected
  (PeerRegistry drops the strong reference on read-loop exit, so
  the weak_ptr expires naturally — better than the legacy
  dangling-pointer behaviour).

The W6-34 deferred tool=1 admin path is closed by this slice.
`OnCmGiftResultAck` now consults the slot:
- empty slot → log + drop (matches legacy `if(m_pCtrlSvr) ...`);
- live slot → `SendCtCmGiftAck(result, gm_id)` on the ctrl-svr
  peer.

1 new sender (`SendCtCmGiftAck`). No registry / no constants.

Tests — extended `tests/test_cashshop_handlers.cpp` with a W6-35
sub-case on the existing fixture: send `CT_CTRLSVR_REQ` on p1
(empty body) → assert `ctrl_svr.Get()` is now non-null → fire
the same `tool=1` admin-path frame from p3 → assert p1 receives
`CT_CMGIFT_ACK(result=0, gm_id=1234)`. The prior W6-34 sub-case
that asserted "empty slot drops the frame" was kept and now reads
"empty slot logs `ctrl-svr offline`, frame still dropped" — both
states (never-registered, registered-then-disconnected) silently
drop.

Build verified: cmake + ctest -R tworldsvr_asio -C Debug on the
representative subset (13/13 passed).

### W6-34 — what landed

**CMGift result relay (in-game GM path)** — `OnCmGiftResultAck`
ports the map-server-to-world result handshake for an in-game GM-
issued cash gift (SSHandler.cpp:13988). The map fires the gift
transaction locally (W6-34 doesn't touch that path) and reports
back `(result, tool, gm_id)`; world routes
`MW_CMGIFTRESULT_REQ(result, gm_id)` to the GM's main map so the
GM's client renders the success/failure dialog.

- `OnCmGiftResultAck` — peek `(result, tool, gm_id)`. `tool=1`
  (admin path: GM ran the command from a ctrl-svr tool) → log +
  drop; legacy SSHandler.cpp:13999-14005 routes a `CT_CMGIFT_ACK`
  back to `m_pCtrlSvr`, but our infra doesn't yet identify the
  ctrl-svr peer (legacy `OnCT_CTRLSVR_REQ` is unported — that's
  the next suggested slice). `tool=0` (in-game GM path) → look up
  the GM char by `gm_id`, find their main map peer, fire
  `SendMwCmGiftResultReq`. Missing char / `main_server_id=0` /
  target peer offline are silent drops, matching legacy
  SSHandler.cpp:13769-13783.

Wire ID quirk — `MW_CMGIFTRESULT_REQ` and `MW_CMGIFTRESULT_ACK`
share the same `0x9178` (MWProtocol.h:522-523 — same `#define`
expansion). The dispatcher keys on the `_ACK` enum, the sender
targets the `_REQ` enum; both resolve to the same uint16. This
is the first wire ID we hit that's bidirectional — worth flagging
because if anyone adds a `MW_CMGIFTRESULT_REQ` case later, it
will dead-strip the `_ACK` branch silently. A `static_assert(
ToUint16(_REQ) == ToUint16(_ACK))` is the long-term hedge.

1 new sender in `senders_cashshop.cpp` (`SendMwCmGiftResultReq` —
2 fields). No new services or constants.

The rest of the CMGift family stays deferred (see README §C
"CMGift"): the `CT_CMGIFT*` admin handlers all need a
`CMGiftRegistry` mirroring legacy `m_mapCMGift` plus boot-time
load from the DB; the `DM_CMGIFT*` handlers need an
`ICmGiftRepository` (the SOCI repo isn't there yet); and the
tool=1 admin path here needs ctrl-svr identification.

Tests — extended `tests/test_cashshop_handlers.cpp` with three
W6-34 sub-cases on the existing 3-peer fixture: seed a GM char
on p1 (main_server_id=0x42), p3 reports `(result=0, tool=0,
gm_id=1234)` → p1 receives `MW_CMGIFTRESULT_REQ(result=0,
gm_id=1234)`; `tool=1` admin path → no frame anywhere (sentinel
verified via the next CASHSHOPSTOP); unknown `gm_id=9999` →
silent drop (sentinel verified).

Build verified: cmake + ctest -R tworldsvr_asio -C Debug on the
representative subset (dispatch + relay + chat + event +
event_replay + cashshop + rps + conn + bow + br + battlemode +
leavebattlefield + arenajoin — 13/13 passed).

### W6-33 — what landed

**Cash-shop sale** — `OnCtCashItemSaleReq` ports the legacy admin
cash-shop sale handler (SSHandler.cpp:342) and `OnCtCashShopStopReq`
ports the emergency-stop relay (SSHandler.cpp:328). Combined with
the W6-32 replay extension, this is the structured-payload cousin
of the W6-31/32 event-update pair: per-row store + broadcast +
replay-on-connect, but with a small typed model rather than the
opaque-tail relay W6-31 needed for the heavy EVENTINFO codec.

- `OnCtCashItemSaleReq` — reads `(dw_index, value, count, items[])`.
  `value != 0` stores the new row in `CashItemSaleRegistry` (replace
  semantics, keyed by `dw_index`) + broadcasts.
  `value == 0` calls `CashItemSaleRegistry::Deactivate(dw_index)`,
  which zeroes every item's `sale_value` *in place* and returns the
  zeroed row — legacy parity SSHandler.cpp:372-385 (the entry stays
  so the replay-on-connect path still surfaces it to joining maps).
  Deactivate-miss is silently dropped without broadcasting (legacy
  parity SSHandler.cpp:393-397).
- `OnCtCashShopStopReq` — single-byte `type`; broadcasts
  `MW_CASHSHOPSTOP_REQ(type, send_player=1)` to every peer
  (legacy sender's 2nd parameter has a default of TRUE; this is the
  only caller that exercises it).
- `OnRelaysvrReq` extension — after the W6-32 event walk, also
  snapshots `cash_sales` and re-fires `SendMwCashItemSaleReq` per
  row to the joining peer. Same shape, same coroutine.

New service: `CashItemSaleRegistry` (services/cash_item_sale_registry.
h/.cpp). `TCashItemSale{id, sale_value}` + `TCashItemSaleEvent
{dw_index, value, items[]}`. `Set` and `Deactivate` mutate under a
unique lock; `Snapshot` / `Erase` / `Size` follow the pattern of
the W6-31 EventRegistry.

2 new senders in `senders_cashshop.cpp`: `SendMwCashItemSaleReq`
(variable-length item list — `DWORD dw_index, WORD value, WORD count,
N x (WORD id, BYTE sale_value)`), `SendMwCashShopStopReq` (2 bytes).

Tests — new `tests/test_cashshop_handlers.cpp` (5 sub-cases on 3
peers): activate (3 items @ 100/50) → both peers receive verbatim;
deactivate (value=0) → both receive the zeroed row (entry kept,
size=1); deactivate-miss (unknown dw_index) → no peer frame
(sentinel verified via the next CASHSHOPSTOP); CASHSHOPSTOP →
`type=7, send_player=1` on both; third peer joins → receives the
W6-33 replay (zeroed dw_index=100 row) before any subsequent traffic.

Build verified: cmake + ctest -R tworldsvr_asio -C Debug on the
representative subset (dispatch + relay + chat + event +
event_replay + cashshop + rps + conn + bow + br + battlemode +
leavebattlefield + arenajoin — 13/13 passed).

### W6-32 — what landed

**Event replay-on-connect** — closes the W6-31 deferred piece by
wiring `EventRegistry::Snapshot()` into the W3a-2 relay handshake.
Legacy parity SSHandler.cpp:662-664: after a new map registers via
`OnMW_CONNECT_ACK`, the world walks `m_mapEVENT` and re-fires
`SendMW_EVENTUPDATE_REQ` for every active row so the joining map
sees the same event state everyone else does.

- `OnRelaysvrReq` extension — after the registry insert, the
  RELAYSVR_ACK reply, and the cluster-wide RELAYCONNECT fan-out,
  the handler grabs `ctx.events->Snapshot()` and emits one
  `SendMwEventUpdateReq` per entry on the *joining* peer (not the
  others — they already saw the activation when it landed). The
  replay is in the same coroutine as the rest of OnRelaysvrReq so
  it serializes against the peer's first inbound game packet
  (the map can rely on "all event state is present" by the time
  it sends its first MW_*).

No new senders / services. The other legacy replays at the same
site (CASHITEMSALE per SSHandler.cpp:666-668, castle applicant
counts per :670-680, expired-buffer init per :682+) touch state
we haven't ported yet and stay deferred — they'll naturally
land alongside their respective handlers.

Tests — new `tests/test_event_replay_handlers.cpp` (2 sub-cases):
pre-seed two events into `EventRegistry`, connect a peer → assert
RW_RELAYSVR_ACK followed by exactly two `MW_EVENTUPDATE_REQ`
frames carrying both rows verbatim (event_id / value / dw_index /
b_id / opaque trailer); erase both, connect a second peer →
assert RW_RELAYSVR_ACK with no follow-up replay (verified by
sending a CT_EVENTMSG_REQ sentinel and confirming the next
inbound frame is the MW_EVENTMSG_REQ, not a stale replay).

Build verified: cmake + ctest -R tworldsvr_asio -C Debug on the
representative subset (dispatch + relay + chat + event +
event_replay + rps + conn + bow + br + battlemode +
leavebattlefield + arenajoin — 12/12 passed).

### W6-31 — what landed

**Event update** — `OnCtEventUpdateReq` ports the legacy
admin-driven event activation handler (SSHandler.cpp:263). The
control server (ControlSvr) sends `BYTE event_id, WORD value, …`
followed by a serialized `EVENTINFO` payload (the heavy CashItem /
MonEvent / MonRegen / Lottery vector struct from
`TWorldType.h:724`). World stores the active events in a new
`EventRegistry` keyed by the EVENTINFO's `dw_index`, then fans
`MW_EVENTUPDATE_REQ` verbatim to every map peer.

- `OnCtEventUpdateReq` — peek `event_id, value, dw_index, b_id`,
  capture the rest of the body as the opaque EVENTINFO tail.
  `event_id > kCount` → drop (legacy parity, SSHandler.cpp:276).
  `b_id == kLottery || b_id == kGiftTime` → log + drop (legacy
  runs `LotteryItem`/`GiftTime` reward subsystems on the world
  server — random char selection + in-game mail via SendPost +
  `MW_EVENTMSGLOTTERY_REQ`; helpers not ported yet). Otherwise:
  `EventRegistry::Erase(dw_index)` always; `Set` only when
  `value != 0` (legacy "wValue==0 deactivates"); always broadcast
  so peers see the deactivation too.

The opaque-tail approach lets world act as a pure relay +
state-tracker without needing to round-trip through the full
`EVENTINFO::WrapPacketOut`/`WrapPacketIn` codec. Legacy
`SendMW_EVENTUPDATE_REQ` (SSSender.cpp:3270) produces the same
wire layout: `<< bEventID << wValue; pEvent->WrapPacketIn(pMSG)`.

New service: `EventRegistry` (services/event_registry.h/.cpp).
One `shared_mutex` over the `dw_index` → `TEventInfo` map; `Set`
and `Erase` mutate under a unique lock, `Snapshot`/`Size` under
a shared lock. `TEventInfo` keeps the routing fields surfaced
(`event_id, value, dw_index, b_id`) plus the full opaque body
ready for re-emit on replay-on-connect.

New header: `services/event_constants.h` —
`event_type::kLottery=14`, `kGiftTime=15`, `kCount=16` (from
`NetCode.h:2442-2444`'s `EVENT_TYPE` enum).

1 new sender in `senders_event.cpp`: `SendMwEventUpdateReq`
(`BYTE event_id, WORD value, <opaque body>`).

Tests — `tests/test_event_handlers.cpp` extended with three
sub-cases (registry handle added to the test ctx): activate +
broadcast + store assertions on both peers (verbatim opaque
trailer survives the round-trip; `Snapshot()` returns the row);
deactivate (`value=0`) + erase + broadcast; LOTTERY body_id
short-circuit (no broadcast, no store).

Build verified: cmake + ctest -R tworldsvr_asio -C Debug on the
representative subset (dispatch + relay + chat + event + rps +
conn + bow + br + battlemode + leavebattlefield + arenajoin —
11/11 passed).

### W6-30 — what landed

**Event subsystem opener** — `OnCtEventMsgReq` ports the legacy
operator event-message broadcast (SSHandler.cpp:310). The
control server (ControlSvr) sends a `(event_id, msg_type, msg)`
triple describing an event-state line the client should render;
world fans `MW_EVENTMSG_REQ` verbatim to every registered map
peer (legacy `m_mapSERVER` loop). Pure broadcast — no per-event
state, no DB.

- `OnCtEventMsgReq` — read `BYTE event_id, BYTE msg_type,
  STRING msg`; for each `PeerRegistry::Snapshot()` peer call
  `senders::SendMwEventMsgReq(peer, event_id, msg_type, msg)`.
  Short-body / missing-peers paths log + drop, matching the
  W6-1 / W6-29 handler shape.

1 new sender in `senders_event.cpp`: `SendMwEventMsgReq`
(`MW_EVENTMSG_REQ`: BYTE event_id, msg_type, STRING msg —
matches legacy `SSSender.cpp:3283`).

Companion to the W6-1 `SM_EVENTQUARTER_REQ` /
`SM_EVENTQUARTERNOTIFY_REQ` broadcasts. The legacy event family
also includes `CT_EVENTUPDATE_REQ` (admin-driven event
activation with the heavy EVENTINFO codec + replay-on-connect
of `m_mapEVENT`), `CT/DM_EVENTQUARTERLIST/UPDATE` (DataSvr-
forward admin queries — wait on a DataSvr peer abstraction
absent in our infra), and `SM_EVENTEXPIRED_REQ/_ACK` (the
timer-fed expiry queue; W3a-19's wanted sweep + W3a-36's
tactics sweep already supersede the legacy fan-out path, so
these stay intentionally out of scope). See README §C "Event
subsystem (broader)" for the deferred list.

Tests — `tests/test_event_handlers.cpp` extended with a third
case: `CT_EVENTMSG_REQ(event_id=7, msg_type=2, "Lottery winners
announced!")` → both map peers receive `MW_EVENTMSG_REQ` with
all three fields preserved.

Build verified: cmake + ctest -R tworldsvr_asio -C Debug
on a representative subset (dispatch + relay + chat + event +
rps + conn + bow + br + battlemode + leavebattlefield +
arenajoin — 11/11 passed).

### W6-29 — what landed

**RPS event** — small dedicated subsystem (rock-paper-scissors
event game). Four handlers + a new `RpsRegistry` modeling the
game configs + the per-key win-date ledger that gates how often a
player can re-win the same prize. Legacy parity SSHandler.cpp:
13173 / 13232 / 13259 / 13272.

- `OnRpsGameAck` — char played one round against an RPS NPC.
  Runs `RpsRegistry::RecordWin`, which prunes >30-day-old
  win-date entries (legacy `m_timeCurrent - rps.m_vWinDate[i] >
  DAY_ONE * 30`), counts entries within `win_period` days, and
  denies if the count >= `win_keep`. Reply
  `MW_RPSGAME_REQ(char_id, key, result, player_rps)` —
  result=1 on allowed / 0 on cap-hit or missing config. The
  `player_rps` byte is echoed verbatim (legacy ships the
  player's RPS pick alongside the result so the client can
  render the throw).
- `OnRpsGameDataReq` — control-server snapshot query.
  `RpsRegistry::Snapshot()` + `CT_RPSGAMEDATA_ACK(change=0,
  group, rows)`.
- `OnRpsGameChangeReq` — control-server admin update. Reads N
  rows of `(type, win_count, win_prob, draw_prob, lose_prob,
  win_keep, win_period)` and `Set`s each (silent-drop on unknown
  key, legacy parity). If any update applied: reply
  `CT_RPSGAMEDATA_ACK(change=1)` to the requester +
  `MW_RPSGAMECHANGE_REQ` verbatim broadcast to every map peer
  (legacy `m_mapSERVER` fan-out).
- `OnRpsGameRecordReq` — DB-fan-in stub. Legacy persists to
  TRPSGAMERECORDTABLE via the `CSPRPSGameRecord` stored proc;
  our port logs + drops (no `IRpsRepository` yet — same shape
  as the W3a-20 vestigial DB-server ACK echoes). The
  `RpsRegistry::RecordWin` PersistOp ledger surfaces what would
  have been persisted, so a future write-back has a clear hook.

New service: `RpsRegistry` (services/rps_registry.h/.cpp). One
`shared_mutex` over the `(type, win_count)` → `TRpsGame` map.
`RecordWin` is the only mutating path under a unique lock — it
both prunes expired entries (yielding a `PersistOp{insert=false}`
per pruned row) and appends `now` on allowed wins (yielding a
`PersistOp{insert=true}`).

3 new senders in `senders_rps.cpp`:
`SendMwRpsGameReq` (4-field reply), `SendMwRpsGameChangeReq`
(opaque body relay — same shape as the inbound CHANGE_REQ; legacy
SSSender.cpp:3649 just copies the source packet),
`SendCtRpsGameDataAck` (8-field header + N × 7-field row).

Tests — `tests/test_rps_handlers.cpp` (2 peers, 1 char): no
config → `result=0`; seed cap=2 / 3 attempts → 1st + 2nd allowed,
3rd denied (cap reached); CT_RPSGAMEDATA_REQ → ACK with the
seeded row (all 7 fields asserted); CT_RPSGAMECHANGE_REQ
updates the row + ACK(change=1) on the requester +
MW_RPSGAMECHANGE_REQ on both map peers + registry snapshot
reflects the new probs.

Build verified: cmake + ctest -R tworldsvr_asio -C Release -j 1
(86/86 passed, first try, no flake).

### W6-28 — what landed

**ARENAJOIN** — closes the W6-27 deferred third of the Arena /
BattleMode trio. `OnArenaJoinAck` (handlers_party.cpp) mirrors
legacy SSHandler.cpp:13477 — the party's chief signals the
party is entering or leaving arena mode with a specific roster.

- Reads `(char_id, key, BYTE join, DWORD count, × DWORD
  keep_member_id)` from the wire.
- Snapshots the char's `party_id` under the char lock; bails
  silently on key mismatch or no party (legacy parity).
- Snapshots `party.corps_id` + `party.members` under the party
  lock and sets `party.arena = join` (the flag flip is the
  legacy's only state mutation on the leave path — early-return
  follows for `!join`).
- On the join path:
  1. If the party was in a corps, `NotifyCorpsLeave(corps,
     party_id)` unwinds the corps (arena parties must be
     standalone — legacy parity).
  2. For each member NOT in `keep_ids`, runs `LeaveParty` with
     `kick=1` so the survivors get the right PARTYDEL flag
     (legacy `LeaveParty(it->second, TRUE)`).

Refactor enabling this slice: `NotifyCorpsLeave` was file-local
in handlers_corps.cpp's anonymous namespace; it's been moved
into the file's outer `tworldsvr::handlers` namespace + declared
in `handlers.h`. The function's internal helpers
(`SnapshotSquad`, `CorpsJoinBroadcast`, `FindMapPeer`) stay in
the anon namespace — the moved function still finds them via
the implicit using-injection that's how anonymous namespaces
work. No call-site or behaviour change for the existing W3c
corps-leave path. `LeaveParty` stays file-local in
handlers_party.cpp — putting `OnArenaJoinAck` there too avoided
a second extraction.

No reply (legacy parity). The corps-unwind branch relies on
W3c-3's existing `NotifyCorpsLeave` coverage; this slice's
test focuses on the arena flag + non-keep-member kick path.

Tests — `tests/test_arenajoin_handlers.cpp` (1 peer, 3 chars
Alice / Bob / Carol in one party with Alice as chief):
- Arena ENTER with keep=[Alice, Bob] → 3 PARTYDEL_REQ packets
  fan out (one per member, including the kicked Carol);
  `TParty.arena` flips to true; party size drops to 2; Carol's
  `party_id` back-pointer cleared.
- Arena LEAVE → `TParty.arena` flips back to false, party size
  unchanged.

Build verified: cmake + ctest -R tworldsvr_asio -C Release -j 1
(85/85 passed, first try, no flake).

### W6-27 — what landed

**BattleMode status + CM teleport** — two of the three Arena /
BattleMode trio handlers; the third (`MW_ARENAJOIN_ACK`) is
deferred to its own slice (needs cross-file helper extraction).
Mirrors legacy SSHandler.cpp:570 and 14377.

- `OnBattleModeStatusReq` (handlers_bow.cpp) — char's map asks
  for the current Bow + BR status. World replies
  `MW_BATTLEMODESTATUS_ACK` on the char's main map peer carrying
  the quiescent payload (`bow_status=0, bow_start=0,
  bow_winner=TCONTRY_N (3), br_status=0, br_start=0, br_type=0`).
  The richer status — `m_bStatus / m_dwStart / m_bWinner /
  m_bType` from the legacy modules — lands with the scheduler
  / status state machine slice; emitting the "no module" branch
  is wire-compatible with the legacy fallback (SSSender.cpp:3962
  / 3978).
- `OnCmTeleportBattleModeReq` — admin / GM force-add. Switches
  on `system_type`: `SYSTEM_BOW (0)` → `BowRegistry::AddPlayer`
  with country=TCONTRY_C + the `tactics_guild_id > guild_id > 0`
  group hint (our registry doesn't model the BS_ALARM / Admin
  bypass — it accepts unconditionally — so a single AddPlayer
  call is enough); `SYSTEM_BR (1)` → no-op (the legacy body is
  empty here, a TODO in the original). No reply.

One new sender in senders_bow.cpp: `SendMwBattleModeStatusAck`
(8-field wire — char_id, key, then the Bow + BR triplets).

Deferred — these come with the scheduler / status state machine
or their own cross-cutting refactors:
- `MW_ARENAJOIN_ACK` — needs `LeaveParty` / `NotifyCorpsLeave`
  out of their anonymous namespaces; tracked in the suggested-
  next-slice list above.
- BS_PEACE / BS_ALARM status fields in `MW_BATTLEMODESTATUS_ACK`
  (the registry would need a status field + the Bow / BR
  scheduler).

Tests — `tests/test_battlemode_handlers.cpp` (one peer, one
char): `BATTLEMODESTATUS_REQ` → ACK with the quiescent payload
(all 8 fields asserted, `bow_winner=3`);
`CMTELEPORTBATTLEMODE_REQ(SYSTEM_BOW)` enqueues Alice into the
Bow registry (verified via direct read); `CMTELEPORTBATTLEMODE_REQ
(SYSTEM_BR)` is a no-op (BR queue untouched — verified by sending
a follow-up status query to sync handler progress, then asserting
`QueueSize == 0`).

Build verified: cmake + ctest -R tworldsvr_asio -C Release -j 1
(84/84 passed on the third run; first two hit the known
parallel-serial flake from W6-13 era documented since W4-21).

### W6-26 — what landed

**LEAVEBATTLEFIELD cleanup** — small shared handler that closes
the loop on W6-24 Bow + W6-25 BR. Mirrors legacy
SSHandler.cpp:14112: the char's location determines which
subsystem cleans up.

- `OnLeaveBattlefieldReq` (handlers_bow.cpp) reads `(char_id,
  key)` and snapshots the char's `channel` + `map_id`. When
  `channel == BR_SERVER_ID` (50), routes to
  `BrRegistry::ReleaseSinglePlayer`; otherwise when
  `map_id == BOW_MAP_ID` (3000), routes to
  `BowRegistry::ReleaseSinglePlayer`. Off-battlefield chars are a
  silent no-op (legacy parity).
- `BowRegistry::ReleaseSinglePlayer` — key-checked drop from the
  pre-match queue. Legacy operates on the active-match roster
  (m_mapBOWPLAYERS) and teleports the player home; the active
  match isn't modelled yet, so this is best-effort hygiene.
- `BrRegistry::ReleaseSinglePlayer` — key-checked drop from the
  solo queue + chief-dissolve / mate-drop on any premade entry
  (mirrors the registry's existing `ErasePlayerFromPremade`
  semantics). Same active-match deferral as Bow.

No new senders or message ids — `MW_LEAVEBATTLEFIELD_REQ` has no
reply (legacy parity). The W6-25 "ReleaseSinglePlayer on logout"
deferral retired.

Tests — `tests/test_leavebattlefield_handlers.cpp` (1 peer, 4
chars): Char A on BR channel + in solo queue → LEAVE drops her;
Char B on BR channel + chief of a premade team → LEAVE dissolves
the team; Char C on Bow map + in Bow queue → LEAVE drops her;
Char D off-battlefield (channel=0, map_id=0) + in both BR and Bow
queues → LEAVE is a no-op (verified by sending a Bow enqueue
afterward to sync, then asserting Char D's pre-LEAVE state is
intact). 

Build verified: cmake + ctest -R tworldsvr_asio -C Release -j 1
(83/83 passed on the re-run; first run hit the same
parallel-serial flake from W6-13 era that's been noted since W4-21).

### W6-25 — what landed

**Battle Royale opener** — second W6 🚧 content subsystem.
Five player-driven handlers + a per-team UPDATEBRTEAM broadcast,
backed by a new `BrRegistry` modeling the solo queue, premade
teams (chief + members + ready flags), and per-user map / mode
vote tallies. Legacy parity SSHandler.cpp:14133 / 14181 / 14234 /
14259 / 14346.

- `OnAddToBrQueueReq` — `only_ready=0` runs
  `BrRegistry::AddPlayerToQueue` + emits
  `MW_ADDTOBRQUEUE_ACK(result, char_id, key, tick)` on the
  reporter's map peer. `only_ready=1` flips the ready flag —
  `FlagTeamReady(char_id)` when the char is a chief
  (`GetPremadePlayerCountByChief > 0`), else
  `FlagPlayerReady(char_id, key)` — and broadcasts UPDATEBRTEAM.
- `OnBrTeamMateAddReq` — chief invites a mate by name. Missing /
  self target → `MW_BRTEAMMATEADD_ACK(NOTFOUND)` on the chief's
  map (legacy SSHandler.cpp:14202). Otherwise forwards
  `MW_BRTEAMMATEADD_ACK(SUCCESS, inviter_name)` to the *target's*
  map so their client pops the join dialog.
- `OnBrTeamMateDelReq` — chief drops a mate by name, or any char
  self-leaves. Runs `ErasePlayerFromPremade`. No reply.
- `OnBrTeamMateAddResultAck` — mate's reply to the invite dialog.
  SUCCESS runs the duplicate-in-team gate and the team-size cap
  (`+ 1 > BR_TEAMMATE_MAX_COUNT(BR_3V3)` = 3), then
  `JoinPremadeTeam` and the UPDATEBRTEAM broadcast. Non-SUCCESS
  forwards the result code back to the chief's map.
- `OnVoteForBrMapReq` — map name non-empty → `VoteForMap`; map
  empty AND mode != 0xFF → `VoteForMode`. First vote wins,
  per-user; the scheduler picks the winning entry at match time.

New service: `BrRegistry` (services/br_registry.h/.cpp) +
`br_constants.h` (TEAMADD_* result codes + BR_3V3 mode constant
+ kTeamMaxCount3v3 + kBrServerId). `TBrTeam` carries the chief
id, the member roster keyed by char_id, and per-member +
team-wide `ready` flags. `TBrPlayer` mirrors the legacy
`TBRPLAYERS` row (id, key, class, name, ready). One
`shared_mutex` guards the registry; the broadcast snapshots the
team under that lock, then sends outside it.

W6-24 follow-up wired: `OnCancelBowQueueReq` now falls through
to `BrRegistry::ErasePlayerFromQueue` when the Bow remove
returned `kFail`, mirroring legacy SSHandler.cpp:14078. The
W6-24 README row already noted this as deferred; the W6-25
fall-through removes that note.

Deferred — each its own follow-up:
- BS_PEACE / BS_ALARM status gating (no scheduler).
- `UpdatePlayerQueue` auto-fill from solo queue into teams once a
  size threshold is reached.
- `CreateMatch` / team balancing.
- BR_SOLO vs BR_TEAM type switch (`SwitchType` / `m_bType`).
- `ReleaseSinglePlayer` on logout (cleanup hook).

Tests — `tests/test_br_handlers.cpp` (2 peers, Alice on 0x42 +
Bob on 0x43, mutual invitee/inviter): enqueue / re-enqueue /
BR fall-through from W6-24 cancel-bow / invite-forward /
invite-self-or-unknown → NOTFOUND / accept → JoinPremadeTeam +
UPDATEBRTEAM on both peers (verified `team_ready=0`, 2 rows,
chief="Alice") / Bob ready (UPDATEBRTEAM with Bob's row ready=1,
team_ready=0) / Alice ready (team_ready=1) / chief drops Bob /
map+mode vote count both at 1.

Build verified: cmake + ctest -R tworldsvr_asio -C Release -j 1
(82/82 passed, first try, no flake).

### W6-24 — what landed

**Bow battleground opener** — the first W6 🚧 content subsystem
ported. Three handlers covering the player-driven queue actions
plus a thin in-memory `BowRegistry` for the matchmaking state.
Legacy parity SSHandler.cpp:14027 / 14062 / 14099.

- `OnAddToBowQueueReq` — derives an effective country (primary,
  falling back to `aid_country` when the primary is past
  `TCONTRY_C` — covers B / N / PEACE players whose aid still
  names one of the two warring nations); picks
  `tactics_guild_id > guild_id > 0` for the queue-grouping
  hint (legacy parity); calls `BowRegistry::AddPlayer`; replies
  `MW_ADDTOBOWQUEUE_ACK(result, char_id, key, tick)` on the
  char's main map peer.
- `OnCancelBowQueueReq` — `BowRegistry::RemovePlayer` +
  `MW_CANCELBOWQUEUE_ACK` reply. Legacy fall-through to a BR
  retry on a Bow miss isn't wired (BR isn't ported yet — picks
  up in the W6-25 BR opener).
- `OnBowPointsUpdateReq` — bumps the per-country scoreboard; no
  reply.

New service: `BowRegistry` (services/bow_registry.h /.cpp) +
`bow_constants.h`. The registry holds a `char_id → TBowEntry`
queue under a `shared_mutex`, plus a per-country point counter
and a `Tick()` timestamp. Result codes mirror legacy `BOWREG_*`
(SUCCESS / COUNTRY / ALREADYINQUEUE / FAIL). `HandlerContext.bow`
threads it through dispatch; `main.cpp` instantiates one alongside
the existing registries.

Deferred — these come with the scheduler / status state machine
follow-up:
- `BS_PEACE` / `BS_ALARM` status gating (legacy rejects everything
  outside the pre-match window).
- Match creation + team balancing (`CreateMatch`).
- Teleportation into the Bow map (`TeleportBOWPlayer`).
- Per-guild queue grouping (legacy `m_mapGuildMember` /
  `m_mapBOWREG` split — we store `guild_id` on the entry but
  don't yet branch on it).
- The legacy `UpdatePoints` clamp wrap (the legacy expression
  `m_bPoints[X] == BOW_MAX_POINTS / BOW_MAX_POINTS` is a `==1`
  literal — a bug; the port clamps at 255 and leaves the
  intent-vs-bug call to the scheduler slice).

Tests — `tests/test_bow_handlers.cpp` (one peer, two chars):
enqueue Alice (country=C) → SUCCESS; enqueue Alice again →
ALREADYINQUEUE; enqueue Bob (country=B with aid=PEACE) → COUNTRY
(both > kCountryC); cancel Alice → SUCCESS + registry empty;
cancel Alice again → FAIL; two BOWPOINTSUPDATE(D) + one (C) bump
the scoreboard to `D=2 / C=1` (verified directly + via a
follow-up enqueue ACK to make sure the handler queue drained).

Build verified: cmake + ctest -R tworldsvr_asio -C Release -j 1
(81/81 passed).

### W4-23 — what landed

**Fresh-login fidelity polish** — two small but visible W4-22
follow-ups closing the deferred zero-defaults that the bulk slice
shipped with.

1. **Soulmate target in MW_FRIENDLIST_REQ**. `SendMwFriendListReq`
   now takes a `soulmate_target` DWORD (declared next to `groups`
   in the signature). Both callers — `OnFriendListAck` (W4-4) and
   the fresh-login emit in `OnEnterSvrAck` (W4-22) — populate it
   from `TChar.soulmate.target`, so the client now renders a live
   soulmate slot in the friend window instead of the previous
   hardcoded sentinel-zero. Legacy parity SSSender.cpp:1723.

2. **bow_release in MW_CHARINFO_REQ**. The fresh-login path now
   sets `CharInfoPayload.bow_release = 1` when the captured
   `chg_main_id` is `BOW_SERVER_ID` (30) or `BR_SERVER_ID` (50)
   — i.e., the char is returning from a BR/Bow battleground (the
   W6-16 chg_main_id branch already excluded those from the
   normal-handoff fast-path and falls through here). Mirrors
   legacy SSHandler.cpp:1456. The legacy quirk that `chg_main_id`
   stays set after the fresh-login emit (no clear) is preserved
   verbatim.

No new senders or handlers; both are wire-field populations on
existing calls. The work is the call-site updates plus the
`SendMwFriendListReq` signature change with its two-call-site
mechanical follow-up.

Tests:
- `test_friend_list_handlers.cpp` (W4-4) — Alice now has
  `soulmate.target = 777` and the reply asserts `L.soulmate ==
  777` (was `EXPECT(L.soulmate == 0)`).
- `test_entersvr_fresh_login_handlers.cpp` (W4-22) —
  Alice gets `soulmate.target = 88` so Test A asserts
  `soul == 88` in her FRIENDLIST_REQ; a new **Test C** sets
  Bob's `chg_main_id = 50` (BR), re-emits ENTERSVR_ACK, and
  asserts `c.bow_release == 1` in the resulting CHARINFO_REQ.

Build verified: cmake + ctest -R tworldsvr_asio -j 1 (80/80
passed; the same parallel-serial flake in the W6-13 era cluster
tests as W4-21 noted occasionally trips on tight runs, but
re-runs reach 80/80 reliably).

### W6-23 — what landed

**CHARDATA_ACK drift fan-out** — closes the W6-20 deferral. When
the main map sends `MW_CHARDATA_ACK` but some of the char's cons
haven't ENTERCHAR_ACKed yet, world now fans the legacy fat
`MW_ENTERCHAR_REQ` composite to each not-yet-ready con (legacy
SSHandler.cpp:920 / SSSender.cpp:258). Each map loads the char and
replies `MW_ENTERCHAR_ACK` (W6-20) flipping its con `ready`; once
all are ready, the existing W6-20 all-ready path runs
`CheckMainCon`, completing the in-flight reconcile.

One new sender in `senders_relay.cpp` — `SendMwEnterCharReq`
carrying the new `EnterCharReqPayload` POD (33 fields:
char_id/key/start_act, name + map/pos, the guild block + member's
duty/peer/castle/camp, tactics_id + name, party id/type/chief,
corps commander, per-char appearance + chat_ban_time + soulmate
slot, class). The opaque recall-mon table + comment tail (~5 bytes
in the minimal case, more with actual recall mons) is lifted from
the inbound `CHARDATA_ACK` body and appended verbatim — both
packets emit the same structured shape, so a byte-copy is
wire-equivalent and avoids a fragile per-recall-mon parse. The
soulmate-silence DWORD is emitted as 0 today (legacy
`m_dwSoulSilence` isn't modelled on `TChar` yet — see "Suggested
next slices").

The handler change is a single branch in `OnCharDataAck`: parse
the 10-field header (same as before), then capture
`body[r.Remaining():]` as `opaque_tail`, snapshot the per-char
fields the composite needs (name / map / pos / appearance /
soulmate slot / class), resolve guild / tactics / party / corps
via the existing registries (`GuildRegistry::Find` →
`FindMember` / `FindTactics`, `PartyRegistry::Find`,
`CorpsRegistry::Find`), and emit one `ENTERCHAR_REQ` per
`pending_servers` entry. The all-ready branch is unchanged
(`CheckMainCon`).

Tests — `tests/test_chardata_drift_handlers.cpp` (2 map peers,
Bob with main 0x42 manually marked ready + 0x43 not-ready, in
guild "Eagles" id=10 + party id=7): CHARDATA_ACK from p1 emits one
`ENTERCHAR_REQ` on p2 carrying the full composite (33 fields
asserted, including guild_id=10 / fame=500 / duty=1 / peer=2 /
castle=50 / party_id=7 / chief=200 / level=40 / class=3 / riding=999
/ soulmate=42 / soulmate_name="Alice") + the opaque tail
(recall_count=0 + empty comment) preserved verbatim, and refreshes
the char's level / HP / max_hp. A follow-up `ENTERCHAR_ACK` from
p2 closes the loop with the `MW_CHECKMAIN_REQ` broadcast on both
peers (verifying the drift → all-ready transition works end-to-end).

Build verified: cmake + ctest -R tworldsvr_asio -j 1 (80/80
passed).

### W4-22 — what landed

**Fresh-login ENTERSVR completion** — `OnEnterSvrAck`
(handlers_char.cpp) now does the bulk of the legacy fresh-login
chain after the W4-20 identity load. Mirrors the legacy
`OnMW_ENTERSVR_ACK` (SSHandler.cpp:1379), which after the bulk-
identity store fires four operations on the responder peer:

1. **`MW_CHARINFO_REQ`** — the 18-field composite identity packet.
   Built from in-memory state: the char's guild via
   `GuildRegistry::Find(guild_id)` (name / country / fame /
   fame_color + `FindMember(char_id)` for duty / peer / castle /
   camp), the tactics-guild via the same path with a castle/camp
   *fallback* to the tactics member when the guild member didn't
   surface a castle (legacy parity), and the party via
   `PartyRegistry::Find(party_id)` (id / obtain_type / chief_id).
   `title_id` + `rank_point` come straight off the inbound ACK
   wire fields. The `bow_release` flag stays 0 — populating it on
   BR/Bow handoffs is a small follow-up (see §B).
2. **`MW_ROUTE_REQ`** — `(char_id, key, channel, map_id, pos x/y/z)`
   so the main map starts resolving the char's additional
   connections (answered by W6-20's `OnRouteAck`).
3. **`MW_FRIENDLIST_REQ`** — same groups + non-pending friends
   snapshot W4-4's `OnFriendListAck` builds, with online state +
   level / class / region resolved live from the CharRegistry.
   The soulmate sentinel still emits 0 (same TODO as W4-4 — a
   small cross-cutting fix listed in the suggested next slices).
4. **`MW_CHATBAN_REQ`** when `TChar.chat_ban_time > now` — enforces
   an active ban on the freshly-connected map. The cluster-wide
   `m_mapBanChar` list isn't ported (W4-19 deferral); the
   per-char `chat_ban_time` is the source of truth here.

Two new senders in `senders_relay.cpp`: `SendMwCharInfoReq`
(carrying the `CharInfoPayload` POD declared in `senders.h`) and
`SendMwRouteReq` (the 7-field channel/map/pos shape). Reuses
`SendMwFriendListReq` (W4-4) and `SendMwChatBanReq` (W4-19).
Lock ordering follows README §5 — the char / guild / tactics /
party locks are taken sequentially, never overlapping, with
`FindMember` / `FindTactics` performed under the matching guild's
lock.

The W6-16 BR/Bow chg_main_id-exclusion path (where the handoff
"falls through to fresh-login") now correctly emits this chain on
re-entry to a BR / Bow instance. The W6-16 test was updated to
drain the three emitted packets (CHARINFO / ROUTE / FRIENDLIST)
before reading the handoff char's `MAPSVRLIST_REQ`, matching the
new legacy-parity behaviour.

Tests — `tests/test_entersvr_fresh_login_handlers.cpp`: Alice
(guildless / partyless, chat-banned 1h into the future) → empty
CHARINFO_REQ (zeros + TCONTRY_N + ""), ROUTE_REQ with her position,
empty FRIENDLIST_REQ, CHATBAN_REQ carrying her ban time. Bob (in
guild "Eagles" #10 + party #7, no ban) → CHARINFO_REQ populated
with the guild meta + member's duty / peer / castle / camp +
party id / obtain_type / chief_id + title_id (99) + rank_point
(12345); ROUTE_REQ; empty FRIENDLIST_REQ; **no** CHATBAN_REQ
verified by re-emitting ENTERSVR_ACK and confirming the next
packet is its own CHARINFO_REQ.

Build verified: cmake + ctest -R tworldsvr_asio -j 1 (79/79
passed).

### W4-21 — what landed

**Friend-protected presence sync** — `OnProtectedCheckAck`
(handlers_friend.cpp, `MW_PROTECTEDCHECK_ACK`) is the symmetric
partner to W6-9's `OnFriendProtectedAskAck`. While W6-9 relays an
auto-refuse when *adding* a protected friend, W4-21 syncs the
*presence* of an already-mutual protected friendship between two
chars.

Behaviour mirrors the legacy `OnMW_PROTECTEDCHECK_ACK`
(SSHandler.cpp:5769):

- Read `(char_id, key, connect, protected_name)`. Drop on missing
  char / key mismatch / friend-name not in requester's list.
- On `connect == FRIEND_DISCONNECTION`, fire the W4-17
  `IFriendRepository::EraseFriend` write-back via `CoOffloadVoidIf`
  (matching legacy `SendDM_FRIENDERASE_REQ`). The in-memory edge
  stays — legacy keeps it until the next reload.
- Look up the target by name (`CharRegistry::FindByName`); drop if
  not online. Under the target's lock, mutate the target→requester
  edge — on `FRIEND_CONNECTION` set `connected=true` and copy the
  requester's region; on disconnect drop `connected`. Snapshot the
  target's identity + edge type + region for the relay step.
- Under the requester's lock, mutate the requester→target edge
  cross-wise (connect picks up the *target's* region). Drop if the
  reverse edge is missing (legacy double-check).
- Relay `MW_FRIENDCONNECTION_REQ` to the target's main map naming
  the requester, with region populated only on connect (legacy
  `!bConnect ? region : 0`). Skip the relay when the target's edge
  is `FT_TARGET` (a pending invite — no toast yet, legacy parity).

The two char locks are taken sequentially (target first, then
requester) — never overlapping. Eventual-consistency window between
the two mutations is fine for presence sync; the in-flight relay
already carries the fully-resolved state.

Reuses `SendMwFriendConnectionReq` (W4-7) and the `FindMapPeer`
helper local to `handlers_friend.cpp`. No new senders.

Tests — `tests/test_protected_check_handlers.cpp` (2 map peers,
Alice on 0x42 and Bob on 0x43, mutual FT_FRIENDFRIEND): connect →
FRIENDCONNECTION(CONN) relay on Bob's map naming Alice + Alice's
region (555); both edges flip connected with cross-wise regions
(Alice's edge → Bob's 777, Bob's edge → Alice's 555). Disconnect →
FRIENDCONNECTION(DISC) relay with region=0; both edges drop
`connected`. Unknown friend name → silent drop (verified by sending
a known-name packet behind it and confirming the relay is the known
one's). Unknown char_id → same silent-drop verification.

Build verified: cmake + ctest -R tworldsvr_asio (78/78 passed).
Note: a longstanding parallel-run flake in the W6-13 era cluster
tests (releasemain / route_completion occasionally fail in
isolation between rapid serial test launches, presumably ephemeral-
port reuse or TIME_WAIT) is unrelated; each affected test passes in
isolation, and re-running the full suite reaches 78/78 reliably.

### W6-21 — what landed

**Teleport confirm** — `OnTeleportAck` (handlers_conn.cpp,
`MW_TELEPORT_ACK`) finishes the W6-17 teleport flow that
`BEGINTELEPORT_ACK` starts. The legacy handler at SSHandler.cpp:1490
reads `(char_id, key, dest_server_id)` from the responding map (the
one that owned the client during the teleport) and either confirms or
fails the teleport client-side:

- **Happy path** — clears the char's `party_waiter` (legacy
  `m_bPartyWaiter = FALSE`) and emits `MW_TELEPORT_REQ(TPR_SUCCESS)`
  back to the responder carrying the char's current channel / map /
  position (which W6-17 already updated to the destination on
  `BEGINTELEPORT_ACK`), then fires `MW_CONLIST_REQ` to the destination
  map (the new home) so it re-enters the W6-13 reconcile and joins
  the char's connection set.
- **Destination offline** — `MW_TELEPORT_REQ(TPR_NODESTINATION)` to
  the responder (carrying the same channel / map / pos so the client
  can sanity-check), then the W6-19 `CloseChar` helper tears the
  char down (DELCHAR fan-out across its cons + registry remove +
  friend/soulmate/TMS offline fan-out). Mirrors the legacy
  `CloseChar(pTCHAR)` line that the `pTMAP` lookup-failure branch
  runs.
- **Unknown char / key mismatch** — `MW_DELCHAR_REQ(logout=1,save=0)`
  to the reporting map (legacy `SendMW_DELCHAR_REQ(…, TRUE, FALSE)`).

Two senders in `senders_conn.cpp` — `SendMwTeleportReq`
(8-field composite: char_id, key, channel, map_id, pos × 3, result)
and `SendMwConListReq` (the position-bearing version, distinct from
the W6-13 reply-side handlers `MW_CONLIST_ACK`). The two
TTELEPORT_RESULT constants the handler emits (TPR_SUCCESS = 0,
TPR_NODESTINATION = 3) are declared in the same anonymous namespace
as W6-14's `kConSuccess`; the rest of the enum ports as the
war/portal/etc. teleport branches need them.

The cession queue (W6-17 `m_qConCess`) is **not** popped here — its
pop is driven by the destination map's eventual `CHECKMAIN_ACK`
(W6-14, after the CONLIST → reconcile → ROUTELIST → ENTERCHAR cycle
that the W6-21 CONLIST_REQ kicks off), matching legacy ordering.
Legacy parity preserved: TELEPORT_ACK on the happy path triggers
the destination-map handshake, the cession-queue front then drains
when the new main session confirms.

Tests — `tests/test_teleport_confirm_handlers.cpp` (3 map peers):
the happy path on char A (TELEPORT_ACK with `dest=0x43` →
TELEPORT_REQ(SUCCESS) on p1 carrying channel=7 / map=1234 / pos +
CONLIST_REQ on p2 carrying the same; `party_waiter` cleared);
NODESTINATION on char B (dest=0x99 unknown → TELEPORT_REQ(NODEST) on
p1, then CloseChar fans DELCHAR to 0x42 + 0x43 and drops the char
from the registry); unknown char on p3 → DELCHAR.

Build verified: cmake + ctest -R tworldsvr_asio (77/77 passed).

### W6-20 — what landed

**Connection-completion sub-flow** — the reply path that actually
materialises the new connections asked for by the W6-13/W6-18 reconcile
(`MW_ROUTELIST_REQ`). Three new handlers in `handlers_conn.cpp` (with
two new senders in `senders_conn.cpp`) close the loop end-to-end:

- `OnRouteAck` (`MW_ROUTE_ACK`, the main's reply to ROUTELIST): when
  `count == 0` the main is asked for a `MW_CHARDATA_REQ` round-trip
  (legacy parity — no new cons needed); when `count > 0` each
  `(ip, port, server_id)` tuple is registered as a *pending* TCharCon
  (`ready=false, valid=false`, preserving any matching entry's
  `valid` bit across the replace), and `MW_ADDCONNECT_REQ` is forwarded
  back to the reporter so it can hand the new endpoints to the client.
  Unknown char / key mismatch → `MW_DELCHAR_REQ`.
- `OnEnterCharAck` (`MW_ENTERCHAR_ACK`, per-connection entry handshake):
  flips the reporting con's `ready` bit; once every con is ready, fires
  `CheckMainCon` to re-confirm the main session across the whole new
  set. Missing con for the reporter → `MW_INVALIDCHAR_REQ`; unknown
  char → `MW_DELCHAR_REQ`.
- `OnCharDataAck` (`MW_CHARDATA_ACK`, main's reply to CHARDATA_REQ):
  refreshes the char's `level / hp / max_hp / mp / max_mp` (legacy
  `SetCharLevel` + `SetCharStatus`), then fires `CheckMainCon` if every
  con is ready (the typical count==0 path). Errors: unknown char →
  DELCHAR; main offline → INVALIDCHAR.

The full chain that now closes:

```
reconcile (W6-13)                 — ROUTELIST_REQ via main
  → ROUTE_ACK (W6-20)             — main returns IP/port set
    → ADDCONNECT_REQ (W6-20)      — reporter hands endpoints to client
    → CHARDATA_REQ (W6-20)        — count==0 branch
      → CHARDATA_ACK (W6-20)      — main returns level/HP/MP
  → ENTERCHAR_ACK (W6-20)         — per new con: ready=true
  → CheckMainCON (W6-13)          — broadcast CHECKMAIN_REQ to all cons
    → CHECKMAIN_ACK (W6-14)       — close dead_cons + CONRESULT
```

Two senders in `senders_conn.cpp` — `SendMwAddConnectReq` (uses a new
`senders::AddConnectEntry` POD for the per-entry tuple) +
`SendMwCharDataReq`. Reuses `MW_DELCHAR_REQ` / `MW_INVALIDCHAR_REQ` /
`MW_CHECKMAIN_REQ` from the W6-13 batch.

Deferred — the legacy `CHARDATA_ACK` non-ready branch fans
`MW_ENTERCHAR_REQ` to each not-yet-ready con carrying a fat
guild/tactics/party/soulmate/region/chat-ban/riding composite (the
same body the fresh-login path emits). That fan-out lands with §B
priority #2 ("Fresh-login ENTERSVR completion"); in the typical
`count==0` ROUTE path every con is already ready and the deferred
branch doesn't fire.

Drive-by: `services/guild_constants.h` needed `<cstddef>` (MSVC 14.51
doesn't pull `std::size_t` via `<cstdint>` the way GCC does); the
fix unblocks the Windows build of `tworldsvr_asio_core` without
changing any behaviour.

Tests — `tests/test_route_completion_handlers.cpp` (3 map peers):
ROUTE_ACK count=0 → CHARDATA_REQ; ROUTE_ACK count=1 with a new server
0x44 → ADDCONNECT_REQ + the pending con is registered with the right
ip/port/ready=false/valid=false; ENTERCHAR_ACK from 0x42 → only its
con becomes ready (no broadcast); ENTERCHAR_ACK from 0x44 → all
ready, CheckMainCon broadcasts CHECKMAIN_REQ to 0x42 + 0x44 carrying
the char's channel/map/pos; CHARDATA_ACK with level=42 + HP/MP →
CHECKMAIN sweep and the char's stats refresh; ROUTE_ACK for an
unknown char → DELCHAR.

Build verified: cmake + ctest -R tworldsvr_asio (76/76 passed).

### W6-19 — what landed

**CloseChar teardown** — the W2 `OnCloseCharAck` stub (registry remove +
social fan-out) is now the full legacy `CloseChar` (TWorldSvr.cpp:3061),
extracted into a shared `CloseChar(ch, ctx)` helper (handlers_char.cpp,
declared in handlers.h). Tearing a char down now:

- if a main-session handoff was in flight (`chg_main_id` set), voids the
  would-be new main's takeover (`MW_INVALIDCHAR_REQ`, release_main=1);
- DELCHARs every map the char is connected to — `dead_cons` first, then
  live `cons` — with the logout / save flags set **only** on the main
  connection (`sid == main_server_id ? char.logout/saving : 0`), matching
  the legacy per-con flag logic;
- removes it from the registry (which also clears the name index);
- fans the offline presence out to friends / soulmate / TMS (the W4
  notifiers, unchanged).

`OnCloseCharAck` now replies `MW_DELCHAR_REQ` to the reporting map on a
stale / wrong-key close (retiring the W2 TODO), then calls `CloseChar`.
The same helper is wired into the connection-cluster error paths
(`BeginTeleport` / `CheckConnect` when the main map is offline), retiring
their deferred "CloseChar" notes. Still deferred (un-ported dependencies):
the party-leave on logout (`LeaveParty` resolution) and the guild/tactics
DB persistence (PVPRECORD / TACTICSPOINT / SaveGuildStats); guild/tactics
"connection clear" is a no-op here since online state is resolved live
from the registry. The sketchy legacy `if(!m_bSave) CloseChar` at the top
of CHECKMAIN_ACK (a use-after-free in the original) is intentionally not
ported.

Tests — `tests/test_closechar_handlers.cpp` (3 map peers): closing a char
with a dead con + main/live cons DELCHARs all three (dead first, main
carrying logout=save=1, the rest 0/0) and removes it; closing a char with
a handoff in flight INVALIDCHARs the would-be new main before the DELCHAR;
an unknown/wrong-key close replies DELCHAR. The existing close / friend /
TMS-logout tests still pass (the DELCHAR fan-out targets the char's own
con peers, not the friends' peers the presence tests read).

Build verified: cmake + ctest -R tworldsvr_asio (75/75 passed).

### W6-18 — what landed

**Connect-check reconcile** — `OnCheckConnectAck` (handlers_conn.cpp,
`MW_CHECKCONNECT_ACK`) is the second cession-queue producer (alongside
W6-17's teleport). A map periodically re-asserts a char's position and
the set of servers it should be connected to; world serialises it on the
same cession queue (`PushConCess` / `PopConCess` replay), then runs
`CheckConnect`:

- must originate from the char's main map (else the queue advances);
- updates the char's channel / map / position from the report;
- `count == 0` → `CheckMainCon` sweep (CHECKMAIN to every con);
- otherwise reconciles `cons` against the reported set — drop
  no-longer-listed cons to `dead_cons`, and either `MW_ROUTELIST_REQ`
  the newly-needed servers via the main map or, if none are new, sweep
  with CHECKMAIN. Unlike CONLIST/MAPSVRLIST the reporting map is **not**
  auto-added to the needed set (legacy `OnCheckConnect` parity).

Factored a shared `CheckMainCon` helper (the CHECKMAIN broadcast) used by
the count=0 and no-new-server paths. Unknown char / key mismatch →
`MW_DELCHAR_REQ`. No new senders (reuses ROUTELIST / CHECKMAIN / DELCHAR).
`PopConCess`'s replay switch now dispatches both BEGINTELEPORT and
CHECKCONNECT entries. Deferred: the main-offline `CloseChar` teardown.

Tests — `tests/test_checkconnect_handlers.cpp` (3 map peers): a count=0
report updates position + sweeps CHECKMAIN to both cons; a report naming
a new server (0x44, with the existing cons retained) updates position +
ROUTELISTs only the new server via the main, dropping nothing; an unknown
char replies DELCHAR.

Build verified: cmake + ctest -R tworldsvr_asio (74/74 passed).

### W6-17 — what landed

**Teleport begin + cession queue** — `OnBeginTeleportAck`
(handlers_conn.cpp, `MW_BEGINTELEPORT_ACK`) starts a char teleport, and
introduces the connection **cession queue** (`TChar::con_cess`, legacy
`m_qConCess`) that serialises a char's multi-round-trip handoffs.

- A *same-channel* teleport just records the new channel and returns —
  no map handoff, never queued.
- Otherwise the request is pushed onto the cession queue. `PushConCess`
  returns whether something was already in flight: if so the new request
  waits; if it's the only entry, `BeginTeleport` runs immediately. It
  must originate from the char's main map (else it's ignored and the
  queue advances), updates the char's destination channel/map/pos, and
  broadcasts `MW_STARTTELEPORT_REQ` to every map the char is connected
  to (the valid cons).
- The entry stays at the queue front until its handoff round-trip
  completes. `PopConCess` — now wired into the `CHECKMAIN_ACK`
  main-confirmed branch (W6-14's deferred TODO) — pops the finished
  entry and replays the next one (`BeginTeleport` again). `PopConCess`
  and `BeginTeleport` are mutually recursive so a skipped/non-main entry
  advances the queue without stalling.

Errors: unknown char / key mismatch → `MW_DELCHAR_REQ`. One sender
(`SendMwStartTeleportReq`). The cession entry stores the reporting map's
server id + msg id + raw body; the peer is re-resolved at replay time
(legacy `FindTServer`). Deferred: the `MW_CHECKCONNECT_ACK` producer
(the other queue feeder — reconcile-with-position) and the
main-offline `CloseChar` (logout teardown).

Tests — `tests/test_teleport_handlers.cpp` (2 map peers): the
same-channel fast path (records channel, never queues); a single
teleport broadcasting STARTTELEPORT to both cons; and a second teleport
deferred behind the first (queue size 2) that replays + broadcasts when
`CHECKMAIN_ACK` pops the first (queue back to 1). A test-side note: poll
loops must read state under the char lock and sleep *outside* it —
holding the entity lock across a sleep starves the handler coroutine.

Build verified: cmake + ctest -R tworldsvr_asio (73/73 passed).

### W6-16 — what landed

**Handoff completion** — `OnEnterSvrAck` (handlers_char.cpp,
`MW_ENTERSVR_ACK`) now branches on the char's `chg_main_id` before the
fresh-login path. When it's set to a normal map id, this ENTERSVR_ACK is
the new main confirming it loaded the handed-off char (the W6-15
RELEASEMAIN_ACK → ENTERSVR_REQ → here chain): world clears `chg_main_id`
and asks the new main for the char's full server list (`MAPSVRLIST_REQ`
carrying the just-received channel/map/pos), which re-enters the W6-13
reconcile. This is a map move, not a login, so the friend-presence
fan-out is skipped (early return). A `chg_main_id` of `BR_SERVER_ID` (50)
or `BOW_SERVER_ID` (30) — the Battle-Royale / Bow battleground instances
(NetCode.h) — is excluded and falls through to the fresh-login path,
matching legacy `OnMW_ENTERSVR_ACK:1343`.

This closes the main-session handoff loop end-to-end:

```
CONLIST/MAPSVRLIST reconcile (W6-13)
  → CHECKMAIN broadcast
  → CHECKMAIN_ACK from a non-main (W6-14): RELEASEMAIN old main, re-point main
  → RELEASEMAIN_ACK from old main (W6-15): forward ENTERSVR_REQ to new main
  → ENTERSVR_ACK from new main (W6-16): MAPSVRLIST_REQ → reconcile (W6-13)
```

One sender (`SendMwMapSvrListReq`). The legacy BR/Bow `CHARINFO_REQ`
sub-case and the full fresh-login completion (guild/tactics resolve,
CHARINFO_REQ + ROUTE_REQ + SOULMATELIST/FRIENDLIST) remain as later
slices — the ported fresh-login path so far is the W4-20 identity load +
friend fan-out.

Tests — `tests/test_entersvr_handoff_handlers.cpp` (2 map peers): two
ENTERSVR_ACKs on one socket (ordered) — a BR-excluded char (chg=50) that
must emit nothing, then a normal-handoff char — confirm only the handoff
char's MAPSVRLIST is sent (with its channel/map/pos), its `chg_main_id`
is cleared, and the BR char's flag is left intact (proving exclusion).

Build verified: cmake + ctest -R tworldsvr_asio (72/72 passed).

### W6-15 — what landed

**Main-session handoff forward** — `OnReleaseMainAck` (handlers_conn.cpp,
`MW_RELEASEMAIN_ACK`) completes the handoff W6-14 starts. When a char's
main session is being moved to a different map, W6-14's CHECKMAIN_ACK
re-points `main_server_id` at the new map and asks the *old* main to
release; the old main's `MW_RELEASEMAIN_ACK` lands here. World forwards
the released char to the new main — the inbound body (BYTE db_load,
DWORD char_id, key, + the char's saved state) is re-tagged verbatim as
`MW_ENTERSVR_REQ` (opaque passthrough; world reads only the first three
fields to find the char) — and records the old main in the char's new
`chg_main_id` byte (legacy `m_bCHGMainID`, replacing the misnomer
`bool main_id_changing`; 0 = no handoff). The new main loads the char
and replies `MW_ENTERSVR_ACK` to finish.

Errors match legacy: unknown char / key mismatch → `MW_DELCHAR_REQ`; new
main offline → `MW_INVALIDCHAR_REQ(release_main=1)`. One opaque-passthrough
sender (`SendMwEnterSvrReq`).

Deferred (its own slice): the `MW_ENTERSVR_ACK` handoff-completion logic
that *consumes* `chg_main_id` — the legacy `OnMW_ENTERSVR_ACK` branches on
`m_bCHGMainID` (incl. the BOW_/BR_SERVER special cases) to finalize the
takeover, CONRESULT the new connection set, and clear the flag. The
Asio `OnEnterSvrAck` ported so far only loads identity; the
handoff-completion path lands later.

Tests — `tests/test_releasemain_handlers.cpp` (2 map peers): an old main
releasing a char whose main was re-pointed at the new map forwards the
verbatim body (incl. a saved-state marker) to the new main as
ENTERSVR_REQ + sets `chg_main_id`; a char whose new main is offline
replies INVALIDCHAR(release_main=1); an unknown char replies DELCHAR.

Build verified: cmake + ctest -R tworldsvr_asio (71/71 passed).

### W6-14 — what landed

**Main-session confirmation** — `OnMW_CHECKMAIN_ACK` (handlers_conn.cpp)
handles a map's answer to the W6-13 `CHECKMAIN_REQ` broadcast. The
responding map either *is* the char's main session or claims it:

- **responder is the main** (`responding_id == main_server_id`) — world
  drains `TChar::dead_cons`, sending `MW_CLOSECHAR_REQ` to each (legacy
  `ClearDeadCON`), then green-lights the connection set back to the main
  with `MW_CONRESULT_REQ` / `CN_SUCCESS` (NetCode.h `TCONNECT_RESULT`)
  carrying the live server list. The responder is the packet's sender,
  so its main peer is always present — the dead-con drain happens under
  the same lock as the snapshot, no INVALIDCHAR can intervene.
- **responder is a different map** — main-session handoff: world tells
  the *old* main to release (`MW_RELEASEMAIN_REQ` with the char's
  channel/map/pos) and re-points `main_server_id` at the responder
  (clearing `saving`). The mutation is deferred until after the old-main
  lookup succeeds, matching legacy's pMAIN-before-reassign ordering.

Error replies match legacy: unknown char / key mismatch →
`MW_DELCHAR_REQ(logout=1,save=0)`; old main offline → `MW_INVALIDCHAR_REQ`.
Three senders in `senders_conn.cpp` (CONRESULT / CLOSECHAR / RELEASEMAIN).

Deferred (each a slice of its own): the `if(!m_bSave) CloseChar` logout
teardown (friend/TMS/party/guild/tactics unwind + DELCHAR every con),
and `PopConCess` — the cession queue is only populated by
`CHECKCONNECT_ACK` (not yet ported), so it is always empty today and the
PopConCess call would be a no-op.

Tests — `tests/test_checkmain_handlers.cpp` (3 map peers): a CHECKMAIN
from the main closes the dead con + CONRESULTs the live set (and drains
`dead_cons`); a CHECKMAIN from a non-main RELEASEMAINs the old main +
re-points `main_server_id`; a CHECKMAIN for an unknown char replies
DELCHAR. Both connection tests now serialise the two per-char ADDCHARs
(let p1's insert win the main slot before p2 adds its con) to remove a
cross-socket ordering race.

Build verified: cmake + ctest -R tworldsvr_asio (70/70 passed).

### W6-13 — what landed

**Connection-list reconcile** — the first, self-contained slice of the
legacy connection/teleport cluster. A map server periodically reports
which map servers a char is connected to (`MW_CONLIST_ACK`) or which it
must mirror to (`MW_MAPSVRLIST_ACK`); the two legacy handlers
(SSHandler.cpp:2020 / 2133) are byte-identical, so one `ReconcileConList`
serves both. Given the reported server set (always including the
reporting map itself), world:

- moves connections the map no longer reports out of `TChar::cons` into
  the new `TChar::dead_cons` (legacy `m_vTDEADCON`) — these are closed
  later by `ClearDeadCON` at `CHECKMAIN_ACK` time (next slice);
- if the char must connect to *new* servers, asks its main map to route
  it there (`SendMwRouteListReq` → the map answers `MW_ROUTE_ACK`, which
  it forwards down to the client);
- otherwise re-confirms the main session on every remaining connection
  (`SendMwCheckMainReq` to each, legacy `CheckMainCON`).

Error replies match legacy: char not in the registry (or key mismatch) →
`SendMwDelCharReq(logout=1,save=0)` to the reporting map; main session's
peer offline → `SendMwInvalidCharReq`. Reconcile runs under the char
lock and snapshots the new-server list / live-con list / channel-map-pos
before any peer lookup or send (README §5 — never hold the lock across a
co_await). Added `TChar::channel` (legacy `m_bChannel`, also now stored
by `OnEnterSvrAck`) so `CHECKMAIN_REQ` carries the right channel.

Four senders in `senders_conn.cpp` (ROUTELIST / CHECKMAIN / DELCHAR /
INVALIDCHAR). The cession queue, `CHECKMAIN_ACK`, `RELEASEMAIN`,
`ENTERSVR` handoff and `BEGINTELEPORT`/`CHECKCONNECT` teleport flow are
the remaining cluster slices.

Tests — `tests/test_conn_handlers.cpp` (3 map peers): a CONLIST naming a
new server routes via the main map + drops the stale con to `dead_cons`;
a CONLIST naming only existing cons broadcasts CHECKMAIN to each
(carrying the char's channel/map/pos); a CONLIST for an unknown char
replies DELCHAR.

Build verified: cmake + ctest -R tworldsvr_asio (69/69 passed).

### W6-12 — what landed

**GM user-tracking relays** — two control-server (GM) tools routed to
the target char's map:

- `OnUserPositionAck` (handlers_admin.cpp, `CT_USERPOSITION_ACK`) —
  "where is this player": relays `MW_USERPOSITION_REQ` (char id/key +
  GM name) to the target's map; requires both target and GM online.
- `OnUserMoveAck` (`CT_USERMOVE_ACK`) — GM force-move: relays the
  destination (channel/map/pos/party) to the user's map, re-sent as
  `CT_USERMOVE_ACK` (the legacy world→map form).

Two senders in `senders_admin.cpp`; both reachable via the same
Dispatch as the other CT_/SM_ messages.

Tests — `tests/test_admin_handlers.cpp`: a locate request and a
force-move both reach the target's map with the right payload.

Build verified: cmake + ctest -R tworldsvr_asio (68/68 passed).

### W6-11 — what landed

**Day-change guild ranking** — the scheduler's daily rollover
(`SM_CHANGEDAY_REQ`) recomputes every guild's PvP rank. `OnChangeDayReq`
(handlers_guild.cpp) snapshots each guild's `(pvp_total_point,
pvp_month_point)` under its lock, then assigns `rank_total` /
`rank_month` = (guilds with strictly more points) + 1, counting only
guilds that have points (pointless guilds rank 0). Mirrors legacy
`CalcGuildRanking`; the ranks are read back by `OnGuildInfoAck`. No
reply, no persistence; snapshot-then-write keeps guild locks disjoint.

Tests — `tests/test_guildrank_handlers.cpp`: three guilds with
distinct total/month points get the expected ranks (incl. the
unranked zero-point guild).

Build verified: cmake + ctest -R tworldsvr_asio (67/67 passed).

### W6-10 — what landed

**Item-result relays** — two cross-server item routings:

- `OnAddItemResultAck` (handlers_item.cpp, `MW_ADDITEMRESULT_ACK`) —
  an item-add result computed elsewhere, routed to the *requesting*
  map server (by `bMapSvrID`) via the W3b `SendMwAddItemResultReq`.
- `OnDealItemErrorAck` (`MW_DEALITEMERROR_ACK`) — a trade/deal error
  routed to the target char's main map (by name) via the new
  `SendMwDealItemErrorReq` (senders_item.cpp).

Tests — `tests/test_item_handlers.cpp`: ADDITEMRESULT reaches the
named map server with its payload; DEALITEMERROR reaches the target's
map.

Build verified: cmake + ctest -R tworldsvr_asio (66/66 passed).

### W6-9 — what landed

**Friend-protected refuse relay** — completes the friend-ask
protection sub-case. When a char tries to friend a target that has
friend-protection enabled (the map gates that), the map sends
`MW_FRIENDPROTECTEDASK_ACK`; `OnFriendProtectedAskAck`
(handlers_friend.cpp) verifies the requester, resolves the target by
name, and relays an auto-refuse (`FRIEND_REFUSE` + the requester's
name) to the target's map via the existing `SendMwFriendAddReq`.
World's whole role is the relay — the protection state lives
map/DB-side, so no new state is modelled.

Tests — `tests/test_friend_protected_handlers.cpp`: Alice's request to
protected Bob arrives at Bob's map as a refuse naming Alice.

Build verified: cmake + ctest -R tworldsvr_asio (65/65 passed).

### W6-8 — what landed

**GM char message relay** — the control server's `CT_CHARMSG` (a
system / GM message addressed to a char by name). `OnCharMsgAck`
(handlers_chat.cpp) resolves the char by name, truncates the message
to 1 KiB (legacy `strMsg.Left(ONE_KBYTE)`), and routes it to the
char's main map as `MW_CHARMSG_REQ` (`SendMwCharMsgReq`). Reachable
via the same Dispatch as the SM_ scheduler messages — the control
server is just another peer sending by wID.

Tests — `tests/test_charmsg_handlers.cpp`: a message for an online
char reaches its map; one for an unknown name is dropped.

Build verified: cmake + ctest -R tworldsvr_asio (64/64 passed).

### W6-7 — what landed

**Solo-instance party lifecycle** — entering a solo instance puts the
char in a one-member `PT_SOLO` party so the instance's party-scoped
mechanics work; leaving tears it down.

- `OnEnterSoloMapAck` (handlers_party.cpp): if the char has no party,
  allocate one through `PartyRegistry::GenId` + `Insert` (obtain =
  `kObtainSolo`, the char as sole member + chief), set
  `TChar.party_id`, then mirror `MW_ENTERSOLOMAP_REQ`
  `(party_id, type, chief)` to each of the char's valid connections
  (`SendMwEnterSoloMapReq`).
- `OnLeaveSoloMapAck`: if the char's party is `PT_SOLO`, `Remove` it
  and clear `party_id` (no reply — legacy parity).

Reuses the W3b PartyRegistry; lock-ordering keeps the char + party
locks disjoint.

Tests — `tests/test_solomap_handlers.cpp`: a char on two connections
enters (a solo party appears, same id mirrored to both connections,
char.party_id set) then leaves (party removed, party_id cleared).

Build verified: cmake + ctest -R tworldsvr_asio (63/63 passed).

### W6-6 — what landed

**Monster-result relays** — `MONSTERDIE` (a monster the char killed
died) and `TAKEMONMONEY` (collect a monster's money drop) are
resolved on the map where the monster lives and routed by world back
to the char's *main* map. Both find the char by id+key and forward
the body verbatim (handlers_combat.cpp `RouteMonResult` +
`senders_combat.cpp`).

Deferred: `MONSTERBUY` (buying a siege NPC with guild treasury) — it
spends guild money (no `UseMoney` helper yet) and replies with the
`MSB_*` result enum, which is absent from the source tree.

Tests — `tests/test_monresult_handlers.cpp`: a result resolved on a
second peer routes verbatim to the char's main map.

Build verified: cmake + ctest -R tworldsvr_asio (62/62 passed).

### W6-5 — what landed

**Companion-mon (spolecnik) sync** — recall-mon's direct sibling
(`spolecnik` = companion). `OnCreateSpolecnikMonAck` +
`OnSpolecnikMonDelAck` (handlers_recallmon.cpp) reuse the recall-id
counter and the valid-connection fan-out: CREATE assigns the id when
the map sent 0 (patched into the body), DEL forwards verbatim; both
mirror to every valid connection of the char. Two passthrough senders.

Tests — `tests/test_spolecnikmon_handlers.cpp`: a char on two
connections gets a CREATE (freshly-assigned id, identical on both,
opaque tail preserved) + DEL mirrored to both.

Build verified: cmake + ctest -R tworldsvr_asio (61/61 passed).

### W6-4 — what landed

**Recall-mon (summoned creature) sync** — a char's summoned recall
monster is mirrored across all the char's valid map connections so
every client it's visible on renders it.

- `handlers_recallmon.cpp` — `OnCreateRecallMonAck` assigns the recall
  id when the map sent 0 (legacy `GenRecallID`, a `++counter`),
  patching it into the body, then forwards to each valid connection;
  `OnRecallMonDataAck` / `OnRecallMonDelAck` forward verbatim. All
  three are opaque passthroughs — the ACK and REQ share one wire
  layout — so no wide per-field sender is needed (`senders_recallmon.cpp`
  re-tags the body). DEL keys off char_id only (legacy parity).
- Deferred: the id counter's DB-seed at boot (legacy reads the last
  id from the DB); it starts at 0 here.

Tests — `tests/test_recallmon_handlers.cpp`: a char on two
connections gets a CREATE (with a freshly-assigned id, identical on
both, opaque tail preserved) + DATA + DEL mirrored to both.

Build verified: cmake + ctest -R tworldsvr_asio (60/60 passed).

### W6-3 — what landed

**Global announcement broadcasts** — two stateless fan-outs to every
map peer so the cluster stays in sync:

- `OnFameRankUpdateAck` (handlers_rank.cpp, `MW_FAMERANKUPDATE_ACK`) —
  the fame-ranking table refresh, forwarded **verbatim** (world never
  interprets it).
- `OnHeroSelectAck` (`MW_HEROSELECT_ACK`) — a battle-zone hero was
  chosen; broadcast `(battle_zone, hero_name, time)` to every peer.

Two senders in `senders_rank.cpp`.

Tests — `tests/test_rank_handlers.cpp`: FAMERANKUPDATE reaches both
peers byte-for-byte; HEROSELECT reaches both with the right
zone/name/time.

Build verified: cmake + ctest -R tworldsvr_asio (59/59 passed).

### W6-2 — what landed

**Combat / taming cross-server relays** — when the attacker and the
affected object sit on different map servers, the effect result is
routed to the *attacker's* map (the world server is the only thing
that knows which map a char is on).

- `handlers_combat.cpp` — `OnMagicMirrorAck` (spell reflection,
  forwarded **verbatim**), `OnMonTemptAck` (taming attempt),
  `OnMonTemptEvoAck` (taming evolution). Each resolves the attacker
  char by id and forwards to its main map via the matching sender
  (`senders_combat.cpp`).
- Deferred: `GETBLOOD` (lifesteal) — its routing branches on `OT_PC`,
  an object-type enum **absent from the source tree** (referenced
  across the map server but defined in a header not checked in).

Tests — `tests/test_combat_handlers.cpp`: an attacker on one peer has
each effect routed to its home peer (MAGICMIRROR byte-for-byte;
MONTEMPT/MONTEMPTEVO with the char's key + payload).

Build verified: cmake + ctest -R tworldsvr_asio (58/58 passed).

### W6-1 — what landed

**Timed-event broadcast** — opens the W6 event vertical. The scheduler
fires a "present quarter" timed event; world relays it cluster-wide.

- `OnEventQuarterReq` (handlers_event.cpp, `SM_EVENTQUARTER_REQ`) reads
  (day, hour, minute, present), picks the present bucket **once**
  (legacy `rand() % 100`) so every map shows the same one, and fans
  `MW_EVENTQUARTER_REQ` to every peer (`SendMwEventQuarterReq`).
- `OnEventQuarterNotifyReq` (`SM_EVENTQUARTERNOTIFY_REQ`) broadcasts
  the event announcement as a world-chat line to every peer, reusing
  the W4-5 `SendMwChatReq` (channel `CHAT_WORLD`). The operator display
  name (legacy `GetSvrMsg(NAME_OPERATOR)`) is deferred — the same
  server-message-table gap as the W4-5 operator-whisper — so the sender
  name is empty.

Tests — `tests/test_event_handlers.cpp`: EVENTQUARTER reaches both
peers with the same bucket + matching day/hour/minute/present;
EVENTQUARTERNOTIFY reaches both as a world-chat line carrying the
announcement.

Build verified: cmake + ctest -R tworldsvr_asio (57/57 passed).

### W5-4 — what landed

**War-window enable broadcast** — the front of the W5 war loop: when
the scheduler opens or closes a war window, world fans the matching
enable packet to every map peer. This is the trigger that *starts*
the sieges whose apply / occupy / reset W5-1..3 handle.

- `OnBattleStatusReq` (handlers_occupy.cpp, `SM_BATTLESTATUS_REQ`)
  reads (type, status, start, second) and, per the battle type,
  broadcasts `MW_LOCALENABLE_REQ` / `MW_CASTLEENABLE_REQ` /
  `MW_MISSIONENABLE_REQ` to every peer. Three senders in
  senders_occupy.cpp.
- `battle_constants.h` — the `BATTLE_TYPE` selector, reconstructed
  (the enum is absent from the tree): `kTypeCastle = 1` matches the
  committed reconstruction in `TControlSvrAsio` (kBattleTypeCastle);
  LOCAL=0 / MISSION=2 follow the legacy declaration order.

Deferred: the `BS_PEACE` peace-time bookkeeping (record-date reset +
per-guild `CalcWeekRecord` + castle-war-info clear) — it needs the
PvP-record-date + castle-war-info systems; and SKYGARDEN (`#ifdef`).

Tests — `tests/test_battlestatus_handlers.cpp`: each of the three
battle types fans the correct enable packet to both peers with the
right status/timer fields.

Build verified: cmake + ctest -R tworldsvr_asio (56/56 passed).

### W5-3 — what landed

**Castle-occupy application reset** — closes the reset that W5-1
deferred. When a castle is occupied the siege is over, so every
applicant to that castle (in both the winning and losing guild) has
their application cleared.

- `ResetCastleApply(ctx, guild_id, castle_id)` (handlers_occupy.cpp):
  under the guild lock, zero each member's + tactics member's
  castle/camp where it matches and collect their char_ids; then route
  a `MW_CASTLEAPPLY_REQ(SUCCESS, castle=0)` to each affected char's
  map (separate char locks — disjoint from the guild lock). Mirrors
  legacy `CTWorldSvrModule::ResetCastleApply`.
- `OnCastleOccupyAck` now calls it for the winning guild and (when
  distinct) the losing guild before the occupation broadcast. The
  guild stat-exp award is still deferred (absent CALCULATE_NEXTGEXP /
  *_STATEXP constants).

Reuses W5-2's castle/camp fields + `SendMwCastleApplyReq` — no new
wire. With W5-1/W5-2 the castle-siege state now has a full lifecycle:
apply (cap-gated) → applicant-count broadcast → occupy → reset.

Tests — `tests/test_castle_occupy_reset_handlers.cpp`: a member
applied to a castle has their application cleared (with the map
notified) when that castle is occupied, alongside the occupy
broadcast.

Build verified: cmake + ctest -R tworldsvr_asio (55/55 passed).

### W5-2 — what landed

**Castle-war apply** — a guild chief assigns members (and hired
tactics members) to attack/defend a castle; this is the per-member
apply state that W5-1's deferred `ResetCastleApply` will later clear.

- `TGuildMember` already carried `castle`/`camp`; `TTacticsMember`
  gains them. `TGuild::CastleApplicantCount` + `CanApplyWar` (the
  legacy literal 49-applicant cap) count members + tactics per castle.
- `OnCastleApplyAck` (handlers_occupy.cpp): chief-only gate; resolves
  the target as a member (rejecting one who's a merc elsewhere) or a
  tactics member; re-applying to the same castle cancels; the cap
  yields `CBS_FULL`. On a change it toggles the target's castle/camp,
  replies `MW_CASTLEAPPLY_REQ(SUCCESS)` to the chief's map and the
  assigned member's map, and re-broadcasts the applicant count for the
  vacated + joined castle (`MW_CASTLEAPPLICANTCOUNT_REQ`) to every
  peer. Snapshot-then-release locking keeps the guild + char locks
  disjoint.
- `castle_constants.h` — `CBS_*` result codes. `kSuccess = 0` is
  certain (the client's `!=` sentinel + codebase convention); the
  error codes (only `kFull` is emitted) are reconstructed from the
  client switch ordering since the real enum is **absent from the
  source tree**, and flagged as inferred.

Deferred: DB persistence (legacy `DM_CASTLEAPPLY_REQ`) — castle/camp
aren't in the guild-member load query yet, so the assignment is
in-memory only (a world restart loses pending applications).

Tests — `tests/test_castle_apply_handlers.cpp`: chief Alice assigns
member Bob to a castle; both maps get the SUCCESS reply, every peer
gets the applicant count, and Bob's member row holds the assignment.

Build verified: cmake + ctest -R tworldsvr_asio (54/54 passed).

### W5-1 — what landed

**Territory occupation broadcasts** — opens the W5 war vertical with
the cluster coordinator's core territory role: when a castle / local
(territory) / mission objective changes hands, world fans the new
owner + flag to **every** map peer so the whole cluster renders it
consistently.

- `handlers_occupy.cpp` — `OnCastleOccupyAck` (wID 0x9087),
  `OnLocalOccupyAck` (0x906B), `OnMissionOccupyAck` (0x9168). CASTLE
  and LOCAL resolve the guild name from the registry; LOCAL applies
  the legacy B-country display flip (a `TCONTRY_B` guild's capture
  shows as the opposing flag and reports guild-less when the prior
  holder wasn't neutral). MISSION is a bare type/local/country fan-out.
- `senders_occupy.cpp` — `SendMwCastleOccupyReq` / `SendMwLocalOccupyReq`
  / `SendMwMissionOccupyReq`, each broadcast over `peers->Snapshot()`.

Deferred — **blocked on data absent from the source tree**: the
winning guild's stat-exp award + level-up (CASTLEOCCUPY / LOCALOCCUPY)
needs `CALCULATE_NEXTGEXP` + the `*_STATEXP` award constants, which
are referenced by client + both servers but **defined nowhere in the
repo**; and CASTLEOCCUPY's `ResetCastleApply` (clearing members'
castle-application flags) needs the castle-apply subsystem, not yet
ported. Both are follow-up W5 slices once the constants / model land.

Tests — `tests/test_occupy_handlers.cpp` (two-peer): each occupy
reaches both peers with the right fields, and a B-country guild's
LOCAL capture arrives flag-flipped + guild-less.

Build verified: cmake + ctest -R tworldsvr_asio (53/53 passed).

### W4-20 — what landed

**Login finalization + connect presence** — `MW_ENTERSVR_ACK` is the
"char is fully loaded on its map" signal. It was the missing piece
that blocked the connect-side friend presence: at `OnAddCharAck` the
char's own name/region aren't known yet (ADDCHAR carries neither), so
any "X came online" toast would have shipped a blank name.

- `OnEnterSvrAck` (handlers_char.cpp, wID 0x9009) reads the 25-field
  login packet, validates char + key (drops on mismatch / enter
  error — the legacy DELCHAR / INVALIDCHAR / CONRESULT replies are
  deferred), indexes the name via `CharRegistry::Rename` (so
  `FindByName` resolves a logged-in char without a prior rename), and
  bulk-sets identity + position + region (the values the incremental
  W3a-3 / W4-8 / W4-9 / W4-14 handlers later refine).
- It then calls `NotifyFriendsOnLogin` — the connect-side mirror of
  the W4-7 logout fan-out (reverted once in W4-16 as premature, now
  correct). For each friend online *right now* (resolved live), it
  flips both sides' connected flag and, for real friends (mutual /
  target), pushes `MW_FRIENDCONNECTION_REQ(CONNECTION)` to their map.

With W4-15..18 (persistence) this makes the friend subsystem behave
across a relog: friends load, the player's arrival is announced, and
their friends' rosters light up live.

Deferred: the big `CHARINFO_REQ` composite reply (guild / duty / peer
/ castle), the relay-server `CHANGEMAP`, and the enter-failure replies
— client-facing pieces that don't affect the world-side identity +
presence this slice delivers.

Tests — `tests/test_entersvr_handlers.cpp`: Bob is online with Alice
as a mutual friend; Alice's ENTERSVR indexes her name and Bob receives
a `FRIEND_CONNECTION` toast naming her, with his roster entry flipped
connected.

Build verified: cmake + ctest -R tworldsvr_asio (52/52 passed).

### W4-19 — what landed

**GM chat ban** — a moderator silences a player's chat for N minutes
(0 = unban). The world resolves the target by name, owns the ban
timer, enforces it on the target's map, and echoes the result to the
issuing GM.

- `TChar.chat_ban_time` (legacy m_nChatBanTime) — Unix second until
  which chat is banned; 0 = clear.
- `OnChatBanAck` (handlers_chat.cpp, wID 0x9117) — `FindByName` the
  target (INVALIDCHAR back to the GM if missing), then for minutes>0
  start a fresh ban or stack onto an active one (legacy timer math),
  for minutes==0 clear it. Sends `MW_CHATBAN_REQ` to the target's map
  (enforce, no GM id) and, when the GM supplied char_id/key, echoes
  the result to the GM's map. `SendMwChatBanReq` in senders_chat.cpp;
  CHATBAN_RESULT codes in chat_constants.h.

Deferred: the cluster-wide ban list (legacy AddChatBan — so a ban
survives the target reconnecting on another map) and the RW
relay-server propagation, both of which need the operator/ban-list
infra (same family as the RW_RELAYSVR operator list).

Tests — `tests/test_chatban_handlers.cpp`: ban (target map enforces +
GM echo, timer set), unknown target (INVALIDCHAR to the GM only),
unban (timer cleared).

Build verified: cmake + ctest -R tworldsvr_asio (51/51 passed).

### W4-18 — what landed

**Soulmate write-back** — the last friend/soulmate persistence gap.
The pairing flows mutated only the in-memory `TChar.soulmate`; a
relog reloaded the old pairing (or lost a fresh one).

- `IFriendRepository` gains `RegSoulmate(char_id, target)` (upsert,
  `dwTime` reset to 0 — mirrors the `TSoulmateReg` proc via a
  portable delete-then-insert since `dwCharID` is the PK) +
  `DelSoulmate(char_id, target)` (mirrors `TSoulmateDel`), Soci + Fake.
- A pairing is mutual (one row per char), so every site persists
  both directions: `OnSoulmateSearchAck` (matchmaking pair),
  `OnSoulmateRegAck` (register branch), `OnSoulmateEndAck` (dissolve),
  and the W4-9 `OnLevelUpAck` level-gap auto-dissolve. All via
  `CoOffloadVoidIf`, best-effort.

This closes friend/soulmate persistence end to end: groups, group
membership, friend edges, and soulmate pairings all survive a relog
(read path: W4-15; writes: W4-16/17/18).

Deferred: the connect-side login-presence fan-out is still blocked on
a "char identity loaded" signal — at OnAddCharAck the char's own name
and region aren't set yet (they arrive with the later
CHANGECHARBASE), so the FRIEND_CONNECTION toast would carry a blank
name; it needs a post-identity-load hook the port doesn't have yet.

Tests — `tests/test_soulmate_persist_handlers.cpp`: Alice registers
Bob (both rows land in the repo), then dissolves (both rows deleted).

Build verified: cmake + ctest -R tworldsvr_asio (50/50 passed).

### W4-17 — what landed

**Friend-edge write-back** — persists the friendship graph itself
(W4-16 did the named groups). The accept paths added the mutual
friendship only in memory; a relog reloaded the old graph.

- `IFriendRepository` gains `InsertFriend(char_id, friend_id)` (one
  directed TFRIENDTABLE row, group 0) + `EraseFriend(char_id,
  friend_id)` (Soci + Fake), mirroring CSPFriendInsert /
  CSPFriendErase (SSHandler.cpp:6185/6202).
- Both accept paths — the OnFriendAskAck both-sides-pending shortcut
  and the OnFriendReplyAck YES branch — persist the two directed
  edges (legacy fired DM_FRIENDINSERT_REQ twice). OnFriendEraseAck
  deletes the requester's forward edge on the FT_FRIENDFRIEND demote
  and the FT_FRIEND one-way removal; an FT_TARGET erase leaves the DB
  untouched (legacy parity). All via CoOffloadVoidIf, best-effort.

With W4-16 this completes the friend write-back: groups, group
membership, and the edges all survive a relog.

Deferred to W4-18: soulmate write-back (TSOULMATETABLE reg/del). The
connect-side login presence remains blocked on a char-identity-loaded
signal (name/region unset at OnAddCharAck).

Tests — `tests/test_friend_edge_handlers.cpp`: Bob accepts Alice's
invite (both edges land in the repo); Alice removes Bob (only her
forward edge is deleted, Bob still friends Alice).

Build verified: cmake + ctest -R tworldsvr_asio (49/49 passed).

### W4-16 — what landed

**Friend-group write-back** — the write half of the W4-15 read path,
for the four friend-group mutations. The W4-3 group handlers
(MAKE / DELETE / CHANGE / NAME) updated only the in-memory registry;
a relog reloaded the old groups from the DB. Now each success also
persists.

- `IFriendRepository` gains `MakeGroup` / `DeleteGroup` /
  `RenameGroup` / `ChangeFriendGroup` (Soci + Fake), each one legacy
  CSP wrapper (CSPFriendGroupMake/Delete/Name/Change,
  SSHandler.cpp:6490+) — single-table INSERT / DELETE / UPDATE.
- The four handlers fire the matching write via `CoOffloadVoidIf`
  only on a successful in-memory mutation, after the client reply.
  Best-effort like the guild writes (a false return doesn't reverse
  the in-memory change; the registry stays authoritative in-session).

Deferred: friend insert/erase write-back (the accept path inserts
both directions; the erase path deletes the forward edge in both the
demote and one-way cases) + soulmate write-back land in W4-17. The
connect-side login-presence fan-out is still blocked on a
"char identity loaded" signal — at OnAddCharAck the char's own name
and region aren't set yet (they arrive with the later
CHANGECHARBASE), so the FRIEND_CONNECTION toast would carry a blank
name; it needs a post-identity-load hook the port doesn't have yet.

Tests — `tests/test_friend_persist_handlers.cpp`: a fake-backed run
drives MAKE / NAME / CHANGE / (MAKE+DELETE) over the wire and reads
the repo back to confirm each write landed.

Build verified: cmake + ctest -R tworldsvr_asio (48/48 passed).

### W4-15 — what landed

**Friend/soulmate load-at-login** — the friend subsystem's missing
persistence read path. Until now the in-memory friend list was only
what FRIENDASK/REPLY built up during a single session; a relog
started empty. Now a char's social graph is hydrated from the DB
when it comes online — exactly what the legacy DM_FRIENDLIST
round-trip did, collapsed to a direct query like the guild repo.

- `services/friend_repository.h` — `IFriendRepository::LoadForChar`
  returns a `FriendLoad { groups, forward[], reverse[], soulmate }`.
  Friend rows carry id/name (+ level/class on the forward edges)
  resolved by the legacy JOIN against TCHARTABLE.
- `SociFriendRepository` (TFRIENDGROUPTABLE / TFRIENDTABLE forward +
  reverse / TSOULMATETABLE) + `FakeFriendRepository` (test seed).
  Wired into `HandlerContext.friend_repo` + `main.cpp` (Soci when
  `[database]` is set, else nullptr — no-DB dev path skips the load).
- `OnAddCharAck` hydrates the char on first connect via
  `CoOffloadIf` (never blocks the io_context). `ApplyFriendLoad`
  derives the friend **type** from the forward/reverse intersection
  — forward-only = FT_FRIEND, mutual = FT_FRIENDFRIEND, reverse-only
  = FT_TARGET (legacy OnDM_FRIENDLIST_ACK, SSHandler.cpp:1683) — and
  resolves each friend's `connected` flag live from the registry
  (CharRegistry::Find is shard-lock only, so it's safe under the
  char's entity lock; region/level stay live-resolved by the
  W4-4 FRIENDLIST reader).

This makes the W4-4 FRIENDLIST reader and W4-7 logout-presence work
against persistent data. The connect-side login-presence fan-out
(notifying online friends that this char just came online) is the
W4-16 follow-up; friend-mutation write-back stays deferred (the
registry is authoritative within a session).

Tests — `tests/test_friend_load_handlers.cpp`: a seeded fake repo
hydrates Alice on ADDCHAR with Bob (forward → FT_FRIEND, connected
because online) + Carol (reverse-only → FT_TARGET, offline) + a
named group.

Build verified: cmake + ctest -R tworldsvr_asio (47/47 passed).

### W4-14 — what landed

**Per-character visual state sync** — continues the W4-8 (region) /
W4-9 (level) "live per-char state propagation" theme with two
cosmetic-state handlers (handlers_char.cpp).

- `OnPetRidingAck` (wID 0x90D0) — a char mounted / dismounted.
  Stores `TChar.riding` and fans `MW_PETRIDING_REQ` to the char's
  *other* (non-originating, valid) map sessions so every client it
  is visible on renders the mount. Mirrors legacy
  SSHandler.cpp:8604, which excludes the originating server (it
  already applied the change locally).
- `OnHelmetHideAck` (wID 0x9103) — a char toggled helmet
  visibility. Stores `TChar.helmet_hide` and confirms
  `MW_HELMETHIDE_REQ` back to the originating map (legacy
  SSHandler.cpp:8683).

`TChar` gains `riding` (u32) + `helmet_hide` (u8); senders
`SendMwPetRidingReq` / `SendMwHelmetHideReq` live in
`senders_relay.cpp` next to the W4-9 LevelUp sender.

Tests — `tests/test_appearance_handlers.cpp` (two-peer, one char on
both): PETRIDING from the main session reaches the secondary
session with the mount id; HELMETHIDE echoes back to the sender;
both fields land on the registry entry.

Build verified: cmake + ctest -R tworldsvr_asio (46/46 passed).

### W4-13 — what landed

**Mail delivery relay** — world's entire role in the mail/post
system. The mailbox itself (list / read / delete) lives DB-side and
map-side; world only routes the "you have new mail" delivery ping to
the recipient's map, keyed by the target's name.

- `OnPostRecvAck` (wID 0x907E) — a player sent mail; relay the
  notification to the recipient's map.
- `OnReservedPostRecvAck` (wID 0x5909) — DB-side system / scheduled
  ("reserved") mail; identical relay.

Both share one opaque-passthrough helper (`RelayPostRecv`): parse
the target name off the `(post_id, sender, target, title, type)`
body, look the char up, and forward the bytes **verbatim** as
`MW_POSTRECV_REQ` via `SendMwPostRecvReq`. An offline target is
dropped (the mail is already persisted DB-side and shown next time
the mailbox opens) — matching legacy `OnMW_POSTRECV_ACK` /
`OnDM_RESERVEDPOSTRECV_ACK`.

Deferred: the reserved-post *generator* poll
(`DM_RESERVEDPOSTSEND_REQ`, a `CSPGetReservedPost` stored-proc
sweep that emits the RESERVEDPOSTRECV pings) — it needs the
reserved-post table/SP, so it lands with the DB-persistence batch.

Tests — `tests/test_post_handlers.cpp` (two-peer): both the player
and system mail paths forward the body unchanged to the recipient's
map, and an unknown-target ping is silently dropped without
disturbing the next valid delivery.

Build verified: cmake + ctest -R tworldsvr_asio (45/45 passed).

### W4-12 — what landed

**TMS presence on logout** — completes W4-11's lifecycle for the
disconnect path. `CloseChar` in the legacy calls `LeaveTMS`
alongside `LeaveFriend` / `LeaveSoulmate`; W4-11 wired the first two
(W4-7) but not the TMS cleanup, leaving a logging-out member as a
stale id in any conference they were in.

- `NotifyTmsOnLogout(ctx, who)` (handlers_tms.cpp) — for each
  conference in the char's `tms` id-set: stash the leaver's name in
  `last_member`, drop them from the roster, tell the surviving
  members via `MW_TMSOUT_REQ`, and destroy an emptied conference.
  Mirrors legacy `LeaveTMS` (TWorldSvr.cpp:2845) exactly.
- Wired into `OnCloseCharAck` after the W4-7 friend/soulmate
  notifies (the removed char is out of the registry but kept alive
  by the caller's shared_ptr).

Tests — `tests/test_tms_logout_handlers.cpp` (two-peer): two chars
share a conference; one logs out and the survivor receives a
`MW_TMSOUT_REQ` naming the departed member.

Build verified: cmake + ctest -R tworldsvr_asio (44/44 passed).

### W4-11 — what landed

**TMS conference channels** — the in-game multi-party "temporary
messaging system" (a group/conference chat that survives members
joining and leaving). World owns the conference roster; the four
`MW_TMS*_ACK` handlers drive the whole lifecycle.

New cluster state:
- `services/tms_registry.{h,cpp}` — `TTms { id, last_member,
  members[] }` + a 16-shard `TmsRegistry` (same partitioning /
  per-group-mutex model as PartyRegistry; 32-bit rolling `GenId`,
  id 0 reserved). Wired into `HandlerContext.tms` + `main.cpp`.
- `TChar.tms` — the conference id-set a char belongs to (legacy
  `m_mapTMS`), the cycle-free back-link resolved through the
  registry on demand.

Handlers (`handlers_tms.cpp`):
- **SEND** (0x9077) — post to a conference. A solo (size==1)
  conference re-pairs its last departed member by popping a
  `MW_TMSINVITEASK_REQ` dialog on their client; a populated
  conference fans the message to every member as `MW_TMSRECV_REQ`.
- **INVITEASK** (0x90CE) — answer to that dialog. On accept the new
  member joins (roster announced via `MW_TMSINVITE_REQ`); either
  way the pending message is delivered to the roster.
- **INVITE** (0x907A) — open / expand a conference with a target
  list (filtered to online, same-war-country targets). Handles the
  1:1 re-pair shortcut and fresh-group creation, then broadcasts
  the full roster.
- **OUT** (0x907C) — leave. The roster is told via `MW_TMSOUT_REQ`,
  the leaver is dropped (its name stashed in `last_member` for a
  future re-pair), and an emptied conference is destroyed.

Senders (`senders_tms.cpp`) — `SendMwTmsRecvReq` /
`SendMwTmsInviteAskReq` / `SendMwTmsInviteReq` (variable roster) /
`SendMwTmsOutReq`.

Deferred: the legacy `TMS_NORECEIVER` server-message (a localized
`GetSvrMsg` string shown when a solo re-pair finds no target) is
replaced by an empty message — the server-message table isn't
ported (same deferral as the W4-5 chat operator-whisper sub-case).

Tests — `tests/test_tms_handlers.cpp` (two-peer) walks the full
lifecycle: INVITE opens a conference, SEND fans a message, OUT
tears it down to a solo channel, a solo SEND re-pairs the departed
member, and INVITEASK-accept restores the roster + delivers the
pending message.

Build verified: cmake + ctest -R tworldsvr_asio (43/43 passed).

### W4-10 — what landed

**Inspect-player stat relay** — the cross-shard "examine another
player" flow (the client opens a target's stat window).

- `OnCharStatInfoAck` (wID 0x9065): the requester asks about a
  target by id; world routes `MW_CHARSTATINFOANS_REQ(req, target)`
  to the *target's* map so it can gather the live stat block.
- `OnCharStatInfoAnsAck` (wID 0x9067): the target's map returns the
  block (leading with the requester id); world forwards it
  **verbatim** as `MW_CHARSTATINFO_REQ` to the requester's map —
  an opaque passthrough (world never interprets the stats).

Senders — `SendMwCharStatInfoAnsReq` (2-field) +
`SendMwCharStatInfoReq` (raw-body passthrough), in
`senders_relay.cpp`.

Tests — `tests/test_charstat_handlers.cpp` (two-peer): the request
reaches the target's map with the right ids, and a stat block with
an opaque tail round-trips unchanged back to the requester.

Build verified: cmake + ctest -R tworldsvr_asio (42/42 passed).

### W4-9 — what landed

**Level update** — `MW_LEVELUP_ACK` is the authoritative source for
`TChar.level` (which the party / guild member-list / friend-list /
soulmate displays read), so this makes level live world-side.

`OnLevelUpAck` (wID 0x9028): stores the char's `level`, then:
- fans `MW_LEVELUP_REQ` to the char's *other* (non-main, valid) map
  connections so every map showing the char updates;
- syncs the new level into the soulmate partner's reverse entry,
  and **auto-dissolves** the pairing (both sides) when the level
  gap now exceeds `SOULMATE_LEVEL` (legacy CheckSoulmateEnd).

Deferred: the legacy war-country level-gap index (a W5 matchmaking
structure) and DB persistence. Lock discipline: snapshot/update the
char under its lock, then each partner under its own.

Sender — `SendMwLevelUpReq` (3-field, `senders_relay.cpp`).

Tests — `tests/test_levelup_handlers.cpp` (3 scenarios): a small
level gain (soulmate view synced, pairing intact), a large jump
(soulmate auto-dissolved both sides), and a level-up fanned to a
char's second map connection.

Build verified: cmake + ctest -R tworldsvr_asio (41/41 passed).

### W4-8 — what landed

**Region update** — makes the `TChar.region` field (added in W4-1
but always 0) live, so the friend/soulmate presence views carry an
accurate last-seen zone.

`OnRegionAck` (wID 0x90BD): stores the char's `region`, then
mirrors it into the soulmate partner's reverse entry (marking it
connected — a region update implies online) and into each
connected real-friend's (non-`FT_FRIEND`) reverse entry. Pure
in-memory state propagation — no outbound packet, no DB. Lock
discipline: the char is snapshotted/updated under its lock, then
each partner under its own (never two at once).

Tests — `tests/test_region_handlers.cpp`: a region update for a
char that's a mutual friend + soulmate of one peer and a one-way
friend of another; verifies the region lands on the char, on the
soulmate + mutual-friend reverse entries, but not on the one-way
`FT_FRIEND` entry.

Build verified: cmake + ctest -R tworldsvr_asio (40/40 passed).

### W4-7 — what landed

**Social presence on logout** — the offline half of the
friend/soulmate presence flow, wired into the existing char
lifecycle.

`OnCloseCharAck` now, after removing the char from the registry
(the `shared_ptr` keeps it alive), calls:
- `NotifyFriendsOnLogout` (handlers_friend.cpp, legacy
  LeaveFriend): for each connected friend, marks that friend's
  reverse entry offline and — for a real (mutual / pending) friend,
  not a one-way `FT_FRIEND` stub — pushes
  `MW_FRIENDCONNECTION_REQ(DISCONNECTION)` to the friend's map.
- `NotifySoulmateOnLogout` (handlers_soulmate.cpp, legacy
  LeaveSoulmate): marks the partner's soulmate entry offline
  (no packet, per legacy).

Both operate on the in-memory social state (set by the W4-1..6
handlers) and snapshot-then-release per char (no two char locks
held at once). Sender — `SendMwFriendConnectionReq` (5-field).

Deferred: the **connect** side (login presence fan-out) and the
friend/soulmate **DB load** — both need the friend/soulmate
repository + char-load wiring, a coupled cluster for a later slice.

Tests — `tests/test_friend_presence_handlers.cpp`: a mutual
friend + soulmate + a one-way friend, all online; on logout the
mutual friend gets the disconnect toast, all three reverse entries
(+ the soulmate) flip offline, and the char leaves the registry.

Build verified: cmake + ctest -R tworldsvr_asio (39/39 passed).

### W4-6 — what landed

The **soulmate** (marriage/pairing) subsystem — the last W4 social
feature with a clean wire surface.

Model — `TChar` gains `real_sex` (m_bRealSex, the account gender,
distinct from the avatar `sex`) + `soulmate` (a `TSoulmate`: target
id / name / level / class / connected / region; target 0 =
unpaired).

Handlers (`handlers_soulmate.cpp`):
- `OnSoulmateSearchAck` (0x9109): matchmaking over the online
  roster — same country, within `SOULMATE_LEVEL` (10), picking the
  lowest-level candidate, with the legacy tiebreak chain (opposite
  real-sex → no existing soulmate → opposite avatar-sex). On a
  match it pairs both chars mutually + replies `SOULMATE_SUCCESS`
  with the partner; none → `SOULMATE_NOTFOUND`.
- `OnSoulmateRegAck` (0x910B): a named pairing — gates same country
  + level window (`SOULMATE_FAIL`). `bReg=1` commits the mutual
  pairing; `bReg=0` is a preview (success reply, no pairing).
- `OnSoulmateEndAck` (0x910D): dissolves the current pairing on
  both sides (`SOULMATE_SUCCESS` + timestamp); no pairing →
  `SOULMATE_FAIL`.

The legacy DB round-trip (`DM_SOULMATEREG/END_REQ`) is collapsed to
an in-memory mutual pairing (consistent with the guild-establish /
friend-mutual-add collapses); persistence is deferred. Lock
discipline: each char's `soulmate` is mutated under its own lock,
sequentially.

Senders — `SendMwSoulmateSearch/Reg/EndReq` in the new
`senders_soulmate.cpp`.

Tests — `tests/test_soulmate_handlers.cpp` (6 scenarios,
three-peer loopback): SEARCH match with the real-sex tiebreak +
mutual pairing, REG preview (no pairing) vs register (pairing),
cross-country REG FAIL, END dissolve (both cleared), and END with
no soulmate FAIL.

Build verified: cmake + ctest -R tworldsvr_asio (38/38 passed).

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
