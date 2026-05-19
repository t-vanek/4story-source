
CREATE PROCEDURE [dbo].[TCashItemBuy]
@dwCash INT OUTPUT,
@dwCashBonus INT OUTPUT,
@szPresent VARCHAR(50) OUTPUT,
@dwUserID INT,
@wCashItemID SMALLINT,
@dwMoney INT,
@bBuyType TINYINT,
@dwTarget INT
AS

DECLARE @nRet INT
DECLARE @bConsumType TINYINT 		--구매 타입(0:자동, 1:캐쉬, 2:마일리지)
DECLARE @bMsgCode	TINYINT		--오류정보(0:오류 없음, 1:아이템이 없음, 2:캐쉬 포인트가 없음, 3:캐쉬가 부족)
DECLARE @dwPcBangID INT
DECLARE @dwRealTarget INT
DECLARE @szLoginIP VARCHAR(50)
DECLARE @dCurTime SMALLDATETIME
DECLARE @dSetTime SMALLDATETIME
DECLARE @wUseTime SMALLINT

SET @bConsumType = 0
SET @dwPcBangID  = 0
SET @szLoginIP = ''
SET @nRet = 0
SET @szPresent = ''
SET @dCurTime = GetDate()
SET @dwCash = 0
SET @dwCashBonus = 0
SET @wUseTime = 0

SELECT @szPresent = szName, @wUseTime = wUseTime FROM TCASHSHOPITEMCHART WHERE wID = @wCashItemID 
IF(@@ROWCOUNT <> 1 OR @szPresent IS NULL)
BEGIN
	SET @szPresent = ''
	RETURN 1
END

SELECT @dwPcBangID = dwPcBangID, @szLoginIP = szLoginIP FROM TCURRENTUSER WHERE dwUserID = @dwUserID

-- TEST TABLE############################
SELECT @dwCash = dwCash, @dwCashBonus=dwBonus FROM TCASHTESTTABLE WHERE dwUserID = @dwUserID
IF(@@ROWCOUNT < 1 OR @dwCash<@dwMoney)
	RETURN 3

SET @dwCash = @dwCash - @dwMoney
UPDATE TCASHTESTTABLE SET dwCash = @dwCash WHERE dwUserID = @dwUserID

--######################################
--EXEC @nRet = [192.168.1.9,6121].FourStory_Cash.FourStory_Web.Fun_BuyTheItem @bConsumType, @wCashItemID, @dwUserID, @dwMoney, @dwPcBangID, @bBuyType, @dwTarget, @szLoginIP, @bMsgCode OUTPUT, @dwCash OUTPUT, @dwCashBonus OUTPUT

IF(@nRet = 0)
BEGIN
	SET @dSetTime = DATEADD(MINUTE, 60-DATEPART(MINUTE, @dCurTime), @dCurTime)
--	SET @dSetTime = DATEADD(HOUR, 24-DATEPART(HOUR, @dSetTime), @dSetTime) + 1


	IF(@bBuyType = 2)
		SET @dwRealTarget = @dwTarget
	ELSE
		SET @dwRealTarget = @dwUserID

	IF EXISTS(SELECT * FROM TCASHBONUSITEMCHART WHERE wCashItemID = @wCashItemID)
	BEGIN
		IF(@wUseTime > 0)
			INSERT INTO TCASHITEMCABINETTABLE(
				dwUserID, wItemID, bLevel, bCount, bGLevel, dwDuraMax, dwDuraCur, bRefineCur, dEndTime,bGradeEffect,
				bMagic1, bMagic2, bMagic3, bMagic4, bMagic5, bMagic6,
				wValue1, wValue2, wValue3, wValue4, wValue5, wValue6,
				dwTime1, dwTime2, dwTime3, dwTime4, dwTime5, dwTime6)
			SELECT @dwRealTarget, CS.wItemID, CS.bLevel, CB.bBonusItemCount, CS.bGLevel, CS.dwDuraMax, CS.dwDuraCur, CS.bRefineCur, @dSetTime+wUseTime, bGradeEffect,
				bMagic1, bMagic2, bMagic3, bMagic4, bMagic5, bMagic6,
				wValue1, wValue2, wValue3, wValue4, wValue5, wValue6,
				dwTime1, dwTime2, dwTime3, dwTime4, dwTime5, dwTime6
			FROM TCASHSHOPITEMCHART AS CS INNER JOIN TCASHBONUSITEMCHART AS CB ON CS.wID = CB.wBonusItem WHERE CB.wCashItemID = @wCashItemID
		ELSE
			INSERT INTO TCASHITEMCABINETTABLE(
				dwUserID, wItemID, bLevel, bCount, bGLevel, dwDuraMax, dwDuraCur, bRefineCur, dEndTime,bGradeEffect,
				bMagic1, bMagic2, bMagic3, bMagic4, bMagic5, bMagic6,
				wValue1, wValue2, wValue3, wValue4, wValue5, wValue6,
				dwTime1, dwTime2, dwTime3, dwTime4, dwTime5, dwTime6)
			SELECT @dwRealTarget, CS.wItemID, CS.bLevel, CB.bBonusItemCount, CS.bGLevel, CS.dwDuraMax, CS.dwDuraCur, CS.bRefineCur, 0,bGradeEffect,
				bMagic1, bMagic2, bMagic3, bMagic4, bMagic5, bMagic6,
				wValue1, wValue2, wValue3, wValue4, wValue5, wValue6,
				dwTime1, dwTime2, dwTime3, dwTime4, dwTime5, dwTime6
			FROM TCASHSHOPITEMCHART AS CS INNER JOIN TCASHBONUSITEMCHART AS CB ON CS.wID = CB.wBonusItem WHERE CB.wCashItemID = @wCashItemID

		UPDATE TCASHITEMCABINETTABLE SET dlID = 8718968878589280256+dwID WHERE dlID=0
	END
END

RETURN @nRet

