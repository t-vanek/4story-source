-- VIEW: SKILLLINK

CREATE VIEW [dbo].[SKILLLINK] AS 
SELECT   dbo.TSKILLTABLE.dwCharID, dbo.TSKILLTABLE.wSkillID, 
                dbo.TSKILLTABLE.bLevel, dbo.TSKILLCHART.bMaxLevel
FROM      dbo.TSKILLCHART INNER JOIN
                dbo.TSKILLTABLE ON dbo.TSKILLCHART.wID = dbo.TSKILLTABLE.wSkillID

GO

-- VIEW: TALLCHARVIEW

CREATE VIEW [dbo].[TALLCHARVIEW] AS 
select  dwUserID, bWorldID, dwCharID, bSlot, szNAME, bClass, bRace, bCountry, bSex, 					
bHair, bFace, bBody, bPants, bHand, bFoot, bLevel, dwEXP, bDelete, dCreateDate, dDeleteDate					
FROM 	TGLOBAL_GSP.DBO.TALLCHARTABLE

GO

-- VIEW: TALLITEM_ARCHER1

CREATE VIEW [dbo].[TALLITEM_ARCHER1] AS 
SELECT   wItemID, szNAME, dwSlotID, dwClassID
FROM      dbo.TITEMCHART
WHERE   (dwSlotID > 0) AND (dwClassID & 4 = 4)

GO

-- VIEW: TALLITEM_PRIEST1

CREATE VIEW [dbo].[TALLITEM_PRIEST1] AS 
SELECT   wItemID, szNAME, dwSlotID, dwClassID
FROM      dbo.TITEMCHART
WHERE   (dwSlotID > 0) AND (dwClassID & 16 = 16)

GO

-- VIEW: TALLITEM_RANGER1

CREATE VIEW [dbo].[TALLITEM_RANGER1] AS 
SELECT   wItemID, szNAME, dwSlotID, dwClassID
FROM      dbo.TITEMCHART
WHERE   (dwSlotID > 0) AND (dwClassID & 2 = 2)

GO

-- VIEW: TALLITEM_SORCERER1

CREATE VIEW [dbo].[TALLITEM_SORCERER1] AS 
SELECT   wItemID, szNAME, dwSlotID, dwClassID
FROM      dbo.TITEMCHART
WHERE   (dwSlotID > 0) AND (dwClassID & 32 = 32)

GO

-- VIEW: TALLITEM_WARRIOR1

CREATE VIEW [dbo].[TALLITEM_WARRIOR1] AS 
SELECT   wItemID, szNAME, dwSlotID, dwClassID
FROM      dbo.TITEMCHART
WHERE   (dwClassID & 1 = 1) AND (dwSlotID > 0)

GO

-- VIEW: TALLITEM_WIZARD1

CREATE VIEW [dbo].[TALLITEM_WIZARD1] AS 
SELECT   wItemID, szNAME, dwSlotID, dwClassID
FROM      dbo.TITEMCHART
WHERE   (dwSlotID > 0) AND (dwClassID & 8 = 8)

GO

-- VIEW: TALLSKILL_WIZARD1

CREATE VIEW [dbo].[TALLSKILL_WIZARD1] AS 
select wID from tskillchart where (dwClassID > 0 ) AND (dwClassID & 8 = 8) AND bCanLearn =1

GO

-- VIEW: TCHANNEL
CREATE VIEW [dbo].[TCHANNEL] AS 
SELECT bGroupID, bChannel FROM TGLOBAL_GSP.dbo.TCHANNEL
GO

-- VIEW: TGUILDMEMBER

CREATE VIEW [dbo].[TGUILDMEMBER] AS 
SELECT   TOP 100 PERCENT G.dwCharID, G.dwGuildID, C.szNAME, C.bLevel, C.bClass, 
                G.bDuty, G.bPeer, G.dwService, C.dLogoutDate, C.bCountry, A.bCountry AS bWarCountry
FROM      dbo.TGUILDMEMBERTABLE G INNER JOIN
                dbo.TCHARTABLE C ON G.dwCharID = C.dwCharID
	   LEFT OUTER JOIN dbo.TAIDTABLE AS A ON A.dwCharID=G.dwCharID
