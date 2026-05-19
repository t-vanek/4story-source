

CREATE PROCEDURE [dbo].[TPutItemInInven]
@dwCharID INT,
@bInven TINYINT,
@bSlot TINYINT,
@bChartType TINYINT,
@wItemID SMALLINT,
@bCount TINYINT
 AS

DECLARE @dlID BIGINT
DECLARE @dwDuraMax INT

IF(@bChartType = 1)
BEGIN
	SELECT @dwDuraMax = dwDuraMax FROM TITEMCHART WHERE wItemID = @wItemID
	IF(@@ROWCOUNT <> 1)
		RETURN 0
END

IF EXISTS(SELECT dlID FROM TITEMTABLE WHERE dwOwnerID=@dwCharID AND bOwnerType=0 AND bStorageType=0 AND dwStorageID = @bInven AND bItemID = @bSlot)
	RETURN 1

EXEC TGenerateDBItemID @dlID OUTPUT

IF(@bChartType = 1)
	INSERT INTO TITEMTABLE(
			dlID,bStorageType,dwStorageID,bOwnerType,dwOwnerID,
			bItemID,wItemID,bLevel,	bCount,	bGLevel,
			dwDuraMax,dwDuraCur,
			bRefineCur,
			dEndTime,
			bGradeEffect,
			bMagic1,bMagic2,bMagic3,bMagic4,bMagic5,bMagic6,
			wValue1,wValue2,wValue3,wValue4,wValue5,wValue6,
			dwTime1,dwTime2,dwTime3,dwTime4,dwTime5,dwTime6,bGem,wMoggItemID)
	 VALUES(	@dlID,0,@bInven,0,@dwCharID,
			@bSlot,	@wItemID,0,@bCount,0,
			@dwDuraMax,@dwDuraMax,
			0,	--RefineCur
			0,	--EndTime
			0,	--GradeEffect
			0,0,0,0,0,0, --MAGIC
			0,0,0,0,0,0, --VALUE
			0,0,0,0,0,0,0,0) --TIME
ELSE
	INSERT INTO TITEMTABLE(
			dlID,bStorageType,dwStorageID,bOwnerType,dwOwnerID,
			bItemID,wItemID,bLevel,	bCount,	bGLevel,
			dwDuraMax,dwDuraCur,
			bRefineCur,
			dEndTime,
			bGradeEffect,
			bMagic1,bMagic2,bMagic3,bMagic4,bMagic5,bMagic6,
			wValue1,wValue2,wValue3,wValue4,wValue5,wValue6,
			dwTime1,dwTime2,dwTime3,dwTime4,dwTime5,dwTime6,bGem,wMoggItemID)
	SELECT @dlID,0,@bInven,0,@dwCharID,
		@bSlot,	wItemID,bLevel,@bCount,bGLevel,
		dwDuraMax, dwDuraCur,
		bRefineCur,
		0,
		bGradeEffect,
		bMagic1, bMagic2, bMagic3, bMagic4, bMagic5, bMagic6,
		wValue1, wValue2, wValue3, wValue4, wValue5, wValue6,
		dwTime1, dwTime2, dwTime3, dwTime4, dwTime5, dwTime6, 0, 0
	FROM TQUESTITEMCHART WHERE dwID = @wItemID


