-- CONFIRMED table metadata from this backup; no module behavior or rows.
-- Apply ONLY to a new empty disposable database, never an existing database.

CREATE SCHEMA [tgame];
GO

CREATE TABLE [dbo].[ACCESSORYMAGICTABLE] (
    [ID] int NOT NULL,
    [MagicID] int NOT NULL,
    [MinValue] int NOT NULL,
    [MaxValue] int NOT NULL,
    PRIMARY KEY ([ID])
);
GO

CREATE TABLE [dbo].[OTESTER] (
    [dwCharID] int NULL
);
GO

CREATE TABLE [dbo].[STATISTICACCOUNT] (
    [nAccount] int NULL
);
GO

CREATE TABLE [dbo].[STATISTICTEMP] (
    [nValue1] int NULL
);
GO

CREATE TABLE [dbo].[TACTIVECHARTABLE] (
    [dwCharID] int NOT NULL,
    [dateEnter] smalldatetime NOT NULL,
    PRIMARY KEY ([dwCharID])
);
GO

CREATE TABLE [dbo].[TAICHART] (
    [bAIType] tinyint NOT NULL,
    [dwCmdID] int NOT NULL,
    [bTriggerType] tinyint NOT NULL,
    [dwTriggerID] int NOT NULL,
    [dwDelay] int NOT NULL,
    [bLoop] tinyint NOT NULL,
    PRIMARY KEY ([bAIType], [dwCmdID], [bTriggerType], [dwTriggerID])
);
GO

CREATE TABLE [dbo].[TAICMDCHART] (
    [dwCmdID] int NOT NULL,
    [bCmdType] tinyint NOT NULL,
    PRIMARY KEY ([dwCmdID])
);
GO

CREATE TABLE [dbo].[TAICONCHART] (
    [dwCmdID] int NOT NULL,
    [bConditionType] tinyint NOT NULL,
    [dwConditionID] int NOT NULL,
    PRIMARY KEY ([dwCmdID], [bConditionType])
);
GO

CREATE TABLE [dbo].[TAIDTABLE] (
    [dwCharID] int NOT NULL,
    [bCountry] tinyint NOT NULL,
    [dDate] smalldatetime NOT NULL,
    PRIMARY KEY ([dwCharID])
);
GO

CREATE TABLE [dbo].[TARENACHART] (
    [wID] smallint NOT NULL,
    [bType] tinyint NOT NULL,
    [dwFee] int NOT NULL,
    [wInPos] smallint NOT NULL,
    [wOutPos] smallint NOT NULL,
    PRIMARY KEY ([wID])
);
GO

CREATE TABLE [dbo].[TAUCTIONBIDDER] (
    [dwAuctionID] int NOT NULL,
    [dwCharID] int NOT NULL,
    [dlBidPrice] bigint NOT NULL,
    [DateBid] smalldatetime NOT NULL,
    PRIMARY KEY ([dwAuctionID], [dwCharID])
);
GO

CREATE TABLE [dbo].[TAUCTIONINTEREST] (
    [dwCharID] int NOT NULL,
    [dwAuctionID] int NOT NULL,
    PRIMARY KEY ([dwCharID], [dwAuctionID])
);
GO

CREATE TABLE [dbo].[TAUCTIONTABLE] (
    [dwAuctionID] int IDENTITY(1,1) NOT NULL,
    [wNpcID] smallint NOT NULL,
    [dwCharID] int NOT NULL,
    [DateStart] smalldatetime NOT NULL,
    [DateEnd] smalldatetime NOT NULL,
    [dlDirectPrice] bigint NOT NULL,
    [dlStartPrice] bigint NOT NULL,
    [dlItemID] bigint NOT NULL,
    [bBidCount] tinyint NOT NULL,
    PRIMARY KEY ([dwAuctionID])
);
GO

CREATE TABLE [dbo].[TBATTLERANKCHART] (
    [bRank] tinyint NOT NULL,
    [dwPoint] int NOT NULL
);
GO

CREATE TABLE [dbo].[TBATTLETIMECHART] (
    [bType] tinyint NOT NULL,
    [dwBattleDur] int NOT NULL,
    [dwBattleStart] int NOT NULL,
    [dwAlarmStart] int NOT NULL,
    [dwAlarmEnd] int NOT NULL,
    [dwPeaceDur] int NOT NULL,
    [bDay] tinyint NOT NULL DEFAULT ((0)),
    [bWeek] tinyint NOT NULL DEFAULT ((0)),
    PRIMARY KEY ([bType])
);
GO

CREATE TABLE [dbo].[TBATTLEZONECHART] (
    [wID] smallint NOT NULL,
    [szName] varchar(50) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [wMapID] smallint NOT NULL,
    [wCastle] smallint NOT NULL,
    [wBossSpawnID] smallint NOT NULL DEFAULT ((0)),
    [wLGateKeeperSpawnID] smallint NOT NULL DEFAULT ((0)),
    [wRGateKeeperSpawnID] smallint NOT NULL DEFAULT ((0)),
    [dwLSwitchID] int NOT NULL DEFAULT ((0)),
    [dwRSwitchID] int NOT NULL DEFAULT ((0)),
    [wNormalItem] smallint NOT NULL DEFAULT ((0)),
    [wChiefItem] smallint NOT NULL DEFAULT ((0)),
    [bLine] tinyint NOT NULL DEFAULT ((0)),
    [wCGateKeeperSpawnID] smallint NOT NULL DEFAULT ((0)),
    [dwCSwitchID] int NOT NULL DEFAULT ((0)),
    [wSkill1] smallint NOT NULL DEFAULT ((0)),
    [wSkill2] smallint NOT NULL DEFAULT ((0)),
    [bItemLevel] tinyint NOT NULL DEFAULT ((0)),
    [wValorianBossDefend] smallint NOT NULL,
    [wValorianBossAttack] smallint NOT NULL,
    [wDerionBossDefend] smallint NOT NULL,
    [wDerionBossAttack] smallint NOT NULL,
    [wMiddleSpawnID] smallint NOT NULL,
    [wRightSpawnID] smallint NOT NULL,
    [wLeftSpawnID] smallint NOT NULL,
    PRIMARY KEY ([wID])
);
GO

CREATE TABLE [dbo].[TBOWBONUSITEMCHART] (
    [bItemID] tinyint NOT NULL,
    [wItemID] smallint NOT NULL,
    [wPrice] smallint NOT NULL
);
GO

CREATE TABLE [dbo].[TBOWITEMCHART] (
    [bClass] tinyint NOT NULL,
    [bInvenID] tinyint NOT NULL,
    [wMinItemID] smallint NOT NULL,
    [wMaxItemID] smallint NOT NULL,
    [wDependOnItemID] smallint NOT NULL,
    [bCount] tinyint NOT NULL
);
GO

CREATE TABLE [dbo].[TBOWITEMCHART1] (
    [bClass] tinyint NOT NULL,
    [bInvenID] tinyint NOT NULL,
    [wMinItemID] smallint NOT NULL,
    [wMaxItemID] smallint NOT NULL,
    [wDependOnItemID] smallint NOT NULL,
    [bCount] tinyint NOT NULL
);
GO

CREATE TABLE [dbo].[TBOWSETTINGSCHART] (
    [wMapID] smallint NOT NULL,
    [bMinPlayersCount] tinyint NOT NULL,
    [bMaxNationDifference] tinyint NOT NULL,
    [dwAlarmDur] int NOT NULL,
    [dwBuyTimeDur] int NOT NULL,
    [dwBattleDur] int NOT NULL
);
GO

CREATE TABLE [dbo].[TBRPLAYERTABLE] (
    [dwCharID] int NOT NULL,
    [dwUserID] int NOT NULL
);
GO

CREATE TABLE [dbo].[TBRSETTINGSCHART] (
    [bMinPlayerCount] smallint NOT NULL,
    [dwAlarmDur] int NOT NULL,
    [dwBuyTimeDur] int NOT NULL,
    [dwBattleDur] int NOT NULL
);
GO

CREATE TABLE [dbo].[TBRSPAWNPOSCHART] (
    [wMapID] smallint NOT NULL,
    [fPosX] float(53) NOT NULL,
    [fPosY] float(53) NOT NULL,
    [fPosZ] float(53) NOT NULL
);
GO

CREATE TABLE [dbo].[TBRSUPPLIESCHART] (
    [dwTick] int NOT NULL,
    [wItemID] smallint NOT NULL,
    [bCount] tinyint NOT NULL
);
GO

CREATE TABLE [dbo].[TCABINETITEMTABLE] (
    [dwCharID] int NOT NULL,
    [bCabinetID] tinyint NOT NULL,
    [dwStItemID] int NOT NULL,
    [wItemID] smallint NOT NULL,
    [bLevel] tinyint NOT NULL,
    [bCount] tinyint NOT NULL,
    [bGLevel] tinyint NOT NULL DEFAULT ((0)),
    [bMagic1] tinyint NOT NULL,
    [bMagic2] tinyint NOT NULL,
    [bMagic3] tinyint NOT NULL,
    [bMagic4] tinyint NOT NULL,
    [bMagic5] tinyint NOT NULL,
    [bMagic6] tinyint NOT NULL,
    [wValue1] smallint NOT NULL,
    [wValue2] smallint NOT NULL,
    [wValue3] smallint NOT NULL,
    [wValue4] smallint NOT NULL,
    [wValue5] smallint NOT NULL,
    [wValue6] smallint NOT NULL,
    [dwTime1] int NOT NULL,
    [dwTime2] int NOT NULL,
    [dwTime3] int NOT NULL,
    [dwTime4] int NOT NULL,
    [dwTime5] int NOT NULL,
    [dwTime6] int NOT NULL,
    PRIMARY KEY ([dwCharID], [bCabinetID], [dwStItemID])
);
GO

CREATE TABLE [dbo].[TCABINETTABLE] (
    [dwCharID] int NOT NULL,
    [bCabinetID] tinyint NOT NULL,
    [bUse] tinyint NOT NULL,
    PRIMARY KEY ([dwCharID], [bCabinetID])
);
GO

CREATE TABLE [dbo].[TCASTLEAPPLICANTTABLE] (
    [wCastleID] smallint NOT NULL,
    [dwCharID] int NOT NULL,
    [bCamp] tinyint NOT NULL,
    PRIMARY KEY ([dwCharID])
);
GO

CREATE TABLE [dbo].[TCASTLETABLE] (
    [wCastle] smallint NOT NULL,
    [bCountry] tinyint NOT NULL,
    [dwGuildID] int NOT NULL,
    [dateWarTime] smalldatetime NOT NULL,
    [szHero] varchar(50) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [dateHero] smalldatetime NOT NULL DEFAULT ((0))
);
GO

CREATE TABLE [dbo].[TCHANGEDITEM] (
    [wOld] smallint NOT NULL,
    [wNew] smallint NOT NULL
);
GO

CREATE TABLE [dbo].[TCHANNELCHART] (
    [bGroupID] tinyint NOT NULL,
    [wMapID] smallint NOT NULL,
    [wUnitID] smallint NOT NULL,
    [bLogChannel] tinyint NOT NULL,
    [bPhyChannel] tinyint NOT NULL
);
GO

CREATE TABLE [dbo].[TCHARTABLE] (
    [dwCharID] int IDENTITY(1,1) NOT NULL,
    [dwUserID] int NOT NULL,
    [bSlot] tinyint NOT NULL,
    [szNAME] varchar(50) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [bStartAct] tinyint NOT NULL DEFAULT ((1)),
    [bClass] tinyint NOT NULL,
    [bRace] tinyint NOT NULL,
    [bCountry] tinyint NOT NULL,
    [bRealSex] tinyint NOT NULL,
    [bSex] tinyint NOT NULL,
    [bHair] tinyint NOT NULL,
    [bFace] tinyint NOT NULL,
    [bBody] tinyint NOT NULL,
    [bPants] tinyint NOT NULL,
    [bHand] tinyint NOT NULL,
    [bFoot] tinyint NOT NULL,
    [bHelmetHide] tinyint NOT NULL DEFAULT ((0)),
    [bLevel] tinyint NOT NULL,
    [dwEXP] int NOT NULL,
    [dwHP] int NOT NULL,
    [dwMP] int NOT NULL,
    [wSkillPoint] smallint NOT NULL DEFAULT ((0)),
    [dwRegion] int NOT NULL DEFAULT ((0)),
    [dwGold] int NOT NULL,
    [dwSilver] int NOT NULL,
    [dwCooper] int NOT NULL,
    [bGuildLeave] tinyint NOT NULL DEFAULT ((0)),
    [dwGuildLeaveTime] int NOT NULL DEFAULT ((0)),
    [wMapID] smallint NOT NULL,
    [wSpawnID] smallint NOT NULL DEFAULT ((0)),
    [wLastSpawnID] smallint NOT NULL DEFAULT ((0)),
    [wTemptedMon] smallint NOT NULL,
    [bAftermath] tinyint NOT NULL,
    [fPosX] real NOT NULL,
    [fPosY] real NOT NULL,
    [fPosZ] real NOT NULL,
    [wDIR] smallint NOT NULL,
    [dwRankPoint] int NOT NULL DEFAULT ((0)),
    [bDelete] tinyint NOT NULL DEFAULT ((0)),
    [dCreateDate] smalldatetime NOT NULL DEFAULT (getdate()),
    [dDeleteDate] smalldatetime NULL,
    [dwLastDestination] int NOT NULL DEFAULT ((0)),
    [bOriCountry] tinyint NOT NULL DEFAULT ((0)),
    [dLogoutDate] smalldatetime NOT NULL DEFAULT ((0)),
    [bStatLevel] tinyint NULL,
    [bStatPoint] tinyint NULL,
    [dwStatExp] int NULL,
    PRIMARY KEY ([dwCharID])
);
GO

