

CREATE PROCEDURE [dbo].[TClearCurrentUser]
	@dwCharID	INT
AS
	DECLARE @dwUserID INT
	SELECT @dwUserID = dwUserID FROM TCHARTABLE WHERE dwCharID = @dwCharID
	IF(@@ROWCOUNT <> 0)
		EXEC TGLOBAL_GSP.dbo.TClearCurrentUser @dwUserID



