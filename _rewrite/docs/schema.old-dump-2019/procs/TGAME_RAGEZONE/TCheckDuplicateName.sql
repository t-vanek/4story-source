
CREATE PROCEDURE [dbo].[TCheckDuplicateName]
@dwCharID INT,
@szName VARCHAR(50)
AS
	DECLARE @nRet INT
	DECLARE @dwUserID INT
	SET @dwUserID = 0

	SELECT @dwUserID = dwUserID FROM TCHARTABLE WHERE @dwCharID = dwCharID
	IF(@dwUserID IS NULL OR @dwUserID = 0)
		RETURN 1

	IF EXISTS( SELECT TOP 1 dwCharID FROM TCHARTABLE WHERE szNAME = @szNAME)
		RETURN 1
	/*
   	 * Duplicate npc name 
	 */
	IF EXISTS(SELECT TOP 1 * FROM TNPCCHART WHERE szNAME = @szNAME)
		RETURN 1
	/*
	 * Duplicate monster name
	 */
	IF EXISTS(SELECT TOP 1 * FROM TMONSTERCHART WHERE szNAME = @szNAME)
		RETURN 1

	EXEC @nRet = TGLOBAL_GSP.DBO.TCheckDuplicateName @dwUserID, @szName
	RETURN @nRet