CREATE TABLE [dbo].[TCLASSCHART] (
    [bClassID] tinyint NOT NULL,
    [wSTR] smallint NOT NULL,
    [wDEX] smallint NOT NULL,
    [wCON] smallint NOT NULL,
    [wINT] smallint NOT NULL,
    [wWIS] smallint NOT NULL,
    [wMEN] smallint NOT NULL,
    PRIMARY KEY ([bClassID])
);
GO

CREATE TABLE [dbo].[TCMGIFTCHART] (
    [wGiftID] smallint IDENTITY(1,1) NOT NULL,
    [bGiftType] tinyint NOT NULL,
    [dwValue] int NOT NULL,
    [bCount] tinyint NOT NULL,
    [bTakeType] tinyint NOT NULL,
    [bMaxTakeCount] tinyint NOT NULL,
    [bLevel] tinyint NOT NULL,
    [bToolOnly] tinyint NOT NULL,
    [wErrGiftID] smallint NOT NULL,
    [szTitle] varchar(256) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [szMsg] varchar(1024) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    PRIMARY KEY ([wGiftID])
);
GO

CREATE TABLE [dbo].[TCMGIFTTABLE] (
    [dwUserID] int NOT NULL,
    [dwCharID] int NOT NULL,
    [wGiftID] smallint NOT NULL,
    [dwGMCharID] int NOT NULL,
    [wErrID] smallint NOT NULL,
    [tTakeDate] smalldatetime NOT NULL
);
GO

CREATE TABLE [dbo].[TCOMPANIONBONUSCHART] (
    [bBonusID] tinyint NOT NULL,
    [fBase] real NOT NULL,
    [fLevelMultiplier] real NOT NULL
);
GO

CREATE TABLE [dbo].[TCOMPANIONBONUSCHART_EMPTY] (
    [bBonusID] tinyint NOT NULL,
    [fBase] float(53) NOT NULL,
    [fLevelMultiplier] float(53) NOT NULL
);
GO

CREATE TABLE [dbo].[TCOMPANIONITEMTABLE] (
    [bSlot] tinyint NOT NULL,
    [wFirstItemID] smallint NOT NULL,
    [wSecondItemID] smallint NOT NULL,
    [dFirstEndTime] smalldatetime NOT NULL,
    [dSecondEndTime] smalldatetime NOT NULL,
    [dwTick] int NOT NULL,
    [dwCharID] int NOT NULL
);
GO

CREATE TABLE [dbo].[TCOMPANIONTABLE] (
    [bSlot] tinyint NOT NULL,
    [dwMonID] int NOT NULL,
    [bLevel] tinyint NOT NULL,
    [strName] varchar(55) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [dwExp] int NOT NULL,
    [wLife] smallint NOT NULL,
    [bStatusPoints] tinyint NOT NULL,
    [bEffect] tinyint NOT NULL,
    [wSTR] smallint NOT NULL,
    [wDEX] smallint NOT NULL,
    [wCON] smallint NOT NULL,
    [wINT] smallint NOT NULL,
    [wWIS] smallint NOT NULL,
    [wMEN] smallint NOT NULL,
    [wBonusID] smallint NOT NULL,
    [dwCharID] int NOT NULL
);
GO

CREATE TABLE [dbo].[TCUSTOMCLOAKTABLE] (
    [wID] smallint NULL,
    [dDateCreate] smalldatetime NULL,
    [dwUserID] int NULL
);
GO

CREATE TABLE [dbo].[TCUSTOMTIMECHART] (
    [bType] smallint NOT NULL,
    [dwTime] int NOT NULL
);
GO

CREATE TABLE [dbo].[TDBITEMINDEXTABLE] (
    [bWorld] tinyint NOT NULL DEFAULT ((0)),
    [dlID] bigint NOT NULL DEFAULT ((1))
);
GO

CREATE TABLE [dbo].[TDESTINATIONCHART] (
    [wPortalID] smallint NOT NULL,
    [wDestID] smallint NOT NULL,
    [dwPrice] int NOT NULL,
    [bEnable] tinyint NOT NULL,
    [bCondition1] tinyint NOT NULL DEFAULT ((0)),
    [dwConditionID1] int NOT NULL DEFAULT ((0)),
    [bCondition2] tinyint NOT NULL DEFAULT ((0)),
    [dwConditionID2] int NOT NULL DEFAULT ((0)),
    [bCondition3] tinyint NOT NULL DEFAULT ((0)),
    [dwConditionID3] int NOT NULL DEFAULT ((0)),
    PRIMARY KEY ([wPortalID], [wDestID])
);
GO

CREATE TABLE [dbo].[TDUELCHARTABLE] (
    [dwCharID] int NOT NULL,
    [szName] varchar(50) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [bClass] tinyint NOT NULL,
    [bLevel] tinyint NOT NULL,
    [bWin] tinyint NOT NULL,
    [dwPoint] int NOT NULL,
    [dTime] datetime NOT NULL
);
GO

CREATE TABLE [dbo].[TDUELSCORETABLE] (
    [dwCharID] int NOT NULL,
    [dwWarriorWin] int NOT NULL,
    [dwWarriorLose] int NOT NULL,
    [dwRangerWin] int NOT NULL,
    [dwRangerLose] int NOT NULL,
    [dwArcherWin] int NOT NULL,
    [dwArcherLose] int NOT NULL,
    [dwWizardWin] int NOT NULL,
    [dwWizardLose] int NOT NULL,
    [dwPriestWin] int NOT NULL,
    [dwPriestLose] int NOT NULL,
    [dwSorcererWin] int NOT NULL,
    [dwSorcererLose] int NOT NULL,
    PRIMARY KEY ([dwCharID])
);
GO

CREATE TABLE [dbo].[TEQUIPCREATECHARCHART] (
    [bCountry] tinyint NOT NULL,
    [bClass] tinyint NOT NULL,
    [bSex] tinyint NOT NULL,
    [wPrmWeapon] smallint NOT NULL,
    [wSndWeapon] smallint NOT NULL,
    [wLongWeapon] smallint NOT NULL,
    [wHead] smallint NOT NULL,
    [wBody] smallint NOT NULL,
    [wPants] smallint NOT NULL,
    [wHand] smallint NOT NULL,
    [wFoot] smallint NOT NULL,
    [wBack] smallint NOT NULL,
    PRIMARY KEY ([bCountry], [bClass], [bSex])
);
GO

CREATE TABLE [dbo].[TERASECHARLOG] (
    [dwCharID] int NOT NULL,
    [bType] tinyint NOT NULL,
    [dEraseDate] smalldatetime NOT NULL DEFAULT (getdate())
);
GO

CREATE TABLE [dbo].[TERASEITEM] (
    [wID] smallint NULL
);
GO

CREATE TABLE [dbo].[TEST] (
    [DATA] varchar(100) COLLATE Latin1_General_CI_AS_KS NULL
);
GO

CREATE TABLE [dbo].[TEVENTITEMTABLE] (
    [dwCharID] int NOT NULL,
    [bGiveItemCount] tinyint NOT NULL,
    PRIMARY KEY ([dwCharID])
);
GO

CREATE TABLE [dbo].[TEVENTQUARTERCHART] (
    [bDay] tinyint NOT NULL,
    [bHour] tinyint NOT NULL,
    [bMinute] tinyint NOT NULL,
    [wID] smallint NOT NULL,
    [wItemID1] smallint NOT NULL,
    [wItemID2] smallint NOT NULL,
    [wItemID3] smallint NOT NULL,
    [wItemID4] smallint NOT NULL,
    [wItemID5] smallint NOT NULL,
    [bCount] tinyint NOT NULL,
    [szTitle] varchar(50) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [szMessage] varchar(500) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [szPresent] varchar(50) COLLATE Latin1_General_CI_AS_KS NOT NULL DEFAULT (''),
    [szAnnounce] varchar(1024) COLLATE Latin1_General_CI_AS_KS NOT NULL DEFAULT (''),
    PRIMARY KEY ([wID])
);
GO

CREATE TABLE [dbo].[TEVENTQUARTERGIVETABLE] (
    [bDay] tinyint NOT NULL,
    [bHour] tinyint NOT NULL,
    [bMinute] tinyint NOT NULL,
    [szName] varchar(50) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [dGiveDate] smalldatetime NOT NULL DEFAULT (getdate())
);
GO

CREATE TABLE [dbo].[TEXPITEMTABLE] (
    [dwCharID] int NOT NULL,
    [wItemID] smallint NOT NULL,
    [bType] tinyint NOT NULL,
    [dwRemainTime] int NOT NULL,
    [dEndTime] smalldatetime NOT NULL
);
GO

CREATE TABLE [dbo].[TFAKECHARCHART] (
    [dwCharID] int NOT NULL,
    [bCountry] tinyint NOT NULL,
    [dwUserID] int NOT NULL DEFAULT ((0)),
    PRIMARY KEY ([dwCharID])
);
GO

CREATE TABLE [dbo].[TFAKESHOPCHART] (
    [wIndex] smallint NOT NULL,
    [wItemID] smallint NOT NULL,
    [bCountry] tinyint NOT NULL,
    [bKind] tinyint NOT NULL,
    PRIMARY KEY ([wIndex], [bCountry])
);
GO

CREATE TABLE [dbo].[TFORMULACHART] (
    [bID] tinyint NOT NULL,
    [szName] varchar(50) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [dwinit] int NOT NULL,
    [fRateX] float(53) NOT NULL,
    [fRateY] float(53) NOT NULL,
    PRIMARY KEY ([bID])
);
GO

CREATE TABLE [dbo].[TFRIENDGROUPTABLE] (
    [dwCharID] int NOT NULL,
    [bGroup] tinyint NOT NULL,
    [szName] varchar(20) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    PRIMARY KEY ([dwCharID], [bGroup])
);
GO

CREATE TABLE [dbo].[TFRIENDTABLE] (
    [dwCharID] int NOT NULL,
    [dwFriendID] int NOT NULL,
    [bGroup] tinyint NOT NULL,
    PRIMARY KEY ([dwCharID], [dwFriendID])
);
GO

CREATE TABLE [dbo].[TGAMBLECHART] (
    [bType] tinyint NOT NULL,
    [bKind] tinyint NOT NULL,
    [wReplaceID] smallint NOT NULL,
    [bCountMax] tinyint NOT NULL,
    [bMinLevel] tinyint NOT NULL,
    [bMaxLevel] tinyint NOT NULL,
    [wProb] smallint NOT NULL
);
GO

CREATE TABLE [dbo].[TGATECHART] (
    [dwGateID] int NOT NULL,
    [dwSwitchID] int NOT NULL,
    [bType] tinyint NOT NULL DEFAULT ((0)),
    [wMapID] smallint NOT NULL,
    [wPosX] smallint NOT NULL,
    [wPosY] smallint NOT NULL,
    [wPosZ] smallint NOT NULL
);
GO

CREATE TABLE [dbo].[TGEMGRADECHART] (
    [bGem] tinyint NOT NULL,
    [bProb] tinyint NOT NULL,
    PRIMARY KEY ([bGem])
);
GO

CREATE TABLE [dbo].[TGMREWARDCHART] (
    [bLevel] tinyint NOT NULL,
    [dlMoney] bigint NOT NULL,
    [bChartType] tinyint NOT NULL,
    [wID] smallint NOT NULL
);
GO

CREATE TABLE [dbo].[TGMREWARDTABLE] (
    [dwUserID] int NOT NULL,
    [bLevel] tinyint NOT NULL,
    [dwCharID] int NOT NULL,
    [dDate] smalldatetime NOT NULL,
    PRIMARY KEY ([dwUserID], [bLevel])
);
GO

CREATE TABLE [dbo].[TGODBALLCHART] (
    [wID] smallint NOT NULL,
    [bCamp] tinyint NOT NULL,
    [wMapID] smallint NOT NULL,
    [fPosX] float(53) NOT NULL,
    [fPosY] float(53) NOT NULL,
    [fPosZ] float(53) NOT NULL,
    PRIMARY KEY ([wID])
);
GO

CREATE TABLE [dbo].[TGODTOWERCHART] (
    [wID] smallint NOT NULL,
    [wMapID] smallint NOT NULL,
    [fPosX] float(53) NOT NULL,
    [fPosY] float(53) NOT NULL,
    [fPosZ] float(53) NOT NULL,
    PRIMARY KEY ([wID])
);
GO

CREATE TABLE [dbo].[TGUILDARTICLETABLE] (
    [dwGuildID] int NOT NULL,
    [dwID] int NOT NULL,
    [bDuty] tinyint NOT NULL,
    [szWritter] varchar(50) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [szTitle] varchar(256) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [szArticle] varchar(2048) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [dwTime] int NOT NULL,
    PRIMARY KEY ([dwGuildID], [dwID])
);
GO

CREATE TABLE [dbo].[TGUILDCABINETTABLE] (
    [dwGuildID] int NOT NULL,
    [dwItemID] int NOT NULL,
    [wItemID] smallint NOT NULL,
    [bLevel] tinyint NOT NULL,
    [bCount] tinyint NOT NULL,
    [bGLevel] tinyint NOT NULL DEFAULT ((0)),
    [bMagic1] tinyint NOT NULL,
    [bMagic2] tinyint NOT NULL,
    [bMagic3] tinyint NOT NULL,
    [bMagic4] tinyint NOT NULL,
    [bMagic5] tinyint NOT NULL,
    [bMagic6] tinyint NOT NULL,
    [wValue1] smallint NOT NULL,
    [wValue2] smallint NOT NULL,
    [wValue3] smallint NOT NULL,
    [wValue4] smallint NOT NULL,
    [wValue5] smallint NOT NULL,
    [wValue6] smallint NOT NULL,
    [dwTime1] int NOT NULL,
    [dwTime2] int NOT NULL,
    [dwTime3] int NOT NULL,
    [dwTime5] int NOT NULL,
    [dwTime4] int NOT NULL,
    [dwTime6] int NOT NULL,
    PRIMARY KEY ([dwGuildID], [dwItemID])
);
GO

