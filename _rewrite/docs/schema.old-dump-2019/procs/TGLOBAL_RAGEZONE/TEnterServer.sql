/* ENTER SERVER PROCESS

========================================================
PARAMETER
========================================================
@dwKEY		INT
@dwUserID		INT
@dwCharID		INT
@bGroupID		TINYINT
@bChannel		TINYINT
@szIPAddr		VARCHAR(50)
@wPort			SMALLINT

========================================================
RETURN VALUE
========================================================
0	: SUCCESS
1	: FAILED

========================================================
PROCESS
========================================================

*/


CREATE PROCEDURE [dbo].[TEnterServer]
	@dwKEY		INT,
	@dwUserID		INT,
	@dwCharID		INT,
	@bGroupID		TINYINT,
	@bChannel		TINYINT,
	@szIPAddr		VARCHAR(50),
	@wPort			SMALLINT
AS
	BEGIN TRAN TENTERSERVER
	IF NOT EXISTS(SELECT TOP 1 dwUserID FROM TCURRENTUSER WHERE dwKEY = @dwKEY AND dwUserID = @dwUserID AND bLocked = 0)
	BEGIN
		/* Certification failed */
		COMMIT TRAN TENTERSERVER
		RETURN 1
	END

	DECLARE @dReleaseDate SMALLDATETIME
	SELECT @dReleaseDate = dReleaseDate FROM releaseDate

	DECLARE @dCheckTime SMALLDATETIME
	SET @dCheckTime = DATEADD("d", 7, @dReleaseDate)

	IF(CONVERT(date, GETDATE()) < @dCheckTime)
	BEGIN
		IF NOT EXISTS(SELECT dwCharID FROM TGAME_GSP.dbo.TTITLETABLE WHERE dwCharID = @dwCharID AND wTitleID = 50)
			INSERT INTO TGAME_GSP.dbo.TTITLETABLE VALUES(@dwCharID, 50, 0)
	END

	UPDATE TCURRENTUSER SET
		dwCharID = @dwCharID,
		bGroupID = @bGroupID,
		bChannel = @bChannel,
		szIPAddr = @szIPAddr,
		wPort = @wPort
	WHERE dwUserID = @dwUserID

	COMMIT TRAN TENTERSERVER
	RETURN 0

