# MW Ownership Matrix — TWorld ↔ TMap coordination

Single source of truth for the Map↔World wire family. Generated from code: the MW_ id universe is `Lib/Own/TProtocol/include/MessageId.h`; the World columns are `Server/TWorldSvrAsio/handlers/dispatch.cpp` (recv) + `senders/*.cpp` (send); the TMap column scans `Server/TMapSvrAsio/{handlers_world,services/world_client,handlers/*}`.

Direction convention: World **recv** = Map→World (TMap must **send** it); World **send** = World→Map (TMap must **handle** it). The wire shape is frozen (shipped client + legacy binaries), so both halves must match TProtocol byte-for-byte.

## How to use this for coordination

- Pick a vertical from a family with high **World-ready** — the World half is already wired+tested, so the slice cost is **only the TMap mirror half**.
- Families with World-ready = 0 (Tournament, Rank, APEX) need **both** halves — schedule later.
- Each slice = the MW message(s) for one feature, wired on both ends + an end-to-end test, in one PR.

## Summary by family

| Family | MW msgs | World-ready | TMap started | Slice cost |
|---|--:|--:|--:|---|
| Guild | 87 | 76 | 0 | mixed |
| Other | 54 | 29 | 1 | mixed |
| Friend/Soulmate | 28 | 23 | 0 | mixed |
| Party | 26 | 21 | 1 | mixed |
| Conn/Teleport | 24 | 21 | 1 | mixed |
| Char state | 21 | 19 | 0 | mixed |
| Corps | 27 | 18 | 0 | mixed |
| Combat/Monster | 22 | 18 | 0 | mixed |
| Chat/TMS | 13 | 13 | 0 | cheap (World done) |
| Castle/War | 28 | 10 | 0 | mixed |
| Event | 8 | 6 | 0 | mixed |
| BattleRoyale | 8 | 5 | 0 | mixed |
| Item | 14 | 4 | 0 | mixed |
| Cash/CMGift | 7 | 4 | 0 | mixed |
| Arena/BattleMode | 3 | 3 | 0 | cheap (World done) |
| Mail | 3 | 2 | 0 | mixed |
| Bow | 3 | 1 | 0 | mixed |
| Tournament | 25 | 0 | 0 | expensive (World 0) |
| Rank | 6 | 0 | 0 | expensive (World 0) |
| APEX | 3 | 0 | 0 | expensive (World 0) |
| **TOTAL** | **410** | **273** | **3** | |

## Full matrix (by family)

### Guild — 76/87 World-ready

