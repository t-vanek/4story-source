
CREATE PROCEDURE [dbo].[TCashItemPutin]
@dwNewID INT OUTPUT,
@dwCash INT OUTPUT,
@dwCashBonus INT OUTPUT,
@dwCharID INT,
@bInven TINYINT,
@bItemID TINYINT
AS

DECLARE @dwReturn INT
DECLARE @dwUserID INT
DECLARE @wCount SMALLINT
DECLARE @dUseTime SMALLDATETIME
DECLARE @dwMoney INT
DECLARE @wCashItemID SMALLINT
DECLARE @szPresent VARCHAR(50)
DECLARE @dwTarget INT
DECLARE @bBuyType TINYINT
DECLARE @bType TINYINT
DECLARE @bMax_Dura TINYINT
DECLARE @bMax_Quan TINYINT
DECLARE @wItemID SMALLINT
DECLARE @bWorldID TINYINT
DECLARE @bLevel TINYINT
DECLARE @bRefine TINYINT

SET @bMax_Dura = 30
SET @bMax_Quan = 10

SET @dwReturn = 0
SET @dwNewID= 0
SET @dwCash = 0
SET @dwCashBonus = 0
SET @dwMoney = 0
SET @wCashItemID = 3001
SET @szPresent = ''
SET @dwTarget = 0
SET @bBuyType = 1
SET @bType=1
SET @bWorldID = 0

SELECT @dwUserID = dwUserID FROM TCHARTABLE WHERE dwCharID = @dwCharID
IF(@@ROWCOUNT = 0)
	RETURN 4

SELECT @wItemID = wItemID, @bLevel=bLevel, @bRefine=bRefineCur FROM TITEMTABLE WHERE dwOwnerID=@dwCharID AND bOwnerType=0 AND bStorageType=0 AND dwStorageID=@bInven AND bItemID = @bItemID
IF(@@ROWCOUNT = 0)
	RETURN 5

SELECT @dwMoney = dwMoney FROM TVIEW_CASHSHOPITEMCHART WHERE wID = @wCashItemID
IF(@@ROWCOUNT = 0)
	RETURN 1

IF(@bLevel > 0 OR  @bRefine > 0)
	SELECT @bWorldID = bWorld+1 FROM TDBITEMINDEXTABLE

IF EXISTS(SELECT wItemID FROM TITEMCHART WHERE wItemID = @wItemID AND bIsSpecial = 0)
BEGIN
	SELECT @dUseTime = dCabinetUse FROM  TGLOBAL.DBO.TUSERINFOTABLE WHERE dwUserID=@dwUserID
	IF(@dUseTime IS NULL OR @dUseTime < GetDate())
	BEGIN
		SELECT @wCount = COUNT(dwID) FROM  TVIEW_CASHITEMCABINET INNER JOIN TITEMCHART ON TVIEW_CASHITEMCABINET.wItemID = TITEMCHART.wItemID WHERE dwUserID = @dwUserID AND bIsSpecial = 0
		IF(@@ROWCOUNT > 0 AND @wCount >= @bMax_Quan)
			RETURN 6
	
		EXEC @dwReturn =  TGLOBAL.DBO.TCashItemBuy @dwCash OUTPUT, @dwCashBonus OUTPUT, @szPresent OUTPUT, @dwUserID, @wCashItemID, @dwMoney, @bBuyType, @dwTarget
		IF(@dwReturn <> 0)
			RETURN @dwReturn
	END
	ELSE
	BEGIN
		SELECT @wCount = COUNT(dwID) FROM  TVIEW_CASHITEMCABINET  INNER JOIN TITEMCHART ON TVIEW_CASHITEMCABINET.wItemID = TITEMCHART.wItemID WHERE dwUserID = @dwUserID AND bIsSpecial = 0
		IF(@@ROWCOUNT>0 AND @wCount >= @bMax_Dura)
		BEGIN
			IF(@wCount >= @bMax_Quan + @bMax_Dura)
				RETURN 6
	
			SET @dwMoney = @dwMoney/2
			EXEC @dwReturn =  TGLOBAL.DBO.TCashItemBuy @dwCash OUTPUT, @dwCashBonus OUTPUT, @szPresent OUTPUT, @dwUserID, @wCashItemID, @dwMoney, @bBuyType, @dwTarget
			IF(@dwReturn <> 0)
				RETURN @dwReturn
		END
	END
END

INSERT INTO  TGLOBAL.DBO.TCASHITEMCABINETTABLE(
	dwUserID, wItemID, bLevel, bCount, bGLevel, dwDuraMax, dwDuraCur, bRefineCur, dEndTime,bGradeEffect,
	bMagic1, bMagic2, bMagic3, bMagic4, bMagic5, bMagic6,
	wValue1, wValue2, wValue3, wValue4, wValue5, wValue6,
	dwTime1, dwTime2, dwTime3, dwTime4, dwTime5, dwTime6,
	bWorldID, bGem)
SELECT @dwUserID, wItemID, bLevel, bCount, bGLevel, dwDuraMax, dwDuraCur, bRefineCur, dEndTime,bGradeEffect,
	bMagic1, bMagic2, bMagic3, bMagic4, bMagic5, bMagic6,
	wValue1, wValue2, wValue3, wValue4, wValue5, wValue6,
	dwTime1, dwTime2, dwTime3, dwTime4, dwTime5, dwTime6, bGem,
	@bWorldID 
 FROM TITEMTABLE WHERE dwOwnerID=@dwCharID AND bOwnerType=0 AND bStorageType=0 AND dwStorageID=@bInven AND bItemID = @bItemID

SET @dwNewID = @@IDENTITY
EXEC TCashGet @dwUserID, @dwCash OUTPUT, @dwCashBonus OUTPUT

DELETE TITEMTABLE WHERE dwOwnerID=@dwCharID AND bOwnerType=0 AND bStorageType=0 AND dwStorageID=@bInven AND bItemID = @bItemID