ORDER BY G.dwGuildID

GO

-- VIEW: tguildview

CREATE VIEW [dbo].[tguildview] AS 
SELECT
dbo.TGUILDTABLE.dwID AS guild_ID,
dbo.TGUILDTABLE.bLevel AS guild_level,
dbo.TGUILDTABLE.szName AS guild_name,
dbo.TGUILDTABLE.dwChief AS guild_leader_ID,
dbo.TGUILDTABLE.dwPvPTotalPoint AS total_honor,
dbo.TGUILDTABLE.dwPvPMonthPoint AS month_honor,
dbo.TCHARTABLE.dwCharID AS char_charid,
dbo.TCHARTABLE.szNAME AS guild_leader,
dbo.TCHARTABLE.bCountry AS char_country
FROM
dbo.TGUILDTABLE
INNER JOIN dbo.TCHARTABLE ON
dbo.TCHARTABLE.dwCharID = dbo.TGUILDTABLE.dwChief

GO

-- VIEW: TVIEW_CASHCATEGORYCHART

CREATE VIEW dbo.TVIEW_CASHCATEGORYCHART
AS
SELECT bID, szName, wOrder
FROM      TGLOBAL_GSP.dbo.TCASHCATEGORYCHART















GO

-- VIEW: TSVRCHART

CREATE VIEW [dbo].[TSVRCHART] AS 
SELECT   dbo.TUNITCHART.bGroup, dbo.TUNITCHART.bServerID, 
                dbo.TUNITCHART.wMapID, dbo.TUNITCHART.wUnitID, 
                dbo.TMAPCHART.bChannel
FROM      dbo.TUNITCHART INNER JOIN
                dbo.TMAPCHART ON dbo.TUNITCHART.wMapID = dbo.TMAPCHART.wMapID AND 
                dbo.TUNITCHART.bServerID = dbo.TMAPCHART.bServerID AND 
                dbo.TUNITCHART.bGroup = dbo.TMAPCHART.bGroupID

GO

-- VIEW: TVIEW_CASHGAMBLECHART
CREATE VIEW [dbo].[TVIEW_CASHGAMBLECHART] AS 
SELECT  dwID, wGroup, wUseTime, wItemID, bCount, dwProb, bLevel, bGLevel, bGradeEffect,
	dwDuraMax, dwDuraCur, bRefineCur,
	bMagic1, bMagic2, bMagic3, bMagic4, bMagic5, bMagic6,
	wValue1, wValue2, wValue3, wValue4, wValue5, wValue6,
	dwTime1, dwTime2, dwTime3, dwTime4, dwTime5, dwTime6,
	bGem, wMoggItemID
FROM      TGLOBAL_GSP.dbo.TCASHGAMBLECHART
GO

-- VIEW: TVIEW_CASHITEMCABINET
CREATE VIEW [dbo].[TVIEW_CASHITEMCABINET] AS 
SELECT   dwID, dwUserID, wItemID, bLevel, bCount, bGLevel, dwDuraMax, dwDuraCur, bRefineCur,dEndTime,bGradeEffect,
	bMagic1, bMagic2, bMagic3, bMagic4, bMagic5, bMagic6,
	wValue1, wValue2, wValue3, wValue4, wValue5, wValue6,
	dwTime1, dwTime2, dwTime3, dwTime4, dwTime5, dwTime6, dlID, bGem, wMoggItemID
FROM      TGLOBAL_GSP.dbo.TCASHITEMCABINETTABLE AS GC
WHERE GC.bWorldID=0 OR GC.bWorldID=(SELECT bWorld+1 FROM TDBITEMINDEXTABLE)
GO

-- VIEW: TVIEW_CASHSHOPITEMCHART

