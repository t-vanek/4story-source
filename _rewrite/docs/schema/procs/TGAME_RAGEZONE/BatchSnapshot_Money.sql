
/*
	캐릭터가 현재 보유한 돈에 관련된 통계
*/
CREATE PROCEDURE [dbo].[BatchSnapshot_Money]
	@pWorld	INT,			
	@pDate	SMALLDATETIME
AS

	DECLARE @mAllMoney	MONEY
	DECLARE @nAllCount	INT

	DECLARE @mStartDate		SMALLDATETIME

	DECLARE @mActiveMoney	MONEY
	DECLARE @nActiveCount	INT

	DECLARE @mGuildMoney	MONEY
	DECLARE @nGuildCount	INT

	--
	--  Get All  Moeny
	--
	SELECT 	@mAllMoney 	= 	SUM( CONVERT( MONEY, dwGold * 1000000 + dwSilver * 1000 + dwCooper )), 
			@nAllCount	=	COUNT(*)
	FROM	TCHARTABLE
	WHERE	bDelete = 0

	--	
	-- Get Active Moeny
	--	
	SET	@mStartDate	= DATEADD(HOUR, -1, GETDATE())

	SELECT 	@mActiveMoney 	= 	ISNULL( SUM( CONVERT( MONEY, dwGold * 1000000 + dwSilver * 1000 + dwCooper )), 0 ),
			@nActiveCount		=	COUNT(*)
	FROM	TCHARTABLE
	WHERE	bDelete = 0
	AND		dwCharID	IN(
						SELECT 	dwCharID
						FROM		TGLOBAL_GSP.DBO.TCURRENTUSER
						WHERE	dwCharID <> 0
						      AND	bGroupID  = 	@pWorld
					)
	


	--
	-- GET Guild Money
	--
	SELECT	@mGuildMoney	= ISNULL( SUM( CONVERT( MONEY, dwGold * 1000000 + dwSilver * 1000 + dwCooper )), 0 ),
			@nGuildCount  = COUNT(*) 
	FROM	TGUILDTABLE



	--
	-- Insert Statistic
	--

	DELETE TGLOBAL_GSP.DBO.SDATA130TL WHERE SDATA130_DATE = @pDate AND SDATA130_WORLD = @pWorld

	INSERT INTO TGLOBAL_GSP.DBO.SDATA130TL
	(
		SDATA130_DATE,
		SDATA130_WORLD,
		SDATA130_ALLMONEY,
		SDATA130_ALLCNT,
		SDATA130_ACTIVEMONEY,
		SDATA130_ACTIVECNT,
		SDATA130_GUILDMONEY,
		SDATA130_GUILDCNT
	)
	VALUES
	(
		@pDate,
		@pWorld,

		@mAllMoney,
		@nAllCount,

		@mActiveMoney,
		@nActiveCount,

		@mGuildMoney,
		@nGuildCount
	)

