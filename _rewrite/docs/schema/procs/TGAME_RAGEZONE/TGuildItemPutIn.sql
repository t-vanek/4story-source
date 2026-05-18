




CREATE PROCEDURE [dbo].[TGuildItemPutIn]
	@dlID		BIGINT,
	@dwGuildID	INT,
	@dwItemID	INT OUTPUT,
	@wItemID	SMALLINT,
	@bLevel	TINYINT,
	@bGem TINYINT,
	@wMoggItemID SMALLINT,
	@bCount	TINYINT,
	@bGLevel	TINYINT,
	@dwDuraMax	INT,
	@dwDuraCur	INT,
	@bRefineCur	TINYINT,
	@dEndTime	SMALLDATETIME,
	@bGradeEffect	TINYINT,
	@bMagic1	TINYINT,
	@bMagic2	TINYINT,
	@bMagic3	TINYINT,
	@bMagic4	TINYINT,
	@bMagic5	TINYINT,
	@bMagic6	TINYINT,
	@wValue1	SMALLINT,
	@wValue2	SMALLINT,
	@wValue3	SMALLINT,
	@wValue4	SMALLINT,
	@wValue5	SMALLINT,
	@wValue6	SMALLINT,
	@dwTime1	INT,
	@dwTime2	INT,
	@dwTime3	INT,
	@dwTime4	INT,
	@dwTime5	INT,
	@dwTime6	INT
AS

DECLARE @bStack INT

SELECT @bStack = bStack FROM TITEMCHART WHERE wItemID = @wItemID
IF @@ROWCOUNT = 0
	RETURN 1

SELECT TOP 1 @dwItemID = dwStorageID FROM TITEMTABLE WHERE dwOwnerID = @dwGuildID AND bOwnerType=1 AND bStorageType=1 AND wItemID = @wItemID AND (CAST(bCount AS INT)+@bCount) <= @bStack
IF @@ROWCOUNT != 0
BEGIN
	BEGIN TRAN GUILD_ITEM_PUTIN
		UPDATE TITEMTABLE 
		SET bCount = bCount + @bCount
		WHERE dwOwnerID = @dwGuildID AND bOwnerType=1 AND bStorageType=1 AND dwStorageID=@dwItemID

		DELETE TITEMTABLE WHERE dlID = @dlID
	COMMIT TRAN GUILD_ITEM_PUTIN
	
	RETURN 0
END

DECLARE @bGuildLevel TINYINT
DECLARE @bMaxCnt TINYINT
DECLARE @bCabinetCount TINYINT

SELECT @bGuildLevel = bLevel, @bMaxCnt = bMaxCabinet FROM TGUILDTABLE WHERE dwID = @dwGuildID
IF @@ROWCOUNT = 0
	RETURN 1

IF @bMaxCnt = 0
	RETURN 5

SELECT @bCabinetCount = COUNT(*), @dwItemID = MAX(dwStorageID) FROM TITEMTABLE WHERE dwOwnerID = @dwGuildID AND bOwnerType=1 AND bStorageType=1
IF @bMaxCnt <= @bCabinetCount
	RETURN 2

IF @bCabinetCount = 0
	SET @dwItemID = 1
ELSE
	SET @dwItemID = @dwItemID + 1

BEGIN TRAN GUILD_ITEM_PUTIN
	IF EXISTS (SELECT dlID FROM TITEMTABLE WHERE dlID=@dlID)
		DELETE TITEMTABLE WHERE dlID = @dlID

	INSERT INTO TITEMTABLE(
		dlID,bStorageType, dwStorageID, bOwnerType, dwOwnerID, bItemID, wItemID, bLevel, bCount, bGLevel, dwDuraMax, dwDuraCur, bRefineCur,dEndTime,bGradeEffect,
		bMagic1, bMagic2, bMagic3, bMagic4, bMagic5, bMagic6,
		wValue1, wValue2, wValue3, wValue4, wValue5, wValue6,
		dwTime1, dwTime2, dwTime3, dwTime4, dwTime5, dwTime6, bGem, wMoggItemID)
	VALUES
	(
		@dlID, 1,@dwItemID,1,	@dwGuildID,0,@wItemID,@bLevel,@bCount,@bGLevel,@dwDuraMax,@dwDuraCur,@bRefineCur,@dEndTime,@bGradeEffect,
		@bMagic1, @bMagic2, @bMagic3, @bMagic4, @bMagic5, @bMagic6,
		@wValue1, @wValue2, @wValue3, @wValue4, @wValue5, @wValue6,
		@dwTime1, @dwTime2, @dwTime3, @dwTime4, @dwTime5, @dwTime6, @bGem, @wMoggItemID
	)
COMMIT TRAN GUILD_ITEM_PUTIN

RETURN 0


