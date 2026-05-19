CREATE PROCEDURE [dbo].[TItemInsert_runes]
@dwTarget INT,
@wItemID INT,
@bCount INT,
@wMagic1 INT,
@wMagic2 INT,
@wMagic3 INT,
@wMagic4 INT,
@wMagic5 INT,
@wMagic6 INT,
@wValue1 INT,
@wValue2 INT,
@wValue3 INT,
@wValue4 INT,
@wValue5 INT,
@wValue6 INT
AS

IF NOT EXISTS ( SELECT witemID FROM  TITEMCHART WHERE witemID = @wItemID )
	RETURN

INSERT INTO TGLOBAL_GSP.DBO.TCASHITEMCABINETTABLE(
	dwUserID, wItemID, bLevel, bCount, bGLevel, dwDuraMax, dwDuraCur, bRefineCur, dEndTime, bGradeEffect,
	bMagic1, bMagic2, bMagic3, bMagic4, bMagic5, bMagic6,
	wValue1, wValue2, wValue3, wValue4, wValue5, wValue6,
	dwTime1, dwTime2, dwTime3, dwTime4, dwTime5, dwTime6, bGem, wMoggItemID)
 	VALUES(@dwTarget, @wItemID, 0, @bCount, 0, 0, 0, 0, 0, 0,
	@wMagic1, @wMagic2, @wMagic3, @wMagic4, @wMagic5, @wMagic6,
	@wValue1, @wValue2, @wValue3, @wValue4, @wValue5, @wValue6,
	0, 0, 0, 0, 0, 0, 0, 0)


