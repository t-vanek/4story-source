CREATE PROCEDURE [dbo].[TItemInsert]
@dwTarget INT,
@wItemID SMALLINT,
@bCount TINYINT
AS

IF NOT EXISTS ( SELECT witemID FROM  TITEMCHART WHERE witemID = @wItemID )
	RETURN

INSERT INTO TGLOBAL_GSP.DBO.TCASHITEMCABINETTABLE(
	dwUserID, wItemID, bLevel, bCount, bGLevel, dwDuraMax, dwDuraCur, bRefineCur, dEndTime, bGradeEffect,
	bMagic1, bMagic2, bMagic3, bMagic4, bMagic5, bMagic6,
	wValue1, wValue2, wValue3, wValue4, wValue5, wValue6,
	dwTime1, dwTime2, dwTime3, dwTime4, dwTime5, dwTime6, bGem, wMoggItemID)
 	VALUES(@dwTarget, @wItemID, 0, @bCount, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0)

