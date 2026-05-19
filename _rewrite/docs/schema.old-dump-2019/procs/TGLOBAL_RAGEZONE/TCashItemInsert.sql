CREATE PROCEDURE [dbo].[TCashItemInsert]
@dwTarget INT,
@wCashItemID SMALLINT,
@bCount TINYINT
AS

DECLARE @dCurTime SMALLDATETIME
DECLARE @dSetTime SMALLDATETIME

SET @dCurTime = GetDate()

IF( NOT EXISTS ( SELECT wID FROM TCASHSHOPITEMCHART WHERE wID = @wCashItemID ))
	RETURN

SET @dSetTime = DATEADD(MINUTE, 60-DATEPART(MINUTE, @dCurTime), @dCurTime)
SET @dSetTime = DATEADD(HOUR, 24-DATEPART(HOUR, @dSetTime), @dSetTime) + 1

INSERT INTO TCASHITEMCABINETTABLE(
	dwUserID, wItemID, bLevel, bCount, bGLevel, dwDuraMax, dwDuraCur, bRefineCur, dEndTime,bGradeEffect,
	bMagic1, bMagic2, bMagic3, bMagic4, bMagic5, bMagic6,
	wValue1, wValue2, wValue3, wValue4, wValue5, wValue6,
	dwTime1, dwTime2, dwTime3, dwTime4, dwTime5, dwTime6)
SELECT @dwTarget, wItemID, bLevel, @bCount, bGLevel, dwDuraMax, dwDuraCur, bRefineCur, @dSetTime+wUseTime, bGradeEffect,
	bMagic1, bMagic2, bMagic3, bMagic4, bMagic5, bMagic6,
	wValue1, wValue2, wValue3, wValue4, wValue5, wValue6,
	dwTime1, dwTime2, dwTime3, dwTime4, dwTime5, dwTime6
FROM TCASHSHOPITEMCHART WHERE @wCashItemID=wID AND wUseTime>0

	INSERT INTO TCASHITEMCABINETTABLE(
	dwUserID, wItemID, bLevel, bCount, bGLevel, dwDuraMax, dwDuraCur, bRefineCur, dEndTime,bGradeEffect,
	bMagic1, bMagic2, bMagic3, bMagic4, bMagic5, bMagic6,
	wValue1, wValue2, wValue3, wValue4, wValue5, wValue6,
	dwTime1, dwTime2, dwTime3, dwTime4, dwTime5, dwTime6)
SELECT @dwTarget, wItemID, bLevel, @bCount, bGLevel, dwDuraMax, dwDuraCur, bRefineCur, 0, bGradeEffect,
	bMagic1, bMagic2, bMagic3, bMagic4, bMagic5, bMagic6,
	wValue1, wValue2, wValue3, wValue4, wValue5, wValue6,
	dwTime1, dwTime2, dwTime3, dwTime4, dwTime5, dwTime6
FROM TCASHSHOPITEMCHART WHERE @wCashItemID=wID AND wUseTime=0