CREATE TABLE [dbo].[TGUILDCHART] (
    [bLevel] tinyint NOT NULL,
    [dwEXP] int NOT NULL,
    [bMaxCnt] tinyint NOT NULL,
    [bMinCnt] tinyint NOT NULL,
    [bCabinetCnt] tinyint NOT NULL,
    [bTacticsCnt] tinyint NOT NULL,
    [bBattleSetCnt] tinyint NOT NULL,
    [bGuardCnt] tinyint NOT NULL,
    [bRoyalGuardCnt] tinyint NOT NULL,
    [bTurretCnt] tinyint NOT NULL,
    [bPeer1] tinyint NOT NULL,
    [bPeer2] tinyint NOT NULL,
    [bPeer3] tinyint NOT NULL,
    [bPeer4] tinyint NOT NULL,
    [bPeer5] tinyint NOT NULL,
    PRIMARY KEY ([bLevel])
);
GO

CREATE TABLE [dbo].[TGUILDMEMBERSKILLTABLE] (
    [wSkillID] smallint NULL,
    [bLevel] tinyint NULL,
    [tEndTime] smalldatetime NULL,
    [dwCharID] int NULL
);
GO

CREATE TABLE [dbo].[TGUILDMEMBERTABLE] (
    [dwCharID] int NOT NULL,
    [dwGuildID] int NOT NULL,
    [bDuty] tinyint NOT NULL,
    [bPeer] tinyint NOT NULL,
    [dwService] int NOT NULL DEFAULT ((0)),
    PRIMARY KEY ([dwCharID])
);
GO

CREATE TABLE [dbo].[TGUILDPLAYLOG] (
    [dwGuildID] int NOT NULL,
    [dwUserID] int NOT NULL,
    [dwCharID] int NOT NULL,
    [dwPlayTime] int NOT NULL
);
GO

CREATE TABLE [dbo].[TGUILDPVPOINTREWARDTABLE] (
    [dwGuildID] int NOT NULL,
    [szName] varchar(50) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [dwPoint] int NOT NULL,
    [dlDate] smalldatetime NOT NULL
);
GO

CREATE TABLE [dbo].[TGUILDPVPRECORDTABLE] (
    [dwGuildID] int NOT NULL,
    [dwCharID] int NOT NULL,
    [dwDate] int NOT NULL,
    [wKillCount] smallint NOT NULL,
    [wDieCount] smallint NOT NULL,
    [dwPoint_1] int NOT NULL,
    [dwPoint_2] int NOT NULL,
    [dwPoint_3] int NOT NULL,
    [dwPoint_4] int NOT NULL,
    [dwPoint_5] int NOT NULL,
    [dwPoint_6] int NOT NULL,
    [dwPoint_7] int NOT NULL,
    [dwPoint_8] int NOT NULL
);
GO

CREATE TABLE [dbo].[TGUILDRELATION] (
    [bType] tinyint NOT NULL,
    [dwGuildOne] int NOT NULL,
    [dwGuildTwo] int NOT NULL
);
GO

CREATE TABLE [dbo].[TGUILDSKILLCHART] (
    [bType] tinyint NULL,
    [wSkillID] smallint NULL
);
GO

CREATE TABLE [dbo].[TGUILDSTATSTABLE] (
    [dwGuildID] int NOT NULL,
    [bSkillPoint] smallint NOT NULL,
    [bLevel] smallint NOT NULL,
    [dwExp] int NOT NULL
);
GO

CREATE TABLE [dbo].[TGUILDTABLE] (
    [dwID] int IDENTITY(1,1) NOT NULL,
    [szName] varchar(50) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [dwChief] int NOT NULL,
    [bLevel] tinyint NOT NULL DEFAULT ((0)),
    [dwFame] int NOT NULL DEFAULT ((0)),
    [dwFameColor] int NOT NULL DEFAULT ((0)),
    [bMaxCabinet] tinyint NOT NULL DEFAULT ((0)),
    [dwGold] int NOT NULL DEFAULT ((0)),
    [dwSilver] int NOT NULL DEFAULT ((0)),
    [dwCooper] int NOT NULL DEFAULT ((0)),
    [dwGI] int NOT NULL DEFAULT ((0)),
    [dwExp] int NOT NULL DEFAULT ((0)),
    [bGPoint] tinyint NOT NULL DEFAULT ((0)),
    [bStatus] tinyint NOT NULL,
    [bDisorg] tinyint NOT NULL DEFAULT ((0)),
    [dwTime] int NOT NULL DEFAULT ((0)),
    [timeEstablish] smalldatetime NOT NULL,
    [dwPvPTotalPoint] int NOT NULL DEFAULT ((0)),
    [dwPvPUseablePoint] int NOT NULL DEFAULT ((0)),
    [dwPvPMonthPoint] int NOT NULL DEFAULT ((0)),
    PRIMARY KEY ([dwID])
);
GO

CREATE TABLE [dbo].[TGUILDTABLE_copy] (
    [dwID] int IDENTITY(1,1) NOT NULL,
    [szName] varchar(50) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [dwChief] int NOT NULL,
    [bLevel] tinyint NOT NULL DEFAULT ((0)),
    [dwFame] int NOT NULL DEFAULT ((0)),
    [dwFameColor] int NOT NULL DEFAULT ((0)),
    [bMaxCabinet] tinyint NOT NULL DEFAULT ((0)),
    [dwGold] int NOT NULL DEFAULT ((0)),
    [dwSilver] int NOT NULL DEFAULT ((0)),
    [dwCooper] int NOT NULL DEFAULT ((0)),
    [dwGI] int NOT NULL DEFAULT ((0)),
    [dwExp] int NOT NULL DEFAULT ((0)),
    [bGPoint] tinyint NOT NULL DEFAULT ((0)),
    [bStatus] tinyint NOT NULL,
    [bDisorg] tinyint NOT NULL DEFAULT ((0)),
    [dwTime] int NOT NULL DEFAULT ((0)),
    [timeEstablish] smalldatetime NOT NULL,
    [dwPvPTotalPoint] int NOT NULL DEFAULT ((0)),
    [dwPvPUseablePoint] int NOT NULL DEFAULT ((0)),
    [dwPvPMonthPoint] int NOT NULL DEFAULT ((0)),
    PRIMARY KEY ([dwID])
);
GO

CREATE TABLE [dbo].[TGUILDTACTICSTABLE] (
    [dwCharID] int NOT NULL,
    [dwGuildID] int NOT NULL,
    [dwRewardPoint] int NOT NULL,
    [dwGainPoint] int NOT NULL,
    [bDay] tinyint NOT NULL,
    [dEndTime] smalldatetime NOT NULL,
    [dlRewardMoney] bigint NOT NULL DEFAULT ((0)),
    PRIMARY KEY ([dwCharID], [dwGuildID])
);
GO

CREATE TABLE [dbo].[TGUILDTACTICSWANTEDTABLE] (
    [dwID] int NOT NULL,
    [dwGuildID] int NOT NULL,
    [bMaxLevel] tinyint NOT NULL,
    [bMinLevel] tinyint NOT NULL,
    [dEndTime] smalldatetime NOT NULL,
    [szTitle] varchar(256) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [szText] varchar(2048) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [bDay] tinyint NOT NULL,
    [dwGold] int NOT NULL,
    [dwSilver] int NOT NULL,
    [dwCooper] int NOT NULL,
    [dwPvPoint] int NOT NULL,
    PRIMARY KEY ([dwID])
);
GO

CREATE TABLE [dbo].[TGUILDVOLUNTEERTABLE] (
    [dwCharID] int NOT NULL,
    [bType] tinyint NOT NULL,
    [dwID] int NOT NULL,
    PRIMARY KEY ([dwCharID], [bType])
);
GO

CREATE TABLE [dbo].[TGUILDWANTEDTABLE] (
    [dwGuildID] int NOT NULL,
    [bMaxLevel] tinyint NOT NULL,
    [bMinLevel] tinyint NOT NULL,
    [dEndTime] smalldatetime NOT NULL,
    [szTitle] varchar(256) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [szText] varchar(2048) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    PRIMARY KEY ([dwGuildID])
);
GO

CREATE TABLE [dbo].[THELPMESSAGETABLE] (
    [bID] tinyint NOT NULL,
    [dStart] smalldatetime NOT NULL,
    [dEnd] smalldatetime NOT NULL,
    [szMessage] varchar(2048) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    PRIMARY KEY ([bID])
);
GO

CREATE TABLE [dbo].[THEROTABLE] (
    [bMonth] tinyint NOT NULL,
    [bType] tinyint NOT NULL,
    [bOrder] tinyint NOT NULL,
    [dwMonthRank] tinyint NOT NULL,
    [dwTotalRank] int NOT NULL,
    [dwTotalPoint] int NOT NULL,
    [dwMonthPoint] int NOT NULL,
    [wMonthWin] smallint NOT NULL,
    [wMonthLose] smallint NOT NULL,
    [dwTotalWin] int NOT NULL,
    [dwTotalLose] int NOT NULL,
    [dwCharID] int NOT NULL,
    [szName] varchar(50) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [bCountry] tinyint NOT NULL,
    [bLevel] tinyint NOT NULL,
    [bClass] tinyint NOT NULL,
    [bRace] char(10) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [bSex] tinyint NOT NULL,
    [bHair] tinyint NOT NULL,
    [bFace] tinyint NOT NULL,
    [szGuild] varchar(50) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [szSay] varchar(256) COLLATE Latin1_General_CI_AS_KS NOT NULL DEFAULT ('')
);
GO

CREATE TABLE [dbo].[THOTKEYTABLE] (
    [dwCharID] int NOT NULL,
    [bInvenID] tinyint NOT NULL,
    [bType1] tinyint NOT NULL,
    [wID1] smallint NOT NULL,
    [bType2] tinyint NOT NULL,
    [wID2] smallint NOT NULL,
    [bType3] tinyint NOT NULL,
    [wID3] smallint NOT NULL,
    [bType4] tinyint NOT NULL,
    [wID4] smallint NOT NULL,
    [bType5] tinyint NOT NULL,
    [wID5] smallint NOT NULL,
    [bType6] tinyint NOT NULL,
    [wID6] smallint NOT NULL,
    [bType7] tinyint NOT NULL,
    [wID7] smallint NOT NULL,
    [bType8] tinyint NOT NULL,
    [wID8] smallint NOT NULL,
    [bType9] tinyint NOT NULL,
    [wID9] smallint NOT NULL,
    [bType10] tinyint NOT NULL,
    [wID10] smallint NOT NULL,
    [bType11] tinyint NOT NULL,
    [wID11] smallint NOT NULL,
    [bType12] tinyint NOT NULL,
    [wID12] smallint NOT NULL
);
GO

CREATE TABLE [dbo].[TINDUNCHART] (
    [wMapID] smallint NOT NULL,
    [wInSpawn] smallint NOT NULL,
    [wOutSpawn_D] smallint NOT NULL,
    [wOutSpawn_C] smallint NOT NULL,
    PRIMARY KEY ([wMapID])
);
GO

CREATE TABLE [dbo].[TINVENTABLE] (
    [dwCharID] int NOT NULL,
    [bInvenID] tinyint NOT NULL,
    [wItemID] smallint NOT NULL,
    [dEndTime] smalldatetime NOT NULL,
    [bELD] tinyint NOT NULL DEFAULT ((0)),
    PRIMARY KEY ([dwCharID], [bInvenID])
);
GO

CREATE TABLE [dbo].[TINVENTOURNAMENTCHART] (
    [bInvenID] tinyint NOT NULL,
    [wItemID] tinyint NOT NULL,
    [bELD] tinyint NOT NULL
);
GO

CREATE TABLE [dbo].[TITEMATTRCHART] (
    [wID] smallint NOT NULL,
    [bKind] tinyint NOT NULL,
    [bGrade] tinyint NOT NULL,
    [wMinAP] smallint NOT NULL,
    [wMaxAP] smallint NOT NULL,
    [wDP] smallint NOT NULL,
    [wMinMAP] smallint NOT NULL,
    [wMaxMAP] smallint NOT NULL,
    [wMDP] smallint NOT NULL,
    [bBlockProb] tinyint NOT NULL,
    PRIMARY KEY ([wID])
);
GO

CREATE TABLE [dbo].[TITEMCHANGE] (
    [bStorageType] tinyint NOT NULL,
    [dwStorageID] int NOT NULL,
    [bOwnerType] tinyint NOT NULL,
    [dwOwnerID] int NOT NULL,
    [bItemID] tinyint NOT NULL,
    [wItemID] smallint NOT NULL,
    [bLevel] tinyint NOT NULL,
    [bCount] tinyint NOT NULL,
    [bGLevel] tinyint NOT NULL,
    [dwDuraMax] int NOT NULL,
    [dwDuraCur] int NOT NULL,
    [bRefineCur] tinyint NOT NULL,
    [bMagic1] tinyint NOT NULL,
    [bMagic2] tinyint NOT NULL,
    [bMagic3] tinyint NOT NULL,
    [bMagic4] tinyint NOT NULL,
    [bMagic5] tinyint NOT NULL,
    [bMagic6] tinyint NOT NULL,
    [wValue1] smallint NOT NULL,
    [wValue2] smallint NOT NULL,
    [wValue3] smallint NOT NULL,
    [wValue4] smallint NOT NULL,
    [wValue5] smallint NOT NULL,
    [wValue6] smallint NOT NULL,
    [dwTime1] int NOT NULL,
    [dwTime2] int NOT NULL,
    [dwTime3] int NOT NULL,
    [dwTime4] int NOT NULL,
    [dwTime5] int NOT NULL,
    [dwTime6] int NOT NULL
);
GO

