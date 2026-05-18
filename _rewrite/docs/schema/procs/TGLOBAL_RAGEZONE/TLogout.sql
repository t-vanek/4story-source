/* LOGOUT PROCESS

========================================================
PARAMETER
========================================================
@dwUserID		INT

========================================================
RETURN VALUE
========================================================
0	: SUCCESS
1	: NO USER

========================================================
PROCESS
========================================================
1. Check TCURRENTUSER table
2. Delete user from TCURRENTUSER
3. Update log data

*/

CREATE PROCEDURE [dbo].[TLogout]
	@dwUserID	INT,
	@dwCharID	INT,
	@bLevel	TINYINT,
	@dwExp	INT
AS
	DECLARE @dwCurCharID	INT
	DECLARE @dwKEY		INT

	DECLARE @bGroupID		TINYINT
	DECLARE @bChannel		TINYINT

	DECLARE @dLoginDate		SMALLDATETIME

	BEGIN TRAN LOGOUT
	SELECT TOP 1 @dwKEY = dwKEY, @dwCurCharID = dwCharID, @bGroupID = bGroupID, @bChannel = bChannel, @dLoginDate = dLoginDate FROM TCURRENTUSER WHERE dwUserID = @dwUserID
	IF @@ROWCOUNT = 0
	BEGIN
		/* Not found connected user */
		COMMIT TRAN LOGOUT
		RETURN 1
	END

	UPDATE TLOG SET
		dwCharID = @dwCurCharID,
		bGroupID = @bGroupID,
		bChannel = @bChannel,
		timeLOGOUT = GETDATE()
	WHERE dwKEY = @dwKEY


	/*
	 *	Update Connect Info
	 */
	IF( @dwCurCharID <>  0 AND @dwCharID = @dwCurCharID )
	BEGIN

		UPDATE TALLCHARTABLE
		    SET	 bLevel 		= 	@bLevel,
			 dwExp		=	@dwExp,
			 dLoginDate	=	@dLoginDate,
			 dLogoutDate	=	GETDATE(),
		 	 dwPlayTime	=	dwPlayTime + DATEDIFF(second, @dLoginDate, getdate()) 
		WHERE dwCharID	=	@dwCharID
		AND	 bWorldID	=	@bGroupID

	END

	DELETE FROM TCURRENTUSER WHERE dwUserID = @dwUserID
	COMMIT TRAN LOGOUT

	RETURN 0

