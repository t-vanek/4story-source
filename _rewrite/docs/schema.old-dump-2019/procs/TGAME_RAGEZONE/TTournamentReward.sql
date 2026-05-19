

CREATE PROCEDURE [dbo].[TTournamentReward]
@bType TINYINT=0  OUTPUT,
@bOrder TINYINT=0 OUTPUT,
@dwMonthRank INT=0 OUTPUT,
@dwTotalRank INT=0 OUTPUT,
@dwMonthPoint INT=0 OUTPUT,
@dwTotalPoint INT=0 OUTPUT,
@wMonthWin SMALLINT=0 OUTPUT,
@wMonthLose SMALLINT=0 OUTPUT,
@dwTotalWin INT=0 OUTPUT,
@dwTotalLose INT=0 OUTPUT,
@bCountry TINYINT=0 OUTPUT,
@bLevel TINYINT=0 OUTPUT,
@bClass TINYINT=0 OUTPUT,
@bRace TINYINT=0 OUTPUT,
@bSex TINYINT=0 OUTPUT,
@bFace TINYINT=0 OUTPUT,
@bHair TINYINT=0 OUTPUT,
@szName VARCHAR(50)='' OUTPUT,
@szGuild VARCHAR(50)='' OUTPUT,
@bMonth TINYINT,
@wTournamentID SMALLINT,
@bEntry TINYINT,
@dwChief INT,
@dwWin INT,
@bIsEquipShield TINYINT
AS

DECLARE @szTitle VARCHAR(256)
DECLARE @szMessage VARCHAR(1024)

DECLARE @wRewardID SMALLINT
DECLARE @dwClassID INT
DECLARE @bCheckShield TINYINT
DECLARE @bChartType TINYINT
DECLARE @wItemID SMALLINT
DECLARE @bItemCount TINYINT
DECLARE @bShield TINYINT
DECLARE @dwGuildID INT

SELECT @bCountry=bCountry, @bLevel=bLevel, @bClass=bClass, @bRace=bRace, @bSex=bSex, @bFace=bFace, @bHair=bHair, @szName=szNAME FROM TCHARTABLE WHERE dwCharID=@dwWin
IF(@@ROWCOUNT <> 1)
	RETURN 1

SET @dwMonthRank = 0
SET @dwMonthPoint = 0
SET @wMonthWin = 0
SET @wMonthLose = 0
SET @dwTotalRank = 0
SET @dwTotalPoint = 0
SET @dwTotalWin = 0
SET @dwTotalLose = 0
SET @szGuild = ''

SELECT @dwMonthRank = dwRank, @dwMonthPoint=dwPoint, @wMonthWin=wWin, @wMonthLose=wLose FROM tgame.TLASTMONTHPOINTTABLE WHERE dwCharID=@dwWin
SELECT @dwTotalRank = dwRank, @dwTotalPoint=dwPoint, @dwTotalWin=dwWin, @dwTotalLose=dwLose FROM tgame.TLASTTOTALPOINTTABLE WHERE dwCharID=@dwWin
SELECT @dwGuildID = dwGuildID FROM TGUILDMEMBERTABLE WHERE dwCharID=@dwWin
IF(@@ROWCOUNT=1)
	SELECT @szGuild = szName FROM TGUILDTABLE WHERE dwID=@dwGuildID

IF(@bEntry = 1)
BEGIN
	SET @bType = 2
	IF(@dwWin = @dwChief)
		SET @bOrder = 0
	ELSE
		SELECT @bOrder = COUNT(*)+1  FROM THEROTABLE WHERE bMonth = @bMonth AND bType=@bType AND bOrder > 0
END
ELSE
BEGIN
	SET @bType =1
	SET @bOrder = @bEntry-2
END

SET @wRewardID = 1

SELECT @szTitle = szMessage FROM TSVRMSGCHART WHERE dwID = 40
SELECT @szMessage = szMessage FROM TSVRMSGCHART WHERE dwID = 41

WHILE(@wRewardID>0)
BEGIN
	IF(@wTournamentID = 1)
		SELECT TOP 1 @wRewardID=wID+1, @dwClassID=dwClass, @bCheckShield=bCheckShield, @bChartType=bChartType, @wItemID=wItemID, @bItemCount=bItemCount FROM TTOURNAMENTREWARDCHART WHERE bEntryID=@bEntry AND wID>=@wRewardID
	ELSE
		SELECT TOP 1 @wRewardID=wID+1, @dwClassID=dwClass, @bCheckShield=bCheckShield, @bChartType=bChartType, @wItemID=wItemID, @bItemCount=bItemCount FROM TTNMTEVENTREWARDTABLE WHERE wTournamentID=@wTournamentID AND bEntryID=@bEntry AND wID>=@wRewardID

	IF(@@ROWCOUNT>0)
	BEGIN
		IF( @bItemCount > 0 AND (@dwClassID & POWER(2,@bClass)) > 0  AND (@bCheckShield = 0 OR (@bCheckShield = 1 AND @bIsEquipShield=1) OR (@bCheckShield = 2 AND @bIsEquipShield=0)))
		BEGIN
			IF(@bChartType = 0)
				EXEC TEventQuestItemGive @dwWin, @wItemID, @bItemCount, @szTitle, @szMessage
			ELSE
				EXEC TEventItemGive @szName, @wItemID, @bItemCount, @szTitle, @szMessage
		END
	END
	ELSE
		SET @wRewardID = 0
END

IF(@wTournamentID = 1)
	INSERT INTO THEROTABLE (
		bMonth,bType,bOrder,dwMonthRank,dwTotalRank,dwTotalPoint,dwMonthPoint,wMonthWin,wMonthLose,dwTotalWin,dwTotalLose,
		dwCharID,szName,bCountry,bLevel,bClass,bRace,bSex,bHair,bFace,szGuild) 
		 VALUES(
		@bMonth,@bType,@bOrder,@dwMonthRank,@dwTotalRank,@dwTotalPoint,@dwMonthPoint,@wMonthWin,@wMonthLose,@dwTotalWin,@dwTotalLose,
		@dwWin,@szName,@bCountry,@bLevel,@bClass,@bRace,@bSex,@bHair,@bFace,@szGuild )