CREATE TABLE [dbo].[TITEMCHART] (
    [wItemID] smallint NOT NULL,
    [bType] tinyint NOT NULL DEFAULT ((0)),
    [bKind] tinyint NOT NULL DEFAULT ((0)),
    [szNAME] nvarchar(100) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [wAttrID] smallint NOT NULL DEFAULT ((0)),
    [wUseValue] smallint NOT NULL DEFAULT ((0)),
    [dwSlotID] int NOT NULL,
    [dwClassID] int NOT NULL,
    [bPrmSlotID] tinyint NOT NULL,
    [bSubSlotID] tinyint NOT NULL,
    [bLevel] tinyint NOT NULL,
    [fPrice] float(53) NOT NULL,
    [bIsSell] tinyint NOT NULL,
    [bMinRange] tinyint NOT NULL,
    [bMaxRange] tinyint NOT NULL,
    [bStack] tinyint NOT NULL,
    [bEquipSkill] tinyint NOT NULL,
    [bSlotCount] tinyint NOT NULL,
    [bUseItemKind] tinyint NOT NULL,
    [bUseItemCount] tinyint NOT NULL,
    [bGrade] tinyint NOT NULL,
    [wUseTime] smallint NOT NULL,
    [bUseType] tinyint NOT NULL,
    [bCanGrade] tinyint NOT NULL,
    [bCanMagic] tinyint NOT NULL,
    [bCanRare] tinyint NOT NULL,
    [bDropLevel] tinyint NOT NULL,
    [dwSpeedInc] int NOT NULL,
    [bItemCountry] tinyint NOT NULL DEFAULT ((0)),
    [bIsSpecial] tinyint NOT NULL,
    [dwDelay] int NOT NULL,
    [fRevision] float(53) NOT NULL DEFAULT ((0)),
    [fMRevision] float(53) NOT NULL DEFAULT ((0)),
    [fAtRate] float(53) NOT NULL DEFAULT ((0)),
    [fMAtRate] float(53) NOT NULL DEFAULT ((0)),
    [bCanGamble] tinyint NOT NULL DEFAULT ((0)),
    [wItemProb_G] smallint NOT NULL DEFAULT ((0)),
    [bDestroyProb] tinyint NOT NULL DEFAULT ((0)),
    [bGambleProb] tinyint NOT NULL DEFAULT ((0)),
    [dwDuraMax] int NOT NULL DEFAULT ((0)),
    [bRefineMax] tinyint NOT NULL DEFAULT ((0)),
    [bCanRepair] tinyint NOT NULL DEFAULT ((0)),
    [wDelayGroupID] smallint NOT NULL,
    [wWeight] smallint NOT NULL,
    [bGroupID] tinyint NOT NULL,
    [bInitState] tinyint NOT NULL,
    [bCanWrap] tinyint NOT NULL DEFAULT ((0)),
    [dwCode] int NOT NULL DEFAULT ((0)),
    [bCanColor] tinyint NOT NULL DEFAULT ((0)),
    [fPvPrice] float(53) NOT NULL DEFAULT ((0)),
    [bConsumable] tinyint NOT NULL,
    [wExpandValue] smallint NOT NULL DEFAULT ((0)),
    PRIMARY KEY ([wItemID])
);
GO

CREATE TABLE [dbo].[TITEMCHART_bak] (
    [wItemID] smallint NOT NULL,
    [bType] tinyint NOT NULL DEFAULT ((0)),
    [bKind] tinyint NOT NULL DEFAULT ((0)),
    [szNAME] nvarchar(100) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [wAttrID] smallint NOT NULL DEFAULT ((0)),
    [wUseValue] smallint NOT NULL DEFAULT ((0)),
    [dwSlotID] int NOT NULL,
    [dwClassID] int NOT NULL,
    [bPrmSlotID] tinyint NOT NULL,
    [bSubSlotID] tinyint NOT NULL,
    [bLevel] tinyint NOT NULL,
    [fPrice] float(53) NOT NULL,
    [bIsSell] tinyint NOT NULL,
    [bMinRange] tinyint NOT NULL,
    [bMaxRange] tinyint NOT NULL,
    [bStack] tinyint NOT NULL,
    [bEquipSkill] tinyint NOT NULL,
    [bSlotCount] tinyint NOT NULL,
    [bUseItemKind] tinyint NOT NULL,
    [bUseItemCount] tinyint NOT NULL,
    [bGrade] tinyint NOT NULL,
    [wUseTime] smallint NOT NULL,
    [bUseType] tinyint NOT NULL,
    [bCanGrade] tinyint NOT NULL,
    [bCanMagic] tinyint NOT NULL,
    [bCanRare] tinyint NOT NULL,
    [bDropLevel] tinyint NOT NULL,
    [dwSpeedInc] int NOT NULL,
    [bItemCountry] tinyint NOT NULL DEFAULT ((0)),
    [bIsSpecial] tinyint NOT NULL,
    [dwDelay] int NOT NULL,
    [fRevision] float(53) NOT NULL DEFAULT ((0)),
    [fMRevision] float(53) NOT NULL DEFAULT ((0)),
    [fAtRate] float(53) NOT NULL DEFAULT ((0)),
    [fMAtRate] float(53) NOT NULL DEFAULT ((0)),
    [bCanGamble] tinyint NOT NULL DEFAULT ((0)),
    [wItemProb_G] smallint NOT NULL DEFAULT ((0)),
    [bDestroyProb] tinyint NOT NULL DEFAULT ((0)),
    [bGambleProb] tinyint NOT NULL DEFAULT ((0)),
    [dwDuraMax] int NOT NULL DEFAULT ((0)),
    [bRefineMax] tinyint NOT NULL DEFAULT ((0)),
    [bCanRepair] tinyint NOT NULL DEFAULT ((0)),
    [wDelayGroupID] smallint NOT NULL,
    [wWeight] smallint NOT NULL,
    [bGroupID] tinyint NOT NULL,
    [bInitState] tinyint NOT NULL,
    [bCanWrap] tinyint NOT NULL DEFAULT ((0)),
    [dwCode] int NOT NULL DEFAULT ((0)),
    [bCanColor] tinyint NOT NULL DEFAULT ((0)),
    [fPvPrice] float(53) NOT NULL DEFAULT ((0)),
    [bConsumable] tinyint NOT NULL,
    [wExpandValue] smallint NOT NULL DEFAULT ((0)),
    PRIMARY KEY ([wItemID])
);
GO

CREATE TABLE [dbo].[TITEMGRADECHART] (
    [bLevel] tinyint NOT NULL,
    [bGrade] tinyint NOT NULL,
    [bProb] tinyint NOT NULL,
    [dwMoney] int NOT NULL DEFAULT ((0))
);
GO

CREATE TABLE [dbo].[TITEMLEVELCHART] (
    [bGrade] tinyint NOT NULL,
    [bLevel] tinyint NOT NULL,
    PRIMARY KEY ([bGrade])
);
GO

CREATE TABLE [dbo].[TITEMLOG] (
    [dwCharID] int NOT NULL,
    [dwGold] int NOT NULL,
    [dwSilver] int NOT NULL,
    [dwCooper] int NOT NULL,
    [bInvenID] tinyint NOT NULL,
    [bItemID] tinyint NOT NULL,
    [wItemID] smallint NOT NULL,
    [bLevel] tinyint NOT NULL,
    [bCount] tinyint NOT NULL,
    [bInsType] tinyint NOT NULL,
    [dwTargetID] int NOT NULL,
    [szTargetName] varchar(50) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [bMagic1] tinyint NOT NULL,
    [bMagic2] tinyint NOT NULL,
    [bMagic3] tinyint NOT NULL,
    [bMagic4] tinyint NOT NULL,
    [bMagic5] tinyint NOT NULL,
    [bMagic6] tinyint NOT NULL,
    [wValue1] smallint NOT NULL,
    [wValue2] smallint NOT NULL,
    [wValue3] smallint NOT NULL,
    [wValue4] smallint NOT NULL,
    [wValue5] smallint NOT NULL,
    [wValue6] smallint NOT NULL,
    [dwTime1] int NOT NULL,
    [dwTime2] int NOT NULL,
    [dwTime3] int NOT NULL,
    [dwTime4] int NOT NULL,
    [dwTime5] int NOT NULL,
    [dwTime6] int NOT NULL,
    [timeInsert] smalldatetime NOT NULL
);
GO

CREATE TABLE [dbo].[TITEMMAGICCHART] (
    [bMagic] tinyint NOT NULL,
    [dwKind] int NOT NULL,
    [bRvType] tinyint NOT NULL,
    [wMaxValue] smallint NOT NULL,
    [bIsMagic] tinyint NOT NULL,
    [bIsRare] tinyint NOT NULL,
    [bMinLevel] tinyint NOT NULL,
    [bExclIndex] tinyint NOT NULL,
    [bOptionKind] tinyint NOT NULL,
    [wAutoSkill] smallint NOT NULL DEFAULT ((0)),
    [bRefine] tinyint NOT NULL DEFAULT ((0)),
    [wMaxBound] smallint NOT NULL DEFAULT ((0)),
    [wRareBound] smallint NOT NULL DEFAULT ((0)),
    PRIMARY KEY ([bMagic])
);
GO

CREATE TABLE [dbo].[TITEMMAGICLEVELCHART] (
    [dwSection] int NOT NULL,
    [bLevel] tinyint NOT NULL,
    PRIMARY KEY ([dwSection])
);
GO

CREATE TABLE [dbo].[TITEMMAGICSKILLCHART] (
    [bMagic] tinyint NOT NULL,
    [dwKind] int NOT NULL,
    [wSkillID] smallint NOT NULL,
    [bIsMagic] tinyint NOT NULL,
    [bIsRare] tinyint NOT NULL,
    [bMinLevel] tinyint NOT NULL
);
GO

CREATE TABLE [dbo].[TITEMSETCHART] (
    [wBaseID] smallint NOT NULL,
    [wSetID] smallint NOT NULL,
    [bLevel] tinyint NOT NULL,
    [bMagic1] tinyint NOT NULL,
    [bMin1] tinyint NOT NULL,
    [bValue1] tinyint NOT NULL,
    [bMagic2] tinyint NOT NULL,
    [bMin2] tinyint NOT NULL,
    [bValue2] tinyint NOT NULL,
    [bMagic3] tinyint NOT NULL,
    [bMin3] tinyint NOT NULL,
    [bValue3] tinyint NOT NULL,
    [bMagic4] tinyint NOT NULL,
    [bMin4] tinyint NOT NULL,
    [bValue4] tinyint NOT NULL,
    [bMagic5] tinyint NOT NULL,
    [bMin5] tinyint NOT NULL,
    [bValue5] tinyint NOT NULL,
    [bMagic6] tinyint NOT NULL,
    [bMin6] tinyint NOT NULL,
    [bValue6] tinyint NOT NULL,
    PRIMARY KEY ([wBaseID], [wSetID])
);
GO

CREATE TABLE [dbo].[TITEMTABLE] (
    [dlID] bigint NOT NULL,
    [bStorageType] tinyint NOT NULL,
    [dwStorageID] int NOT NULL,
    [bOwnerType] tinyint NOT NULL,
    [dwOwnerID] int NOT NULL,
    [bItemID] tinyint NOT NULL,
    [wItemID] smallint NOT NULL,
    [bLevel] tinyint NOT NULL,
    [bCount] tinyint NOT NULL,
    [bGLevel] tinyint NOT NULL DEFAULT ((0)),
    [dwDuraMax] int NOT NULL,
    [dwDuraCur] int NOT NULL,
    [bRefineCur] tinyint NOT NULL,
    [dEndTime] smalldatetime NOT NULL,
    [bGradeEffect] tinyint NOT NULL DEFAULT ((0)),
    [bMagic1] tinyint NOT NULL,
    [bMagic2] tinyint NOT NULL,
    [bMagic3] tinyint NOT NULL,
    [bMagic4] tinyint NOT NULL,
    [bMagic5] tinyint NOT NULL,
    [bMagic6] tinyint NOT NULL,
    [wValue1] smallint NOT NULL,
    [wValue2] smallint NOT NULL,
    [wValue3] smallint NOT NULL,
    [wValue4] smallint NOT NULL,
    [wValue5] smallint NOT NULL,
    [wValue6] smallint NOT NULL,
    [dwTime1] int NOT NULL,
    [dwTime2] int NOT NULL,
    [dwTime3] int NOT NULL,
    [dwTime4] int NOT NULL,
    [dwTime5] int NOT NULL,
    [dwTime6] int NOT NULL,
    [bGem] tinyint NOT NULL,
    [wMoggItemID] smallint NOT NULL,
    PRIMARY KEY ([dlID])
);
GO

CREATE TABLE [dbo].[TITEMTOURNAMENTCHART] (
    [bClass] tinyint NOT NULL,
    [bInvenID] tinyint NOT NULL,
    [bItemID] tinyint NOT NULL,
    [wItemID] smallint NOT NULL,
    [bLevel] tinyint NOT NULL,
    [bCount] tinyint NOT NULL,
    [bGLevel] tinyint NOT NULL,
    [dwDuraMax] int NOT NULL,
    [bGradeEffect] tinyint NOT NULL,
    [bMagic1] tinyint NOT NULL,
    [bMagic2] tinyint NOT NULL,
    [bMagic3] tinyint NOT NULL,
    [bMagic4] tinyint NOT NULL,
    [bMagic5] tinyint NOT NULL,
    [bMagic6] tinyint NOT NULL,
    [wValue1] smallint NOT NULL,
    [wValue2] smallint NOT NULL,
    [wValue3] smallint NOT NULL,
    [wValue4] smallint NOT NULL,
    [wValue5] smallint NOT NULL,
    [wValue6] smallint NOT NULL,
    [dwTime1] int NOT NULL,
    [dwTime2] int NOT NULL,
    [dwTime3] int NOT NULL,
    [dwTime4] int NOT NULL,
    [dwTime5] int NOT NULL,
    [dwTime6] int NOT NULL,
    [bGem] tinyint NOT NULL,
    [wMoggItemID] smallint NOT NULL
);
GO

CREATE TABLE [dbo].[TITEMUSEDTABLE] (
    [dwCharID] int NOT NULL,
    [wDelayGroupID] smallint NOT NULL,
    [dwTick] int NOT NULL,
    PRIMARY KEY ([dwCharID], [wDelayGroupID])
);
GO

