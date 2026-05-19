/*
 *	OPTool_GameCharBlock
 *
 */
CREATE PROCEDURE [dbo].[OPTool_GameCharBlock]
	@pIP			VARCHAR(10),
	@pDBOP		VARCHAR(10),
	@pSeq			INT,

	@pUserID		INT,
	@pBlockType		INT,

	@pWorld		TINYINT,
	@pCharID		INT,
	@pCharName		VARCHAR(20),

	@pStartTime		DATETIME,
	@pEternal		INT,
	@dwDuration		INT,

	@pBlockReason	TINYINT,

	@pComment		VARCHAR(4000),
	@pGMID		VARCHAR(20)
AS

	DECLARE @strLog 	VARCHAR(1024)


	IF @dwDuration > 10000
	BEGIN
		SET @dwDuration = 10000
	END

	---------------------------------------------------------------------------------------------------------------------------------------------------
	-- INSERT
	---------------------------------------------------------------------------------------------------------------------------------------------------
	IF @pDBOP = 'I'
	BEGIN

		INSERT INTO TUSERPROTECTED
		(
			dwUserID		,    
			bBlockType		,
			bWorld 			,
			dwCharID		,
			szCharName		,   
			startTime		,  
			bEternal                           ,          
			dwDuration		,
			bBlockReason		,
			szComment		,
			szGMID
		)
		VALUES
		(
			@pUserID		,
			@pBlockType		,
		
			@pWorld		,
			@pCharID		,
			@pCharName		,
		
			@pStartTime		,

			@pEternal		,

			@dwDuration		,		
			@pBlockReason	,		
			@pComment		,
			@pGMID
		)

	END
	---------------------------------------------------------------------------------------------------------------------------------------------------
	-- UPDATE
	---------------------------------------------------------------------------------------------------------------------------------------------------
	ELSE IF	@pDBOP = 'U'
	BEGIN
		
		UPDATE 	TUSERPROTECTED
			SET	bBlockType	=	@pBlockType		,
				
				bWorld 		=	@pWorld		,
				dwCharID	=	@pCharID		,
				szCharName	=	@pCharName		,  

				startTime	= 	@pStartTime		,
				bEternal		=	@pEternal		,
				dwDuration	= 	@dwDuration		,

				bBlockReason	=	@pBlockReason	,
				szComment	=	@pComment
		WHERE  dwSeq = @pSeq	

	END
	---------------------------------------------------------------------------------------------------------------------------------------------------
	-- DELETE
	---------------------------------------------------------------------------------------------------------------------------------------------------
	ELSE  IF	@pDBOP = 'D'
	BEGIN

		SELECT	@pBlockType	=	bBlockType,				
				@pWorld	=	bWorld,
				@pCharID	=	dwCharID,
				@pCharName	=	szCharName,
				@pStartTime	=	startTime,
				@pEternal	=	bEternal,
				@dwDuration	=	dwDuration,
				@pBlockReason=	bBlockReason,				
				@pComment	=	szComment
		FROM	TUSERPROTECTED	
		WHERE  dwSeq = @pSeq		


		DELETE TUSERPROTECTED	WHERE  dwSeq = @pSeq		

	END

	
	---------------------------------------------------------------------------------------------------------------------------------------------------
	-- LOG
	---------------------------------------------------------------------------------------------------------------------------------------------------
	IF @@ROWCOUNT <> 0
	BEGIN
		
		SET @strLog = 		  	' UserID : ' 	+ CONVERT( VARCHAR,@pUserID		)
		SET @strLog = @strLog + 	' BlockType : '	+ CONVERT( VARCHAR, @pBlockType 		)
		SET @strLog = @strLog + 	' World : '	+ CONVERT( VARCHAR, @pWorld		)
		SET @strLog = @strLog + 	' CharID : '	+ CONVERT( VARCHAR,@pCharID		)
		SET @strLog = @strLog + 	' CharName : '	+ @pCharName
		SET @strLog = @strLog + 	' startTime : '	+ CONVERT( VARCHAR(10), @pStartTime,121	)

		IF @pEternal = 0
		BEGIN
			SET @strLog = @strLog + 	' Duration : '	+ CONVERT( VARCHAR, @dwDuration	)
		END
		ELSE
		BEGIN
			SET @strLog = @strLog + 	' Forever '
		END

		SET @strLog = @strLog + 	' BlockReason : '+ CONVERT( VARCHAR, @pBlockReason	)
		--	SET @strLog = @strLog +  	' szComment :'	+ @pComment

		IF @pDBOP = 'I'
		BEGIN		
			EXEC  TGLOBAL.DBO.OPTool_ManagerLog  @pIP, @pGMID, 'BLOCK INSERT', @strLog
		END
		ELSE IF @pDBOP = 'U'


		BEGIN
			EXEC  TGLOBAL.DBO.OPTool_ManagerLog  @pIP, @pGMID, 'BLOCK UPDATE', @strLog
		END		
		ELSE IF @pDBOP = 'D'
		BEGIN
			EXEC  TGLOBAL.DBO.OPTool_ManagerLog  @pIP, @pGMID, 'BLOCK DELETE', @strLog
		END
	
	END

