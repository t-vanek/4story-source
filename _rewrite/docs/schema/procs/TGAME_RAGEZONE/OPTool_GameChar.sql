


/* 
 *	Character의 Spwon위치를 변경 시킴
 *
 *
 */
CREATE   PROCEDURE OPTool_GameChar
	@pIP		VARCHAR(15),
	@pGMID	VARCHAR(10),
	
	@pDBOP	VARCHAR(10),
	@pCharID	INT,

	@pName	VARCHAR(20),

	@pLevel	INT,
	@pSkillPoint	INT,
	@pX		INT,
	@pY		INT,
	@pZ		INT,
	@pMapID	INT,

	@nGold		INT,
	@nSilver	INT,
	@nCooper	INT
AS

	DECLARE @wSpawnID 	SMALLINT
	DECLARE @wMapID	SMALLINT

	DECLARE @nExp	INT

	DECLARE @fX		FLOAT
	DECLARE @fY		FLOAT
	DECLARE @fZ		FLOAT

	DECLARE @bWorldID	TINYINT

	DECLARE @bSlot		TINYINT

	DECLARE @strOldName		VARCHAR(20)	-- 변경전 Name
	DECLARE @bOldLevel		TINYINT	-- 변경전 Level

	DECLARE @nOldGold		INT		-- 변경전 Gold
	DECLARE @nOldSilver		INT		-- 변경전 Silver
	DECLARE @nOldCooper	INT		-- 변경전 Cooper

	DECLARE  @nOldSkillPoint	INT		-- 변경전 SKILL POINT

	DECLARE @strLog		VARCHAR(500)

	--
	-- Set World ID
	--
	SET @bWorldID = 1

	-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	-- GET OLD CHARACTER INFO
	-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	IF @pDBOP <> 'RECOVERY'
	BEGIN 

		SELECT   	@strOldName 	= szName,
				@bSlot 		= bSlot,
				@bOldLevel 	= bLevel,
				@nOldGold	= dwGold,
				@nOldSilver	= dwSilver,
				@nOldCooper	= dwCooper,
				@nOldSkillPoint = wSkillPoint,
				@wSpawnID 	= wSpawnID
		FROM	   TCHARTABLE
		WHERE dwCharID 	= @pCharID
		AND	 bDelete 	= 0		

	END
	ELSE
	BEGIN

		SELECT   	@strOldName 	= szName,
				@bSlot 		= bSlot,
				@bOldLevel 	= bLevel,
				@nOldGold	= dwGold,
				@nOldSilver	= dwSilver,
				@nOldCooper	= dwCooper,
				@nOldSkillPoint = wSkillPoint,
				@wSpawnID 	= wSpawnID
		FROM	   TCHARTABLE
		WHERE dwCharID 	= @pCharID
		AND	 bDelete 	= 1		

	END


	IF @@ROWCOUNT = 0
	BEGIN
		RETURN 0
	END


	-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	-- CHANGE NAME
	-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	IF @pDBOP = 'NAME'
	BEGIN
		--
		-- Global DB Checking
		--
		IF EXISTS( SELECT dwCharID FROM  TGLOBAL_GSP.DBO.TALLCHARTABLE WHERE  szName = @pName )
		BEGIN
				
			SET @strLog = 'Fail Duplicate Name ' + @pName
			EXEC  TGLOBAL_GSP.DBO.OPTool_ManagerLog  @pIP, @pGMID, 'CHG NAME', @strLog

			RETURN 0

		END

		--
		-- Logcal DB Checking
		--
		IF EXISTS(  SELECT  dwCharID 	FROM	TCHARTABLE	WHERE szName = @pName )
		BEGIN
			
			SET @strLog = 'Fail Duplicate Name ' + @pName
			EXEC  TGLOBAL_GSP.DBO.OPTool_ManagerLog  @pIP, @pGMID, 'CHG NAME', @strLog

			RETURN 0
		END

		UPDATE  TCHARTABLE
			SET 	szName	= @pName
		WHERE   dwCharID = @pCharID

		IF @@ROWCOUNT =  1
		BEGIN
	
			UPDATE TGLOBAL_GSP.DBO.TALLCHARTABLE
				SET 	szName	= @pName
			WHERE   dwCharID = @pCharID
			AND	   bWorldID  = @bWorldID

			SET @strLog = @strOldName +  ' -> '  +  @pName

			EXEC  TGLOBAL_GSP.DBO.OPTool_ManagerLog  @pIP, @pGMID,  'CHG NAME',  @strLog

		END

	END
	-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	-- RECOVERY CHARACTER
	-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	ELSE IF @pDBOP = 'RECOVERY'
	BEGIN


		--
		-- Check the Slot
		--	
		IF EXISTS ( 	SELECT  dwCharID
				FROM	TCHARTABLE
				WHERE dwCharID = @pCharID
				AND	 bSlot = @bSlot
				AND	 bDelete = 0		
			  )
		BEGIN

			SET @strLog = 'Fail Exists Slot  ' + @pName + ' Slot:' + CONVERT( VARCHAR, @bSlot )
			EXEC  TGLOBAL_GSP.DBO.OPTool_ManagerLog  @pIP, @pGMID, 'RECOVERY CHAR', @strLog

			RETURN 0
		END


		--
		-- Local DB
		--
		UPDATE TCHARTABLE	
		  SET	bDelete		=	0,
			dDeleteDate	=	NULL
		WHERE   dwCharID = @pCharID

		IF @@ROWCOUNT = 0
		BEGIN

			SET @strLog = 'Fail No Data Found ' + @pName
			EXEC  TGLOBAL_GSP.DBO.OPTool_ManagerLog  @pIP, @pGMID, 'RECOVERY CHAR', @strLog

			RETURN 0
		END
		
		--
		-- GLOBAL DB
		--
		UPDATE  TGLOBAL_GSP.DBO.TALLCHARTABLE
			SET 	bDelete 	= 0,
				dDeleteDate	= NULL
		WHERE   dwCharID = @pCharID
		AND	   bWorldID  = @bWorldID


		SET @strLog =  @pName 
		EXEC  TGLOBAL_GSP.DBO.OPTool_ManagerLog  @pIP, @pGMID, 'RECOVERY CHAR', @strLog

	END
	-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	-- CHANGE LEVEL
	-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	ELSE IF @pDBOP = 'LEVEL'
	BEGIN

		SELECT	@nExp = dwEXP
		FROM		TLEVELCHART
		WHERE	bLevel	= @pLevel -1

		IF @@ROWCOUNT = 0
		BEGIN
	
			SET @strLog = @strOldName +' Fail No Data Found  TLEVELCHART Level='   + CONVERT( VARCHAR, @pLevel -1 )
			EXEC  TGLOBAL_GSP.DBO.OPTool_ManagerLog  @pIP, @pGMID, 'CHG LEVEL', @strLog

			RETURN 0
		END

		UPDATE  TCHARTABLE
			SET 	bLevel	= @pLevel,
				dwExp 	= @nExp	
		WHERE   dwCharID = @pCharID

		
		IF @@ROWCOUNT <> 0 
		BEGIN
				
			SET @strLog =  @strOldName + ' ' + CONVERT( VARCHAR, @bOldLevel ) + ' -> ' + CONVERT( VARCHAR, @pLevel )
			EXEC  TGLOBAL_GSP.DBO.OPTool_ManagerLog  @pIP, @pGMID, 'CHG LEVEL', @strLog

		END

	END
	-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	-- CHANGE MONEY
	-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	ELSE 	IF @pDBOP = 'MONEY'
	BEGIN

		UPDATE  TCHARTABLE
		SET	   	dwGold		=	@nGold	,
				dwSilver	=	@nSilver,
				dwCooper	=	@nCooper
		WHERE   dwCharID = @pCharID


		IF @@ROWCOUNT <> 0
		BEGIN
			SET @strLog = 	@strOldName + ' ' +  CONVERT( VARCHAR, @nOldGold) 	+ '/' + CONVERT( VARCHAR, @nOldSilver ) +'/' + CONVERT( VARCHAR, @nOldCooper ) + ' -> ' 
			SET @strLog = 	@strLog 	+ CONVERT( VARCHAR, @nGold  ) 	+ '/' + CONVERT( VARCHAR, @nSilver )	     +'/' + CONVERT( VARCHAR, @nCooper )

			EXEC  TGLOBAL_GSP.DBO.OPTool_ManagerLog  @pIP, @pGMID, 'CHG MONEY', @strLog
		END

	END
	-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	-- CHANGE SKILLPOINT
	-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	ELSE 	IF @pDBOP = 'SKILLPOINT'
	BEGIN
				
		UPDATE  TCHARTABLE
			SET 	wSkillPoint	= @pSkillPoint
		WHERE   dwCharID = @pCharID

		IF @@ROWCOUNT <> 0
		BEGIN
			SET @strLog =  @strOldName + ' ' + CONVERT( VARCHAR, @nOldSkillPoint ) + ' -> ' + CONVERT(  VARCHAR, @pSkillPoint )
			EXEC  TGLOBAL_GSP.DBO.OPTool_ManagerLog  @pIP, @pGMID, 'CHG SKILLPOINT', @strLog
		END



	END
	-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	-- CHANGE SPAWN
	-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	ELSE 	IF @pDBOP = 'SPAWN'
	BEGIN
	

		--
		-- 	Get Spwan Info
		--
		SELECT	@wMapID	=	wMapID,
			 	@fX		=	fPosX,
				@fY		=	fPosY,
				@fZ		=	fPosZ
		FROM	TSPAWNPOSCHART
		WHERE wID = @wSpawnID
	
	
		IF @@ROWCOUNT = 0
		BEGIN
			EXEC  TGLOBAL_GSP.DBO.OPTool_ManagerLog  @pIP, @pGMID, 'CHG SPAWN', 'No Data Found Spawn Data'
			RETURN 0
		END
	
	
		UPDATE  TCHARTABLE
			SET 	wMapID		= 	@wMapID,
			  	fPosX			=	@fX,
			  	fPosY			=	@fY,
			  	fPosZ			=	@fZ
		WHERE   dwCharID = @pCharID


		IF @@ROWCOUNT <> 0
		BEGIN

			SET @strLog =   @strOldName +' ' + CONVERT( VARCHAR, @wMapID ) + '(' +  CONVERT( VARCHAR, @fX ) + ',' +  CONVERT( VARCHAR, @fY ) + ',' + CONVERT( VARCHAR, @fZ ) + ')'
			EXEC  TGLOBAL_GSP.DBO.OPTool_ManagerLog  @pIP, @pGMID, 'CHG SPAWN', @strLog

		END

	END
	-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	-- CHANGE POSTION
	-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------	
	ELSE
	BEGIN

		UPDATE  TCHARTABLE
			SET 	wMapID			=	@pMapID,
				fPosX			=	@pX,
			  	fPosY			=	@pY,
			  	fPosZ			=	@pZ
		WHERE   dwCharID = @pCharID


		IF @@ROWCOUNT <> 0
		BEGIN

			SET @strLog =  @strOldName + ' (' +  CONVERT( VARCHAR, @pX ) + ',' + CONVERT( VARCHAR, @pY ) + ',' +  CONVERT( VARCHAR, @pZ )  + ')'
			EXEC  TGLOBAL_GSP.DBO.OPTool_ManagerLog  @pIP, @pGMID, 'CHG POS', @strLog
			
		END

	END


	RETURN 0

