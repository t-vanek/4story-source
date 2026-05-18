
CREATE PROCEDURE [dbo].[TCashItemGet]
	@dlID		BIGINT OUTPUT,
	@wItemID	SMALLINT OUTPUT,
	@bLevel	TINYINT OUTPUT,
	@bCount	TINYINT OUTPUT,
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
	@dwTime6	INT OUTPUT,
	@bGem 		TINYINT OUTPUT,
	@wMoggItemID SMALLINT OUTPUT,
	@dwUserID INT,
	@dwCharID INT,
	@dwCashItemID INT,
	@bInven TINYINT,
	@bItemID TINYINT
AS

DECLARE @dwCharUserID INT
DECLARE @dwOwner INT

SELECT @dwCharUserID = dwUserID FROM TCHARTABLE WHERE dwCharID = @dwCharID
IF(@@ROWCOUNT <> 1 OR @dwCharUserID <> @dwUserID)
	RETURN 3

SELECT @dwOwner = dwUserID,
	 @wItemID = wItemID,
	 @bLevel = bLevel,
	 @bCount = bCount,
	 @bGLevel = bGLevel,
	 @dwDuraMax = dwDuraMax,
	 @dwDuraCur = dwDuraCur,
	 @bRefineCur = bRefineCur,
	 @dEndTime = dEndTime,
	@bGradeEffect = bGradeEffect,
	 @bMagic1 = bMagic1,
	 @bMagic2 = bMagic2,
	 @bMagic3 = bMagic3,
	 @bMagic4 = bMagic4,
	 @bMagic5 = bMagic5,
	 @bMagic6 = bMagic6,
	 @wValue1 = wValue1,
	 @wValue2 = wValue2,
	 @wValue3 = wValue3,
	 @wValue4 = wValue4,
	 @wValue5 = wValue5,
	 @wValue6 = wValue6,
	 @dwTime1 = dwTime1,
	 @dwTime2 = dwTime2,
	 @dwTime3 = dwTime3,
	 @dwTime4 = dwTime4,
	 @dwTime5 = dwTime5,
	 @dwTime6 = dwTime6,
	 @dlID = dlID,
	 @bGem = bGem,
	 @wMoggItemID = wMoggItemID
FROM TVIEW_CASHITEMCABINET WHERE dwID = @dwCashItemID

IF(@@ROWCOUNT <> 1 OR @dwOwner <> @dwUserID)
	RETURN 3

SELECT wItemID FROM TITEMTABLE WHERE dwOwnerID=@dwCharID AND bOwnerType=0 AND bStorageType=0 AND dwStorageID=@bInven AND bItemID = @bItemID
IF(@@ROWCOUNT > 0)
	RETURN 1

IF(@dlID = 0)
	EXEC TGenerateDBItemID @dlID OUTPUT
ELSE IF EXISTS( SELECT dlID FROM TITEMTABLE WHERE dlID = @dlID)
	RETURN 5

INSERT INTO TITEMTABLE(
	dlID, bStorageType, dwStorageID, bOwnerType, dwOwnerID, bItemID, wItemID, bLevel, bCount, bGLevel, dwDuraMax, dwDuraCur, bRefineCur,dEndTime,bGradeEffect,
	bMagic1, bMagic2, bMagic3, bMagic4, bMagic5, bMagic6, 
	wValue1, wValue2, wValue3, wValue4, wValue5, wValue6, 
	dwTime1, dwTime2, dwTime3, dwTime4, dwTime5, dwTime6, bGem, wMoggItemID) VALUES(
	@dlID, 0,@bInven,0,@dwCharID, @bItemID, @wItemID, @bLevel, @bCount, @bGLevel, @dwDuraMax, @dwDuraCur, @bRefineCur, @dEndTime,@bGradeEffect,
	@bMagic1, @bMagic2, @bMagic3, @bMagic4, @bMagic5, @bMagic6, 
	@wValue1, @wValue2, @wValue3, @wValue4, @wValue5, @wValue6, 
	@dwTime1, @dwTime2, @dwTime3, @dwTime4, @dwTime5, @dwTime6, @bGem, @wMoggItemID)

EXEC TGLOBAL_GSP.DBO.TCashItemGet @dwCashItemID

RETURN @@ERROR


