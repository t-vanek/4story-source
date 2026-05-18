
CREATE PROCEDURE [dbo].[TCMGiftLog]
	@dwUserID	INT,
	@dwCharID	INT,
	@wGiftID	SMALLINT,
	@dwGMCharID	INT,
	@wErrID	SMALLINT
AS

INSERT TCMGIFTTABLE (dwUserID, dwCharID, wGiftID, tTakeDate, dwGMCharID, wErrID)
VALUES(@dwUserID, @dwCharID, @wGiftID, GETDATE(), @dwGMCharID, @wErrID)


