CREATE PROCEDURE [dbo].[TTESTGivePowerItem_copy]
						@dwCharID INT,
						@wItemID SMALLINT,
						@bItemID TINYINT,
						@bEffect TINYINT
AS

declare @bType TINYINT
declare @bKind TINYINT
declare @GenID BIGINT
declare @dwUserID INT
declare @bLevel TINYINT

declare @bMagic1 TINYINT
declare @bMagic2 TINYINT
declare @bMagic3 TINYINT
declare @bMagic4 TINYINT
declare @bMagic5 TINYINT
declare @bMagic6 TINYINT
declare @wValue1 SMALLINT
declare @wValue2 SMALLINT
declare @wValue3 SMALLINT
declare @wValue4 SMALLINT
declare @wValue5 SMALLINT
declare @wValue6 SMALLINT
declare @dwTime1 INT

SELECT @dwUserID = dwUserID FROM TCHARTABLE WHERE dwCharID = @dwCharID
IF(@@ROWCOUNT <> 1)
	RETURN 1

SELECT @bType = bType, @bKind = bKind, @dwTime1 =  blevel FROM TITEMCHART WHERE wItemID = @wItemID
IF(@@ROWCOUNT <> 1)
	RETURN 1

-- Effect Type {  0 None, 1 Water, 2 Fire, 3 Litening, 4 ICE, 5 Dark  }
IF(@bEffect > 5)
	RETURN 1

IF(@bKind IN(1, 2, 3, 4, 5) )	SELECT @bMagic1 = 7, @bMagic2 = 11,  @bMagic3 = 13,  @bMagic4 = 54,  @bMagic5 = 0,  @bMagic6 = 0
ELSE IF(@bKind IN(6, 7) )	SELECT @bMagic1 = 9, @bMagic2 = 11,  @bMagic3 = 13,  @bMagic4 = 55,  @bMagic5 = 0,  @bMagic6 = 0
ELSE IF(@bKind IN(8, 9) )	SELECT @bMagic1 = 17, @bMagic2 = 86,  @bMagic3 = 21,  @bMagic4 = 56,  @bMagic5 = 20,  @bMagic6 = 0
ELSE IF(@bKind = 11 )		SELECT @bMagic1 = 34, @bMagic2 = 87,  @bMagic3 = 0,  @bMagic4 = 0,  @bMagic5 = 0,  @bMagic6 = 0
ELSE IF(@bKind = 12 )		SELECT @bMagic1 = 34, @bMagic2 = 12,  @bMagic3 = 0,  @bMagic4 = 0,  @bMagic5 = 0,  @bMagic6 = 0
ELSE IF(@bType = 2 )		SELECT @bMagic1 = 8, @bMagic2 = 12,  @bMagic3 = 16,  @bMagic4 = 87,  @bMagic5 = 0,  @bMagic6 = 0
ELSE IF(@bType = 3 )		SELECT @bMagic1 = 1, @bMagic2 = 2,  @bMagic3 = 3,  @bMagic4 = 4,  @bMagic5 = 5,  @bMagic6 = 6
ELSE				RETURN 1

IF(@bType = 3  AND @bKind <> 20)
	SET @bLevel = 0
ELSE SET @bLevel = 24

IF(@wItemID = 50)	SELECT @bMagic1 = 1, @bMagic2 = 2,  @bMagic3 = 3,  @bMagic4 = 4,  @bMagic5 = 5,  @bMagic6 = 6, @bLevel = 0

EXEC TTESTGetMaxMagicValue @wItemID, @bMagic1, @wValue1 OUTPUT
EXEC TTESTGetMaxMagicValue @wItemID, @bMagic2, @wValue2 OUTPUT
EXEC TTESTGetMaxMagicValue @wItemID, @bMagic3, @wValue3 OUTPUT
EXEC TTESTGetMaxMagicValue @wItemID, @bMagic4, @wValue4 OUTPUT
EXEC TTESTGetMaxMagicValue @wItemID, @bMagic5, @wValue5 OUTPUT
EXEC TTESTGetMaxMagicValue @wItemID, @bMagic6, @wValue6 OUTPUT