| MW message | World | TMap action needed |
|---|---|---|
| `MW_CASTLEGUILDCHG_REQ` | — | handle/send (+World half) |
| `MW_GUILDARTICLEADD_ACK` | recv | **send** |
| `MW_GUILDARTICLEADD_REQ` | send | **handle** |
| `MW_GUILDARTICLEDEL_ACK` | recv | **send** |
| `MW_GUILDARTICLEDEL_REQ` | send | **handle** |
| `MW_GUILDARTICLELIST_ACK` | recv | **send** |
| `MW_GUILDARTICLELIST_REQ` | send | **handle** |
| `MW_GUILDARTICLEUPDATE_ACK` | recv | **send** |
| `MW_GUILDARTICLEUPDATE_REQ` | send | **handle** |
| `MW_GUILDCABINETLIST_ACK` | recv | **send** |
| `MW_GUILDCABINETLIST_REQ` | send | **handle** |
| `MW_GUILDCABINETPUTIN_ACK` | recv+send | handle+send |
| `MW_GUILDCABINETPUTIN_REQ` | — | handle/send (+World half) |
| `MW_GUILDCABINETTAKEOUT_ACK` | recv | **send** |
| `MW_GUILDCABINETTAKEOUT_REQ` | — | handle/send (+World half) |
| `MW_GUILDCONTRIBUTION_ACK` | recv | **send** |
| `MW_GUILDCONTRIBUTION_REQ` | send | **handle** |
| `MW_GUILDDISORGANIZATION_ACK` | recv | **send** |
| `MW_GUILDDISORGANIZATION_REQ` | send | **handle** |
| `MW_GUILDDUTY_ACK` | recv | **send** |
| `MW_GUILDDUTY_REQ` | send | **handle** |
| `MW_GUILDESTABLISH_ACK` | recv | **send** |
| `MW_GUILDESTABLISH_REQ` | send | **handle** |
| `MW_GUILDFAME_ACK` | recv | **send** |
| `MW_GUILDFAME_REQ` | send | **handle** |
| `MW_GUILDINFO_ACK` | recv | **send** |
| `MW_GUILDINFO_REQ` | send | **handle** |
| `MW_GUILDINVITEANSWER_ACK` | recv | **send** |
| `MW_GUILDINVITE_ACK` | recv | **send** |
| `MW_GUILDINVITE_REQ` | send | **handle** |
| `MW_GUILDJOIN_REQ` | send | **handle** |
| `MW_GUILDKICKOUT_ACK` | recv | **send** |
| `MW_GUILDLEAVE_ACK` | recv | **send** |
| `MW_GUILDLEAVE_REQ` | send | **handle** |
| `MW_GUILDLOCALLIST_REQ` | — | handle/send (+World half) |
| `MW_GUILDMEMBERLIST_ACK` | recv | **send** |
| `MW_GUILDMEMBERLIST_REQ` | send | **handle** |
| `MW_GUILDMONEYRECOVER_ACK` | recv | **send** |
| `MW_GUILDPEER_ACK` | recv | **send** |
| `MW_GUILDPEER_REQ` | send | **handle** |
| `MW_GUILDPOINTLOG_ACK` | recv | **send** |
| `MW_GUILDPOINTLOG_REQ` | send | **handle** |
| `MW_GUILDPOINTREWARD_ACK` | recv | **send** |
| `MW_GUILDPOINTREWARD_REQ` | send | **handle** |
| `MW_GUILDPOINTTAKE_REQ` | — | handle/send (+World half) |
| `MW_GUILDPVPRECORD_ACK` | recv | **send** |
| `MW_GUILDPVPRECORD_REQ` | send | **handle** |
| `MW_GUILDSKILLACTION_ACK` | — | handle/send (+World half) |
| `MW_GUILDSKILLACTION_REQ` | — | handle/send (+World half) |
| `MW_GUILDTACTICSANSWER_ACK` | recv | **send** |
| `MW_GUILDTACTICSANSWER_REQ` | send | **handle** |
| `MW_GUILDTACTICSINVITE_ACK` | recv | **send** |
| `MW_GUILDTACTICSINVITE_REQ` | send | **handle** |
| `MW_GUILDTACTICSKICKOUT_ACK` | recv | **send** |
| `MW_GUILDTACTICSKICKOUT_REQ` | send | **handle** |
| `MW_GUILDTACTICSLIST_ACK` | recv | **send** |
| `MW_GUILDTACTICSLIST_REQ` | send | **handle** |
| `MW_GUILDTACTICSREPLY_ACK` | recv | **send** |
| `MW_GUILDTACTICSREPLY_REQ` | send | **handle** |
| `MW_GUILDTACTICSVOLUNTEERINGDEL_ACK` | recv | **send** |
| `MW_GUILDTACTICSVOLUNTEERINGDEL_REQ` | — | handle/send (+World half) |
| `MW_GUILDTACTICSVOLUNTEERING_ACK` | recv | **send** |
| `MW_GUILDTACTICSVOLUNTEERING_REQ` | send | **handle** |
| `MW_GUILDTACTICSVOLUNTEERLIST_ACK` | recv | **send** |
| `MW_GUILDTACTICSVOLUNTEERLIST_REQ` | — | handle/send (+World half) |
| `MW_GUILDTACTICSWANTEDADD_ACK` | recv | **send** |
| `MW_GUILDTACTICSWANTEDADD_REQ` | send | **handle** |
| `MW_GUILDTACTICSWANTEDDEL_ACK` | recv | **send** |
| `MW_GUILDTACTICSWANTEDDEL_REQ` | send | **handle** |
| `MW_GUILDTACTICSWANTEDLIST_ACK` | recv | **send** |
| `MW_GUILDTACTICSWANTEDLIST_REQ` | send | **handle** |
| `MW_GUILDVOLUNTEERINGDEL_ACK` | recv | **send** |
| `MW_GUILDVOLUNTEERINGDEL_REQ` | send | **handle** |
| `MW_GUILDVOLUNTEERING_ACK` | recv | **send** |
| `MW_GUILDVOLUNTEERING_REQ` | send | **handle** |
| `MW_GUILDVOLUNTEERLIST_ACK` | recv | **send** |
| `MW_GUILDVOLUNTEERLIST_REQ` | send | **handle** |
| `MW_GUILDVOLUNTEERREPLY_ACK` | recv | **send** |
| `MW_GUILDVOLUNTEERREPLY_REQ` | send | **handle** |
| `MW_GUILDWANTEDADD_ACK` | recv | **send** |
| `MW_GUILDWANTEDADD_REQ` | send | **handle** |
| `MW_GUILDWANTEDDEL_ACK` | recv | **send** |
| `MW_GUILDWANTEDDEL_REQ` | send | **handle** |
| `MW_GUILDWANTEDLIST_ACK` | recv | **send** |
| `MW_GUILDWANTEDLIST_REQ` | send | **handle** |
| `MW_UPDATEGUILDCOOLDOWN_ACK` | — | handle/send (+World half) |
| `MW_UPDATEGUILDCOOLDOWN_REQ` | — | handle/send (+World half) |

