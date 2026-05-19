
CREATE PROCEDURE [dbo].[TNewAccountEvent]
@dwUserID INT
AS

INSERT INTO TCASHITEMCABINETTABLE(
	dwUserID, wItemID, bLevel, bCount, bGLevel, dwDuraMax, dwDuraCur, bRefineCur, dEndTime,bGradeEffect,
	bMagic1, bMagic2, bMagic3, bMagic4, bMagic5, bMagic6,
	wValue1, wValue2, wValue3, wValue4, wValue5, wValue6,
	dwTime1, dwTime2, dwTime3, dwTime4, dwTime5, dwTime6)
	SELECT @dwUserID,wItemID,bLevel,1,bGLevel,dwDuraMax,dwDuraCur,bRefineCur,0,bGradeEffect,
		bMagic1, bMagic2, bMagic3, bMagic4, bMagic5, bMagic6,
		wValue1, wValue2, wValue3, wValue4, wValue5, wValue6,
		dwTime1, dwTime2, dwTime3, dwTime4, dwTime5, dwTime6 FROM TCASHSHOPITEMCHART WHERE wID IN(1032,1007)

INSERT INTO TCASHITEMCABINETTABLE(
	dwUserID, wItemID, bLevel, bCount, bGLevel, dwDuraMax, dwDuraCur, bRefineCur, dEndTime,bGradeEffect,
	bMagic1, bMagic2, bMagic3, bMagic4, bMagic5, bMagic6,
	wValue1, wValue2, wValue3, wValue4, wValue5, wValue6,
	dwTime1, dwTime2, dwTime3, dwTime4, dwTime5, dwTime6)
	SELECT @dwUserID,wItemID,bLevel,3,bGLevel,dwDuraMax,dwDuraCur,bRefineCur,0,bGradeEffect,
		bMagic1, bMagic2, bMagic3, bMagic4, bMagic5, bMagic6,
		wValue1, wValue2, wValue3, wValue4, wValue5, wValue6,
		dwTime1, dwTime2, dwTime3, dwTime4, dwTime5, dwTime6 FROM TCASHSHOPITEMCHART WHERE wID = 1087

INSERT INTO TCASHITEMCABINETTABLE(
	dwUserID, wItemID, bLevel, bCount, bGLevel, dwDuraMax, dwDuraCur, bRefineCur, dEndTime,bGradeEffect,
	bMagic1, bMagic2, bMagic3, bMagic4, bMagic5, bMagic6,
	wValue1, wValue2, wValue3, wValue4, wValue5, wValue6,
	dwTime1, dwTime2, dwTime3, dwTime4, dwTime5, dwTime6)
	SELECT @dwUserID,wItemID,bLevel,5,bGLevel,dwDuraMax,dwDuraCur,bRefineCur,0,bGradeEffect,
		bMagic1, bMagic2, bMagic3, bMagic4, bMagic5, bMagic6,
		wValue1, wValue2, wValue3, wValue4, wValue5, wValue6,
		dwTime1, dwTime2, dwTime3, dwTime4, dwTime5, dwTime6 FROM TCASHSHOPITEMCHART WHERE wID = 1058

INSERT INTO TCASHITEMCABINETTABLE(
	dwUserID, wItemID, bLevel, bCount, bGLevel, dwDuraMax, dwDuraCur, bRefineCur, dEndTime,bGradeEffect,
	bMagic1, bMagic2, bMagic3, bMagic4, bMagic5, bMagic6,
	wValue1, wValue2, wValue3, wValue4, wValue5, wValue6,
	dwTime1, dwTime2, dwTime3, dwTime4, dwTime5, dwTime6)
	SELECT @dwUserID,wItemID,bLevel,4,bGLevel,dwDuraMax,dwDuraCur,bRefineCur,0,bGradeEffect,
		bMagic1, bMagic2, bMagic3, bMagic4, bMagic5, bMagic6,
		wValue1, wValue2, wValue3, wValue4, wValue5, wValue6,
		dwTime1, dwTime2, dwTime3, dwTime4, dwTime5, dwTime6 FROM TCASHSHOPITEMCHART WHERE wID = 1075