CREATE TABLE [dbo].[TLASTCOMPANIONTABLE] (
    [dwCharID] int NOT NULL,
    [bCompanionSlot] tinyint NOT NULL
);
GO

CREATE TABLE [dbo].[TLEVELCHART] (
    [bLevel] tinyint NOT NULL,
    [dwEXP] int NOT NULL,
    [dwHP] int NOT NULL,
    [dwMP] int NOT NULL,
    [bSkillPoint] tinyint NOT NULL,
    [dwMoney] int NOT NULL,
    [dwScore] int NOT NULL DEFAULT ((0)),
    [dwRegCost] int NOT NULL DEFAULT ((0)),
    [dwSearchCost] int NOT NULL DEFAULT ((0)),
    [dwGambleCost] int NOT NULL DEFAULT ((0)),
    [dwRepCost] int NOT NULL DEFAULT ((0)),
    [dwRepairCost] int NOT NULL DEFAULT ((0)),
    [dwRefineCost] int NOT NULL DEFAULT ((0)),
    [wPvPoint] smallint NOT NULL DEFAULT ((0)),
    [dwPvPMoney] int NOT NULL DEFAULT ((0)),
    [dwPvPExp] int NOT NULL DEFAULT ((0)),
    PRIMARY KEY ([bLevel])
);
GO

CREATE TABLE [dbo].[TLOCALOCCUPYTABLE] (
    [wLocalID] smallint NOT NULL,
    [bDay] tinyint NOT NULL,
    [dwGuildID] int NOT NULL,
    [bType] tinyint NOT NULL,
    PRIMARY KEY ([wLocalID], [bDay])
);
GO

CREATE TABLE [dbo].[TLOCALTABLE] (
    [wLocalID] smallint NOT NULL,
    [bCountry] tinyint NOT NULL,
    [dwGuild] int NOT NULL,
    [dateOccupy] smalldatetime NOT NULL,
    [dateDefend] smalldatetime NOT NULL,
    [szHero] varchar(50) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [dateHero] smalldatetime NOT NULL DEFAULT ((0)),
    PRIMARY KEY ([wLocalID])
);
GO

CREATE TABLE [dbo].[TMAPCHART] (
    [bGroupID] tinyint NOT NULL,
    [wMapID] smallint NOT NULL,
    [bServerID] tinyint NOT NULL,
    [bChannel] tinyint NOT NULL,
    PRIMARY KEY ([bGroupID], [wMapID], [bServerID])
);
GO

CREATE TABLE [dbo].[TMAPMONCHART] (
    [wSpawnID] smallint NOT NULL,
    [wMonID] smallint NOT NULL,
    [bEssential] tinyint NOT NULL,
    [bLeader] tinyint NOT NULL,
    [bProb] tinyint NOT NULL,
    PRIMARY KEY ([wSpawnID], [wMonID], [bEssential])
);
GO

CREATE TABLE [dbo].[TMEDALS] (
    [dwCharID] int NOT NULL,
    [dwMedals] int NOT NULL
);
GO

CREATE TABLE [dbo].[TMENTORTABLE] (
    [dwCharID] int NOT NULL,
    [dwMentorID] int NOT NULL,
    [dwExp] int NOT NULL,
    PRIMARY KEY ([dwCharID])
);
GO

CREATE TABLE [dbo].[TMISSIONTABLE] (
    [wMissionID] smallint NOT NULL,
    [bCountry] tinyint NOT NULL
);
GO

CREATE TABLE [dbo].[TMONATTRCHART] (
    [wID] smallint NOT NULL,
    [bLevel] tinyint NOT NULL,
    [wAP] smallint NOT NULL,
    [wLAP] smallint NOT NULL,
    [dwAtkSpeed] int NOT NULL,
    [wAL] smallint NOT NULL,
    [wDL] smallint NOT NULL,
    [bCriticalPP] tinyint NOT NULL,
    [dwMaxHP] int NOT NULL,
    [bHPRecover] tinyint NOT NULL,
    [wMAP] smallint NOT NULL,
    [bCriticalMP] smallint NOT NULL,
    [dwMaxMP] int NOT NULL,
    [bMPRecover] tinyint NOT NULL,
    [wDP] smallint NOT NULL DEFAULT ((0)),
    [wMDP] smallint NOT NULL DEFAULT ((0)),
    [wMinWAP] smallint NOT NULL DEFAULT ((0)),
    [wMaxWAP] smallint NOT NULL DEFAULT ((0)),
    [wWDP] smallint NOT NULL DEFAULT ((0)),
    [wMAL] smallint NOT NULL,
    [wMDL] smallint NOT NULL,
    PRIMARY KEY ([wID], [bLevel])
);
GO

CREATE TABLE [dbo].[TMONITEMCHART] (
    [bChartType] tinyint NOT NULL DEFAULT ((0)),
    [wMonID] smallint NOT NULL,
    [wItemID] smallint NOT NULL,
    [wItemIDMin] smallint NOT NULL DEFAULT ((0)),
    [wItemIDMax] smallint NOT NULL,
    [bLevelMin] tinyint NOT NULL,
    [bLevelMax] tinyint NOT NULL,
    [bItemProb_N1] tinyint NOT NULL,
    [bItemProb_N2] tinyint NOT NULL,
    [bItemProb_N3] tinyint NOT NULL,
    [bItemProb_N4] tinyint NOT NULL,
    [bItemProb_M] tinyint NOT NULL,
    [bItemProb_S] tinyint NOT NULL,
    [bItemProb_R] tinyint NOT NULL,
    [bItemMagicOpt] tinyint NOT NULL,
    [bItemRareOpt] tinyint NOT NULL,
    [wWeight] smallint NOT NULL DEFAULT ((1)),
    PRIMARY KEY ([wMonID], [wItemID], [bChartType])
);
GO

CREATE TABLE [dbo].[TMONSPAWNCHART] (
    [wID] smallint NOT NULL,
    [wGroup] smallint NOT NULL DEFAULT ((0)),
    [wLocalID] smallint NOT NULL DEFAULT ((0)),
    [wMapID] smallint NOT NULL,
    [fPosX] real NOT NULL,
    [fPosY] real NOT NULL,
    [fPosZ] real NOT NULL,
    [wDir] smallint NOT NULL,
    [bCountry] tinyint NOT NULL,
    [bCount] tinyint NOT NULL,
    [bRange] tinyint NOT NULL,
    [bArea] tinyint NOT NULL,
    [bLink] tinyint NOT NULL,
    [bProb] tinyint NOT NULL,
    [bRoamType] tinyint NOT NULL,
    [dwRegion] int NOT NULL DEFAULT ((0)),
    [dwDelay] int NOT NULL,
    [bEvent] tinyint NOT NULL DEFAULT ((0)),
    [wPartyID] smallint NOT NULL DEFAULT ((0)),
    PRIMARY KEY ([wID])
);
GO

CREATE TABLE [dbo].[TMONSTERCHART] (
    [wID] smallint NOT NULL,
    [szName] varchar(50) COLLATE Latin1_General_CI_AS_KS NOT NULL DEFAULT ((0)),
    [szName2] varchar(50) COLLATE Latin1_General_CI_AS_KS NOT NULL DEFAULT (''),
    [bRace] tinyint NOT NULL,
    [bClass] tinyint NOT NULL,
    [wKind] smallint NOT NULL DEFAULT ((0)),
    [bLevel] tinyint NOT NULL,
    [bAIType] tinyint NOT NULL,
    [bRange] tinyint NOT NULL,
    [wChaseRange] smallint NOT NULL,
    [bRoamProb] tinyint NOT NULL,
    [bMoneyProb] tinyint NOT NULL,
    [dwMinMoney] int NOT NULL,
    [dwMaxMoney] int NOT NULL,
    [bItemProb] tinyint NOT NULL,
    [bDropCount] tinyint NOT NULL,
    [wExp] smallint NOT NULL,
    [bIsSelf] tinyint NOT NULL,
    [bRecallType] tinyint NOT NULL,
    [bCanSelect] tinyint NOT NULL,
    [bCanAttack] tinyint NOT NULL,
    [bTame] tinyint NOT NULL,
    [bCall] tinyint NOT NULL DEFAULT ((0)),
    [bIsSpecial] tinyint NOT NULL,
    [bRemove] tinyint NOT NULL,
    [wMonAttr] smallint NOT NULL,
    [wSummonAttr] smallint NOT NULL,
    [wTransSkillID] smallint NOT NULL,
    [fSize] float(53) NOT NULL,
    [wSkill1] smallint NOT NULL,
    [wSkill2] smallint NOT NULL,
    [wSkill3] smallint NOT NULL,
    [wSkill4] smallint NOT NULL,
    PRIMARY KEY ([wID])
);
GO

CREATE TABLE [dbo].[TMONSTERSHOPCHART] (
    [wID] smallint NOT NULL,
    [wNpcID] smallint NOT NULL,
    [wSpawnID] smallint NOT NULL,
    [dwPrice] int NOT NULL,
    [wTowerID] smallint NOT NULL,
    PRIMARY KEY ([wID])
);
GO

CREATE TABLE [dbo].[TMONTHPVPOINTTABLE] (
    [dwCharID] int NOT NULL,
    [bCountry] tinyint NOT NULL,
    [dwPoint] int NOT NULL,
    [wWin] smallint NOT NULL,
    [wLose] smallint NOT NULL,
    [szSay] varchar(256) COLLATE Latin1_General_CI_AS_KS NOT NULL DEFAULT (''),
    PRIMARY KEY ([dwCharID], [bCountry])
);
GO

CREATE TABLE [dbo].[TMONTHRANKCHART] (
    [bRank] tinyint NOT NULL,
    [bChartType1] tinyint NOT NULL DEFAULT ((1)),
    [wItemID1] smallint NOT NULL DEFAULT ((0)),
    [bCount1] tinyint NOT NULL DEFAULT ((0)),
    [bChartType2] tinyint NOT NULL DEFAULT ((1)),
    [wItemID2] smallint NOT NULL DEFAULT ((0)),
    [bCount2] tinyint NOT NULL DEFAULT ((0)),
    [bChartType3] tinyint NOT NULL DEFAULT ((1)),
    [wItemID3] smallint NOT NULL DEFAULT ((0)),
    [bCount3] tinyint NOT NULL DEFAULT ((0)),
    [bChartType4] tinyint NOT NULL DEFAULT ((1)),
    [wItemID4] smallint NOT NULL DEFAULT ((0)),
    [bCount4] tinyint NOT NULL DEFAULT ((0)),
    PRIMARY KEY ([bRank])
);
GO

CREATE TABLE [dbo].[TMONTHRANKTABLE] (
    [bMonth] tinyint NOT NULL,
    [bCountry] tinyint NOT NULL,
    [bRank] tinyint NOT NULL,
    [bMonthRank] tinyint NOT NULL,
    [dwTotalRank] int NOT NULL,
    [dwCharID] int NOT NULL,
    [szName] varchar(50) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [dwTotalPoint] int NOT NULL,
    [dwMonthPoint] int NOT NULL,
    [wMonthWin] smallint NOT NULL,
    [wMonthLose] smallint NOT NULL,
    [dwTotalWin] int NOT NULL,
    [dwTotalLose] int NOT NULL,
    [bLevel] tinyint NOT NULL,
    [bClass] tinyint NOT NULL,
    [bRace] tinyint NOT NULL,
    [bSex] tinyint NOT NULL,
    [bHair] tinyint NOT NULL,
    [bFace] tinyint NOT NULL,
    [szSay] varchar(256) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [szGuild] varchar(50) COLLATE Latin1_General_CI_AS_KS NOT NULL
);
GO

CREATE TABLE [dbo].[TMOUNTCHART] (
    [wMountID] smallint NOT NULL,
    [wDefMonID] smallint NOT NULL,
    [wUpgMonID] smallint NOT NULL,
    PRIMARY KEY ([wMountID])
);
GO

CREATE TABLE [dbo].[TMOUNTITEMTABLE] (
    [dwUserID] int NOT NULL,
    [wItemID] smallint NOT NULL,
    [bType] tinyint NOT NULL,
    [dEndTime] smalldatetime NOT NULL,
    PRIMARY KEY ([dwUserID], [wItemID])
);
GO

CREATE TABLE [dbo].[TMOUNTTABLE] (
    [dwCharID] int NOT NULL,
    [wMountID] smallint NOT NULL,
    [szName] varchar(50) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [dEndTime] smalldatetime NOT NULL,
    PRIMARY KEY ([dwCharID], [wMountID])
);
GO

CREATE TABLE [dbo].[TNPCCHART] (
    [wID] smallint NOT NULL,
    [NC_szName2] varchar(50) COLLATE Latin1_General_CI_AS_KS NOT NULL DEFAULT ('_'),
    [szName] varchar(50) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [bType] tinyint NOT NULL,
    [dwClass] int NOT NULL,
    [bCountryID] tinyint NOT NULL,
    [wLocalID] smallint NOT NULL,
    [bCondition] tinyint NOT NULL DEFAULT ((0)),
    [bDiscountRate] tinyint NOT NULL DEFAULT ((0)),
    [bAddProb] tinyint NOT NULL DEFAULT ((0)),
    [wItemID] smallint NOT NULL DEFAULT ((0)),
    [wMapID] smallint NOT NULL DEFAULT ((0)),
    [fPosX] float(53) NOT NULL DEFAULT ((0)),
    [fPosY] float(53) NOT NULL DEFAULT ((0)),
    [fPosZ] float(53) NOT NULL DEFAULT ((0)),
    [szLocal] varchar(100) COLLATE Latin1_General_CI_AS_KS NOT NULL DEFAULT (''),
    PRIMARY KEY ([wID])
);
GO

CREATE TABLE [dbo].[TNPCITEMCHART] (
    [wNpcID] smallint NOT NULL,
    [dwItemID] int NOT NULL,
    PRIMARY KEY ([wNpcID], [dwItemID])
);
GO

