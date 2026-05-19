
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

CREATE PROCEDURE [dbo].[TLogin]
	@szUserID		VARCHAR(50),
	@szPasswd		VARCHAR(50),
	@szLoginIP		VARCHAR(50),
	@bIPCheck		TINYINT,
	@dwKEY		INT		OUTPUT,
	@dwCharID		INT		OUTPUT,
	@dwUserID		INT		OUTPUT,
	@szIPAddr		VARCHAR(50)	OUTPUT,
	@wPort			SMALLINT	OUTPUT,
	@bCreateCnt		TINYINT	OUTPUT,
	@bInPcBang		TINYINT	OUTPUT,
	@dwPremiumID		INT		OUTPUT
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
	DECLARE @bEternal TINYINT

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

--	EXEC @nWebReturn = [192.168.1.9,6121].fourstory_ob.memlogin.csp_gamelogincheck @szUserID, @szPasswd, @szLoginIP, @dwUserID output, @dwPcBangID output, @bPremium output
	
--	IF (@@ERROR <> 0 OR @nWebReturn <> 0)
--	BEGIN
		/* Not found user */
--		RETURN @nWebReturn
--	END

--	INSERT INTO TLogTest (dwId, szLog) VALUES (10, @szPasswd)
DECLARE @BLACKLISTIP VARCHAR(50)
SET @BLACKLISTIP = (SELECT szIP FROM IPBLACKLIST_game WHERE szIP = @szLoginIP)
IF @szLoginIP = @BLACKLISTIP
RETURN 7

	SELECT TOP 1 @dwUserID = dwUserID, @bCheck=bCheck FROM TACCOUNT_PW WHERE szUserID = @szUserID
	IF @@ROWCOUNT = 0
	BEGIN
		/* Not found user */
		RETURN 1
	END
	
	SELECT TOP 1 @dwUserID = dwUserID, @bCheck=bCheck FROM TACCOUNT_PW WHERE szUserID = @szUserID AND @szPasswd = szPasswd
	IF @@ROWCOUNT = 0
	BEGIN
		/* Invalid Password */
		RETURN 2
	END

/*DECLARE @dwChanged INT

SET @dwChanged = (SELECT dwChanged FROM TACCOUNT_PW WHERE @dwUserID = dwUserID)

IF @dwChanged = 0
RETURN 2 */

	IF(@bIPCheck = 6)
	BEGIN
		RETURN 6
	END

	SELECT TOP 1 @startTime = startTime, @dwDuration = dwDuration, @bEternal = bEternal 
	FROM TUSERPROTECTED 
	WHERE dwUserID = @dwUserID 
	AND DATEADD( DAY, dwDuration, startTime) >= GETDATE()

	SELECT dwUserID FROM TUSERPROTECTED  WHERE dwUserID = @dwUserID  AND DATEADD( DAY, dwDuration, startTime) >= GETDATE()
	IF @@ROWCOUNT > 0
	BEGIN
		RETURN 7
	END
	
	SELECT dwUserID FROM TUSERPROTECTED 	WHERE dwUserID = @dwUserID AND	bEternal = 1	
	IF @@ROWCOUNT > 0
	BEGIN
		RETURN 7
	END


--	EXEC @nReturn = TCheckUnify @dwUserID
--	IF(@nReturn <> 0)
--		RETURN 9

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

	SET @dwPcBangID = 0

	IF(@dwPcBangID <> 0)
	BEGIN
		SET @bInPcBang = 1
		IF(@bPremium <> 0)
			SET @dwPremiumID = @dwPcBangID
	END

	SET @szIPAddr = ''
	SET @wPort = 0
	SET @dwCharID = 0

INSERT INTO USERIPLOG (IP, USERNAME, Date_time) VALUES (@szLoginIP, @szUserID, GETDATE())
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

	IF  NOT EXISTS(SELECT dwUserID FROM TUSERINFOTABLE WHERE dwUserID = @dwUserID AND bAgreement =1)
		RETURN 8

	RETURN 0