### Other — 29/54 World-ready

| MW message | World | TMap action needed |
|---|---|---|
| `MW_ADDBOWPLAYERS_ACK` | — | handle/send (+World half) |
| `MW_ADDCOOLDOWN_REQ` | — | handle/send (+World half) |
| `MW_ADDTOBOWQUEUE_ACK` | send | **handle** |
| `MW_ADDTOBOWQUEUE_REQ` | recv | started |
| `MW_ADDTOBRQUEUE_ACK` | send | **handle** |
| `MW_ADDTOBRQUEUE_REQ` | recv | **send** |
| `MW_ASSISTANTANSWER_ACK` | — | handle/send (+World half) |
| `MW_ASSISTANTANSWER_REQ` | — | handle/send (+World half) |
| `MW_ASSISTANTDEL_ACK` | — | handle/send (+World half) |
| `MW_ASSISTANTDEL_REQ` | — | handle/send (+World half) |
| `MW_ASSISTANT_ACK` | — | handle/send (+World half) |
| `MW_ASSISTANT_REQ` | — | handle/send (+World half) |
| `MW_BASE` | — | handle/send (+World half) |
| `MW_CANCELBOWQUEUE_ACK` | send | **handle** |
| `MW_CANCELBOWQUEUE_REQ` | recv | **send** |
| `MW_CONLIST_ACK` | recv | **send** |
| `MW_CONLIST_REQ` | send | **handle** |
| `MW_CONRESULT_REQ` | send | **handle** |
| `MW_ENEMYPOS_ACK` | — | handle/send (+World half) |
| `MW_ENEMYPOS_REQ` | — | handle/send (+World half) |
| `MW_ENTERSOLOMAP_ACK` | recv | **send** |
| `MW_ENTERSOLOMAP_REQ` | send | **handle** |
| `MW_FAMERANKUPDATE_ACK` | recv | **send** |
| `MW_FAMERANKUPDATE_REQ` | send | **handle** |
| `MW_FIRSTGRADEGROUP_REQ` | — | handle/send (+World half) |
| `MW_GAINPVPPOINT_ACK` | recv | **send** |
| `MW_GAINPVPPOINT_REQ` | send | **handle** |
| `MW_HELPMESSAGE_REQ` | — | handle/send (+World half) |
| `MW_HEROSELECT_ACK` | recv | **send** |
| `MW_HEROSELECT_REQ` | send | **handle** |
| `MW_LEAVEBATTLEFIELD_REQ` | recv | **send** |
| `MW_LEAVESOLOMAP_ACK` | recv | **send** |
| `MW_LOCALENABLE_ACK` | — | handle/send (+World half) |
| `MW_LOCALENABLE_REQ` | send | **handle** |
| `MW_LOCALRECORD_ACK` | recv | **send** |
| `MW_MAGICMIRROR_ACK` | recv | **send** |
| `MW_MAGICMIRROR_REQ` | send | **handle** |
| `MW_MAPSVRLIST_ACK` | recv | **send** |
| `MW_MAPSVRLIST_REQ` | send | **handle** |
| `MW_MEETINGROOM_ACK` | — | handle/send (+World half) |
| `MW_MEETINGROOM_REQ` | — | handle/send (+World half) |
| `MW_MISSIONENABLE_REQ` | send | **handle** |
| `MW_NOTIFYNONQUEUEDPLAYER_ACK` | — | handle/send (+World half) |
| `MW_PREPAREFORBOW_REQ` | — | handle/send (+World half) |
| `MW_PREPAREFORBR_REQ` | — | handle/send (+World half) |
| `MW_PROTECTEDCHECK_ACK` | recv | **send** |
| `MW_RECALCCOUNTRYBALANCE_ACK` | — | handle/send (+World half) |
| `MW_RELEASESINGLEBOWPLAYER_REQ` | — | handle/send (+World half) |
| `MW_RELEASESINGLEBRPLAYER_REQ` | — | handle/send (+World half) |
| `MW_REPORTENEMYLIST_REQ` | — | handle/send (+World half) |
| `MW_TERMINATE_ACK` | — | handle/send (+World half) |
| `MW_TERMINATE_REQ` | — | handle/send (+World half) |
| `MW_USERPOSITION_REQ` | send | **handle** |
| `MW_VOTEFORBRMAP_REQ` | recv | **send** |

