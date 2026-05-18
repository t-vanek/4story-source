


CREATE PROCEDURE [dbo].[TCashItemBuy]
@dwCash INT OUTPUT,
@dwCashBonus INT OUTPUT,
@dlID BIGINT OUTPUT,
@dwTargetCharID INT OUTPUT,
@szPresent VARCHAR(50) OUTPUT,
@dwUserID INT,
@dwCharID INT,
@wCashItemID SMALLINT,
@bBuyType TINYINT,
@szTarget VARCHAR(50)
AS

DECLARE @wItemID SMALLINT
DECLARE @dwMoney INT
DECLARE @dwCharUserID INT
DECLARE @dwReturn INT
DECLARE @dwTarget INT

SET @dwCash = 0
SET @dwCashBonus = 0
SET @dwReturn = 1
SET @dwTarget = 0
SET @dwTargetCharID = 0
SET @szPresent = ''
SET @dlID = 0

SELECT @dwCharUserID = dwUserID FROM TCHARTABLE WHERE dwCharID = @dwCharID
IF(@@ROWCOUNT <> 1 OR @dwCharUserID <> @dwUserID)
	RETURN 2

SELECT @dwMoney = dwMoney, @wItemID = wItemID FROM TVIEW_CASHSHOPITEMCHART WHERE wID = @wCashItemID
IF(@@ROWCOUNT <> 1)
	RETURN 1

IF(@bBuyType =1)
BEGIN
	EXEC @dwReturn =  TGLOBAL_GSP.DBO.TCashItemBuy @dwCash OUTPUT, @dwCashBonus OUTPUT, @szPresent OUTPUT, @dwUserID, @wCashItemID, @dwMoney, @bBuyType, @dwTarget
	IF(@dwReturn <> 0)
		RETURN @dwReturn
END
ELSE
BEGIN
	SELECT @dwTarget = dwUserID,  @dwTargetCharID = dwCharID FROM TCHARTABLE WHERE szName = @szTarget
	IF(@@ROWCOUNT <> 1 OR @dwTarget IS NULL)
		RETURN 4
	EXEC @dwReturn = TGLOBAL_GSP.DBO.TCashItemBuy @dwCash OUTPUT, @dwCashBonus OUTPUT, @szPresent OUTPUT, @dwUserID, @wCashItemID, @dwMoney, @bBuyType, @dwTarget
	IF(@dwReturn <> 0)
		RETURN @dwReturn
END