CREATE VIEW [dbo].[TVIEW_CASHSHOPITEMCHART] AS 
SELECT wID, 
	dwMoney AS dwOriMoney,
	(dwMoney * (100 - bSaleValue)/100) AS dwMoney, 	
	wItemID, 
	wInfoID,
	bLevel, 
	bCount, 
	bGLevel,
	dwDuraMax,
	dwDuraCur,
	bRefineCur,
	wUseTime,
	bGradeEffect,
	bMagic1, 
	bMagic2, 
	bMagic3, 
	bMagic4, 
	bMagic5, 
	bMagic6, 
	wValue1, 
	wValue2, 
	wValue3, 
	wValue4, 
	wValue5, 
	wValue6, 
	dwTime1, 
	dwTime2, 
	dwTime3, 
	dwTime4, 
	dwTime5, 
	dwTime6,
	bCanSell,
	bCategory,
	bKind,
	wOrder,
	bSaleValue
FROM      TGLOBAL_GSP.dbo.TCASHSHOPITEMCHART

GO

-- VIEW: TVIEW_CHARTABLE

CREATE VIEW [dbo].[TVIEW_CHARTABLE] AS 
SELECT dwCharID, dwUserID, szName, bClass, bRace, bCountry, bSex, bLevel
FROM TCHARTABLE

GO

-- VIEW: TVIEW_DURINGITEMTABLE

CREATE VIEW [dbo].[TVIEW_DURINGITEMTABLE] AS 
SELECT dwUserID, wItemID, bType, dwRemainTime, dEndTime
FROM      TGLOBAL_GSP.dbo.TDURINGITEMTABLE

GO

-- VIEW: TVIEW_GUILDTACTICSTABLE

CREATE VIEW [dbo].[TVIEW_GUILDTACTICSTABLE] AS 
SELECT   GV.dwGuildID, GV.dwCharID, CA.szNAME, CA.bLevel, CA.bClass, 
                GV.dwRewardPoint, GV.dwGainPoint, GV.bDay, GV.dEndTime, GV.dlRewardMoney
FROM      dbo.TGUILDTACTICSTABLE GV INNER JOIN
                dbo.TCHARTABLE CA ON GV.dwCharID = CA.dwCharID

GO

-- VIEW: TVIEW_GUILDVOLUNTEERTABLE

CREATE VIEW [dbo].[TVIEW_GUILDVOLUNTEERTABLE] AS 
SELECT   GV.bType, GV.dwCharID, CA.szNAME, GV.dwID, CA.bLevel, CA.bClass
FROM      dbo.TGUILDVOLUNTEERTABLE GV INNER JOIN
                dbo.TCHARTABLE CA ON GV.dwCharID = CA.dwCharID

GO

-- VIEW: TVIEW_MENTORMEMBER

CREATE VIEW [dbo].[TVIEW_MENTORMEMBER] AS 
SELECT   M.dwCharID, M.dwMentorID, M.dwExp, C.szNAME, C.bLevel, C.bClass, C.bRace, 
                C.bSex, C.bFace, C.bHair, C.dLogoutDate
FROM      dbo.TMENTORTABLE M INNER JOIN
                dbo.TCHARTABLE C ON M.dwCharID = C.dwCharID

GO

-- VIEW: TVIEW_MONTHRANK

