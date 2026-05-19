

/* LOGIN PROCESS

========================================================
PARAMETER
========================================================
@szUserID		VARCHAR(50)
@szPasswd		VARCHAR(50)
@dwKEY		INT		OUTPUT
@dwCharID		INT		OUTPUT
@dwUserID		INT		OUTPUT
@szIPAddr		VARCHAR(50)	OUTPUT
@wPort			SMALLINT	OUTPUT

========================================================
RETURN VALUE
========================================================
0	: SUCCESS
1	: NO USER
2	: INVALID PASSWD
3	: DUPLICATE

========================================================
PROCESS
========================================================
1. Find User
2. Check Passwd
3. Check Duplicate

*/

CREATE PROCEDURE [dbo].[TTestLogin]
	@szUserID		VARCHAR(50)	OUTPUT,
	@szPasswd		VARCHAR(50)	OUTPUT,
	@dwKEY		INT		OUTPUT,
	@dwCharID		INT		OUTPUT,
	@dwUserID		INT		OUTPUT,
	@szIPAddr		VARCHAR(50)	OUTPUT,
	@wPort			SMALLINT	OUTPUT

AS
	DECLARE @szValidPasswd	VARCHAR(50)
	DECLARE @bCheck TINYINT
	DECLARE @szTeam VARCHAR(50)
	DECLARE @bEnter TINYINT
	DECLARE @nWebReturn INT
	DECLARE @startTime SMALLDATETIME
	DECLARE @dwDuration INT
	DECLARE @dwPcBangID INT
	DECLARE @bPremium TINYINT
	DECLARE @nReturn INT
	DECLARE @szLoginIP		VARCHAR(50)
	DECLARE @bIPCheck		TINYINT
	DECLARE @bCreateCnt		TINYINT	
	DECLARE @bInPcBang		TINYINT	
	DECLARE @dwPremiumID		INT	

	SET @dwKEY = 0
	SET @dwCharID = 0
	SET @dwUserID = 0
	SET @szIPAddr = ''
	SET @wPort = 0
	SET @bCheck = 0
	SET @bCreateCnt = 6
	SET @bInPcBang = 0
	SET @dwPcBangID = 0
	SET @bPremium = 0
	SET @dwPremiumID = 0
	SET @szLoginIP =''
	SET @szPasswd = ''

	-- 비 접속중인 계정 찾기
	SELECT TOP 1 @dwUserID = dwUserID FROM TACCOUNT_PW WHERE dwUserID NOT IN (SELECT dwUserID FROM TCURRENTUSER) AND dwUserID IN (SELECT DISTINCT dwUserID FROM TGAME.dbo.TCHARTABLE)	
	IF @@ROWCOUNT = 0
	BEGIN
		/* Not found user */
		RETURN 1
	END

	SELECT @szUserID = szUserID, @szPasswd = szPasswd, @bCheck = bCheck FROM TACCOUNT_PW WHERE dwUserID = @dwUserID
	
	IF(@bIPCheck = 6)
	BEGIN
		RETURN 6
	END

	SELECT TOP 1 @startTime = startTime, @dwDuration = dwDuration FROM TUSERPROTECTED WHERE dwUserID = @dwUserID
	IF @@ROWCOUNT > 0
	BEGIN
		IF DATEADD(day, @dwDuration, @startTime) >= GETDATE()
		BEGIN
			RETURN 7
		END
	END


	BEGIN TRAN LOGIN
	SELECT TOP 1 @szIPAddr = szIPAddr, @wPort = wPort, @dwCharID = dwCharID FROM TCURRENTUSER WHERE dwUserID = @dwUserID
	IF @@ROWCOUNT > 0
	BEGIN
		/* User already exists */
		UPDATE TCURRENTUSER SET bLocked = 1 WHERE dwUserID = @dwUserID
		COMMIT TRAN LOGIN
		RETURN 3
	END

--	SELECT TOP 1 @bCreateCnt = bCanCreateCharCount FROM TUSERINFOTABLE WHERE dwUserID = @dwUserID

--	IF(@szLoginIP = '211.112.25.2')
--		SET @dwPcBangID = 0


	SET @szIPAddr = ''
	SET @wPort = 0
	SET @dwCharID = 0


	INSERT INTO TCURRENTUSER(
		dwUserID,
		dwCharID,
		bGroupID,
		bChannel,
		szIPAddr,
		wPort,
		bLocked,
		dwPcBangID,
		szLoginIP) VALUES(
		@dwUserID,	-- dwUserID
		@dwCharID,	-- dwCharID
		0,		-- bGroupID
		0,		-- bChannel
		@szIPAddr,	-- szIPAddr
		@wPort	,	-- wPort
		0,		-- bLocked
		@dwPcBangID,
		@szLoginIP)

	SET @dwKEY = @@IDENTITY


	INSERT INTO TLOG(
		dwKEY,
		dwUserID,
		dwCharID,
		bGroupID,
		bChannel,
		timeLOGIN,
		timeLOGOUT) VALUES(
		@dwKEY,	-- dwKEY
		@dwUserID,	-- dwUserID
		0,		-- dwCharID
		0,		-- bGroupID
		0,		-- bChannel
		GETDATE(),	-- timeLOGIN
		GETDATE())	-- timeLOGOUT



		---------------------------------------------------------------------------------------------------------------------------------------------------
		-- For Statistic	(New User)
		---------------------------------------------------------------------------------------------------------------------------------------------------
		IF EXISTS(  SELECT dwUserID FROM TACCOUNT_PW WHERE dwUserID = @dwUserID )
		BEGIN

			UPDATE TACCOUNT_PW
			SET	szUserID    =  @szUserID,
				dLastLogin = GETDATE()
			WHERE dwUserID = @dwUserID	
		END
		ELSE
		BEGIN
			
			INSERT INTO TACCOUNT_PW
			(
			dwUserID,
			szUserID,
			dFirstLogin,
			dLastLogin
			)
			VALUES

			(
			@dwUserID,
			@szUserID,
			GETDATE(),
			GETDATE()
			)

		END
	
/*
	IF NOT EXISTS(SELECT dwUserID FROM TUSERATTENDTABLE WHERE dwUserID = @dwUserID AND bDay = DAY(GETDATE()))
		INSERT INTO TUSERATTENDTABLE (dwUserID, bDay) VALUES(@dwUserID, DAY(GETDATE()))
*/
	COMMIT TRAN LOGIN

--	IF  NOT EXISTS(SELECT dwUserID FROM TUSERINFOTABLE WHERE dwUserID = @dwUserID AND bAgreement =1)
--		RETURN 8

	RETURN 0

