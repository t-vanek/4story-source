




CREATE PROCEDURE [dbo].[TGuildItemTakeOut]
	@dlID		BIGINT OUTPUT,
	@dwGuildID	INT,
	@dwItemID	INT,
	@wItemID	SMALLINT OUTPUT,
	@bLevel	TINYINT OUTPUT,
	@bGem TINYINT OUTPUT,
	@wMoggItemID SMALLINT OUTPUT,
	@bCount	TINYINT,
	@bGLevel	TINYINT OUTPUT,
	@dwDuraMax	INT OUTPUT,
	@dwDuraCur	INT OUTPUT,
	@bRefineCur	TINYINT OUTPUT,
	@dEndTime	SMALLDATETIME OUTPUT,
	@bGradeEffect	TINYINT OUTPUT,
	@bMagic1	TINYINT OUTPUT,
	@bMagic2	TINYINT OUTPUT,
	@bMagic3	TINYINT OUTPUT,
	@bMagic4	TINYINT OUTPUT,
	@bMagic5	TINYINT OUTPUT,
	@bMagic6	TINYINT OUTPUT,
	@wValue1	SMALLINT OUTPUT,
	@wValue2	SMALLINT OUTPUT,
	@wValue3	SMALLINT OUTPUT,
	@wValue4	SMALLINT OUTPUT,
	@wValue5	SMALLINT OUTPUT,
	@wValue6	SMALLINT OUTPUT,
	@dwTime1	INT OUTPUT,
	@dwTime2	INT OUTPUT,
	@dwTime3	INT OUTPUT,
	@dwTime4	INT OUTPUT,
	@dwTime5	INT OUTPUT,
	@dwTime6	INT OUTPUT
AS

DECLARE @bCnt TINYINT
DECLARE @bTemp TINYINT

SELECT @dlID=dlID, @wItemID = wItemID, @bLevel = bLevel, @bCnt = bCount, @bGLevel=bGLevel, @dwDuraMax=dwDuraMax, @dwDuraCur=dwDuraCur,@bRefineCur=bRefineCur,@dEndTime=dEndTime,@bGradeEffect = bGradeEffect,
	 @bMagic1 = bMagic1, @bMagic2 = bMagic2, @bMagic3 = bMagic3, @bMagic4 = bMagic4, @bMagic5 = bMagic5, @bMagic6 = bMagic6,
	 @wValue1 = wValue1, @wValue2 = wValue2, @wValue3 = wValue3, @wValue4 = wValue4, @wValue5 = wValue5, @wValue6 = wValue6, 
	 @dwTime1 = dwTime1, @dwTime2 = dwTime2, @dwTime3 = dwTime3, @dwTime4 = dwTime4, @dwTime5 = dwTime5, @dwTime6 = dwTime6, @bGem = bGem, @wMoggItemID = wMoggItemID
FROM TITEMTABLE WHERE dwOwnerID = @dwGuildID AND bOwnerType=1 AND bStorageType=1 AND dwStorageID=@dwItemID
IF @@ROWCOUNT = 0
	RETURN 3

IF(@bCnt < @bCount)
	RETURN 3

SET @bTemp = @bCnt - @bCount

IF @bTemp = 0
BEGIN
	BEGIN TRAN GUILD_ITEM_TAKEOUT
		DELETE TITEMTABLE  
		WHERE  @dlID=dlID
	COMMIT TRAN GUILD_ITEM_TAKEOUT
END
ELSE
BEGIN
	BEGIN TRAN GUILD_ITEM_TAKEOUT
		UPDATE TITEMTABLE 
		SET bCount = @bTemp  
		WHERE @dlID=dlID
	COMMIT TRAN GUILD_ITEM_TAKEOUT
END

RETURN 0


