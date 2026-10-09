-- CONFIRMED table metadata from this backup; no module behavior or rows.
-- Apply ONLY to a new empty disposable database, never an existing database.

CREATE TABLE [dbo].[IPBLACKLIST] (
    [szIP] varchar(50) COLLATE Latin1_General_CI_AS_KS NULL
);
GO

CREATE TABLE [dbo].[IPBLACKLIST_game] (
    [szIP] varchar(50) COLLATE Latin1_General_CI_AS_KS NULL
);
GO

CREATE TABLE [dbo].[TACCOUNT] (
    [dwUserID] int IDENTITY(1,1) NOT NULL,
    [szUserID] varchar(50) COLLATE Korean_Wansung_CI_AS NULL,
    [szPasswd] varchar(50) COLLATE Korean_Wansung_CI_AS NULL,
    [bCheck] tinyint NULL DEFAULT (0),
    [dFirstLogin] smalldatetime NULL,
    [dLastLogin] smalldatetime NULL,
    PRIMARY KEY ([dwUserID])
);
GO

CREATE TABLE [dbo].[TACCOUNT_PW] (
    [szUserID] varchar(50) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [szPasswd] varchar(255) COLLATE Latin1_General_CI_AS_KS NULL,
    [bCheck] tinyint NULL DEFAULT ((0)),
    [dFirstLogin] smalldatetime NULL,
    [dLastLogin] smalldatetime NULL,
    [dwUserID] int IDENTITY(1,1) NOT NULL,
    [dwChanged] int NULL,
    [dwPWKey] text COLLATE Latin1_General_CI_AS_KS NULL,
    [szRealUsername] varchar(50) COLLATE Latin1_General_CI_AS_KS NULL,
    [szOrigin] varchar(4) COLLATE Latin1_General_CI_AS_KS NULL,
    [dwEMKey] text COLLATE Latin1_General_CI_AS_KS NULL,
    PRIMARY KEY ([dwUserID])
);
GO

CREATE TABLE [dbo].[TALLCHARTABLE] (
    [dwSeq] int IDENTITY(1,1) NOT NULL,
    [dwUserID] int NOT NULL,
    [bWorldID] tinyint NOT NULL,
    [dwCharID] int NOT NULL,
    [bSlot] tinyint NOT NULL,
    [szName] varchar(50) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [bClass] tinyint NOT NULL,
    [bRace] tinyint NOT NULL,
    [bCountry] tinyint NOT NULL,
    [bSex] tinyint NOT NULL,
    [bHair] tinyint NOT NULL,
    [bFace] tinyint NOT NULL,
    [bBody] tinyint NOT NULL,
    [bPants] tinyint NOT NULL,
    [bHand] tinyint NOT NULL,
    [bFoot] tinyint NOT NULL,
    [bLevel] tinyint NOT NULL,
    [dwEXP] int NOT NULL,
    [bDelete] tinyint NOT NULL DEFAULT ((0)),
    [dCreateDate] datetime NOT NULL DEFAULT (getdate()),
    [dDeleteDate] datetime NULL,
    [dLoginDate] datetime NULL,
    [dLogoutDate] datetime NULL,
    [dwPlayTime] int NOT NULL DEFAULT ((0)),
    [dwGold] int NOT NULL DEFAULT ((0)),
    [dwSilver] int NOT NULL DEFAULT ((0)),
    [dwCooper] int NOT NULL DEFAULT ((0)),
    [isOnline] int NULL,
    [Ban] int NULL,
    [BanReason] text COLLATE Latin1_General_CI_AS_KS NULL,
    PRIMARY KEY ([dwSeq])
);
GO

CREATE TABLE [dbo].[TALLCHARTABLE_TRIGGER] (
    [szDBOP] char(1) COLLATE Korean_Wansung_CI_AS NOT NULL,
    [dwSeq] int NOT NULL,
    [dOPDate] datetime NOT NULL DEFAULT (getdate())
);
GO

CREATE TABLE [dbo].[TALLLOCALTABLE] (
    [dDate] smalldatetime NOT NULL,
    [bWorld] tinyint NOT NULL,
    [wLocalID] smallint NOT NULL,
    [szLocalName] varchar(50) COLLATE Korean_Wansung_CI_AS NOT NULL,
    [bCountry] tinyint NULL,
    [dwGuild] int NULL,
    [szGuildName] varchar(50) COLLATE Korean_Wansung_CI_AS NULL,
    [dOccupy] smalldatetime NULL,
    [dDefend] smalldatetime NULL,
    PRIMARY KEY ([dDate], [bWorld], [wLocalID])
);
GO