### Friend/Soulmate — 23/28 World-ready

| MW message | World | TMap action needed |
|---|---|---|
| `MW_FRIENDADD_REQ` | send | **handle** |
| `MW_FRIENDASK_ACK` | recv | **send** |
| `MW_FRIENDASK_REQ` | send | **handle** |
| `MW_FRIENDERASE_ACK` | recv | **send** |
| `MW_FRIENDERASE_REQ` | send | **handle** |
| `MW_FRIENDGROUPCHANGE_ACK` | recv | **send** |
| `MW_FRIENDGROUPCHANGE_REQ` | send | **handle** |
| `MW_FRIENDGROUPDELETE_ACK` | recv | **send** |
| `MW_FRIENDGROUPDELETE_REQ` | send | **handle** |
| `MW_FRIENDGROUPMAKE_ACK` | recv | **send** |
| `MW_FRIENDGROUPMAKE_REQ` | send | **handle** |
| `MW_FRIENDGROUPNAME_ACK` | recv | **send** |
| `MW_FRIENDGROUPNAME_REQ` | send | **handle** |
| `MW_FRIENDLIST_ACK` | recv | **send** |
| `MW_FRIENDLIST_REQ` | send | **handle** |
| `MW_FRIENDPROTECTEDASK_ACK` | recv | **send** |
| `MW_FRIENDREGION_REQ` | — | handle/send (+World half) |
| `MW_FRIENDREPLY_ACK` | recv | **send** |
| `MW_SOULMATEDEL_ACK` | — | handle/send (+World half) |
| `MW_SOULMATEDEL_REQ` | — | handle/send (+World half) |
| `MW_SOULMATEEND_ACK` | recv | **send** |
| `MW_SOULMATEEND_REQ` | send | **handle** |
| `MW_SOULMATEREG_ACK` | recv | **send** |
| `MW_SOULMATEREG_REQ` | send | **handle** |
| `MW_SOULMATESEARCH_ACK` | recv | **send** |
| `MW_SOULMATESEARCH_REQ` | send | **handle** |
| `MW_SOULMATE_ACK` | — | handle/send (+World half) |
| `MW_SOULMATE_REQ` | — | handle/send (+World half) |

### Party — 21/26 World-ready

| MW message | World | TMap action needed |
|---|---|---|
| `MW_CHGPARTYCHIEF_ACK` | recv | **send** |
| `MW_CHGPARTYCHIEF_REQ` | send | **handle** |
| `MW_CHGPARTYTYPE_ACK` | recv | **send** |
| `MW_CHGPARTYTYPE_REQ` | send | **handle** |
| `MW_PARTYADD_ACK` | recv | started |
| `MW_PARTYADD_REQ` | send | **handle** |
| `MW_PARTYATTR_REQ` | send | **handle** |
| `MW_PARTYDEL_ACK` | recv | **send** |
| `MW_PARTYDEL_REQ` | send | **handle** |
| `MW_PARTYJOIN_ACK` | recv | **send** |
| `MW_PARTYJOIN_REQ` | send | **handle** |
| `MW_PARTYMANSTAT_ACK` | recv | **send** |
| `MW_PARTYMANSTAT_REQ` | send | **handle** |
| `MW_PARTYMEMBERRECALLANS_ACK` | recv | **send** |
| `MW_PARTYMEMBERRECALLANS_REQ` | send | **handle** |
| `MW_PARTYMEMBERRECALL_ACK` | recv | **send** |
| `MW_PARTYMEMBERRECALL_REQ` | send | **handle** |
| `MW_PARTYMOVE_ACK` | recv | **send** |
| `MW_PARTYMOVE_REQ` | send | **handle** |
| `MW_PARTYORDERTAKEITEM_ACK` | recv | **send** |
| `MW_PARTYORDERTAKEITEM_REQ` | send | **handle** |
| `MW_TOURNAMENTPARTYADD_ACK` | — | handle/send (+World half) |
| `MW_TOURNAMENTPARTYADD_REQ` | — | handle/send (+World half) |
| `MW_TOURNAMENTPARTYDEL_ACK` | — | handle/send (+World half) |
| `MW_TOURNAMENTPARTYLIST_ACK` | — | handle/send (+World half) |
| `MW_TOURNAMENTPARTYLIST_REQ` | — | handle/send (+World half) |

