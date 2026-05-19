
CREATE PROCEDURE [dbo].[TCashGet]
@dwUserID INT,
@dwCash INT OUTPUT,
@dwCashBonus INT OUTPUT
AS

DECLARE @nRet INT
DECLARE @bMsgCode	TINYINT

--EXEC @nRet = [192.168.1.9,6121].FourStory_Cash.FourStory_Web.Fun_GetCashPoint @dwUserID, @bMsgCode OUTPUT, @dwCash OUTPUT, @dwCashBonus OUTPUT 
--IF(@nRet = 0)
--BEGIN
	SELECT @dwCash = dwCash, @dwCashBonus = dwBonus FROM TCASHTESTTABLE WHERE dwUserID=@dwUserID
--END