CREATE TABLE [dbo].[TALLRANKGUILDTABLE] (
    [dwSeq] int IDENTITY(1,1) NOT NULL,
    [dDate] smalldatetime NOT NULL,
    [bRankType] tinyint NOT NULL,
    [bWorld] tinyint NOT NULL,
    [dwGuildSeq] int NOT NULL,
    [szGuildName] varchar(50) COLLATE Korean_Wansung_CI_AS NOT NULL,
    [dwChief] int NOT NULL,
    [szChiefName] varchar(50) COLLATE Korean_Wansung_CI_AS NOT NULL,
    [bLevel] tinyint NOT NULL,
    [dwExp] int NOT NULL,
    [dwMemberCount] int NOT NULL,
    [timeEstablish] smalldatetime NOT NULL,
    [dwPlayTime] int NOT NULL,
    [nRank] int NULL,
    [nRankUpDown] int NULL
);
GO

CREATE TABLE [dbo].[TALLRANKTABLE] (
    [dwSeq] int IDENTITY(1,1) NOT NULL,
    [dDate] smalldatetime NOT NULL,
    [bWorld] tinyint NOT NULL,
    [bRankType] tinyint NOT NULL,
    [dwUserID] int NULL,
    [dwCharID] int NOT NULL,
    [nRank] int NOT NULL,
    [nRankUpDown] int NULL,
    [szCharName] varchar(50) COLLATE Korean_Wansung_CI_AS NOT NULL,
    [bCountry] tinyint NOT NULL,
    [bClass] tinyint NOT NULL,
    [bRace] tinyint NOT NULL,
    [bSex] tinyint NOT NULL,
    [bLevel] int NOT NULL,
    PRIMARY KEY ([dDate], [bWorld], [bRankType], [dwCharID])
);
GO

