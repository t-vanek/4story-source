
CREATE PROCEDURE [dbo].[TGetCashCabinetUseTime]
@dUse SMALLDATETIME OUTPUT,
@dwCash INT OUTPUT,
@dwBonus INT OUTPUT,
@dwCharID INT
AS

DECLARE @dwUserID INT
SET @dUse = 100
SET @dwCash = 0
SET @dwBonus = 0

SELECT @dwUserID = dwUserID FROM TCHARTABLE WHERE dwCharID = @dwCharID
IF(@@ROWCOUNT <> 1)
	RETURN 1

EXEC TCashGet @dwUserID, @dwCash OUTPUT, @dwBonus OUTPUT

SELECT @dUse = dCabinetUse FROM  TGLOBAL_GSP.DBO.TUSERINFOTABLE WHERE dwUserID = @dwUserID

--SET @dUse = GetDate() + 1

