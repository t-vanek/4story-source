CREATE PROCEDURE [dbo].[GetUserID]
	@szUserID		VARCHAR(50)
AS
	DECLARE @szPasswd		VARCHAR(50)
	DECLARE @nWebReturn	INT
	DECLARE @dwUserID		INT

	SET @dwUserID = 0

	EXEC @nWebReturn = [192.168.1.9,6121].fourstory_ob.memlogin.csp_gamelogincheck @szUserID, @szPasswd, @dwUserID output
	RETURN @dwUserID

