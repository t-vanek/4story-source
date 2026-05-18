
CREATE PROCEDURE [dbo].[TEventItemPutinCashCabinet]
@bItemChartType TINYINT,
@dwUserID INT,
@dwItemID INT,
@bCount TINYINT
AS

IF(@bItemChartType = 1)
BEGIN
	IF NOT EXISTS(SELECT wItemID FROM TITEMCHART WHERE wItemID = @dwItemID)
	BEGIN
		PRINT 'There is no item'
		RETURN 1
	END

	INSERT INTO  TGLOBAL_GSP.DBO.TCASHITEMCABINETTABLE(
		dwUserID, wItemID, bLevel, bCount, bGLevel, dwDuraMax, dwDuraCur, bRefineCur, dEndTime,bGradeEffect,
		bMagic1, bMagic2, bMagic3, bMagic4, bMagic5, bMagic6,
		wValue1, wValue2, wValue3, wValue4, wValue5, wValue6,
		dwTime1, dwTime2, dwTime3, dwTime4, dwTime5, dwTime6,
		bWorldID)
	SELECT @dwUserID, wItemID, 0, @bCount, 0, dwDuraMax, dwDuraMax, 0,0,0,
		0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0,
		0 
	FROM TITEMCHART WHERE wItemID = @dwItemID
END

ELSE IF(@bItemChartType =2)
BEGIN
	IF NOT EXISTS(SELECT dwID FROM TQUESTITEMCHART WHERE dwID = @dwItemID)
	BEGIN
		PRINT 'There is no item'
		RETURN 2
	END

	INSERT INTO  TGLOBAL_GSP.DBO.TCASHITEMCABINETTABLE(
		dwUserID, wItemID, bLevel, bCount, bGLevel, dwDuraMax, dwDuraCur, bRefineCur, dEndTime,bGradeEffect,
		bMagic1, bMagic2, bMagic3, bMagic4, bMagic5, bMagic6,
		wValue1, wValue2, wValue3, wValue4, wValue5, wValue6,
		dwTime1, dwTime2, dwTime3, dwTime4, dwTime5, dwTime6,
		bWorldID)
	SELECT @dwUserID, wItemID, bLevel, @bCount, bGLevel, dwDuraMax, dwDuraCur, bRefineCur, 0,bGradeEffect,
		bMagic1, bMagic2, bMagic3, bMagic4, bMagic5, bMagic6,
		wValue1, wValue2, wValue3, wValue4, wValue5, wValue6,
		dwTime1, dwTime2, dwTime3, dwTime4, dwTime5, dwTime6,
		0
	 FROM TQUESTITEMCHART WHERE dwID = @dwItemID
END

ELSE IF(@bItemChartType =3)
BEGIN
	IF NOT EXISTS(SELECT wID FROM TVIEW_CASHSHOPITEMCHART WHERE wID = @dwItemID)
	BEGIN
		PRINT 'There is no item'
		RETURN 2
	END

	INSERT INTO  TGLOBAL_GSP.DBO.TCASHITEMCABINETTABLE(
		dwUserID, wItemID, bLevel, bCount, bGLevel, dwDuraMax, dwDuraCur, bRefineCur, dEndTime,bGradeEffect,
		bMagic1, bMagic2, bMagic3, bMagic4, bMagic5, bMagic6,
		wValue1, wValue2, wValue3, wValue4, wValue5, wValue6,
		dwTime1, dwTime2, dwTime3, dwTime4, dwTime5, dwTime6,
		bWorldID)
	SELECT @dwUserID, wItemID, bLevel, @bCount, bGLevel, dwDuraMax, dwDuraCur, bRefineCur, 0,bGradeEffect,
		bMagic1, bMagic2, bMagic3, bMagic4, bMagic5, bMagic6,
		wValue1, wValue2, wValue3, wValue4, wValue5, wValue6,
		dwTime1, dwTime2, dwTime3, dwTime4, dwTime5, dwTime6,
		0
	 FROM TVIEW_CASHSHOPITEMCHART WHERE wID = @dwItemID
END

ELSE
	PRINT 'Select ITEMCHARTTYPE between 1~3'


