
CREATE PROCEDURE [dbo].[TCashCabinetBuy]
@dateTime SMALLDATETIME OUTPUT,
@dwCash INT OUTPUT,
@dwCashBonus INT OUTPUT,
@dwUserID INT,
@wCashItemID INT
AS

DECLARE @wUseTime INT
DECLARE @dwMoney INT
DECLARE @bSaleValue INT
DECLARE @dCurTime SMALLDATETIME
DECLARE @dSetTime SMALLDATETIME
DECLARE @dCabinetUse SMALLDATETIME

BEGIN TRAN BUYCASHCABINET

SELECT @wUseTime = wUseTime, @dwMoney = dwMoney, @bSaleValue = bSaleValue FROM TCASHSHOPITEMCHART where wID = @wCashItemID
IF (@@ROWCOUNT = 0)
	RETURN 5 -- Not Found

SET @dwMoney = @dwMoney * (1 - (@bSaleValue) / 100)

EXEC TCashGet @dwUserID, @dwCash OUTPUT, @dwCashBonus OUTPUT

IF (@dwCash < @dwMoney)
	RETURN 3 -- Need Cash

SET @dwCash = @dwCash - @dwMoney
UPDATE TCASHTESTTABLE SET dwCash = @dwCash WHERE dwUserID = @dwUserID

SET @dCurTime = GetDate()

IF NOT EXISTS( SELECT dwUserID FROM TUSERINFOTABLE WHERE dwUserID = @dwUserID)
	BEGIN
		SET @dSetTime = DATEADD(DAY, @wUseTime, @dCurTime)
		INSERT INTO TUSERINFOTABLE (dwUserID, bCanCreateCharCount, bAgreement, dCabinetUse) 
		VALUES(@dwUserID, 6, 1, @dateTime)
	END
ELSE
	BEGIN
		SELECT @dCabinetUse = dCabinetUse FROM TUSERINFOTABLE WHERE dwUserID = @dwUserID
		IF (@dCabinetUse < @dCurTime)
			SET @dSetTime = DATEADD(DAY, @wUseTime, @dCurTime)
		ELSE
			SET @dSetTime = DATEADD(DAY, @wUseTime, @dCabinetUse)
		
		UPDATE TUSERINFOTABLE SET dCabinetUse = @dSetTime WHERE dwUserID=@dwUserID
	END

COMMIT TRAN BUYCASHCABINET

RETURN 0