CREATE TABLE [dbo].[TCABINETITEMCHART] (
    [bWorldID] tinyint NOT NULL,
    [dwCharID] int NOT NULL,
    [dwPostID] int NOT NULL,
    [wItemID] smallint NOT NULL,
    [bLevel] tinyint NOT NULL,
    [bCount] tinyint NOT NULL,
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

CREATE TABLE [dbo].[TCASHBONUSITEMCHART] (
    [wCashItemID] smallint NOT NULL,
    [wBonusItem] smallint NOT NULL,
    [bBonusItemCount] tinyint NOT NULL,
    PRIMARY KEY ([wCashItemID], [wBonusItem])
);
GO

CREATE TABLE [dbo].[TCASHCATEGORYCHART] (
    [bID] tinyint NOT NULL,
    [szName] nvarchar(50) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [wOrder] smallint NOT NULL,
    PRIMARY KEY ([bID])
);
GO

CREATE TABLE [dbo].[TCASHGAMBLECHART] (
    [dwID] int NOT NULL,
    [wItemID] smallint NOT NULL,
    [bCount] tinyint NOT NULL,
    [dwProb] int NOT NULL,
    [bLevel] tinyint NOT NULL,
    [bGLevel] tinyint NOT NULL,
    [bGradeEffect] tinyint NOT NULL,
    [bMagic1] tinyint NOT NULL,
    [wValue1] smallint NOT NULL,
    [dwTime1] int NOT NULL,
    [bMagic2] tinyint NOT NULL,
    [wValue2] smallint NOT NULL,
    [dwTime2] int NOT NULL,
    [bMagic3] tinyint NOT NULL,
    [wValue3] smallint NOT NULL,
    [dwTime3] int NOT NULL,
    [bMagic4] tinyint NOT NULL,
    [wValue4] smallint NOT NULL,
    [dwTime4] int NOT NULL,
    [bMagic5] tinyint NOT NULL,
    [wValue5] smallint NOT NULL,
    [dwTime5] int NOT NULL,
    [bMagic6] tinyint NOT NULL,
    [wValue6] smallint NOT NULL,
    [dwTime6] int NOT NULL,
    [wGroup] smallint NOT NULL DEFAULT ((0)),
    [wUseTime] smallint NOT NULL DEFAULT ((0)),
    [dwDuraMax] int NOT NULL DEFAULT ((0)),
    [dwDuraCur] int NOT NULL DEFAULT ((0)),
    [bRefineCur] tinyint NOT NULL DEFAULT ((0)),
    [bGem] tinyint NULL,
    [wMoggItemID] smallint NULL,
    PRIMARY KEY ([dwID])
);
GO

CREATE TABLE [dbo].[TCASHITEMBUYTABLE] (
    [dDate] smalldatetime NOT NULL,
    [dwUserID] int NOT NULL,
    [wItemID] smallint NOT NULL,
    [bCount] tinyint NOT NULL,
    [bChartType] int NOT NULL
);
GO

CREATE TABLE [dbo].[TCASHITEMCABINETTABLE] (
    [dwID] int IDENTITY(1,1) NOT NULL,
    [dwUserID] int NOT NULL,
    [wItemID] smallint NOT NULL,
    [bLevel] tinyint NOT NULL,
    [bCount] tinyint NOT NULL,
    [bGLevel] tinyint NOT NULL,
    [dwDuraMax] int NOT NULL,
    [dwDuraCur] int NOT NULL,
    [bRefineCur] tinyint NOT NULL,
    [dEndTime] smalldatetime NOT NULL,
    [bGradeEffect] tinyint NOT NULL DEFAULT (0),
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
    [bWorldID] smallint NOT NULL DEFAULT (0),
    [dlID] bigint NOT NULL DEFAULT (0),
    [bGem] tinyint NOT NULL,
    [wMoggItemID] smallint NOT NULL,
    PRIMARY KEY ([dwID])
);
GO

CREATE TABLE [dbo].[TCASHITEMCABINETTABLE_PW] (
    [dwID] int IDENTITY(1,1) NOT NULL,
    [dwUserID] int NOT NULL,
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
    [bWorldID] smallint NOT NULL DEFAULT ((0)),
    [dlID] bigint NOT NULL DEFAULT ((0)),
    PRIMARY KEY ([dwID])
);
GO

CREATE TABLE [dbo].[TCASHITEMCABINETTABLE_temp] (
    [dwID] int IDENTITY(1,1) NOT NULL,
    [dwUserID] int NOT NULL,
    [wItemID] smallint NOT NULL,
    [bLevel] tinyint NOT NULL,
    [bCount] tinyint NOT NULL,
    [bGLevel] tinyint NOT NULL,
    [dwDuraMax] int NOT NULL,
    [dwDuraCur] int NOT NULL,
    [bRefineCur] tinyint NOT NULL,
    [dEndTime] smalldatetime NOT NULL,
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
    [bWorldID] smallint NOT NULL,
    [dlID] bigint NOT NULL
);
GO

CREATE TABLE [dbo].[TCASHITEMINFOCHART] (
    [wCashItemID] smallint NOT NULL,
    [szName] varchar(50) COLLATE Korean_Wansung_CI_AS NOT NULL DEFAULT (''),
    [szUseTime] varchar(50) COLLATE Korean_Wansung_CI_AS NOT NULL,
    [szExplain] varchar(500) COLLATE Korean_Wansung_CI_AS NOT NULL,
    [bSellType] tinyint NOT NULL,
    PRIMARY KEY ([wCashItemID])
);
GO

CREATE TABLE [dbo].[TCASHITEMPRICETABLE] (
    [bPackage] tinyint NOT NULL,
    [wItemID] smallint NOT NULL,
    [bAmount] tinyint NOT NULL,
    [fPrice] float(53) NOT NULL
);
GO

CREATE TABLE [dbo].[TCASHSHOPITEMCHART] (
    [wID] smallint NOT NULL,
    [szName] varchar(50) COLLATE Korean_Wansung_CI_AS NOT NULL,
    [dwMoney] int NOT NULL,
    [wItemID] smallint NOT NULL,
    [wInfoID] int NOT NULL,
    [bLevel] tinyint NOT NULL,
    [bCount] tinyint NOT NULL,
    [bGLevel] tinyint NOT NULL DEFAULT (0),
    [dwDuraMax] int NOT NULL DEFAULT (0),
    [dwDuraCur] int NOT NULL DEFAULT (0),
    [bRefineCur] tinyint NOT NULL DEFAULT (0),
    [wUseTime] smallint NOT NULL DEFAULT (0),
    [bGradeEffect] tinyint NOT NULL DEFAULT (0),
    [bMagic1] tinyint NOT NULL DEFAULT (0),
    [bMagic2] tinyint NOT NULL DEFAULT (0),
    [bMagic3] tinyint NOT NULL DEFAULT (0),
    [bMagic4] tinyint NOT NULL DEFAULT (0),
    [bMagic5] tinyint NOT NULL DEFAULT (0),
    [bMagic6] tinyint NOT NULL DEFAULT (0),
    [wValue1] smallint NOT NULL DEFAULT (0),
    [wValue2] smallint NOT NULL DEFAULT (0),
    [wValue3] smallint NOT NULL DEFAULT (0),
    [wValue4] smallint NOT NULL DEFAULT (0),
    [wValue5] smallint NOT NULL DEFAULT (0),
    [wValue6] smallint NOT NULL DEFAULT (0),
    [dwTime1] int NOT NULL DEFAULT (0),
    [dwTime2] int NOT NULL DEFAULT (0),
    [dwTime3] int NOT NULL DEFAULT (0),
    [dwTime4] int NOT NULL DEFAULT (0),
    [dwTime5] int NOT NULL DEFAULT (0),
    [dwTime6] int NOT NULL DEFAULT (0),
    [bCanSell] tinyint NOT NULL,
    [bCategory] tinyint NOT NULL DEFAULT (0),
    [bKind] tinyint NOT NULL DEFAULT (0),
    [wOrder] smallint NOT NULL DEFAULT (0),
    [bSaleValue] tinyint NOT NULL DEFAULT (0),
    [dwQuantity] int NOT NULL DEFAULT (0),
    [wIconID] smallint NULL,
    [bLimitedType] tinyint NULL,
    [dLimitedEnd] smalldatetime NULL,
    [bItemCountry] tinyint NULL,
    [dwClassID] int NULL,
    PRIMARY KEY ([wID])
);
GO

CREATE TABLE [dbo].[TCASHTESTTABLE] (
    [dwUserID] int NOT NULL,
    [dwCash] int NOT NULL,
    [dwBonus] int NOT NULL,
    [isPosted] datetime NULL,
    [byWho] text COLLATE Latin1_General_CI_AS_KS NULL,
    PRIMARY KEY ([dwUserID])
);
GO

CREATE TABLE [dbo].[TCHANNEL] (
    [bGroupID] tinyint NOT NULL,
    [bChannel] tinyint NOT NULL,
    [szNAME] varchar(50) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [wFull] smallint NOT NULL DEFAULT ((5000)),
    [wBusy] smallint NOT NULL DEFAULT ((500)),
    [bStatus] tinyint NOT NULL DEFAULT ((0)),
    PRIMARY KEY ([bChannel], [bGroupID])
);
GO

CREATE TABLE [dbo].[TCURRENTUSER] (
    [dwKEY] int IDENTITY(1,1) NOT NULL,
    [dwUserID] int NOT NULL,
    [dwCharID] int NOT NULL,
    [bGroupID] tinyint NOT NULL,
    [bChannel] tinyint NOT NULL,
    [szIPAddr] varchar(50) COLLATE Latin1_General_CI_AS_KS NULL,
    [wPort] smallint NOT NULL,
    [bLocked] tinyint NOT NULL,
    [dLoginDate] datetime NOT NULL DEFAULT (getdate()),
    [dEnterDate] datetime NOT NULL DEFAULT (getdate()),
    [dwPcBangID] int NOT NULL DEFAULT ((0)),
    [bLuckyNumber] tinyint NOT NULL DEFAULT ((0)),
    [szLoginIP] varchar(50) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    PRIMARY KEY ([dwKEY])
);
GO

CREATE TABLE [dbo].[TDURINGITEMTABLE] (
    [dwUserID] int NOT NULL,
    [wItemID] smallint NOT NULL,
    [bType] tinyint NOT NULL,
    [dwRemainTime] int NOT NULL,
    [dEndTime] smalldatetime NOT NULL,
    PRIMARY KEY ([dwUserID], [wItemID])
);
GO

CREATE TABLE [dbo].[TEMPUSERID] (
    [dwCharID] int NOT NULL
);
GO

CREATE TABLE [dbo].[TEVENT042501TL] (
    [USER010_Serial] int NOT NULL,
    [EVENT042501_Cupon] smallint NOT NULL
);
GO

CREATE TABLE [dbo].[TEVENTCHART] (
    [dwIndex] int NOT NULL,
    [bID] tinyint NOT NULL,
    [szTitle] varchar(256) COLLATE Korean_Wansung_CI_AS NOT NULL,
    [bGroupID] tinyint NOT NULL,
    [bSvrType] tinyint NOT NULL,
    [bSvrID] tinyint NOT NULL,
    [dStartDate] smalldatetime NOT NULL,
    [dEndDate] smalldatetime NOT NULL,
    [wValue] smallint NOT NULL,
    [dwStartAlarm] int NOT NULL,
    [dwEndAlarm] int NOT NULL,
    [szStartMsg] varchar(1024) COLLATE Korean_Wansung_CI_AS NOT NULL,
    [szEndMsg] varchar(1024) COLLATE Korean_Wansung_CI_AS NOT NULL,
    [szValue] varchar(1024) COLLATE Korean_Wansung_CI_AS NOT NULL,
    [wMapID] smallint NOT NULL DEFAULT (0),
    [szMidMsg] varchar(1024) COLLATE Korean_Wansung_CI_AS NOT NULL DEFAULT (''),
    [bPartTime] tinyint NOT NULL DEFAULT (0),
    PRIMARY KEY ([dwIndex])
);
GO

CREATE TABLE [dbo].[TEVENTGIVEERROR] (
    [bWorld] tinyint NOT NULL,
    [dwCharID] int NOT NULL,
    [wCount] smallint NOT NULL
);
GO

CREATE TABLE [dbo].[TEVENTHORSE] (
    [bWorld] tinyint NOT NULL,
    [dwCharID] int NOT NULL,
    [bCount] tinyint NOT NULL
);
GO

CREATE TABLE [dbo].[TGROUP] (
    [bGroupID] tinyint NOT NULL,
    [bType] tinyint NOT NULL,
    [szNAME] varchar(50) COLLATE Korean_Wansung_CI_AS NOT NULL,
    [szDSN] varchar(50) COLLATE Korean_Wansung_CI_AS NOT NULL,
    [szUserID] varchar(50) COLLATE Korean_Wansung_CI_AS NOT NULL,
    [szPasswd] varchar(255) COLLATE Latin1_General_BIN NULL,
    [wFull] smallint NOT NULL DEFAULT (5000),
    [wBusy] smallint NOT NULL DEFAULT (500),
    [dwMaxUser] int NOT NULL DEFAULT (0),
    [bUseRate] tinyint NOT NULL DEFAULT (1),
    [bStatus] tinyint NOT NULL DEFAULT (0),
    PRIMARY KEY ([bGroupID])
);
GO

CREATE TABLE [dbo].[TIPADDR] (
    [bMachineID] tinyint NOT NULL,
    [szIPAddr] varchar(50) COLLATE Korean_Wansung_CI_AS NOT NULL,
    [szPriAddr] varchar(50) COLLATE Korean_Wansung_CI_AS NOT NULL DEFAULT (''),
    [bActive] tinyint NOT NULL,
    PRIMARY KEY ([bMachineID], [szIPAddr])
);
GO

CREATE TABLE [dbo].[TIPAUTHORITY] (
    [szIP] varchar(50) COLLATE Korean_Wansung_CI_AS NOT NULL,
    [bAuthority] tinyint NOT NULL,
    PRIMARY KEY ([szIP])
);
GO

CREATE TABLE [dbo].[TITEMCHART] (
    [wItemID] smallint NOT NULL,
    [bType] tinyint NOT NULL,
    [bKind] tinyint NOT NULL,
    [szNAME] varchar(50) COLLATE Korean_Wansung_CI_AS NOT NULL,
    [wAttrID] smallint NOT NULL,
    [wUseValue] smallint NOT NULL,
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
    [bItemCountry] tinyint NOT NULL,
    [bIsSpecial] tinyint NOT NULL,
    [dwDelay] int NOT NULL,
    [fRevision] float(53) NOT NULL,
    [fMRevision] float(53) NOT NULL,
    [fAtRate] float(53) NOT NULL,
    [fMAtRate] float(53) NOT NULL,
    [bCanGamble] tinyint NOT NULL,
    [wItemProb_G] smallint NOT NULL,
    [bDestroyProb] tinyint NOT NULL,
    [bGambleProb] tinyint NOT NULL,
    [dwDuraMax] int NOT NULL,
    [bRefineMax] tinyint NOT NULL,
    [bCanRepair] tinyint NOT NULL,
    [wDelayGroupID] smallint NOT NULL,
    [bCanColor] tinyint NOT NULL DEFAULT (0)
);
GO

CREATE TABLE [dbo].[TKEEPINGNAME] (
    [dwSeq] int IDENTITY(1,1) NOT NULL,
    [szName] nvarchar(50) COLLATE Korean_Wansung_CI_AS NOT NULL
);
GO

CREATE TABLE [dbo].[TLIMITEDLEVELCHART] (
    [bMaxLevel] tinyint NOT NULL,
    [bNation] tinyint NOT NULL DEFAULT (0)
);
GO

CREATE TABLE [dbo].[TLOG] (
    [dwKEY] int NOT NULL,
    [dwUserID] int NOT NULL,
    [dwCharID] int NOT NULL,
    [bGroupID] tinyint NOT NULL,
    [bChannel] tinyint NOT NULL,
    [timeLOGIN] smalldatetime NOT NULL,
    [timeLOGOUT] smalldatetime NOT NULL,
    PRIMARY KEY ([dwKEY])
);
GO

CREATE TABLE [dbo].[TLogTest] (
    [dwID] int IDENTITY(1,1) NOT NULL,
    [szLOG] varchar(255) COLLATE Latin1_General_CI_AS_KS NULL,
    PRIMARY KEY ([dwID])
);
GO

CREATE TABLE [dbo].[TMACHINE] (
    [bMachineID] tinyint NOT NULL,
    [szNAME] varchar(50) COLLATE Korean_Wansung_CI_AS NOT NULL,
    [bRouteID] tinyint NOT NULL,
    PRIMARY KEY ([bMachineID])
);
GO

CREATE TABLE [dbo].[TMANAGER] (
    [szID] varchar(50) COLLATE Korean_Wansung_CI_AS NOT NULL,
    [szPasswd] varchar(50) COLLATE Korean_Wansung_CI_AS NOT NULL,
    [bOPAuthority] tinyint NOT NULL DEFAULT (1),
    [bAuthority] tinyint NOT NULL,
    [szName] varchar(50) COLLATE Korean_Wansung_CI_AS NULL,
    [szPhoneNum] varchar(50) COLLATE Korean_Wansung_CI_AS NULL,
    [szOpratorCharID] varchar(50) COLLATE Korean_Wansung_CI_AS NULL,
    [dCreateDate] smalldatetime NULL,
    PRIMARY KEY ([szID])
);
GO

CREATE TABLE [dbo].[TMANAGERLOG] (
    [dwSeq] int IDENTITY(1,1) NOT NULL,
    [dDate] datetime NOT NULL DEFAULT (getdate()),
    [szIP] varchar(15) COLLATE Korean_Wansung_CI_AS NOT NULL,
    [szGMID] varchar(15) COLLATE Korean_Wansung_CI_AS NOT NULL,
    [szCommand] varchar(20) COLLATE Korean_Wansung_CI_AS NOT NULL,
    [szLog] nvarchar(4000) COLLATE Korean_Wansung_CI_AS NOT NULL,
    PRIMARY KEY ([dwSeq])
);
GO

CREATE TABLE [dbo].[TMONSTERCHART] (
    [wID] smallint NOT NULL,
    [szName] varchar(50) COLLATE Korean_Wansung_CI_AS NOT NULL,
    [bRace] tinyint NOT NULL,
    [bClass] tinyint NOT NULL,
    [wKind] smallint NOT NULL,
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
    [bCall] tinyint NOT NULL,
    [bIsSpecial] tinyint NOT NULL,
    [bRemove] tinyint NOT NULL,
    [wMonAttr] smallint NOT NULL,
    [wSummonAttr] smallint NOT NULL,
    [wTransSkillID] smallint NOT NULL,
    [fSize] float(53) NOT NULL,
    [wSkill1] smallint NOT NULL,
    [wSkill2] smallint NOT NULL,
    [wSkill3] smallint NOT NULL,
    [wSkill4] smallint NOT NULL
);
GO

CREATE TABLE [dbo].[TNETWORK] (
    [bMachineID] tinyint NOT NULL,
    [szNetwork] varchar(50) COLLATE Korean_Wansung_CI_AS NOT NULL,
    PRIMARY KEY ([bMachineID])
);
GO

CREATE TABLE [dbo].[TPCBANGMASTERTABLE] (
    [dwPcBangID] int NOT NULL,
    [dwUserID] int NOT NULL,
    [dWorld1] smalldatetime NULL,
    [dWorld2] smalldatetime NULL,
    [dWorld3] smalldatetime NULL,
    [dWorld4] smalldatetime NULL,
    PRIMARY KEY ([dwPcBangID])
);
GO

CREATE TABLE [dbo].[TPCBANGPLAYTABLE] (
    [dwUserID] int NOT NULL,
    [dwPlayDate] int NOT NULL,
    [dwPlayTime] int NOT NULL,
    [bItemCnt] tinyint NOT NULL DEFAULT (0),
    PRIMARY KEY ([dwUserID], [dwPlayDate])
);
GO

CREATE TABLE [dbo].[TPREVERSION] (
    [dwBetaVer] int NOT NULL,
    [szPath] varchar(260) COLLATE Korean_Wansung_CI_AS NOT NULL,
    [szName] varchar(260) COLLATE Korean_Wansung_CI_AS NOT NULL,
    [dwSize] int NOT NULL,
    PRIMARY KEY ([dwBetaVer])
);
GO

CREATE TABLE [dbo].[TREGIONCHART] (
    [dwID] int NOT NULL,
    [szName] varchar(50) COLLATE Korean_Wansung_CI_AS NOT NULL,
    [wCountryID] smallint NOT NULL,
    [bCanfly] tinyint NOT NULL,
    [bLocal] tinyint NOT NULL,
    [fDPosX] float(53) NOT NULL,
    [fDPosY] float(53) NOT NULL,
    [fDPosZ] float(53) NOT NULL,
    [fCPosX] float(53) NOT NULL,
    [fCPosY] float(53) NOT NULL,
    [fCPosZ] float(53) NOT NULL,
    [fBPosX] float(53) NOT NULL,
    [fBPosY] float(53) NOT NULL,
    [fBPosZ] float(53) NOT NULL,
    [bCanMail] tinyint NOT NULL,
    PRIMARY KEY ([dwID])
);
GO

CREATE TABLE [dbo].[TRESERVEDNAME] (
    [dwSeq] int IDENTITY(1,1) NOT NULL,
    [dwUserID] int NOT NULL,
    [szName] varchar(50) COLLATE Korean_Wansung_CI_AS NOT NULL,
    PRIMARY KEY ([szName])
);
GO

CREATE TABLE [dbo].[TSECURECODE] (
    [strSecurityCode] varchar(55) COLLATE Latin1_General_CI_AS_KS NULL,
    [bEnabled] int NULL,
    [bTries] int NULL,
    [iLockTick] int NULL,
    [dwUserID] int NULL
);
GO

CREATE TABLE [dbo].[TSERVER] (
    [bGroupID] tinyint NOT NULL,
    [bServerID] tinyint NOT NULL,
    [bType] tinyint NOT NULL,
    [bMachineID] tinyint NOT NULL,
    [wPort] smallint NOT NULL,
    [szName] varchar(50) COLLATE Latin1_General_CI_AS_KS NOT NULL DEFAULT ('')
);
GO

CREATE TABLE [dbo].[TSMSTABLE] (
    [dwSeq] int IDENTITY(1,1) NOT NULL,
    [bWorld] tinyint NOT NULL,
    [dwUserID] int NOT NULL,
    [szCharName] varchar(20) COLLATE Korean_Wansung_CI_AS NOT NULL,
    [bType] tinyint NOT NULL,
    [szSender] varchar(50) COLLATE Korean_Wansung_CI_AS NOT NULL,
    [szMessage] varchar(255) COLLATE Korean_Wansung_CI_AS NOT NULL,
    [dateSend] smalldatetime NOT NULL DEFAULT (getdate()),
    [dwSMS] int NULL,
    [bResult] char(1) COLLATE Korean_Wansung_CI_AS NULL
);
GO

CREATE TABLE [dbo].[TSVRTYPE] (
    [bType] tinyint NOT NULL,
    [szName] varchar(50) COLLATE Korean_Wansung_CI_AS NOT NULL,
    [bControl] tinyint NOT NULL,
    PRIMARY KEY ([bType])
);
GO

CREATE TABLE [dbo].[TTEMPCASHITEM] (
    [dwID] int NULL,
    [dwUserID] int NULL,
    [wCashItemID] smallint NULL,
    [bCount] tinyint NULL
);
GO

CREATE TABLE [dbo].[TTEMPDURINGITEMTABLE] (
    [dwUserID] int NOT NULL,
    [wItemID] smallint NOT NULL,
    [bType] tinyint NOT NULL,
    [dwRemainTime] int NOT NULL,
    [dEndTime] smalldatetime NOT NULL,
    PRIMARY KEY ([dwUserID], [wItemID])
);
GO

CREATE TABLE [dbo].[TTESTLOGINUSER] (
    [dwuserid] int NOT NULL
);
GO

CREATE TABLE [dbo].[TUNIFYCASHITEM] (
    [dwUserID] int NOT NULL,
    [wItemID] smallint NOT NULL,
    [bCount] tinyint NOT NULL
);
GO

CREATE TABLE [dbo].[TUNIFYPOSTITEMTABLE] (
    [dwUserID] int NOT NULL,
    [dwPostID] int IDENTITY(1,1) NOT NULL,
    [wItemID] smallint NOT NULL,
    [bLevel] tinyint NOT NULL,
    [bCount] tinyint NOT NULL,
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
    PRIMARY KEY ([dwUserID], [dwPostID])
);
GO

CREATE TABLE [dbo].[TUSERATTENDTABLE] (
    [dwUserID] int NOT NULL,
    [bDay] tinyint NOT NULL,
    [bApply] tinyint NOT NULL DEFAULT (0),
    PRIMARY KEY ([dwUserID], [bDay])
);
GO

CREATE TABLE [dbo].[TUSERINFOTABLE] (
    [dwUserID] int NOT NULL,
    [bCanCreateCharCount] tinyint NOT NULL DEFAULT (6),
    [bAgreement] tinyint NOT NULL DEFAULT (0),
    [dCabinetUse] smalldatetime NULL DEFAULT ('2008-01-01'),
    PRIMARY KEY ([dwUserID])
);
GO

CREATE TABLE [dbo].[TUSERPROTECTED] (
    [dwSeq] int IDENTITY(1,1) NOT NULL,
    [dwUserID] int NOT NULL,
    [bBlockType] tinyint NOT NULL,
    [bEternal] tinyint NOT NULL DEFAULT (0),
    [bWorld] tinyint NULL,
    [dwCharID] int NULL,
    [szCharName] varchar(20) COLLATE Korean_Wansung_CI_AS NULL,
    [startTime] datetime NOT NULL,
    [dwDuration] int NOT NULL,
    [bBlockReason] tinyint NOT NULL,
    [szComment] nvarchar(4000) COLLATE Korean_Wansung_CI_AS NOT NULL,
    [szGMID] varchar(20) COLLATE Korean_Wansung_CI_AS NOT NULL,
    [regDate] datetime NOT NULL DEFAULT (getdate()),
    [sentBanMail] tinyint NOT NULL,
    PRIMARY KEY ([dwSeq])
);
GO

CREATE TABLE [dbo].[TUSERPROTECTED_old] (
    [dwSeq] int IDENTITY(1,1) NOT NULL,
    [dwUserID] int NOT NULL,
    [bBlockType] tinyint NOT NULL,
    [bEternal] tinyint NOT NULL DEFAULT ((0)),
    [bWorld] tinyint NULL,
    [dwCharID] int NULL,
    [szCharName] varchar(20) COLLATE Latin1_General_CI_AS_KS NULL,
    [startTime] datetime NOT NULL,
    [dwDuration] int NOT NULL,
    [bBlockReason] tinyint NOT NULL,
    [szComment] nvarchar(4000) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [szGMID] varchar(20) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [regDate] datetime NOT NULL DEFAULT (getdate()),
    [bPerm] int NULL,
    PRIMARY KEY ([dwSeq])
);
GO

CREATE TABLE [dbo].[TUSER_INTERFACE] (
    [bOption] tinyint NOT NULL,
    [szName] varchar(260) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [dwSize] float(53) NOT NULL,
    PRIMARY KEY ([bOption])
);
GO

CREATE TABLE [dbo].[TVERSION] (
    [dwVersion] int NOT NULL,
    [szPath] varchar(260) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [szName] varchar(260) COLLATE Latin1_General_CI_AS_KS NOT NULL,
    [dwSize] int NOT NULL DEFAULT ((0)),
    [dwBetaVer] int NULL,
    PRIMARY KEY ([dwVersion])
);
GO

CREATE TABLE [dbo].[TVETERANCHART] (
    [bID] tinyint NOT NULL,
    [bLevel] tinyint NOT NULL,
    PRIMARY KEY ([bID])
);
GO

CREATE TABLE [dbo].[TempEvent] (
    [dwUserID] int NULL,
    [CheckDate] smalldatetime NULL,
    [dwPlayTime] int NULL,
    [timeStart] datetime NULL,
    [timeEnd] datetime NULL
);
GO

CREATE TABLE [dbo].[USERIPLOG] (
    [IP] varchar(max) COLLATE Latin1_General_CI_AS_KS NULL,
    [Username] varchar(max) COLLATE Latin1_General_CI_AS_KS NULL,
    [Date_time] smalldatetime NULL
);
GO

CREATE TABLE [dbo].[releaseDate] (
    [dReleaseDate] smalldatetime NULL
);
GO

CREATE TABLE [dbo].[ttemplevel] (
    [dwuserid] int NOT NULL,
    [bmaxlevel] tinyint NULL
);
GO

CREATE TABLE [dbo].[ttempuser] (
    [dwUserID] int NOT NULL,
    [dwCharID] int NOT NULL,
    [bLevel] tinyint NOT NULL,
    [bmaxlevel] int NOT NULL
);
GO
