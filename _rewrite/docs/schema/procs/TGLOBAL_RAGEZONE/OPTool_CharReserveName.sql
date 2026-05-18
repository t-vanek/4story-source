--
-- Reserve Name
--
CREATE PROCEDURE [dbo].[OPTool_CharReserveName]
	@nOuput		INT	OUTPUT,

	@pIP			VARCHAR(20),
	@pDBOP		VARCHAR(10),
	@pSeq			INT,
	@pUserID		VARCHAR(50),
	@pReserveName	VARCHAR(20),
	@pGMID		VARCHAR(20)
AS

	DECLARE @strLog 		VARCHAR(512)
	DECLARE @strOldName		VARCHAR(20)
	DECLARE @nUserID		INT

	SET @nOuput = 0

	IF @pDBOP = 'I'
	BEGIN
		
		--
		--	Check the User ID
		--
		SELECT @nUserID = dwUserID
		FROM TACCOUNT 
		WHERE szUserID = @pUserID

		IF @@ROWCOUNT = 0
		BEGIN			

			SET @nOuput = 1
			RETURN 1		
		END

		--
		--	Check the Reserve Character
		--
		SELECT  @pReserveName = szName
		FROM 	 TRESERVEDNAME 
		WHERE szName = @pReserveName

		IF @@ROWCOUNT >  0
		BEGIN			
			SET @nOuput = 2
			RETURN 2		
		END

		INSERT INTO	TRESERVEDNAME
		(
			dwUserID,
			szName
		)
		VALUES
		(
			@nUserID,
			@pReserveName
		)	

	END
	ELSE IF  @pDBOP = 'U'
	BEGIN
	
		--
		--	Check the Reserve Character
		--
		SELECT  @strOldName = szName
		FROM 	 TRESERVEDNAME 
		WHERE szName = @pReserveName

		IF @@ROWCOUNT > 0
		BEGIN			
			SET @nOuput = 2
			RETURN 2		
		END

		SELECT   @strOldName = szName
		FROM	   TRESERVEDNAME
		WHERE  dwSeq = @pSeq

		UPDATE  	TRESERVEDNAME
		SET		szName	= @pReserveName
		WHERE  	dwSeq = @pSeq

	END
	ELSE IF  @pDBOP = 'D'
	BEGIN

		SELECT  @strOldName = szName
		FROM	TRESERVEDNAME
		WHERE  dwSeq = @pSeq

		DELETE	TRESERVEDNAME
		WHERE  	dwSeq = @pSeq

	END




	---------------------------------------------------------------------------------------------------------------------------------------------------
	-- LOG
	---------------------------------------------------------------------------------------------------------------------------------------------------
	IF @@ROWCOUNT <> 0
	BEGIN

		IF @pDBOP = 'I'
		BEGIN		

			SET @strLog =  			' User ID : '	+ @pUserID		
			SET @strLog =  @strLog + 	' CharName : '	+ @pReserveName

			EXEC  TGLOBAL.DBO.OPTool_ManagerLog  @pIP, @pGMID, 'RESERVE  INSERT', @strLog
		END
		ELSE IF @pDBOP = 'U'
		BEGIN

			SET @strLog = 		  	' Seq : ' 	+ CONVERT( VARCHAR, @pSeq		)

			SET @strLog = @strLog + 	' Old Reserve Name : '	+ @strOldName
			SET @strLog = @strLog + 	' New Reserve Name : '	+ @pReserveName


			EXEC  TGLOBAL.DBO.OPTool_ManagerLog  @pIP, @pGMID, 'RESERVE UPDATE', @strLog
		END		
		ELSE IF @pDBOP = 'D'
		BEGIN

			SET @strLog = 		  	' Seq : ' 	+ CONVERT( VARCHAR, @pSeq		)
			SET @strLog = @strLog + 	' Old Reserve Name : '	+ @strOldName

			EXEC  TGLOBAL.DBO.OPTool_ManagerLog  @pIP, @pGMID, 'RESERVE DELETE', @strLog
		END
	
	END


	RETURN 0

