

CREATE PROCEDURE [dbo].[TGainCashBonus]
@dwCharID INT,
@bInven TINYINT,
@bItemID TINYINT,
@dwBonus INT
AS

DECLARE @nRet INT
DECLARE @dwUserID INT
SET @dwUserID = 0

SELECT @dwUserID = dwUserID FROM TCHARTABLE WHERE dwCharID = @dwCharID
IF(@@ROWCOUNT = 0 OR @dwUserID = 0)
	RETURN 1

DELETE TITEMTABLE WHERE dwOwnerID=@dwCharID AND bOwnerType=0 AND bStorageType=0 AND dwStorageID=@bInven AND bItemID = @bItemID
EXEC @nRet = [4SDBGLB].TGLOBAL_GSP.DBO.TGainCashBonus @dwUserID, @dwBonus

RETURN @nRet