### Conn/Teleport — 21/24 World-ready

| MW message | World | TMap action needed |
|---|---|---|
| `MW_ADDCHAR_ACK` | recv | started |
| `MW_ADDCONNECT_REQ` | send | **handle** |
| `MW_BEGINTELEPORT_ACK` | recv | **send** |
| `MW_CHECKCONNECT_ACK` | recv | **send** |
| `MW_CHECKMAIN_ACK` | recv | **send** |
| `MW_CHECKMAIN_REQ` | send | **handle** |
| `MW_CLOSECHAR_ACK` | recv | **send** |
| `MW_CLOSECHAR_REQ` | send | **handle** |
| `MW_CMTELEPORTBATTLEMODE_REQ` | recv | **send** |
| `MW_CONNECT_ACK` | — | handle/send (+World half) |
| `MW_ENTERSVR_ACK` | recv | **send** |
| `MW_ENTERSVR_REQ` | send | **handle** |
| `MW_FRIENDCONNECTION_REQ` | send | **handle** |
| `MW_RELAYCONNECT_REQ` | send | **handle** |
| `MW_RELEASEMAIN_ACK` | recv | **send** |
| `MW_RELEASEMAIN_REQ` | send | **handle** |
| `MW_RESETCONNECTION_ACK` | — | handle/send (+World half) |
| `MW_RESETCONNECTION_REQ` | — | handle/send (+World half) |
| `MW_ROUTELIST_REQ` | send | **handle** |
| `MW_ROUTE_ACK` | recv | **send** |
| `MW_ROUTE_REQ` | send | **handle** |
| `MW_STARTTELEPORT_REQ` | send | **handle** |
| `MW_TELEPORT_ACK` | recv | **send** |
| `MW_TELEPORT_REQ` | send | **handle** |

### Char state — 19/21 World-ready

| MW message | World | TMap action needed |
|---|---|---|
| `MW_CHANGECHARBASE_ACK` | recv | **send** |
| `MW_CHANGECHARBASE_REQ` | — | handle/send (+World half) |
| `MW_CHARDATA_ACK` | recv | **send** |
| `MW_CHARDATA_REQ` | send | **handle** |
| `MW_CHARINFO_REQ` | send | **handle** |
| `MW_CHARSTATINFOANS_ACK` | recv | **send** |
| `MW_CHARSTATINFOANS_REQ` | send | **handle** |
| `MW_CHARSTATINFO_ACK` | recv | **send** |
| `MW_CHARSTATINFO_REQ` | send | **handle** |
| `MW_DELCHAR_REQ` | send | **handle** |
| `MW_ENTERCHAR_ACK` | recv | **send** |
| `MW_ENTERCHAR_REQ` | send | **handle** |
| `MW_HELMETHIDE_ACK` | recv | **send** |
| `MW_HELMETHIDE_REQ` | send | **handle** |
| `MW_INVALIDCHAR_REQ` | send | **handle** |
| `MW_ITEMSTATE_REQ` | — | handle/send (+World half) |
| `MW_LEVELUP_ACK` | recv | **send** |
| `MW_LEVELUP_REQ` | send | **handle** |
| `MW_PETRIDING_ACK` | recv | **send** |
| `MW_PETRIDING_REQ` | send | **handle** |
| `MW_REGION_ACK` | recv | **send** |

### Corps — 18/27 World-ready

| MW message | World | TMap action needed |
|---|---|---|
| `MW_ADDCORPSENEMY_ACK` | recv | **send** |
| `MW_ADDCORPSENEMY_REQ` | — | handle/send (+World half) |
| `MW_ADDCORPSUNIT_REQ` | — | handle/send (+World half) |
| `MW_ADDSQUAD_REQ` | send | **handle** |
| `MW_CHGCORPSCOMMANDER_ACK` | recv | **send** |
| `MW_CHGCORPSCOMMANDER_REQ` | send | **handle** |
| `MW_CHGSQUADCHIEF_REQ` | — | handle/send (+World half) |
| `MW_CORPSASK_ACK` | recv | **send** |
| `MW_CORPSASK_REQ` | send | **handle** |
| `MW_CORPSCMD_ACK` | recv | **send** |
| `MW_CORPSCMD_REQ` | send | **handle** |
| `MW_CORPSENEMYLIST_ACK` | recv | **send** |
| `MW_CORPSENEMYLIST_REQ` | — | handle/send (+World half) |
| `MW_CORPSHP_ACK` | recv | **send** |
| `MW_CORPSHP_REQ` | — | handle/send (+World half) |
| `MW_CORPSJOIN_REQ` | send | **handle** |
| `MW_CORPSLEAVE_ACK` | recv | **send** |
| `MW_CORPSREPLY_ACK` | recv | **send** |
| `MW_CORPSREPLY_REQ` | send | **handle** |
| `MW_DELCORPSENEMY_ACK` | recv | **send** |
| `MW_DELCORPSENEMY_REQ` | — | handle/send (+World half) |
| `MW_DELCORPSUNIT_REQ` | — | handle/send (+World half) |
| `MW_DELSQUAD_REQ` | send | **handle** |
| `MW_MOVECORPSENEMY_ACK` | recv | **send** |
| `MW_MOVECORPSENEMY_REQ` | — | handle/send (+World half) |
| `MW_MOVECORPSUNIT_ACK` | recv | **send** |
| `MW_MOVECORPSUNIT_REQ` | — | handle/send (+World half) |

