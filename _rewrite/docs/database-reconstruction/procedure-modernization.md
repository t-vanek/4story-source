# Procedure modernization index

All 323 original procedures. CONFIRMED names/contracts/dependencies; replacement responsibilities are PROPOSED. No PG business port is claimed. Full machine-readable matrix: [backup-procedure-modernization.json](evidence/backup-procedure-modernization.json).

| Original | Caller evidence (first non-test) | Explicit BEGIN TRAN | Lexical writes | Proposed responsibility | PG status |
|---|---|---|---|---|---|
| `TGLOBAL_RAGEZONE.GetUserID` | No indexed direct caller; may be indirect/admin | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGLOBAL_RAGEZONE.OPTool_CharReserveName` | No indexed direct caller; may be indirect/admin | False | `TRESERVEDNAME` | reviewed admin/background worker | Not ported |
| `TGLOBAL_RAGEZONE.OPTool_ChartCashCategory` | No indexed direct caller; may be indirect/admin | False | `TCASHCATEGORYCHART` | reviewed admin/background worker | Not ported |
| `TGLOBAL_RAGEZONE.OPTool_ChartCashItem` | No indexed direct caller; may be indirect/admin | False | `TCASHSHOPITEMCHART` | reviewed admin/background worker | Not ported |
| `TGLOBAL_RAGEZONE.OPTool_ChartCashItemDetail` | No indexed direct caller; may be indirect/admin | False | `TCASHBONUSITEMCHART` | reviewed admin/background worker | Not ported |
| `TGLOBAL_RAGEZONE.OPTool_ChartKeepName` | No indexed direct caller; may be indirect/admin | False | `TKEEPINGNAME` | reviewed admin/background worker | Not ported |
| `TGLOBAL_RAGEZONE.OPTool_GameCharBlock` | No indexed direct caller; may be indirect/admin | False | `TUSERPROTECTED` | reviewed admin/background worker | Not ported |
| `TGLOBAL_RAGEZONE.OPTool_ManagerLog` | No indexed direct caller; may be indirect/admin | False | `TMANAGERLOG` | reviewed admin/background worker | Not ported |
| `TGLOBAL_RAGEZONE.OPTool_ServerOnOff` | No indexed direct caller; may be indirect/admin | False | `TCHANNEL`, `TGROUP` | reviewed admin/background worker | Not ported |
| `TGLOBAL_RAGEZONE.OPTool_SMSEmergency` | `Server/TControlSvr/DBAccess.h:317` | False | No lexical target found | reviewed admin/background worker | Not ported |
| `TGLOBAL_RAGEZONE.Statistic_Code` | No indexed direct caller; may be indirect/admin | False | No lexical target found | reviewed admin/background worker | Not ported |
| `TGLOBAL_RAGEZONE.TADDCASHDEPOT` | No indexed direct caller; may be indirect/admin | False | `TUSERINFOTABLE` | application domain + persistence transaction | Not ported |
| `TGLOBAL_RAGEZONE.TAgreement` | `Server/TLoginSvr/DBAccess.h:661` | False | `TCASHITEMCABINETTABLE`, `TUSERINFOTABLE` | authentication/domain + persistence | Not ported |
| `TGLOBAL_RAGEZONE.TCashCabinetBuy` | `Server/TMapSvr/DBAccess.h:6199` | True | `TCASHTESTTABLE`, `TUSERINFOTABLE` | application domain + persistence transaction | Not ported |
| `TGLOBAL_RAGEZONE.TCashGet` | `Server/TMapSvr/DBAccess.h:4900` | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGLOBAL_RAGEZONE.TCashItemBuy` | `Server/TMapSvr/DBAccess.h:4857` | False | `TCASHITEMCABINETTABLE`, `TCASHTESTTABLE` | application domain + persistence transaction | Not ported |
| `TGLOBAL_RAGEZONE.TCashItemGet` | `Server/TMapSvr/DBAccess.h:4920` | True | `TCASHITEMCABINETTABLE` | application domain + persistence transaction | Not ported |
| `TGLOBAL_RAGEZONE.TCashItemGet_PW` | No indexed direct caller; may be indirect/admin | True | `TCASHITEMCABINETTABLE` | application domain + persistence transaction | Not ported |
| `TGLOBAL_RAGEZONE.TCashItemInsert` | No indexed direct caller; may be indirect/admin | False | `TCASHITEMCABINETTABLE` | application domain + persistence transaction | Not ported |
| `TGLOBAL_RAGEZONE.TCashItemPutin` | `Server/TMapSvr/DBAccess.h:6222` | False | `TGLOBAL.DBO.TCASHITEMCABINETTABLE`, `TITEMTABLE` | application domain + persistence transaction | Not ported |
| `TGLOBAL_RAGEZONE.TCashItemSale` | `Server/TWorldSvr/DBAccess.h:2292` | False | `TCASHSHOPITEMCHART` | application domain + persistence transaction | Not ported |
| `TGLOBAL_RAGEZONE.TChangeRace` | No indexed direct caller; may be indirect/admin | False | `TALLCHARTABLE`, `TGAME.DBO.TCHARTABLE` | application domain + persistence transaction | Not ported |
| `TGLOBAL_RAGEZONE.TCheckAttend` | No indexed direct caller; may be indirect/admin | False | `TUSERATTENDTABLE` | application domain + persistence transaction | Not ported |
| `TGLOBAL_RAGEZONE.TCheckDuplicateName` | `Server/TMapSvr/DBAccess.h:6336` | True | `TRESERVEDNAME` | application domain + persistence transaction | Not ported |
| `TGLOBAL_RAGEZONE.TCheckIP` | `Server/TLoginSvr/DBAccess.h:109` | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGLOBAL_RAGEZONE.TCheckPasswd` | `Server/TLoginSvr/DBAccess.h:146` | False | No lexical target found | authentication/domain + persistence | Not ported |
| `TGLOBAL_RAGEZONE.TCheckVeteran` | `Server/TLoginSvr/DBAccess.h:188` | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGLOBAL_RAGEZONE.TClearAll` | No indexed direct caller; may be indirect/admin | False | No lexical target found | reviewed admin/background worker | Not ported |
| `TGLOBAL_RAGEZONE.TClearAllAccount` | No indexed direct caller; may be indirect/admin | False | No lexical target found | authentication/domain + persistence | Not ported |
| `TGLOBAL_RAGEZONE.TClearAllChar` | No indexed direct caller; may be indirect/admin | False | No lexical target found | reviewed admin/background worker | Not ported |
| `TGLOBAL_RAGEZONE.TClearCurrentUser` | `Server/TMapSvr/DBAccess.h:5760` | False | `TCURRENTUSER` | application domain + persistence transaction | Not ported |
| `TGLOBAL_RAGEZONE.TClearLoginCurrentUser` | `Server/TLoginSvr/DBAccess.h:699` | False | `TCURRENTUSER` | authentication/domain + persistence | Not ported |
| `TGLOBAL_RAGEZONE.TClearMapCurrentUser` | `Server/TWorldSvr/DBAccess.h:2159` | False | `TCURRENTUSER` | application domain + persistence transaction | Not ported |
| `TGLOBAL_RAGEZONE.TCreateChar` | `Server/TLoginSvr/DBAccess.h:423` | True | `TALLCHARTABLE` | application domain + persistence transaction | Not ported |
| `TGLOBAL_RAGEZONE.TDeleteChar` | `Server/TLoginSvr/DBAccess.h:457` | False | `TALLCHARTABLE` | application domain + persistence transaction | Not ported |
| `TGLOBAL_RAGEZONE.TEnterServer` | `Server/TMapSvr/DBAccess.h:5091` | True | `TCURRENTUSER`, `TGAME_GSP.dbo.TTITLETABLE` | application domain + persistence transaction | Not ported |
| `TGLOBAL_RAGEZONE.Test` | No indexed direct caller; may be indirect/admin | False | `TCASHGAMBLECHART` | reviewed admin/background worker | Not ported |
| `TGLOBAL_RAGEZONE.TEventUpdate` | `Server/TControlSvr/DBAccess.h:383` | False | `TEVENTCHART` | application domain + persistence transaction | Not ported |
| `TGLOBAL_RAGEZONE.TGainCashBonus` | `Server/TMapSvr/DBAccess.h:6761` | True | `TCASHTESTTABLE` | application domain + persistence transaction | Not ported |
| `TGLOBAL_RAGEZONE.TGetMedals` | `Server/TMapSvr/DBAccess.h:7295` | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGLOBAL_RAGEZONE.TGetNation` | `Server/TLoginSvr/DBAccess.h:707` | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGLOBAL_RAGEZONE.TGetUserID` | No indexed direct caller; may be indirect/admin | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGLOBAL_RAGEZONE.TLoadService` | `Server/TControlSvr/DBAccess.h:334` | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGLOBAL_RAGEZONE.TLogin` | `Server/TLoginSvr/DBAccess.h:24` | True | `TACCOUNT_PW`, `TCURRENTUSER`, `TLOG`, `USERIPLOG` | authentication/domain + persistence | Not ported |
| `TGLOBAL_RAGEZONE.TLogout` | `Server/TLoginSvr/DBAccess.h:126` | True | `TALLCHARTABLE`, `TCURRENTUSER`, `TLOG` | application domain + persistence transaction | Not ported |
| `TGLOBAL_RAGEZONE.TMinBetaVer` | `Server/TPatchSvr/DBAccess.h:30` | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGLOBAL_RAGEZONE.TNewAccountEvent` | No indexed direct caller; may be indirect/admin | False | `TCASHITEMCABINETTABLE` | authentication/domain + persistence | Not ported |
| `TGLOBAL_RAGEZONE.TOPLogin` | `Server/TControlSvr/DBAccess.h:351` | False | No lexical target found | authentication/domain + persistence | Not ported |
| `TGLOBAL_RAGEZONE.TPcBangLogout` | No indexed direct caller; may be indirect/admin | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGLOBAL_RAGEZONE.TRoute` | `Server/TLoginSvr/DBAccess.h:169` | True | `TMACHINE` | application domain + persistence transaction | Not ported |
| `TGLOBAL_RAGEZONE.TSaveCharBase` | `Server/TMapSvr/DBAccess.h:6319` | False | `TALLCHARTABLE`, `TRESERVEDNAME` | application domain + persistence transaction | Not ported |
| `TGLOBAL_RAGEZONE.TSaveDuringItem` | `Server/TMapSvr/DBAccess.h:6146` | True | `TDURINGITEMTABLE` | application domain + persistence transaction | Not ported |
| `TGLOBAL_RAGEZONE.TSaveDuringItemStart` | No indexed direct caller; may be indirect/admin | True | `TTEMPDURINGITEMTABLE` | application domain + persistence transaction | Not ported |
| `TGLOBAL_RAGEZONE.TSaveTempDuringItem` | No indexed direct caller; may be indirect/admin | False | `TTEMPDURINGITEMTABLE` | application domain + persistence transaction | Not ported |
| `TGLOBAL_RAGEZONE.TSetPcBangData` | No indexed direct caller; may be indirect/admin | True | `TPCBANGPLAYTABLE` | application domain + persistence transaction | Not ported |
| `TGLOBAL_RAGEZONE.TTestLogin` | `Server/TLoginSvr/DBAccess.h:382` | True | `TACCOUNT_PW`, `TCURRENTUSER`, `TLOG` | authentication/domain + persistence | Not ported |
| `TGLOBAL_RAGEZONE.TUpdateCharMoney` | No indexed direct caller; may be indirect/admin | False | `TALLCHARTABLE` | application domain + persistence transaction | Not ported |
| `TGLOBAL_RAGEZONE.TUpdateEnterLuckyDate` | No indexed direct caller; may be indirect/admin | False | `TCURRENTUSER` | application domain + persistence transaction | Not ported |
| `TGLOBAL_RAGEZONE.TUpdateVersion` | `Server/TControlSvr/DBAccess.h:229` | False | `TVERSION` | application domain + persistence transaction | Not ported |
| `TGLOBAL_RAGEZONE.TUserProtectedAdd` | `Server/TControlSvr/DBAccess.h:297` | False | `TUSERPROTECTED` | authentication/domain + persistence | Not ported |
| `TGAME_RAGEZONE.BatchSnapshot_Item` | No indexed direct caller; may be indirect/admin | False | `TGLOBAL_GSP.DBO.SDATA160TL` | reviewed admin/background worker | Not ported |
| `TGAME_RAGEZONE.BatchSnapshot_Money` | No indexed direct caller; may be indirect/admin | False | `TGLOBAL_GSP.DBO.SDATA130TL` | reviewed admin/background worker | Not ported |
| `TGAME_RAGEZONE.OPTool_ChartBattleTime` | No indexed direct caller; may be indirect/admin | False | `TBATTLETIMECHART` | reviewed admin/background worker | Not ported |
| `TGAME_RAGEZONE.OPTool_GameChar` | No indexed direct caller; may be indirect/admin | False | `TCHARTABLE`, `TGLOBAL_GSP.DBO.TALLCHARTABLE` | reviewed admin/background worker | Not ported |
| `TGAME_RAGEZONE.OPTool_GameItem` | No indexed direct caller; may be indirect/admin | False | `TITEMTABLE` | reviewed admin/background worker | Not ported |
| `TGAME_RAGEZONE.OPTool_GamePost` | No indexed direct caller; may be indirect/admin | False | `TITEMTABLE`, `TPOSTTABLE` | reviewed admin/background worker | Not ported |
| `TGAME_RAGEZONE.OPTool_GameQuest` | No indexed direct caller; may be indirect/admin | False | `TQUESTTABLE`, `TQUESTTERMTABLE` | reviewed admin/background worker | Not ported |
| `TGAME_RAGEZONE.OPTool_GameSkill` | No indexed direct caller; may be indirect/admin | False | `TSKILLTABLE` | reviewed admin/background worker | Not ported |
| `TGAME_RAGEZONE.RandomColor` | No indexed direct caller; may be indirect/admin | False | `TITEMTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TAddBRPlayer` | `Server/TWorldSvr/DBAccess.h:3043` | False | `TBRPLAYERTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TAddQuset` | No indexed direct caller; may be indirect/admin | False | `TQUESTTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TAllCharReward` | No indexed direct caller; may be indirect/admin | False | `TCHARTABLE`, `THOTKEYTABLE`, `TSKILLTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TAllSkillReward` | No indexed direct caller; may be indirect/admin | False | `TCHARTABLE`, `TSKILLREWARDMONEY` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TAuctionBid` | `Server/TMapSvr/DBAccess.h:6665` | False | `TAUCTIONBIDDER`, `TAUCTIONTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TAuctionBuyDirect` | `Server/TMapSvr/DBAccess.h:6722` | False | `TAUCTIONBIDDER`, `TAUCTIONTABLE`, `TITEMTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TAuctionEnd` | `Server/TMapSvr/DBAccess.h:6693` | False | `TAUCTIONBIDDER`, `TAUCTIONINTEREST`, `TAUCTIONTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TAuctionReg` | `Server/TMapSvr/DBAccess.h:6613` | False | `TAUCTIONTABLE`, `TITEMTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TAuctionRegCancel` | `Server/TMapSvr/DBAccess.h:6640` | True | `TAUCTIONBIDDER`, `TAUCTIONINTEREST`, `TAUCTIONTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TBatch_GiveEventItem` | No indexed direct caller; may be indirect/admin | False | `TEVENTITEMTABLE`, `TEVENTITEMTABLEf` | reviewed admin/background worker | Not ported |
| `TGAME_RAGEZONE.TCashCabinetBuy` | `Server/TMapSvr/DBAccess.h:6199` | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TCashGet` | `Server/TMapSvr/DBAccess.h:4900` | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TCashItemBuy` | `Server/TMapSvr/DBAccess.h:4857` | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TCashItemGet` | `Server/TMapSvr/DBAccess.h:4920` | False | `TITEMTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TCashItemGet_PW` | No indexed direct caller; may be indirect/admin | False | `TITEMTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TCashItemPutin` | `Server/TMapSvr/DBAccess.h:6222` | False | `TGLOBAL_GSP.DBO.TCASHITEMCABINETTABLE`, `TITEMTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TCashItemSale` | `Server/TWorldSvr/DBAccess.h:2292` | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TChangedMountItem` | No indexed direct caller; may be indirect/admin | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TChangedPetSystemToMountSystem` | No indexed direct caller; may be indirect/admin | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TChangedPetToMount` | No indexed direct caller; may be indirect/admin | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TCharLevelSet` | No indexed direct caller; may be indirect/admin | False | `TCHARTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TCharSetAsk` | No indexed direct caller; may be indirect/admin | False | `TCHARTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TCheckAttend` | No indexed direct caller; may be indirect/admin | False | `TPOSTTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TCheckConnect` | `Server/TMapSvr/DBAccess.h:5113` | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TCheckDuplicateName` | `Server/TMapSvr/DBAccess.h:6336` | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TChristmasEventPost` | No indexed direct caller; may be indirect/admin | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TClearAll` | No indexed direct caller; may be indirect/admin | False | No lexical target found | reviewed admin/background worker | Not ported |
| `TGAME_RAGEZONE.TClearAllAccount` | No indexed direct caller; may be indirect/admin | False | No lexical target found | authentication/domain + persistence | Not ported |
| `TGAME_RAGEZONE.TClearAllChar` | No indexed direct caller; may be indirect/admin | False | No lexical target found | reviewed admin/background worker | Not ported |
| `TGAME_RAGEZONE.TClearAllQuest` | No indexed direct caller; may be indirect/admin | False | No lexical target found | reviewed admin/background worker | Not ported |
| `TGAME_RAGEZONE.TClearBRPlayers` | `Server/TMapSvr/DBAccess.h:7348` | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TCLEARCHARFOR1YEAR` | No indexed direct caller; may be indirect/admin | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TCLEARCHARFOR4WEEKS` | No indexed direct caller; may be indirect/admin | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TCLEARCHARFOR6MONTHS` | No indexed direct caller; may be indirect/admin | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TClearCharTitle` | `Server/TMapSvr/DBAccess.h:5144` | True | `TTITLETABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TClearCurrentUser` | `Server/TMapSvr/DBAccess.h:5760` | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TClearMapCurrentUser` | `Server/TWorldSvr/DBAccess.h:2159` | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TCMGiftAdd` | `Server/TWorldSvr/DBAccess.h:2975` | False | `TCMGIFTCHART` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TCMGiftCanTake` | `Server/TWorldSvr/DBAccess.h:2921` | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TCMGiftDel` | `Server/TWorldSvr/DBAccess.h:2997` | False | `TCMGIFTCHART` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TCMGiftLog` | `Server/TMapSvr/DBAccess.h:7086` | False | `TCMGIFTTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TCMGiftSet` | `Server/TWorldSvr/DBAccess.h:2944` | False | `TCMGIFTCHART` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TCreateChar` | `Server/TLoginSvr/DBAccess.h:423` | True | `TCABINETTABLE`, `TCHARTABLE`, `TGLOBAL_GSP.DBO.TALLCHARTABLE`, `THOTKEYTABLE`, `TINVENTABLE`, `TPETTABLE`, `TSKILLTABLE`, `TTITLETABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TCreateChar_Dummy` | No indexed direct caller; may be indirect/admin | True | `TCABINETTABLE`, `TCHARTABLE`, `TGLOBAL_GSP.DBO.TALLCHARTABLE`, `THOTKEYTABLE`, `TINVENTABLE`, `TSKILLTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TCreateRecallMon` | `Server/TMapSvr/DBAccess.h:5029` | True | `TRECALLMONTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TDeleteAllQuest` | No indexed direct caller; may be indirect/admin | False | `TQUESTTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TDeleteBRPlayer` | `Server/TMapSvr/DBAccess.h:7356` | False | `TBRPLAYERTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TDeleteChar` | `Server/TLoginSvr/DBAccess.h:457` | False | `TCABINETTABLE`, `TCASTLEAPPLICANTTABLE`, `TCHARTABLE`, `TDUELCHARTABLE`, `TDUELSCORETABLE`, `TEXPITEMTABLE`, `TFRIENDGROUPTABLE`, `TFRIENDTABLE`, `TGLOBAL_GSP.DBO.TALLCHARTABLE`, `THOTKEYTABLE`, `TINVENTABLE`, `TITEMTABLE`, `TITEMUSEDTABLE`, `TPOSTTABLE`, `TPROTECTEDTABLE`, `TPVPOINTTABLE`, `TPVPRECENTTABLE`, `TPVPRECORDTABLE`, `TQUESTTABLE`, `TQUESTTERMTABLE`, `TRECALLMAINTAINTABLE`, `TRECALLMONTABLE`, `TSKILLMAINTAINTABLE`, `TSKILLTABLE`, `TSOULMATETABLE`, `TTEMPCABINETTABLE`, `TTEMPEXPITEMTABLE`, `TTEMPINVENTABLE`, `TTEMPITEMTABLE`, `TTEMPITEMUSEDTABLE`, `TTEMPSKILLMAINTAINTABLE`, `TTEMPSKILLTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TDeleteCompanion` | `Server/TMapSvr/DBAccess.h:7237` | False | `TCOMPANIONITEMTABLE`, `TCOMPANIONTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TDeleteDealItem` | `Server/TMapSvr/DBAccess.h:6273` | False | `TITEMTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TDeleteInven` | No indexed direct caller; may be indirect/admin | False | `TINVENTABLE`, `TITEMTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TDELETEQUESTCHART` | No indexed direct caller; may be indirect/admin | False | `TQCONDITIONCHART`, `TQREWARDCHART`, `TQUESTCHART`, `TQUESTTERMCHART` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TDelMountSaddle` | `Server/TMapSvr/DBAccess.h:616` | False | `TMOUNTITEMTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TDuelCharAdd` | `Server/TMapSvr/DBAccess.h:6570` | False | `TDUELCHARTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TDuelScore` | `Server/TMapSvr/DBAccess.h:6546` | False | `TDUELSCORETABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TEditQuest` | No indexed direct caller; may be indirect/admin | False | `TQUESTTABLE`, `TQUESTTERMTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TEnterServer` | `Server/TMapSvr/DBAccess.h:5091` | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TEraseChar` | No indexed direct caller; may be indirect/admin | False | `TCABINETTABLE`, `TCASTLEAPPLICANTTABLE`, `TCHARTABLE`, `TDUELCHARTABLE`, `TDUELSCORETABLE`, `TEXPITEMTABLE`, `TFRIENDGROUPTABLE`, `TFRIENDTABLE`, `TGLOBAL_GSP.DBO.TALLCHARTABLE`, `TGLOBAL_GSP.DBO.TALLRANKTABLE`, `TGLOBAL_GSP.DBO.TRESERVEDNAME`, `TGUILDMEMBERTABLE`, `TGUILDTACTICSTABLE`, `TGUILDVOLUNTEERTABLE`, `THOTKEYTABLE`, `TINVENTABLE`, `TITEMTABLE`, `TITEMUSEDTABLE`, `TPOSTTABLE`, `TPROTECTEDTABLE`, `TPVPOINTTABLE`, `TPVPRECENTTABLE`, `TPVPRECORDTABLE`, `TQUESTTABLE`, `TQUESTTERMTABLE`, `TRECALLMAINTAINTABLE`, `TRECALLMONTABLE`, `TSKILLMAINTAINTABLE`, `TSKILLTABLE`, `TSOULMATETABLE`, `TTEMPCABINETTABLE`, `TTEMPEXPITEMTABLE`, `TTEMPINVENTABLE`, `TTEMPITEMTABLE`, `TTEMPITEMUSEDTABLE`, `TTEMPSKILLMAINTAINTABLE`, `TTEMPSKILLTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TEventItem3Give` | No indexed direct caller; may be indirect/admin | False | `TITEMTABLE`, `TPOSTTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TEventItemGive` | No indexed direct caller; may be indirect/admin | False | `TRESERVEDPOST` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TEventItemGiveByPost` | No indexed direct caller; may be indirect/admin | False | `TITEMTABLE`, `TPOSTTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TEventItemLevelGive` | No indexed direct caller; may be indirect/admin | False | `TPOSTITEMTABLE`, `TPOSTTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TEventItemPost` | No indexed direct caller; may be indirect/admin | False | `TITEMTABLE`, `TPOSTTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TEventItemPutinCashCabinet` | No indexed direct caller; may be indirect/admin | False | `TGLOBAL_GSP.DBO.TCASHITEMCABINETTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TEventMagicItemGive` | No indexed direct caller; may be indirect/admin | False | `TPOSTITEMTABLE`, `TPOSTTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TEventQuarter` | `Server/TMapSvr/DBAccess.h:6095` | False | `TEVENTQUARTERGIVETABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TEventQuarterUpdate` | `Server/TWorldSvr/DBAccess.h:2616` | False | `TEVENTQUARTERCHART` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TEventQuestItemGive` | No indexed direct caller; may be indirect/admin | False | `TRESERVEDPOST` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TFindBRPlayer` | `Server/TLoginSvr/DBAccess.h:688` | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TFindServerID` | `Server/TLoginSvr/DBAccess.h:479` | False | `TCHARTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TFourYearEventPost` | No indexed direct caller; may be indirect/admin | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TFriendDelete` | `Server/TWorldSvr/DBAccess.h:1786` | False | `TFRIENDTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TFriendGroupChange` | `Server/TWorldSvr/DBAccess.h:1831` | False | `TFRIENDTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TFriendGroupDelete` | `Server/TWorldSvr/DBAccess.h:1816` | False | `TFRIENDGROUPTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TFriendGroupMake` | `Server/TWorldSvr/DBAccess.h:1801` | False | `TFRIENDGROUPTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TFriendGroupName` | `Server/TWorldSvr/DBAccess.h:1847` | False | `TFRIENDGROUPTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TFriendInsert` | `Server/TWorldSvr/DBAccess.h:1772` | True | `TFRIENDTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TGainCashBonus` | `Server/TMapSvr/DBAccess.h:6761` | False | `TITEMTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TGenerateDBItemID` | No indexed direct caller; may be indirect/admin | True | `TDBITEMINDEXTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TGetBattleRankPoint` | `Server/TMapSvr/DBAccess.h:7369` | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TGetCashCabinetUseTime` | `Server/TMapSvr/DBAccess.h:6244` | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TGetCustomCloak` | `Server/TMapSvr/DBAccess.h:7461` | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TGetGuildInfo` | `Server/TLoginSvr/DBAccess.h:722` | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TGetItemName` | `Server/TWorldSvr/DBAccess.h:2658` | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TGetLastCompanion` | `Server/TMapSvr/DBAccess.h:7267` | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TGetLimitedLevel` | `Server/TMapSvr/DBAccess.h:6388` | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TGetMedals` | `Server/TMapSvr/DBAccess.h:7295` | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TGetMentorBaseExp` | No indexed direct caller; may be indirect/admin | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TGetMonthPvPoint` | `Server/TMapSvr/DBAccess.h:6782` | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TGetMountSaddle` | `Server/TMapSvr/DBAccess.h:583` | False | `TMOUNTITEMTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TGetNation` | `Server/TLoginSvr/DBAccess.h:707` | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TGetPcBangData` | `Server/TMapSvr/DBAccess.h:6075` | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TGetPlayTime` | `Server/TMapSvr/DBAccess.h:4785` | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TGetPosition` | `Server/TMapSvr/DBAccess.h:7424` | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TGetPostInfo` | `Server/TMapSvr/DBAccess.h:6949` | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TGetPostInfo_old` | No indexed direct caller; may be indirect/admin | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TGetPvPRecord` | `Server/TMapSvr/DBAccess.h:6462` | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TGetRecallID` | `Server/TWorldSvr/DBAccess.h:2280` | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TGetReservedPost` | `Server/TWorldSvr/DBAccess.h:2106` | False | `TRESERVEDPOST` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TGetServerID` | `Server/TMapSvr/DBAccess.h:5065` | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TGetServerInfo` | No indexed direct caller; may be indirect/admin | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TGetStorageInfo` | No indexed direct caller; may be indirect/admin | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TGiveBRReward` | `Server/TMapSvr/DBAccess.h:7388` | False | `TRANKING`, `TRESERVEDPOST` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TGivePvPoint` | No indexed direct caller; may be indirect/admin | False | `TPVPOINTTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TGiveTitle` | `Server/TMapSvr/DBAccess.h:7337` | False | `TTITLETABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TGuildArticleAdd` | `Server/TWorldSvr/DBAccess.h:1901` | True | `TGUILDARTICLETABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TGuildArticleDel` | `Server/TWorldSvr/DBAccess.h:1920` | True | `TGUILDARTICLETABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TGuildArticleUpdate` | `Server/TWorldSvr/DBAccess.h:1936` | True | `TGUILDARTICLETABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TGuildContribution` | `Server/TWorldSvr/DBAccess.h:2000` | True | `TGUILDTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TGuildDelete` | `Server/TWorldSvr/DBAccess.h:1675` | True | `TGUILDARTICLETABLE`, `TGUILDCABINETTABLE`, `TGUILDMEMBERTABLE`, `TGUILDTABLE`, `TGUILDTACTICSTABLE`, `TGUILDTACTICSWANTEDTABLE`, `TGUILDWANTEDTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TGuildDisorg` | `Server/TWorldSvr/DBAccess.h:1660` | True | `TGUILDTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TGuildDuty` | `Server/TWorldSvr/DBAccess.h:1726` | True | `TGUILDMEMBERTABLE`, `TGUILDTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TGuildEstablish` | `Server/TWorldSvr/DBAccess.h:1642` | True | `TGUILDTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TGuildFame` | `Server/TWorldSvr/DBAccess.h:1953` | True | `TGUILDTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TGuildItemPutIn` | `Server/TMapSvr/DBAccess.h:5934` | True | `TITEMTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TGuildItemRollback` | `Server/TMapSvr/DBAccess.h:6027` | True | `TITEMTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TGuildItemTakeOut` | `Server/TMapSvr/DBAccess.h:5981` | True | `TITEMTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TGuildKickout` | `Server/TWorldSvr/DBAccess.h:1758` | True | `TCHARTABLE`, `TGUILDMEMBERTABLE`, `TGUILDPVPRECORDTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TGuildLeave` | `Server/TWorldSvr/DBAccess.h:1709` | True | `TCHARTABLE`, `TGUILDMEMBERTABLE`, `TGUILDPVPRECORDTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TGuildLevel` | `Server/TWorldSvr/DBAccess.h:1968` | True | `TGUILDTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TGuildMaxCabinet` | `Server/TWorldSvr/DBAccess.h:1982` | True | `TGUILDTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TGuildMemberAdd` | `Server/TWorldSvr/DBAccess.h:1691` | True | `TGUILDMEMBERTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TGuildPeer` | `Server/TWorldSvr/DBAccess.h:1742` | True | `TGUILDMEMBERTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TGuildTacticsAdd` | `Server/TWorldSvr/DBAccess.h:2421` | False | `TGUILDTACTICSTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TGuildTacticsDel` | `Server/TWorldSvr/DBAccess.h:2440` | False | `TGUILDTACTICSTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TGuildTacticsWantedAdd` | `Server/TWorldSvr/DBAccess.h:2380` | False | `TGUILDTACTICSWANTEDTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TGuildTacticsWantedDel` | `Server/TWorldSvr/DBAccess.h:2403` | False | `TGUILDTACTICSWANTEDTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TGuildVolunteering` | `Server/TWorldSvr/DBAccess.h:2341` | False | `TGUILDVOLUNTEERTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TGuildVolunteeringDel` | `Server/TWorldSvr/DBAccess.h:2356` | False | `TGUILDVOLUNTEERTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TGuildWantedAdd` | `Server/TWorldSvr/DBAccess.h:2310` | False | `TGUILDWANTEDTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TGuildWantedDel` | `Server/TWorldSvr/DBAccess.h:2327` | False | `TGUILDWANTEDTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.THalloweenEventPost` | No indexed direct caller; may be indirect/admin | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.THalloweenEventPost2` | No indexed direct caller; may be indirect/admin | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.THarvestFestivalEventPost` | No indexed direct caller; may be indirect/admin | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.THelpMessage` | `Server/TWorldSvr/DBAccess.h:2872` | False | `THELPMESSAGETABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.THeroSelect` | `Server/TMapSvr/DBAccess.h:6371` | False | `TCASTLETABLE`, `TLOCALTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TInitGenItemID` | `Server/TMapSvr/DBAccess.h:6261` | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TInitMonthPvPoint` | `Server/TWorldSvr/DBAccess.h:2553` | False | `TMONTHPVPOINTTABLE`, `TMONTHRANKTABLE`, `tgame.TLASTMONTHPOINTTABLE`, `tgame.TLASTTOTALPOINTTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TInitMonthRank` | `Server/TWorldSvr/DBAccess.h:2583` | False | `THEROTABLE`, `TMONTHRANKTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TInsertSkill` | No indexed direct caller; may be indirect/admin | False | `TSKILLTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TItemDelete` | No indexed direct caller; may be indirect/admin | False | `TITEMTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TItemInsert` | No indexed direct caller; may be indirect/admin | False | `TGLOBAL_GSP.DBO.TCASHITEMCABINETTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TItemInsert_halloween` | No indexed direct caller; may be indirect/admin | False | `TGLOBAL_GSP.DBO.TCASHITEMCABINETTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TItemInsert_runes` | No indexed direct caller; may be indirect/admin | False | `TGLOBAL_GSP.DBO.TCASHITEMCABINETTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TItemInsert_weapons` | No indexed direct caller; may be indirect/admin | False | `TGLOBAL_GSP.DBO.TCASHITEMCABINETTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TItemStateChange` | `Server/TWorldSvr/DBAccess.h:2187` | False | `TITEMCHART` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TJobsPostReturn` | No indexed direct caller; may be indirect/admin | False | `TPOSTITEMTABLE`, `TPOSTTABLE` | reviewed admin/background worker | Not ported |
| `TGAME_RAGEZONE.TLoadService` | `Server/TControlSvr/DBAccess.h:334` | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TLogItem` | No indexed direct caller; may be indirect/admin | False | `TITEMLOG` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TLogout` | `Server/TLoginSvr/DBAccess.h:126` | True | `TCHARTABLE`, `THOTKEYTABLE`, `TITEMTABLE`, `TSKILLTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TLogSkill` | `Server/TMapSvr/DBAccess.h:5305` | False | `TSKILLLOG` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TMagicItemGive` | No indexed direct caller; may be indirect/admin | False | `TRESERVEDPOST` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TMonthPvPointClear` | `Server/TMapSvr/DBAccess.h:7074` | False | `TGUILDTABLE`, `TITLETABLE`, `TMONTHPVPOINTTABLE`, `TTITLETABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TMonthRankReward` | No indexed direct caller; may be indirect/admin | False | `TRESERVEDPOST` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TMoveChar` | No indexed direct caller; may be indirect/admin | False | `TCHARTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TNewYearEventPost` | No indexed direct caller; may be indirect/admin | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TOperatorReset` | No indexed direct caller; may be indirect/admin | False | `TOPERATORTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TPcBangItemGive` | No indexed direct caller; may be indirect/admin | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TPetDelete` | `Server/TMapSvr/DBAccess.h:5806` | False | `TPETTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TPostBillsUpdate` | `Server/TMapSvr/DBAccess.h:7013` | False | `TITEMTABLE`, `TPOSTTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TPostCanSend` | `Server/TMapSvr/DBAccess.h:5594` | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TPostDelete` | `Server/TMapSvr/DBAccess.h:5612` | False | `TPOSTTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TPostGetItem` | `Server/TMapSvr/DBAccess.h:5627` | False | `TPOSTTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TPostRead` | `Server/TMapSvr/DBAccess.h:5732` | False | `TPOSTTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TPostView` | `Server/TMapSvr/DBAccess.h:6980` | False | `TPOSTTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TProtectedDelete` | `Server/TMapSvr/DBAccess.h:5792` | False | `TPROTECTEDTABLE` | authentication/domain + persistence | Not ported |
| `TGAME_RAGEZONE.TProtectedInsert` | `Server/TMapSvr/DBAccess.h:5776` | False | `TPROTECTEDTABLE` | authentication/domain + persistence | Not ported |
| `TGAME_RAGEZONE.TProtectedSearch` | `Server/TWorldSvr/DBAccess.h:1863` | False | No lexical target found | authentication/domain + persistence | Not ported |
| `TGAME_RAGEZONE.TPutItemInInven` | No indexed direct caller; may be indirect/admin | False | `TITEMTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TQuestSendPost` | `Server/TMapSvr/DBAccess.h:6415` | False | `TRESERVEDPOST` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TRecallMonDel` | `Server/TMapSvr/DBAccess.h:5747` | True | `TRECALLMONTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TRoute` | `Server/TLoginSvr/DBAccess.h:169` | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TRPSGameRecord` | `Server/TWorldSvr/DBAccess.h:2891` | False | `TRPSGAMERECORDTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSaveActEnd` | `Server/TMapSvr/DBAccess.h:6182` | False | `TCHARTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSaveAuctionInterest` | `Server/TMapSvr/DBAccess.h:6744` | False | `TAUCTIONINTEREST` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSaveCabinet` | `Server/TMapSvr/DBAccess.h:5379` | True | `TTEMPCABINETTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSaveCabinetItem` | No indexed direct caller; may be indirect/admin | True | `TTEMPCABINETITEMTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSaveCastleApplicant` | `Server/TWorldSvr/DBAccess.h:2266` | False | `TCASTLEAPPLICANTTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSaveCastleOccupy` | `Server/TMapSvr/DBAccess.h:5518` | False | `TCASTLEAPPLICANTTABLE`, `TCASTLETABLE`, `TLOCALOCCUPYTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSaveChar` | `Server/TMapSvr/DBAccess.h:5235` | True | `TCHARTABLE`, `TSKILLMAINTAINTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSaveCharBase` | `Server/TMapSvr/DBAccess.h:6319` | False | `TACTIVECHARTABLE`, `TAIDTABLE`, `TCASTLETABLE`, `TCHARTABLE`, `TLOCALTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSaveCharDataEnd` | `Server/TMapSvr/DBAccess.h:5851` | True | `TCABINETTABLE`, `TEXPITEMTABLE`, `TINVENTABLE`, `TITEMUSEDTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSaveCharDataStart` | `Server/TMapSvr/DBAccess.h:5839` | True | `TTEMPCABINETTABLE`, `TTEMPEXPITEMTABLE`, `TTEMPINVENTABLE`, `TTEMPITEMTABLE`, `TTEMPITEMUSEDTABLE`, `TTEMPSKILLMAINTAINTABLE`, `TTEMPSKILLTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSaveCharKill` | `Server/TMapSvr/DBAccess.h:6531` | False | `charkilling_log` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSaveCharPosition` | `Server/TMapSvr/DBAccess.h:7038` | False | `TCHARTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSaveCharTitle` | `Server/TMapSvr/DBAccess.h:5158` | True | `TTITLETABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSaveCompanion` | `Server/TMapSvr/DBAccess.h:6834` | False | `TCOMPANIONITEMTABLE`, `TCOMPANIONTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSaveDuringItem` | `Server/TMapSvr/DBAccess.h:6146` | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSaveEXP` | `Server/TMapSvr/DBAccess.h:6111` | True | `TCHARTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSaveExpItem` | `Server/TMapSvr/DBAccess.h:6166` | True | `TTEMPEXPITEMTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSaveGuildPointReward` | `Server/TWorldSvr/DBAccess.h:2223` | True | `TGUILDPVPOINTREWARDTABLE`, `TGUILDTABLE`, `TPVPOINTTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSaveGuildPvPoint` | `Server/TWorldSvr/DBAccess.h:2204` | False | `TGUILDTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSaveGuildPvPRecord` | `Server/TWorldSvr/DBAccess.h:2244` | False | `TGUILDPVPRECORDTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSaveHotkey` | `Server/TMapSvr/DBAccess.h:5495` | False | `THOTKEYTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSaveInven` | `Server/TMapSvr/DBAccess.h:5282` | True | `TTEMPINVENTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSaveItem` | `Server/TMapSvr/DBAccess.h:5328` | True | `TTEMPITEMTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSaveItemDataEnd` | `Server/TMapSvr/DBAccess.h:5875` | True | `TINVENTABLE`, `TITEMTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSaveItemDataStart` | `Server/TMapSvr/DBAccess.h:5863` | True | `TTEMPINVENTABLE`, `TTEMPITEMTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSaveItemDirect` | `Server/TMapSvr/DBAccess.h:5684` | False | `TITEMTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSaveItemUsed` | `Server/TMapSvr/DBAccess.h:5576` | True | `TTEMPITEMUSEDTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSaveLastCompanion` | `Server/TMapSvr/DBAccess.h:7252` | False | `TLASTCOMPANIONTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSaveLocalOccupy` | `Server/TMapSvr/DBAccess.h:5556` | False | `TLOCALOCCUPYTABLE`, `TLOCALTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSaveMedals` | `Server/TMapSvr/DBAccess.h:7281` | False | `TMEDALS` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSaveMentor` | No indexed direct caller; may be indirect/admin | False | `TMENTORTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSaveMissionOccupy` | `Server/TMapSvr/DBAccess.h:7060` | False | `TMISSIONTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSaveMoney` | `Server/TMapSvr/DBAccess.h:6127` | True | `TCHARTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSaveMonthPvPoint` | `Server/TMapSvr/DBAccess.h:6804` | False | `TMONTHPVPOINTTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSaveMonthRank` | `Server/TWorldSvr/DBAccess.h:2504` | False | `TMONTHPVPOINTTABLE`, `TMONTHRANKTABLE`, `TPVPOINTTABLE`, `TRESERVEDPOST` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSavePet` | `Server/TMapSvr/DBAccess.h:5823` | True | `TPETTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSavePost` | `Server/TMapSvr/DBAccess.h:5654` | False | `TPOSTERRORTABLE`, `TPOSTTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSaveProtectedOption` | `Server/TMapSvr/DBAccess.h:6590` | False | `TPROTECTEDTABLE` | authentication/domain + persistence | Not ported |
| `TGAME_RAGEZONE.TSavePvPRecent` | `Server/TMapSvr/DBAccess.h:6512` | False | `TPVPRECENTTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSavePvPRecord` | `Server/TMapSvr/DBAccess.h:6487` | True | `TPVPOINTTABLE`, `TPVPRECORDTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSaveQuest` | `Server/TMapSvr/DBAccess.h:5450` | False | `TQUESTTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSaveQuestComplete` | No indexed direct caller; may be indirect/admin | False | `TQUESTCOMPLETE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSaveQuestTerm` | `Server/TMapSvr/DBAccess.h:5474` | False | `TQUESTTERMTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSaveRecallMaintain` | `Server/TMapSvr/DBAccess.h:6294` | True | `TSKILLMAINTAINTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSaveRecallMon` | `Server/TMapSvr/DBAccess.h:4984` | False | `TRECALLMAINTAINTABLE`, `TRECALLMONTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSaveSecureCode` | `Server/TMapSvr/DBAccess.h:7175` | False | `TGLOBAL_GSP.dbo.TSECURECODE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSaveSkill` | `Server/TMapSvr/DBAccess.h:5397` | True | `TTEMPSKILLTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSaveSkillMaintain` | `Server/TMapSvr/DBAccess.h:5422` | True | `TSKILLMAINTAINTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSaveSkyGardenOccupy` | `Server/TMapSvr/DBAccess.h:5536` | False | `TSKYGARDENTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSaveSMS` | `Server/TMapSvr/DBAccess.h:5911` | False | `TGLOBAL_GSP.DBO.TSMSTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSaveTacticsGainPoint` | `Server/TWorldSvr/DBAccess.h:2454` | False | `TGUILDTACTICSTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSaveTax` | No indexed direct caller; may be indirect/admin | False | `TTAXTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSaveWarlordSay` | `Server/TMapSvr/DBAccess.h:6868` | False | `THEROTABLE`, `TMONTHRANKTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TScammingPost` | No indexed direct caller; may be indirect/admin | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSendAllReservedPost` | No indexed direct caller; may be indirect/admin | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TServerRewardPost` | No indexed direct caller; may be indirect/admin | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSetCharLevel` | No indexed direct caller; may be indirect/admin | False | `TCHARTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSetMountSaddle` | `Server/TMapSvr/DBAccess.h:600` | False | `TMOUNTITEMTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSeventhdayEventPost` | No indexed direct caller; may be indirect/admin | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSkillInit` | No indexed direct caller; may be indirect/admin | False | `TCHARTABLE`, `THOTKEYTABLE`, `TSKILLTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSoulmateDel` | `Server/TWorldSvr/DBAccess.h:2084` | True | `TSOULMATETABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSoulmateEnd` | `Server/TWorldSvr/DBAccess.h:2070` | True | `TSOULMATETABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TSoulmateReg` | `Server/TWorldSvr/DBAccess.h:2056` | True | `TSOULMATETABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TStopTheClock` | `Server/TMapSvr/DBAccess.h:6352` | False | No lexical target found | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TTESTGetMaxMagicValue` | No indexed direct caller; may be indirect/admin | False | No lexical target found | reviewed admin/background worker | Not ported |
| `TGAME_RAGEZONE.TTESTGivePowerItem` | No indexed direct caller; may be indirect/admin | False | `TITEMTABLE` | reviewed admin/background worker | Not ported |
| `TGAME_RAGEZONE.TTESTGivePowerItem_copy` | No indexed direct caller; may be indirect/admin | False | `TGLOBAL_GSP.DBO.TCASHITEMCABINETTABLE`, `TITEMTABLE` | reviewed admin/background worker | Not ported |
| `TGAME_RAGEZONE.TTESTGmItemGive` | No indexed direct caller; may be indirect/admin | False | `TCHARTABLE`, `TGLOBAL_GSP.DBO.TCASHITEMCABINETTABLE`, `THOTKEYTABLE`, `TINVENTABLE`, `TITEMTABLE`, `TPOSTTABLE`, `TSKILLTABLE`, `tskilltable` | reviewed admin/background worker | Not ported |
| `TGAME_RAGEZONE.TTnmtEventDel` | `Server/TWorldSvr/DBAccess.h:2801` | False | `TTNMTEVENTREWARDTABLE`, `TTNMTEVENTSCHEDULETABLE`, `TTNMTEVENTTABLE`, `TTNMTEVENTTIMETABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TTnmtEventEntry` | `Server/TWorldSvr/DBAccess.h:2823` | False | `TTNMTEVENTREWARDTABLE`, `TTNMTEVENTTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TTnmtEventReward` | `Server/TWorldSvr/DBAccess.h:2851` | False | `TTNMTEVENTREWARDTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TTnmtEventSchedule` | `Server/TWorldSvr/DBAccess.h:2787` | False | `TTNMTEVENTSCHEDULETABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TTnmtEventTime` | `Server/TWorldSvr/DBAccess.h:2770` | False | `TTNMTEVENTSCHEDULETABLE`, `TTNMTEVENTTIMETABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TTournamentApply` | `Server/TWorldSvr/DBAccess.h:2722` | False | `TTOURNAMENTPLAYERTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TTournamentClear` | `Server/TWorldSvr/DBAccess.h:2739` | False | `TTOURNAMENTPLAYERTABLE`, `TTOURNAMENTSTATUSTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TTournamentPayback` | `Server/TWorldSvr/DBAccess.h:2683` | False | `TTOURNAMENTPLAYERTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TTournamentResult` | `Server/TWorldSvr/DBAccess.h:2702` | False | `TTOURNAMENTPLAYERTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TTournamentReward` | `Server/TMapSvr/DBAccess.h:6908` | False | `THEROTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TTournamentStatus` | `Server/TWorldSvr/DBAccess.h:2753` | False | `TTOURNAMENTSTATUSTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TUpdateActiveChar` | No indexed direct caller; may be indirect/admin | False | `TACTIVECHARTABLE` | application domain + persistence transaction | Not ported |
| `TGAME_RAGEZONE.TUpdateQuest` | No indexed direct caller; may be indirect/admin | False | `TQCLASSCHART`, `TQCONDITIONCHART`, `TQREWARDCHART`, `TQTITLECHART`, `TQUESTCHART`, `TQUESTTERMCHART` | application domain + persistence transaction | Not ported |