CREATE VIEW [dbo].[TVIEW_MONTHRANK] AS 
SELECT TOP 64
	M.dwCharID,  
	C.szName, 
	C.bCountry,
	P.dwTotalPoint,  
	M.dwPoint,  
	M.wWin,  
	M.wLose, 
	R.dwWarrior_win + R.dwRanger_win + R.dwArcher_win + R.dwWizard_win + R.dwPriest_win + R.dwSorcerer_win AS 'dwTotalWin', 
	R.dwWarrior_lose + R.dwRanger_lose + R.dwArcher_lose + R.dwWizard_lose + R.dwPriest_lose + R.dwSorcerer_lose AS 'dwTotalLose', 
	C.bLevel,  
	C.bClass,  
	C.bRace,  
	C.bSex,  
	C.bHair,  
	C.bFace,  
	M.szSay,
	U.szName  AS 'szGuild'
	FROM TMONTHPVPOINTTABLE AS M  
	INNER JOIN TCHARTABLE AS C ON C.dwCharID = M.dwCharID  
	INNER JOIN TPVPOINTTABLE AS P ON  P.dwCharID = M.dwCharID
	INNER JOIN TPVPRECORDTABLE AS R ON R.dwCharID = M.dwCharID  
	LEFT JOIN TGUILDMEMBERTABLE AS G ON M.dwCharID = G.dwCharID  
	LEFT JOIN TGUILDTABLE AS U ON G.dwGuildID = U.dwID  
	WHERE M.dwCharID IN(SELECT TOP 32 dwCharID FROM TMONTHPVPOINTTABLE WHERE bCountry=0 AND dwPoint>0 ORDER BY dwPoint DESC, wWin DESC,wLose DESC, dwCharID ASC)
		OR M.dwCharID IN(SELECT TOP 32 dwCharID FROM TMONTHPVPOINTTABLE WHERE bCountry=1 AND dwPoint>0 ORDER BY dwPoint DESC, wWin DESC,wLose DESC, dwCharID ASC)
	ORDER BY C.bCountry ASC,M.dwPoint DESC,M.wWin DESC,M.wLose DESC, M.dwCharID ASC

GO

-- VIEW: TVIEW_SKILLPOINT

CREATE VIEW [dbo].[TVIEW_SKILLPOINT] AS 
SELECT   dbo.TSKILLTABLE.dwCharID, SUM(dbo.TSKILLPOINTCHART.bSkillPoint) 
                AS dwSum
FROM      dbo.TSKILLTABLE INNER JOIN
                dbo.TSKILLPOINTCHART ON 
                dbo.TSKILLTABLE.wSkillID = dbo.TSKILLPOINTCHART.wID AND 
                dbo.TSKILLTABLE.bLevel = dbo.TSKILLPOINTCHART.bLevel
WHERE   (dbo.TSKILLTABLE.wSkillID > 400) AND (dbo.TSKILLTABLE.wSkillID < 500)
GROUP BY dbo.TSKILLTABLE.dwCharID

GO

-- VIEW: TVIEW_SOULMATE

CREATE VIEW [dbo].[TVIEW_SOULMATE] AS 
SELECT   TOP 100 PERCENT S.dwCharID, S.dwTarget, C.szNAME, C.bLevel, C.bClass,
                S.dwTime
FROM      dbo.TSOULMATETABLE S INNER JOIN
                dbo.TCHARTABLE C ON S.dwTarget = C.dwCharID
ORDER BY S.dwCharID

GO

-- VIEW: TVIEW_TOURNAMENTPLAYER
CREATE VIEW [dbo].[TVIEW_TOURNAMENTPLAYER] AS 
SELECT   TN.dwCharID, TC.szNAME, TN.dwChiefID, TC.bClass, TC.bCountry, TC.bLevel, 
                TN.bEntry, TN.bStep, TN.bResult, TN.dwIPAddr, TN.szHWID
FROM      dbo.TTOURNAMENTPLAYERTABLE TN INNER JOIN
                dbo.TCHARTABLE TC ON TN.dwCharID = TC.dwCharID
GO

-- VIEW: tviewguildplayers

CREATE VIEW [dbo].[tviewguildplayers] AS 
SELECT
TGUILDMEMBERTABLE.dwCharID,
TGUILDMEMBERTABLE.dwGuildID,
TGUILDTABLE.szName AS g_name,
TGUILDTABLE.dwID,
TCHARTABLE.dwCharID AS p_dwcharid,
TCHARTABLE.szNAME AS p_name,
TCHARTABLE.bLevel,
TCHARTABLE.bFace,
TCHARTABLE.bSex,
TCHARTABLE.bRace,
TCHARTABLE.bHair,
TCHARTABLE.bCountry,
TCHARTABLE.bClass
FROM TCHARTABLE
INNER JOIN TGUILDMEMBERTABLE ON
TGUILDMEMBERTABLE.dwCharID = TCHARTABLE.dwCharID
INNER JOIN TGUILDTABLE ON
TGUILDTABLE.dwID = TGUILDMEMBERTABLE.dwGuildID

GO