CREATE TABLE [dbo].[TOPERATORTABLE] (
    [dwOperatorID] int NOT NULL,
    PRIMARY KEY ([dwOperatorID])
);
GO

CREATE TABLE [dbo].[TPETCHART] (
    [wID] smallint NOT NULL,
    [bPetType] tinyint NOT NULL,
    [bRace] tinyint NOT NULL,
    [wMonID] smallint NOT NULL,
    [bRecallKind1] tinyint NOT NULL,
    [bRecallKind2] tinyint NOT NULL,
    [wRecallValue1] smallint NOT NULL,
    [wRecallValue2] smallint NOT NULL,
    [bConditionType] tinyint NOT NULL,
    [dwConditionValue] int NOT NULL,
    PRIMARY KEY ([wID])
);
GO

CREATE TABLE [dbo].[TPETTABLE] (
    [dwUserID] int NOT NULL,
    [wPetID] smallint NOT NULL,
    [szName] varchar(50) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [timeUse] smalldatetime NOT NULL,
    [bEffect] tinyint NULL,
    PRIMARY KEY ([dwUserID], [wPetID])
);
GO

CREATE TABLE [dbo].[TPLAYTIMETABLE] (
    [dwCharID] int NOT NULL,
    [dwPlayTime] int NULL
);
GO

CREATE TABLE [dbo].[TPORTALCHART] (
    [wPortalID] smallint NOT NULL,
    [bCountry] tinyint NOT NULL,
    [wLocalID] smallint NOT NULL,
    [wSpawnID] smallint NOT NULL,
    [bCondition] tinyint NOT NULL,
    PRIMARY KEY ([wPortalID])
);
GO

CREATE TABLE [dbo].[TPOSTERRORTABLE] (
    [dwCharID] int NOT NULL,
    [dwGold] int NOT NULL
);
GO

CREATE TABLE [dbo].[TPOSTITEMTABLE] (
    [dwCharID] int NOT NULL,
    [dwPostID] int NOT NULL,
    [wItemID] smallint NOT NULL,
    [bLevel] tinyint NOT NULL,
    [bCount] tinyint NOT NULL,
    [bGLevel] tinyint NOT NULL DEFAULT ((0)),
    [bMagic1] tinyint NOT NULL,
    [bMagic2] tinyint NOT NULL,
    [bMagic3] tinyint NOT NULL,
    [bMagic4] tinyint NOT NULL,
    [bMagic5] tinyint NOT NULL,
    [bMagic6] tinyint NOT NULL,
    [wValue1] smallint NOT NULL,
    [wValue2] smallint NOT NULL,
    [wValue3] smallint NOT NULL,
    [wValue4] smallint NOT NULL,
    [wValue5] smallint NOT NULL,
    [wValue6] smallint NOT NULL,
    [dwTime1] int NOT NULL,
    [dwTime2] int NOT NULL,
    [dwTime3] int NOT NULL,
    [dwTime4] int NOT NULL,
    [dwTime5] int NOT NULL,
    [dwTime6] int NOT NULL
);
GO

CREATE TABLE [dbo].[TPOSTTABLE] (
    [dwCharID] int NOT NULL,
    [dwPostID] int IDENTITY(1,1) NOT NULL,
    [szRecvName] varchar(50) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [bType] tinyint NOT NULL,
    [bRead] tinyint NOT NULL,
    [timeRecv] smalldatetime NOT NULL,
    [dwSendID] int NOT NULL,
    [szSender] nvarchar(50) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [szTitle] nvarchar(256) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [szMessage] nvarchar(2048) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [dwGold] int NOT NULL,
    [dwSilver] int NOT NULL,
    [dwCooper] int NOT NULL,
    [bContain] tinyint NULL,
    PRIMARY KEY ([dwPostID])
);
GO

CREATE TABLE [dbo].[TPREMIUMSKILLCHART] (
    [bType] tinyint NOT NULL,
    [wSkillID] smallint NOT NULL,
    [wMedals] smallint NOT NULL
);
GO

CREATE TABLE [dbo].[TPROTECTEDTABLE] (
    [dwCharID] int NOT NULL,
    [dwProtected] int NOT NULL,
    [szNAME] varchar(50) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [bOption] tinyint NOT NULL DEFAULT ((1)),
    PRIMARY KEY ([dwCharID], [dwProtected])
);
GO

CREATE TABLE [dbo].[TPVPOINTCHART] (
    [wLocalID] smallint NOT NULL,
    [bStatus] tinyint NOT NULL,
    [bEvent] tinyint NOT NULL,
    [bTarget] tinyint NOT NULL,
    [dwIncPoint] int NOT NULL,
    [dwDecPoint] int NOT NULL
);
GO

CREATE TABLE [dbo].[TPVPOINTTABLE] (
    [dwCharID] int NOT NULL,
    [dwUseablePoint] int NOT NULL,
    [dwTotalPoint] int NOT NULL,
    PRIMARY KEY ([dwCharID])
);
GO

CREATE TABLE [dbo].[TPVPRECENTTABLE] (
    [dwCharID] int NOT NULL,
    [szName] varchar(50) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [bClass] tinyint NOT NULL,
    [bLevel] tinyint NOT NULL,
    [bWin] tinyint NOT NULL,
    [dwPoint] int NOT NULL,
    [dlDate] smalldatetime NOT NULL
);
GO

CREATE TABLE [dbo].[TPVPRECORDTABLE] (
    [dwCharID] int NOT NULL,
    [dwWarrior_win] int NOT NULL DEFAULT ((0)),
    [dwWarrior_lose] int NOT NULL DEFAULT ((0)),
    [dwRanger_win] int NOT NULL DEFAULT ((0)),
    [dwRanger_lose] int NOT NULL DEFAULT ((0)),
    [dwArcher_win] int NOT NULL DEFAULT ((0)),
    [dwArcher_lose] int NOT NULL DEFAULT ((0)),
    [dwWizard_win] int NOT NULL DEFAULT ((0)),
    [dwWizard_lose] int NOT NULL DEFAULT ((0)),
    [dwPriest_win] int NOT NULL DEFAULT ((0)),
    [dwPriest_lose] int NOT NULL DEFAULT ((0)),
    [dwSorcerer_win] int NOT NULL DEFAULT ((0)),
    [dwSorcerer_lose] int NOT NULL DEFAULT ((0)),
    PRIMARY KEY ([dwCharID])
);
GO

CREATE TABLE [dbo].[TQCLASSCHART] (
    [dwClassID] int NOT NULL,
    [szNAME] varchar(1024) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [bClassMain] tinyint NOT NULL DEFAULT ((0)),
    PRIMARY KEY ([dwClassID])
);
GO

CREATE TABLE [dbo].[TQCONDITIONCHART] (
    [dwID] int NOT NULL,
    [dwQuestID] int NOT NULL,
    [bConditionType] tinyint NOT NULL,
    [dwConditionID] int NOT NULL,
    [bCount] tinyint NOT NULL,
    PRIMARY KEY ([dwID])
);
GO

CREATE TABLE [dbo].[TQREWARDCHART] (
    [dwID] int NOT NULL,
    [dwQuestID] int NOT NULL,
    [bRewardType] tinyint NOT NULL,
    [dwRewardID] int NOT NULL,
    [bTakeMethod] tinyint NOT NULL,
    [bTakeData] tinyint NOT NULL,
    [bCount] tinyint NOT NULL,
    [dwQuestMob] int NOT NULL,
    [dwQuestTime] int NOT NULL,
    [dwQuestPathMob] int NOT NULL,
    [dwTicketID] int NOT NULL DEFAULT ((0)),
    [bSendQ] tinyint NOT NULL DEFAULT ((0)),
    PRIMARY KEY ([dwID])
);
GO

CREATE TABLE [dbo].[TQTITLECHART] (
    [dwQuestID] int NOT NULL,
    [dwClassID] int NOT NULL DEFAULT ((0)),
    [szTitle] varchar(255) COLLATE Latin1_General_CI_AS_KS NULL,
    [szMessage] varchar(1024) COLLATE Latin1_General_CI_AS_KS NULL,
    [szComplete] varchar(1024) COLLATE Latin1_General_CI_AS_KS NULL,
    [szAccept] varchar(1024) COLLATE Latin1_General_CI_AS_KS NULL,
    [szReject] varchar(1024) COLLATE Latin1_General_CI_AS_KS NULL,
    [szSummary] varchar(1024) COLLATE Latin1_General_CI_AS_KS NULL,
    [szNPCName] varchar(1024) COLLATE Latin1_General_CI_AS_KS NULL,
    [szReply] varchar(1024) COLLATE Latin1_General_CI_AS_KS NULL,
    PRIMARY KEY ([dwQuestID])
);
GO

CREATE TABLE [dbo].[TQUESTCHART] (
    [dwQuestID] int NOT NULL,
    [dwParentID] int NOT NULL,
    [bType] tinyint NOT NULL,
    [bForceRun] tinyint NOT NULL,
    [bTriggerType] tinyint NOT NULL,
    [dwTriggerID] int NOT NULL,
    [bCountMax] tinyint NOT NULL,
    [bLevel] tinyint NOT NULL,
    [bMain] tinyint NOT NULL DEFAULT ((0)),
    [bConditionCheck] tinyint NOT NULL DEFAULT ((0)),
    PRIMARY KEY ([dwQuestID])
);
GO

CREATE TABLE [dbo].[TQUESTITEMCHART] (
    [dwID] int NOT NULL,
    [wItemID] smallint NOT NULL,
    [bLevel] tinyint NOT NULL,
    [bGLevel] tinyint NOT NULL,
    [bDropLevel] tinyint NOT NULL DEFAULT ((0)),
    [dwDuraMax] int NOT NULL,
    [dwDuraCur] int NOT NULL,
    [bRefineCur] tinyint NOT NULL,
    [wUseTime] smallint NOT NULL DEFAULT ((0)),
    [bGradeEffect] tinyint NOT NULL DEFAULT ((0)),
    [bMagic1] tinyint NOT NULL,
    [bMagic2] tinyint NOT NULL,
    [bMagic3] tinyint NOT NULL,
    [bMagic4] tinyint NOT NULL,
    [bMagic5] tinyint NOT NULL,
    [bMagic6] tinyint NOT NULL,
    [wValue1] smallint NOT NULL,
    [wValue2] smallint NOT NULL,
    [wValue3] smallint NOT NULL,
    [wValue4] smallint NOT NULL,
    [wValue5] smallint NOT NULL,
    [wValue6] smallint NOT NULL,
    [dwTime1] int NOT NULL,
    [dwTime2] int NOT NULL,
    [dwTime3] int NOT NULL,
    [dwTime4] int NOT NULL,
    [dwTime5] int NOT NULL,
    [dwTime6] int NOT NULL,
    [dwMoney] int NOT NULL,
    [bGem] tinyint NOT NULL DEFAULT ((0)),
    PRIMARY KEY ([dwID])
);
GO

CREATE TABLE [dbo].[TQUESTTABLE] (
    [dwCharID] int NOT NULL,
    [dwQuestID] int NOT NULL,
    [dwTick] int NOT NULL,
    [bCompleteCount] tinyint NOT NULL,
    [bTriggerCount] tinyint NOT NULL,
    PRIMARY KEY ([dwCharID], [dwQuestID])
);
GO

CREATE TABLE [dbo].[TQUESTTERMCHART] (
    [dwID] int NOT NULL,
    [dwQuestID] int NOT NULL,
    [bTermType] tinyint NOT NULL,
    [dwTermID] int NOT NULL,
    [bCount] tinyint NOT NULL,
    PRIMARY KEY ([dwID])
);
GO

CREATE TABLE [dbo].[TQUESTTERMTABLE] (
    [dwCharID] int NOT NULL,
    [dwQuestID] int NOT NULL,
    [dwTermID] int NOT NULL,
    [bTermType] tinyint NOT NULL,
    [bCount] tinyint NOT NULL,
    PRIMARY KEY ([dwCharID], [dwQuestID], [dwTermID])
);
GO

CREATE TABLE [dbo].[TRACECHART] (
    [bRaceID] tinyint NOT NULL,
    [wSTR] smallint NOT NULL,
    [wDEX] smallint NOT NULL,
    [wCON] smallint NOT NULL,
    [wINT] smallint NOT NULL,
    [wWIS] smallint NOT NULL,
    [wMEN] smallint NOT NULL,
    PRIMARY KEY ([bRaceID])
);
GO

CREATE TABLE [dbo].[TRANKING] (
    [dwCharID] int NOT NULL,
    [dwRankPoint] int NOT NULL,
    PRIMARY KEY ([dwCharID])
);
GO

CREATE TABLE [dbo].[TRECALLMAINTAINTABLE] (
    [dwCharID] int NOT NULL,
    [dwRecallID] int NOT NULL,
    [wSkillID] smallint NOT NULL,
    [bLevel] tinyint NOT NULL,
    [dwRemainTick] int NOT NULL,
    [bAttackType] tinyint NOT NULL,
    [dwAttackID] int NOT NULL,
    [bHostType] char(10) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [dwHostID] int NOT NULL,
    [bAttackCountry] tinyint NOT NULL
);
GO

CREATE TABLE [dbo].[TRECALLMONTABLE] (
    [dwOwnerID] int NOT NULL,
    [dwID] int NOT NULL,
    [wMonID] smallint NOT NULL,
    [wPetID] smallint NOT NULL DEFAULT ((0)),
    [dwATTR] int NOT NULL,
    [bLevel] tinyint NOT NULL,
    [dwHP] int NOT NULL,
    [dwMP] int NOT NULL,
    [bSkillLevel] tinyint NOT NULL DEFAULT ((0)),
    [wPosX] smallint NOT NULL,
    [wPosY] smallint NOT NULL,
    [wPosZ] smallint NOT NULL,
    [dwTime] int NOT NULL,
    [bEffect] tinyint NOT NULL DEFAULT ((0)),
    PRIMARY KEY ([dwID])
);
GO