### Combat/Monster — 18/22 World-ready

| MW message | World | TMap action needed |
|---|---|---|
| `MW_CREATERECALLMON_ACK` | recv | **send** |
| `MW_CREATERECALLMON_REQ` | send | **handle** |
| `MW_CREATESPOLECNIKMON_ACK` | recv | **send** |
| `MW_CREATESPOLECNIKMON_REQ` | send | **handle** |
| `MW_GETBLOOD_ACK` | — | handle/send (+World half) |
| `MW_GETBLOOD_REQ` | — | handle/send (+World half) |
| `MW_MONSTERBUY_ACK` | — | handle/send (+World half) |
| `MW_MONSTERBUY_REQ` | — | handle/send (+World half) |
| `MW_MONSTERDIE_ACK` | recv | **send** |
| `MW_MONSTERDIE_REQ` | send | **handle** |
| `MW_MONTEMPTEVO_ACK` | recv | **send** |
| `MW_MONTEMPTEVO_REQ` | send | **handle** |
| `MW_MONTEMPT_ACK` | recv | **send** |
| `MW_MONTEMPT_REQ` | send | **handle** |
| `MW_RECALLMONDATA_ACK` | recv | **send** |
| `MW_RECALLMONDATA_REQ` | send | **handle** |
| `MW_RECALLMONDEL_ACK` | recv | **send** |
| `MW_RECALLMONDEL_REQ` | send | **handle** |
| `MW_SPOLECNIKMONDEL_ACK` | recv | **send** |
| `MW_SPOLECNIKMONDEL_REQ` | send | **handle** |
| `MW_TAKEMONMONEY_ACK` | recv | **send** |
| `MW_TAKEMONMONEY_REQ` | send | **handle** |

### Chat/TMS — 13/13 World-ready

| MW message | World | TMap action needed |
|---|---|---|
| `MW_CHARMSG_REQ` | send | **handle** |
| `MW_CHATBAN_ACK` | recv | **send** |
| `MW_CHATBAN_REQ` | send | **handle** |
| `MW_CHAT_ACK` | recv | **send** |
| `MW_CHAT_REQ` | send | **handle** |
| `MW_TMSINVITEASK_ACK` | recv | **send** |
| `MW_TMSINVITEASK_REQ` | send | **handle** |
| `MW_TMSINVITE_ACK` | recv | **send** |
| `MW_TMSINVITE_REQ` | send | **handle** |
| `MW_TMSOUT_ACK` | recv | **send** |
| `MW_TMSOUT_REQ` | send | **handle** |
| `MW_TMSRECV_REQ` | send | **handle** |
| `MW_TMSSEND_ACK` | recv | **send** |

### Castle/War — 10/28 World-ready