IF( @bItemID = 255)
BEGIN	
/*
select @GenID[dlID],0[bStorageType],254[dwStorageID],0[bOwnerType],@dwCharID[dwOwnerID], @bItemID[bItemID],@wItemID[wItemID], @bLevel[bLevel],1[bCount],0[bGLevel],0[dwDuraMax],0[dwDuraCur],0[bRefineCur],0[dEndTime], @bEffect[bGradeEffect],
	@bMagic1[bMagic1], @bMagic2[bMagic2], @bMagic3[bMagic3], @bMagic4[bMagic4], @bMagic5[bMagic5], @bMagic6[bMagic6], 
	@wValue1[wValue1], @wValue2[wValue2], @wValue3[wValue3], @wValue4[wValue4], @wValue5[wValue5], @wValue6[wValue6]
*/
	INSERT INTO TGLOBAL_GSP.DBO.TCASHITEMCABINETTABLE(
		dwUserID, wItemID, bLevel, bCount, bGLevel, dwDuraMax, dwDuraCur, bRefineCur, dEndTime, bGradeEffect,
		bMagic1, bMagic2, bMagic3, bMagic4, bMagic5, bMagic6,
		wValue1, wValue2, wValue3, wValue4, wValue5, wValue6,
		dwTime1, dwTime2, dwTime3, dwTime4, dwTime5, dwTime6)
	 	VALUES(@dwUserID, @wItemID, @bLevel, 1, 0, 0, 0, 0, 0, @bEffect,
		@bMagic1, @bMagic2, @bMagic3, @bMagic4, @bMagic5, @bMagic6, 
		@wValue1, @wValue2, @wValue3, @wValue4, @wValue5, @wValue6, 
		@dwTime1, 0, 0, 0, 0, 0)
	RETURN 0
END

SELECT @GenID = dlID FROM TITEMTABLE WHERE dwOwnerID = @dwCharID AND bOwnerType = 0 AND bStorageType = 0 AND dwStorageID = 254 AND bItemID = @bItemID
IF(@@ROWCOUNT = 1)
	UPDATE TITEMTABLE SET wItemID = @wItemID, blevel = @bLevel, dwduramax = 0, dwduracur = 0,  bGradeEffect = @bEffect,
	bMagic1 = @bMagic1, bMagic2 = @bMagic2, bMagic3 = @bMagic3, bMagic4 = @bMagic4, bMagic5 = @bMagic5, bMagic6 = @bMagic6, 
	wValue1 = @wValue1, wValue2 = @wValue2, wValue3 = @wValue3, wValue4 = @wValue4, wValue5 = @wValue5, wValue6 = @wValue6, dwTime1 = @dwTime1 where dlID = @GenID
ELSE IF(@@ROWCOUNT = 0)
BEGIN

  EXEC TGenerateDBItemID @GenID OUTPUT
/*
select @GenID[dlID],0[bStorageType],254[dwStorageID],0[bOwnerType],@dwCharID[dwOwnerID], @bItemID[bItemID],@wItemID[wItemID], @bLevel[bLevel],1[bCount],0[bGLevel],0[dwDuraMax],0[dwDuraCur],0[bRefineCur],0[dEndTime], @bEffect[bGradeEffect],
	@bMagic1[bMagic1], @bMagic2[bMagic2], @bMagic3[bMagic3], @bMagic4[bMagic4], @bMagic5[bMagic5], @bMagic6[bMagic6], 
	@wValue1[wValue1], @wValue2[wValue2], @wValue3[wValue3], @wValue4[wValue4], @wValue5[wValue5], @wValue6[wValue6]*/
  insert TITEMTABLE (dlID, bStorageType, dwStorageID, bOwnerType, dwOwnerID, bItemID, wItemID, bLevel, bCount, bGLevel, dwDuraMax, dwDuraCur, bRefineCur, dEndTime, bGradeEffect,

	bMagic1, bMagic2, bMagic3, bMagic4, bMagic5, bMagic6, 
	wValue1, wValue2, wValue3, wValue4, wValue5, wValue6,
	dwTime1, dwTime2, dwTime3, dwTime4, dwTime5, dwTime6)
  values(@GenID,0,254,0,@dwCharID, @bItemID,@wItemID, @bLevel,1,0,0,0,0,0, @bEffect,
	@bMagic1, @bMagic2, @bMagic3, @bMagic4, @bMagic5, @bMagic6, 
	@wValue1, @wValue2, @wValue3, @wValue4, @wValue5, @wValue6, 
	@dwTime1,0,0,0,0,0)
END
RETURN 0