CREATE TABLE [dbo].[TRESERVEDPOST] (
    [dwSeq] int IDENTITY(1,1) NOT NULL,
    [dwRecverID] int NOT NULL,
    [szSender] varchar(50) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [szTitle] varchar(256) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [szMessage] varchar(2048) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [bSend] tinyint NOT NULL,
    [wItemID] smallint NOT NULL,
    [bLevel] tinyint NOT NULL,
    [bCount] tinyint NOT NULL,
    [bGLevel] tinyint NOT NULL,
    [dwDuraMax] int NOT NULL,
    [dwDuraCur] int NOT NULL,
    [bRefineCur] tinyint NOT NULL,
    [dEndTime] smalldatetime NOT NULL,
    [bGradeEffect] tinyint NOT NULL DEFAULT ((0)),
    [bMagic1] tinyint NOT NULL,
    [bMagic2] tinyint NOT NULL,
    [bMagic3] tinyint NOT NULL,
    [bMagic4] tinyint NOT NULL,
    [bMagic5] tinyint NOT NULL,
    [bMagic6] tinyint NOT NULL,
    [wValue1] smallint NOT NULL,
    [wValue2] smallint NOT NULL,
    [wValue3] smallint NOT NULL,
    [wValue4] smallint NOT NULL,
    [wValue5] smallint NOT NULL,
    [wValue6] smallint NOT NULL,
    [dwTime1] int NOT NULL,
    [dwTime2] int NOT NULL,
    [dwTime3] int NOT NULL,
    [dwTime4] int NOT NULL,
    [dwTime5] int NOT NULL,
    [dwTime6] int NOT NULL,
    [bGem] tinyint NOT NULL,
    [wMoggItemID] smallint NOT NULL,
    PRIMARY KEY ([dwSeq])
);
GO

CREATE TABLE [dbo].[TRPSGAMECHART] (
    [bType] tinyint NOT NULL,
    [bWinCount] tinyint NOT NULL,
    [dwRewardMoney] int NOT NULL,
    [wRewardItem_1] smallint NOT NULL,
    [wRewardItem_2] smallint NOT NULL,
    [bItemCount_1] tinyint NOT NULL,
    [bItemCount_2] tinyint NOT NULL,
    [bProb_Win] tinyint NOT NULL,
    [bProb_Draw] tinyint NOT NULL,
    [bProb_Lose] tinyint NOT NULL,
    [wWinKeep] smallint NOT NULL,
    [wWinPeriod] smallint NOT NULL,
    [wItemID] smallint NOT NULL DEFAULT ((0)),
    PRIMARY KEY ([bType], [bWinCount])
);
GO

CREATE TABLE [dbo].[TRPSGAMERECORDTABLE] (
    [bType] tinyint NOT NULL,
    [bWinCount] tinyint NOT NULL,
    [dWinDate] smalldatetime NOT NULL,
    [dwCharID] int NOT NULL
);
GO

CREATE TABLE [dbo].[TSAVEDCUSTOMCLOAKTABLE] (
    [wID] smallint NULL
);
GO

CREATE TABLE [dbo].[TSKILLCHART] (
    [wID] smallint NOT NULL,
    [szName] varchar(50) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [wPrevActiveID] smallint NOT NULL,
    [wParentSkillID] smallint NOT NULL,
    [wItemID] smallint NOT NULL,
    [wMaxRange] smallint NOT NULL,
    [wMinRange] smallint NOT NULL,
    [wPosture] smallint NOT NULL,
    [dwConditionID] int NOT NULL,
    [dwWeaponID] int NOT NULL,
    [dwClassID] int NOT NULL,
    [bKind] tinyint NOT NULL,
    [fPrice] float(53) NOT NULL,
    [dwUseMP] int NOT NULL,
    [bUseMPType] tinyint NOT NULL DEFAULT ((0)),
    [dwUseHP] int NOT NULL,
    [bUseHPType] tinyint NOT NULL DEFAULT ((0)),
    [dwReuseDelay] int NOT NULL,
    [nReuseDelayInc] int NOT NULL,
    [dwLoopDelay] int NOT NULL,
    [dwActionTime] int NOT NULL,
    [dwDuration] int NOT NULL DEFAULT ((0)),
    [dwDurationInc] int NOT NULL,
    [dwKindDelay] int NOT NULL,
    [dwAggro] int NOT NULL,
    [dwAggroInc] int NOT NULL DEFAULT ((0)),
    [bLevel] tinyint NOT NULL,
    [bMaxLevel] tinyint NOT NULL DEFAULT ((0)),
    [bNextLevel] tinyint NOT NULL DEFAULT ((0)),
    [bTarget] tinyint NOT NULL,
    [bTargetRange] tinyint NOT NULL,
    [bIsuse] tinyint NOT NULL,
    [bTargetHit] tinyint NOT NULL,
    [bPositive] tinyint NOT NULL,
    [bPriority] tinyint NOT NULL,
    [bSpeedApply] tinyint NOT NULL DEFAULT ((0)),
    [bCanLearn] tinyint NOT NULL,
    [bORadius] tinyint NOT NULL,
    [bIsRide] tinyint NOT NULL,
    [bIsDismount] tinyint NOT NULL,
    [wTargetActiveID] smallint NOT NULL,
    [bMaintainType] tinyint NOT NULL DEFAULT ((0)),
    [bDuraSlot] tinyint NOT NULL,
    [bCanCancel] tinyint NOT NULL,
    [bHitTest] tinyint NOT NULL,
    [bHitInit] tinyint NOT NULL,
    [bHitInc] tinyint NOT NULL,
    [bGlobal] tinyint NOT NULL DEFAULT ((0)),
    [bRadius] tinyint NOT NULL DEFAULT ((0)),
    [bStatic] tinyint NOT NULL DEFAULT ((0)),
    [bEraseAct] tinyint NOT NULL DEFAULT ((0)),
    [bEraseHide] tinyint NOT NULL DEFAULT ((0)),
    [bIsHideSkill] tinyint NOT NULL DEFAULT ((0)),
    [bRunFromServer] tinyint NOT NULL DEFAULT ((0)),
    [bCheckAttacker] tinyint NOT NULL DEFAULT ((0)),
    [wTriggerID] smallint NOT NULL DEFAULT ((0)),
    [wMapID] smallint NOT NULL DEFAULT ((0)),
    [bRepeatCount] tinyint NULL,
    PRIMARY KEY ([wID])
);
GO

CREATE TABLE [dbo].[TSKILLDATA] (
    [wSkillID] smallint NOT NULL,
    [bAction] tinyint NOT NULL,
    [bType] tinyint NOT NULL,
    [bAttr] tinyint NOT NULL,
    [bExec] tinyint NOT NULL,
    [bInc] tinyint NOT NULL,
    [wValue] smallint NOT NULL,
    [wValueInc] smallint NOT NULL DEFAULT ((0)),
    [bCalc] tinyint NOT NULL,
    PRIMARY KEY ([wSkillID], [bAction], [bType], [bAttr], [bExec])
);
GO

CREATE TABLE [dbo].[TSKILLLOG] (
    [dwCharID] int NOT NULL,
    [dwGold] int NOT NULL,
    [dwSilver] int NOT NULL,
    [dwCooper] int NOT NULL,
    [wSkill] smallint NOT NULL,
    [bLevel] tinyint NOT NULL,
    [timeInsert] smalldatetime NOT NULL
);
GO

CREATE TABLE [dbo].[TSKILLMAINTAINTABLE] (
    [dwCharID] int NOT NULL,
    [wSkillID] smallint NOT NULL,
    [bLevel] tinyint NOT NULL,
    [dwRemainTick] int NOT NULL,
    [bAttackType] tinyint NOT NULL,
    [dwAttackID] int NOT NULL,
    [bHostType] tinyint NOT NULL DEFAULT ((0)),
    [dwHostID] int NOT NULL DEFAULT ((0)),
    [bAttackCountry] tinyint NOT NULL DEFAULT ((0)),
    [fPosX] real NOT NULL DEFAULT ((0)),
    [fPosY] real NOT NULL DEFAULT ((0)),
    [fPosZ] real NOT NULL DEFAULT ((0)),
    PRIMARY KEY ([dwCharID], [wSkillID])
);
GO

CREATE TABLE [dbo].[TSKILLPOINTCHART] (
    [wID] smallint NOT NULL,
    [bLevel] tinyint NOT NULL,
    [bSkillPoint] tinyint NOT NULL,
    [bGroupPoint] tinyint NOT NULL,
    [bPrevSkillLevel] tinyint NOT NULL,
    [dwPayback] int NOT NULL DEFAULT ((0)),
    PRIMARY KEY ([wID], [bLevel])
);
GO

CREATE TABLE [dbo].[TSKILLREWARD] (
    [wID] smallint NOT NULL,
    [bLevel] tinyint NOT NULL,
    [dwMoney] int NOT NULL,
    PRIMARY KEY ([wID], [bLevel])
);
GO

CREATE TABLE [dbo].[TSKILLREWARDMONEY] (
    [dwCharID] int NOT NULL,
    [dwMoney] int NOT NULL
);
GO

CREATE TABLE [dbo].[TSKILLTABLE] (
    [dwCharID] int NOT NULL,
    [wSkillID] smallint NOT NULL,
    [bLevel] tinyint NOT NULL,
    [dwRemainTick] int NOT NULL DEFAULT ((0))
);
GO

CREATE TABLE [dbo].[TSKYGARDENTABLE] (
    [wID] smallint NOT NULL,
    [bCountry] tinyint NOT NULL,
    [dateWarTime] smalldatetime NOT NULL,
    PRIMARY KEY ([wID])
);
GO

CREATE TABLE [dbo].[TSOULMATETABLE] (
    [dwCharID] int NOT NULL,
    [dwTarget] int NOT NULL,
    [dwTime] int NOT NULL DEFAULT ((0)),
    PRIMARY KEY ([dwCharID])
);
GO

CREATE TABLE [dbo].[TSPAWNPATHCHART] (
    [wSpawnID] smallint NOT NULL,
    [bPathID] tinyint NOT NULL,
    [fPosX] real NOT NULL,
    [fPosY] real NOT NULL,
    [fPosZ] real NOT NULL,
    [bProb] tinyint NOT NULL,
    [fRadius] real NOT NULL,
    PRIMARY KEY ([wSpawnID], [bPathID])
);
GO

CREATE TABLE [dbo].[TSPAWNPOSCHART] (
    [wID] smallint NOT NULL,
    [wMapID] smallint NOT NULL,
    [fPosX] real NOT NULL,
    [fPosY] real NOT NULL,
    [fPosZ] real NOT NULL,
    [bType] tinyint NOT NULL,
    PRIMARY KEY ([wID])
);
GO

CREATE TABLE [dbo].[TSPECIALBOXCHART] (
    [wID] smallint NOT NULL,
    [wGroup] smallint NULL,
    [wItemID] smallint NULL,
    [bLevel] tinyint NULL,
    [bCount] tinyint NULL,
    [bGLevel] tinyint NULL,
    [dwDuraMax] int NULL,
    [dwDuraCur] int NULL,
    [bRefineCur] tinyint NULL,
    [bGradeEffect] tinyint NULL,
    [bMagic1] tinyint NULL,
    [bMagic2] tinyint NULL,
    [bMagic3] tinyint NULL,
    [bMagic4] tinyint NULL,
    [bMagic5] tinyint NULL,
    [bMagic6] tinyint NULL,
    [wValue1] smallint NULL,
    [wValue2] smallint NULL,
    [wValue3] smallint NULL,
    [wValue4] smallint NULL,
    [wValue5] smallint NULL,
    [wValue6] smallint NULL,
    [dwTime1] int NULL,
    [dwTime2] int NULL,
    [dwTime3] int NULL,
    [dwTime4] int NULL,
    [dwTime5] int NULL,
    [dwTime6] int NULL,
    [bGem] tinyint NULL,
    [wMoggItemID] smallint NULL,
    [wUseTime] smallint NULL,
    [bClass] varchar(1) COLLATE Latin1_General_CI_AS_KS NULL
);
GO

CREATE TABLE [dbo].[TSTARTHOTKEY] (
    [bClassID] tinyint NOT NULL,
    [bInvenID] tinyint NOT NULL,
    [bType1] tinyint NOT NULL,
    [wID1] smallint NOT NULL,
    [bType2] tinyint NOT NULL,
    [wID2] smallint NOT NULL,
    [bType3] tinyint NOT NULL,
    [wID3] smallint NOT NULL,
    [bType4] tinyint NOT NULL,
    [wID4] smallint NOT NULL,
    [bType5] tinyint NOT NULL,
    [wID5] smallint NOT NULL,
    [bType6] tinyint NOT NULL,
    [wID6] smallint NOT NULL,
    [bType7] tinyint NOT NULL,
    [wID7] smallint NOT NULL,
    [bType8] tinyint NOT NULL,
    [wID8] smallint NOT NULL,
    [bType9] tinyint NOT NULL,
    [wID9] smallint NOT NULL,
    [bType10] tinyint NOT NULL,
    [wID10] smallint NOT NULL,
    [bType11] tinyint NOT NULL,
    [wID11] smallint NOT NULL,
    [bType12] tinyint NOT NULL,
    [wID12] smallint NOT NULL
);
GO

CREATE TABLE [dbo].[TSTARTITEMCHART] (
    [bCountry] tinyint NOT NULL,
    [bClass] tinyint NOT NULL,
    [bInven] tinyint NOT NULL,
    [bSlot] tinyint NOT NULL,
    [bChartType] tinyint NOT NULL,
    [wItemID] smallint NOT NULL,
    [bCount] tinyint NOT NULL,
    PRIMARY KEY ([bCountry], [bClass], [bInven], [bSlot])
);
GO

CREATE TABLE [dbo].[TSTARTRECALL] (
    [bClassID] tinyint NOT NULL,
    [bCountryID] tinyint NOT NULL,
    [wMonID] smallint NOT NULL
);
GO