| MW message | World | TMap action needed |
|---|---|---|
| `MW_CASTLEAPPLICANTCOUNT_REQ` | send | **handle** |
| `MW_CASTLEAPPLY_ACK` | recv | **send** |
| `MW_CASTLEAPPLY_REQ` | send | **handle** |
| `MW_CASTLEENABLE_ACK` | — | handle/send (+World half) |
| `MW_CASTLEENABLE_REQ` | send | **handle** |
| `MW_CASTLEINFO_REQ` | — | handle/send (+World half) |
| `MW_CASTLEJOIN_ACK` | — | handle/send (+World half) |
| `MW_CASTLEJOIN_REQ` | — | handle/send (+World half) |
| `MW_CASTLEOCCUPY_ACK` | recv | **send** |
| `MW_CASTLEOCCUPY_REQ` | send | **handle** |
| `MW_CASTLEWARINFO_ACK` | — | handle/send (+World half) |
| `MW_CASTLEWARINFO_REQ` | — | handle/send (+World half) |
| `MW_ENDBOWWAR_REQ` | — | handle/send (+World half) |
| `MW_ENDBRWAR_REQ` | — | handle/send (+World half) |
| `MW_ENDWAR_ACK` | — | handle/send (+World half) |
| `MW_ENDWAR_REQ` | — | handle/send (+World half) |
| `MW_LOCALOCCUPY_ACK` | recv | **send** |
| `MW_LOCALOCCUPY_REQ` | send | **handle** |
| `MW_MISSIONOCCUPY_ACK` | recv | **send** |
| `MW_MISSIONOCCUPY_REQ` | send | **handle** |
| `MW_SKYGARDENENABLE_ACK` | — | handle/send (+World half) |
| `MW_SKYGARDENENABLE_REQ` | — | handle/send (+World half) |
| `MW_SKYGARDENOCCUPY_ACK` | — | handle/send (+World half) |
| `MW_SKYGARDENOCCUPY_REQ` | — | handle/send (+World half) |
| `MW_WARCOUNTRYBALANCE_ACK` | — | handle/send (+World half) |
| `MW_WARCOUNTRYBALANCE_REQ` | — | handle/send (+World half) |
| `MW_WARLORDSAY_ACK` | — | handle/send (+World half) |
| `MW_WARLORDSAY_REQ` | — | handle/send (+World half) |

### Event — 6/8 World-ready

| MW message | World | TMap action needed |
|---|---|---|
| `MW_EVENTMSGLOTTERY_REQ` | — | handle/send (+World half) |
| `MW_EVENTMSG_REQ` | send | **handle** |
| `MW_EVENTQUARTER_REQ` | send | **handle** |
| `MW_EVENTUPDATE_ACK` | — | handle/send (+World half) |
| `MW_EVENTUPDATE_REQ` | send | **handle** |
| `MW_RPSGAMECHANGE_REQ` | send | **handle** |
| `MW_RPSGAME_ACK` | recv | **send** |
| `MW_RPSGAME_REQ` | send | **handle** |

### BattleRoyale — 5/8 World-ready

| MW message | World | TMap action needed |
|---|---|---|
| `MW_ADDBRTEAMS_ACK` | — | handle/send (+World half) |
| `MW_BRCOMMANDEXEC_REQ` | — | handle/send (+World half) |
| `MW_BRTEAMMATEADDRESULT_ACK` | recv | **send** |
| `MW_BRTEAMMATEADD_ACK` | send | **handle** |
| `MW_BRTEAMMATEADD_REQ` | recv | **send** |
| `MW_BRTEAMMATEDEL_REQ` | recv | **send** |
| `MW_BRTIMEUPDATE_ACK` | — | handle/send (+World half) |
| `MW_UPDATEBRTEAM_ACK` | send | **handle** |

### Item — 4/14 World-ready

| MW message | World | TMap action needed |
|---|---|---|
| `MW_ADDITEMRESULT_ACK` | recv | **send** |
| `MW_ADDITEMRESULT_REQ` | send | **handle** |
| `MW_ADDITEM_ACK` | — | handle/send (+World half) |
| `MW_ADDITEM_REQ` | — | handle/send (+World half) |
| `MW_DEALITEMADD_ACK` | — | handle/send (+World half) |
| `MW_DEALITEMADD_REQ` | — | handle/send (+World half) |
| `MW_DEALITEMASK_ACK` | — | handle/send (+World half) |
| `MW_DEALITEMASK_REQ` | — | handle/send (+World half) |
| `MW_DEALITEMERROR_ACK` | recv | **send** |
| `MW_DEALITEMERROR_REQ` | send | **handle** |
| `MW_DEALITEMRLY_ACK` | — | handle/send (+World half) |
| `MW_DEALITEMRLY_REQ` | — | handle/send (+World half) |
| `MW_DEALITEM_ACK` | — | handle/send (+World half) |
| `MW_DEALITEM_REQ` | — | handle/send (+World half) |

### Cash/CMGift — 4/7 World-ready

| MW message | World | TMap action needed |
|---|---|---|
| `MW_CASHITEMSALE_ACK` | — | handle/send (+World half) |
| `MW_CASHITEMSALE_REQ` | send | **handle** |
| `MW_CASHSHOPSTOP_REQ` | send | **handle** |
| `MW_CMGIFTRESULT_ACK` | recv | **send** |
| `MW_CMGIFTRESULT_REQ` | send | **handle** |
| `MW_CMGIFT_ACK` | — | handle/send (+World half) |
| `MW_CMGIFT_REQ` | — | handle/send (+World half) |

