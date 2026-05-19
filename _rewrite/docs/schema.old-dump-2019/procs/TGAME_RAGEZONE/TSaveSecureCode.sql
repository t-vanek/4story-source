CREATE PROCEDURE [dbo].[TSaveSecureCode]
@dwUserID INT,
@strCode VARCHAR(50),
@bTries TINYINT,
@bEnabled TINYINT,
@dwLockTick INT
AS

IF EXISTS( SELECT dwUserID FROM TGLOBAL_GSP.dbo.TSECURECODE WHERE dwUserID = @dwUserID)
BEGIN
	UPDATE TGLOBAL_GSP.dbo.TSECURECODE SET strSecurityCode = @strCode, bEnabled = @bEnabled, bTries = @bTries, iLockTick = @dwLockTick WHERE dwUserID = @dwUserID
END
ELSE
BEGIN
	INSERT INTO TGLOBAL_GSP.dbo.TSECURECODE (strSecurityCode, bEnabled, bTries, iLockTick, dwUserID)
																		VALUES (@strCode, @bEnabled, @bTries, @dwLockTick, @dwUserID)
END
