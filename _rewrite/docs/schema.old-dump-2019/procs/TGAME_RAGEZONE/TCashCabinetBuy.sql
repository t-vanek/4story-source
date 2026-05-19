

CREATE PROCEDURE [dbo].[TCashCabinetBuy]
@dateTime SMALLDATETIME,
@dwCharID INT
AS

DECLARE @dwUserID INT

SELECT @dwUserID = dwUserID FROM TCHARTABLE WHERE dwCharID = @dwCharID
IF(@@ROWCOUNT <> 1)
	RETURN 4

EXEC  TGLOBAL_GSP.DBO.TCashCabinetBuy @dateTime,  @dwUserID


