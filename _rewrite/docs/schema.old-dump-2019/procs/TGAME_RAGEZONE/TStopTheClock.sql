
CREATE PROCEDURE [dbo].[TStopTheClock]
@dwCharID INT,
@wShopItemID SMALLINT
 AS

	DECLARE @dwReturn INT
	DECLARE @dwCash INT
	DECLARE @dwCashBonus INT
	DECLARE @szPresent VARCHAR(50)
	DECLARE @dwUserID INT
	DECLARE @dwMoney INT


	SELECT @dwUserID = dwUserID FROM TCHARTABLE WHERE dwCharID = @dwCharID
	IF(@dwUserID IS NULL)
		RETURN 4
	
	SELECT @dwMoney = dwMoney FROM TVIEW_CASHSHOPITEMCHART WHERE wID = @wShopItemID
	IF(@dwMoney IS NULL)
		RETURN 1

	EXEC @dwReturn =  TGLOBAL_GSP.DBO.TCashItemBuy @dwCash OUTPUT, @dwCashBonus OUTPUT, @szPresent OUTPUT, @dwUserID, @wShopItemID, @dwMoney, 1, 0
	IF(@dwReturn <> 0)
		RETURN @dwReturn