CREATE TABLE [dbo].[TSTARTSKILL] (
    [bClassID] tinyint NOT NULL,
    [wSkillID] smallint NOT NULL,
    [bLevel] tinyint NOT NULL
);
GO

CREATE TABLE [dbo].[TSVRMSGCHART] (
    [dwID] int NOT NULL,
    [szMessage] varchar(256) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    PRIMARY KEY ([dwID])
);
GO

CREATE TABLE [dbo].[TSWITCHCHART] (
    [dwSwitchID] int NOT NULL,
    [wMapID] smallint NOT NULL,
    [wPosX] smallint NOT NULL,
    [wPosY] smallint NOT NULL,
    [wPosZ] smallint NOT NULL,
    [bStart] tinyint NOT NULL,
    [bLockOnOpen] tinyint NOT NULL DEFAULT ((0)),
    [bLockOnClose] tinyint NOT NULL DEFAULT ((0)),
    [dwDuration] int NOT NULL,
    PRIMARY KEY ([dwSwitchID])
);
GO

CREATE TABLE [dbo].[TTAXTABLE] (
    [bCountry] tinyint NOT NULL,
    [wNpcID] smallint NOT NULL,
    [dwMoney] int NOT NULL
);
GO

CREATE TABLE [dbo].[TTEMPCABINETITEMTABLE] (
    [dwCharID] int NOT NULL,
    [bCabinetID] tinyint NOT NULL,
    [dwStItemID] int NOT NULL,
    [wItemID] smallint NOT NULL,
    [bLevel] tinyint NOT NULL,
    [bCount] tinyint NOT NULL,
    [bGLevel] tinyint NOT NULL DEFAULT ((0)),
    [bMagic1] tinyint NOT NULL,
    [bMagic2] tinyint NOT NULL,
    [bMagic3] tinyint NOT NULL,
    [bMagic4] tinyint NOT NULL,
    [bMagic5] tinyint NOT NULL,
    [bMagic6] tinyint NOT NULL,
    [wValue1] smallint NOT NULL,
    [wValue2] smallint NOT NULL,
    [wValue3] smallint NOT NULL,
    [wValue4] smallint NOT NULL,
    [wValue5] smallint NOT NULL,
    [wValue6] smallint NOT NULL,
    [dwTime1] int NOT NULL,
    [dwTime2] int NOT NULL,
    [dwTime3] int NOT NULL,
    [dwTime4] int NOT NULL,
    [dwTime5] int NOT NULL,
    [dwTime6] int NOT NULL,
    PRIMARY KEY ([dwCharID], [bCabinetID], [dwStItemID])
);
GO

CREATE TABLE [dbo].[TTEMPCABINETTABLE] (
    [dwCharID] int NOT NULL,
    [bCabinetID] tinyint NOT NULL,
    [bUse] tinyint NOT NULL,
    PRIMARY KEY ([dwCharID], [bCabinetID])
);
GO

CREATE TABLE [dbo].[TTEMPEXPITEMTABLE] (
    [dwCharID] int NOT NULL,
    [wItemID] smallint NOT NULL,
    [bType] tinyint NOT NULL,
    [dwRemainTime] int NOT NULL,
    [dEndTime] smalldatetime NOT NULL
);
GO

CREATE TABLE [dbo].[TTEMPINVENTABLE] (
    [dwCharID] int NOT NULL,
    [bInvenID] tinyint NOT NULL,
    [wItemID] smallint NOT NULL,
    [dEndTime] smalldatetime NOT NULL,
    [bELD] tinyint NOT NULL DEFAULT ((0)),
    PRIMARY KEY ([dwCharID], [bInvenID])
);
GO

CREATE TABLE [dbo].[TTEMPITEMTABLE] (
    [dlID] bigint NOT NULL,
    [bStorageType] tinyint NOT NULL,
    [dwStorageID] int NOT NULL,
    [bOwnerType] tinyint NOT NULL,
    [dwOwnerID] int NOT NULL,
    [bItemID] tinyint NOT NULL,
    [wItemID] smallint NOT NULL,
    [bLevel] tinyint NOT NULL,
    [bCount] tinyint NOT NULL,
    [bGLevel] tinyint NOT NULL DEFAULT ((0)),
    [dwDuraMax] int NOT NULL,
    [dwDuraCur] int NOT NULL,
    [bRefineCur] tinyint NOT NULL,
    [dEndTime] smalldatetime NOT NULL,
    [bGradeEffect] tinyint NOT NULL DEFAULT ((0)),
    [bMagic1] tinyint NOT NULL,
    [bMagic2] tinyint NOT NULL,
    [bMagic3] tinyint NOT NULL,
    [bMagic4] tinyint NOT NULL,
    [bMagic5] tinyint NOT NULL,
    [bMagic6] tinyint NOT NULL,
    [wValue1] smallint NOT NULL,
    [wValue2] smallint NOT NULL,
    [wValue3] smallint NOT NULL,
    [wValue4] smallint NOT NULL,
    [wValue5] smallint NOT NULL,
    [wValue6] smallint NOT NULL,
    [dwTime1] int NOT NULL,
    [dwTime2] int NOT NULL,
    [dwTime3] int NOT NULL,
    [dwTime4] int NOT NULL,
    [dwTime5] int NOT NULL,
    [dwTime6] int NOT NULL,
    [bGem] tinyint NOT NULL,
    [wMoggItemID] smallint NOT NULL
);
GO

CREATE TABLE [dbo].[TTEMPITEMUSEDTABLE] (
    [dwCharID] int NOT NULL,
    [wDelayGroupID] smallint NOT NULL,
    [dwTick] int NOT NULL,
    PRIMARY KEY ([dwCharID], [wDelayGroupID])
);
GO

CREATE TABLE [dbo].[TTEMPSKILLMAINTAINTABLE] (
    [dwCharID] int NOT NULL,
    [wSkillID] smallint NOT NULL,
    [bLevel] tinyint NOT NULL,
    [dwRemainTick] int NOT NULL,
    [bAttackType] tinyint NOT NULL,
    [dwAttackID] int NOT NULL,
    [bHostType] tinyint NOT NULL DEFAULT ((0)),
    [dwHostID] int NOT NULL DEFAULT ((0)),
    [bAttackCountry] tinyint NOT NULL DEFAULT ((0)),
    [fPosX] real NOT NULL DEFAULT ((0)),
    [fPosY] real NOT NULL DEFAULT ((0)),
    [fPosZ] real NOT NULL DEFAULT ((0)),
    PRIMARY KEY ([dwCharID], [wSkillID])
);
GO

CREATE TABLE [dbo].[TTEMPSKILLTABLE] (
    [dwCharID] int NOT NULL,
    [wSkillID] smallint NOT NULL,
    [bLevel] tinyint NOT NULL,
    [dwRemainTick] int NOT NULL,
    PRIMARY KEY ([dwCharID], [wSkillID])
);
GO

CREATE TABLE [dbo].[TTITLECHART] (
    [wTitleID] smallint NOT NULL,
    [wCategory] smallint NOT NULL,
    [strTitle] text COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [bKind] tinyint NOT NULL,
    [dwRequirement] int NOT NULL,
    PRIMARY KEY ([wTitleID])
);
GO

CREATE TABLE [dbo].[TTITLETABLE] (
    [dwCharID] int NOT NULL,
    [wTitleID] smallint NOT NULL,
    [bSelected] tinyint NOT NULL,
    PRIMARY KEY ([dwCharID], [wTitleID])
);
GO

CREATE TABLE [dbo].[TTNMTEVENTREWARDTABLE] (
    [wID] smallint IDENTITY(1,1) NOT NULL,
    [wTournamentID] smallint NOT NULL,
    [bEntryID] tinyint NOT NULL,
    [dwClass] int NOT NULL,
    [bCheckShield] tinyint NOT NULL,
    [bChartType] tinyint NOT NULL,
    [wItemID] smallint NOT NULL,
    [bItemCount] tinyint NOT NULL,
    PRIMARY KEY ([wID])
);
GO

CREATE TABLE [dbo].[TTNMTEVENTSCHEDULETABLE] (
    [wTournamentID] smallint NOT NULL,
    [bStep] tinyint NOT NULL,
    [dwPeriod] int NOT NULL,
    PRIMARY KEY ([wTournamentID], [bStep])
);
GO

CREATE TABLE [dbo].[TTNMTEVENTTABLE] (
    [wTournamentID] smallint NOT NULL,
    [bEntryID] tinyint NOT NULL,
    [szName] varchar(50) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [bType] tinyint NOT NULL,
    [dwClass] int NOT NULL,
    [dwFee] int NOT NULL,
    [dwFeeBack] int NOT NULL,
    [wItemID] smallint NOT NULL,
    [bItemCount] tinyint NOT NULL,
    [bMaxLevel] tinyint NOT NULL,
    [bMinLevel] tinyint NOT NULL,
    PRIMARY KEY ([wTournamentID], [bEntryID])
);
GO

CREATE TABLE [dbo].[TTNMTEVENTTIMETABLE] (
    [wTournamentID] smallint NOT NULL,
    [bWeek] tinyint NOT NULL,
    [bDay] tinyint NOT NULL,
    [dwStart] int NOT NULL,
    PRIMARY KEY ([wTournamentID])
);
GO

CREATE TABLE [dbo].[TTOURNAMENTCHART] (
    [bEntryID] tinyint NOT NULL,
    [szName] varchar(50) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [bType] tinyint NOT NULL,
    [dwClass] int NOT NULL,
    [dwFee] int NOT NULL,
    [dwFeeBack] int NOT NULL,
    [wItemID] smallint NOT NULL,
    [bItemCount] tinyint NOT NULL,
    [bEnable] tinyint NOT NULL DEFAULT ((0)),
    [bGroup] tinyint NOT NULL DEFAULT ((0)),
    PRIMARY KEY ([bEntryID])
);
GO

CREATE TABLE [dbo].[TTOURNAMENTPLAYERTABLE] (
    [dwCharID] int NOT NULL,
    [dwChiefID] int NOT NULL,
    [bEntry] tinyint NOT NULL,
    [bStep] tinyint NOT NULL,
    [bResult] tinyint NOT NULL,
    [szHWID] varchar(55) COLLATE Latin1_General_CI_AS_KS NULL,
    [dwIPAddr] int NULL,
    PRIMARY KEY ([dwCharID])
);
GO

CREATE TABLE [dbo].[TTOURNAMENTREWARDCHART] (
    [wID] smallint NOT NULL,
    [bEntryID] tinyint NOT NULL,
    [dwClass] int NOT NULL,
    [bCheckShield] tinyint NOT NULL,
    [bChartType] tinyint NOT NULL,
    [wItemID] smallint NOT NULL,
    [bItemCount] tinyint NOT NULL,
    PRIMARY KEY ([wID])
);
GO

CREATE TABLE [dbo].[TTOURNAMENTREWARDCHART_weap] (
    [wID] smallint NOT NULL,
    [bEntryID] tinyint NOT NULL,
    [dwClass] int NOT NULL,
    [bCheckShield] tinyint NOT NULL,
    [bChartType] tinyint NOT NULL,
    [wItemID] smallint NOT NULL,
    [bItemCount] tinyint NOT NULL,
    PRIMARY KEY ([wID])
);
GO

CREATE TABLE [dbo].[TTOURNAMENTSCHEDULECHART] (
    [bStep] tinyint NOT NULL,
    [szName] varchar(50) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [dwPeriod] int NOT NULL,
    [bGroup] tinyint NOT NULL,
    PRIMARY KEY ([bStep], [bGroup])
);
GO

CREATE TABLE [dbo].[TTOURNAMENTSTATUSTABLE] (
    [wID] smallint NOT NULL,
    [bGroup] tinyint NOT NULL,
    [bStep] tinyint NOT NULL
);
GO

CREATE TABLE [dbo].[TUNIFYPET] (
    [dwUserID] int NOT NULL,
    [wPetID] smallint NOT NULL,
    [dwSecond] int NOT NULL,
    PRIMARY KEY ([dwUserID], [wPetID])
);
GO

CREATE TABLE [dbo].[TUNITCHART] (
    [bGroup] tinyint NOT NULL,
    [bServerID] tinyint NOT NULL DEFAULT ((0)),
    [wMapID] smallint NOT NULL,
    [wUnitID] smallint NOT NULL,
    PRIMARY KEY ([bGroup], [bServerID], [wMapID], [wUnitID])
);
GO

CREATE TABLE [dbo].[charkilling_log] (
    [dwKillerID] int NULL,
    [dwTargetID] int NULL,
    [date] smalldatetime NULL DEFAULT (getdate())
);
GO

CREATE TABLE [dbo].[dtproperties] (
    [id] int IDENTITY(1,1) NOT NULL,
    [objectid] int NULL,
    [property] varchar(64) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [value] varchar(255) COLLATE Latin1_General_CI_AS_KS NULL,
    [uvalue] nvarchar(255) COLLATE Latin1_General_CI_AS_KS NULL,
    [lvalue] image NULL,
    [version] int NOT NULL DEFAULT ((0)),
    PRIMARY KEY ([id], [property])
);
GO

CREATE TABLE [tgame].[TLASTMONTHPOINTTABLE] (
    [dwCharID] int NOT NULL,
    [dwRank] int IDENTITY(1,1) NOT NULL,
    [dwPoint] int NOT NULL,
    [wWin] smallint NOT NULL,
    [wLose] smallint NOT NULL,
    PRIMARY KEY ([dwCharID])
);
GO

CREATE TABLE [tgame].[TLASTTOTALPOINTTABLE] (
    [dwCharID] int NOT NULL,
    [dwRank] int IDENTITY(1,1) NOT NULL,
    [dwPoint] int NOT NULL,
    [dwWin] int NOT NULL,
    [dwLose] int NOT NULL,
    PRIMARY KEY ([dwCharID])
);
GO