### Arena/BattleMode — 3/3 World-ready

| MW message | World | TMap action needed |
|---|---|---|
| `MW_ARENAJOIN_ACK` | recv | **send** |
| `MW_BATTLEMODESTATUS_ACK` | send | **handle** |
| `MW_BATTLEMODESTATUS_REQ` | recv | **send** |

### Mail — 2/3 World-ready

| MW message | World | TMap action needed |
|---|---|---|
| `MW_POSTRECV_ACK` | recv | **send** |
| `MW_POSTRECV_REQ` | send | **handle** |
| `MW_WORLDPOSTSEND_REQ` | — | handle/send (+World half) |

### Bow — 1/3 World-ready

| MW message | World | TMap action needed |
|---|---|---|
| `MW_BOWCOMMANDEXEC_REQ` | — | handle/send (+World half) |
| `MW_BOWPOINTSUPDATE_REQ` | recv | **send** |
| `MW_BOWTIMEUPDATE_ACK` | — | handle/send (+World half) |

### Tournament — 0/25 World-ready

| MW message | World | TMap action needed |
|---|---|---|
| `MW_TOURNAMENTAPPLYINFO_ACK` | — | handle/send (+World half) |
| `MW_TOURNAMENTAPPLYINFO_REQ` | — | handle/send (+World half) |
| `MW_TOURNAMENTAPPLY_ACK` | — | handle/send (+World half) |
| `MW_TOURNAMENTAPPLY_REQ` | — | handle/send (+World half) |
| `MW_TOURNAMENTBATPOINT_REQ` | — | handle/send (+World half) |
| `MW_TOURNAMENTENABLE_REQ` | — | handle/send (+World half) |
| `MW_TOURNAMENTENTERGATE_ACK` | — | handle/send (+World half) |
| `MW_TOURNAMENTEVENTINFO_ACK` | — | handle/send (+World half) |
| `MW_TOURNAMENTEVENTINFO_REQ` | — | handle/send (+World half) |
| `MW_TOURNAMENTEVENTJOIN_ACK` | — | handle/send (+World half) |
| `MW_TOURNAMENTEVENTJOIN_REQ` | — | handle/send (+World half) |
| `MW_TOURNAMENTEVENTLIST_ACK` | — | handle/send (+World half) |
| `MW_TOURNAMENTEVENTLIST_REQ` | — | handle/send (+World half) |
| `MW_TOURNAMENTINFO_REQ` | — | handle/send (+World half) |
| `MW_TOURNAMENTJOINLIST_ACK` | — | handle/send (+World half) |
| `MW_TOURNAMENTJOINLIST_REQ` | — | handle/send (+World half) |
| `MW_TOURNAMENTMATCHLIST_ACK` | — | handle/send (+World half) |
| `MW_TOURNAMENTMATCHLIST_REQ` | — | handle/send (+World half) |
| `MW_TOURNAMENTMATCH_REQ` | — | handle/send (+World half) |
| `MW_TOURNAMENTRESULT_ACK` | — | handle/send (+World half) |
| `MW_TOURNAMENTRESULT_REQ` | — | handle/send (+World half) |
| `MW_TOURNAMENTSCHEDULE_ACK` | — | handle/send (+World half) |
| `MW_TOURNAMENTSCHEDULE_REQ` | — | handle/send (+World half) |
| `MW_TOURNAMENT_ACK` | — | handle/send (+World half) |
| `MW_TOURNAMENT_REQ` | — | handle/send (+World half) |

### Rank — 0/6 World-ready

| MW message | World | TMap action needed |
|---|---|---|
| `MW_MONTHRANKLIST_REQ` | — | handle/send (+World half) |
| `MW_MONTHRANKRESETCHAR_ACK` | — | handle/send (+World half) |
| `MW_MONTHRANKRESETCHAR_REQ` | — | handle/send (+World half) |
| `MW_MONTHRANKRESET_REQ` | — | handle/send (+World half) |
| `MW_MONTHRANKUPDATE_ACK` | — | handle/send (+World half) |
| `MW_MONTHRANKUPDATE_REQ` | — | handle/send (+World half) |

### APEX — 0/3 World-ready

| MW message | World | TMap action needed |
|---|---|---|
| `MW_APEXDATA_ACK` | — | handle/send (+World half) |
| `MW_APEXDATA_REQ` | — | handle/send (+World half) |
| `MW_APEXSTART_ACK` | — | handle/send (+World half) |

