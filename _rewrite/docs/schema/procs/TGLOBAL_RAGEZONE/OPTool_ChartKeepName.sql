/*
	Keep Name

*/
CREATE PROCEDURE [dbo].[OPTool_ChartKeepName]
	@pIP		VARCHAR(10),
	@pDBOP	VARCHAR(10),

	@pSeq		INT,
	@pKeepName	VARCHAR(20),

	@pGMID	VARCHAR(20)
AS

	DECLARE @strLog 		VARCHAR(512)
	DECLARE @strOldName		VARCHAR(20)

	IF @pDBOP = 'I'
	BEGIN

		INSERT INTO	TKEEPINGNAME
		(
			szName
		)
		VALUES
		(
			@pKeepName
		)	

	END
	ELSE IF  @pDBOP = 'U'
	BEGIN
	
		SELECT  	@strOldName = szName
		FROM	TKEEPINGNAME
		WHERE  dwSeq = @pSeq

		UPDATE  	TKEEPINGNAME
		SET		szName	= @pKeepName
		WHERE  dwSeq = @pSeq


	END
	ELSE IF  @pDBOP = 'D'
	BEGIN

		SELECT  	@strOldName = szName
		FROM	TKEEPINGNAME
		WHERE  dwSeq = @pSeq

		DELETE	TKEEPINGNAME
		WHERE  dwSeq = @pSeq

	END
		

	---------------------------------------------------------------------------------------------------------------------------------------------------
	-- LOG
	---------------------------------------------------------------------------------------------------------------------------------------------------
	IF @@ROWCOUNT <> 0
	BEGIN
		

		IF @pDBOP = 'I'
		BEGIN		

			SET @strLog =  	' CharName : '	+ @pKeepName



			EXEC  TGLOBAL.DBO.OPTool_ManagerLog  @pIP, @pGMID, 'KEEPNAME  INSERT', @strLog
		END
		ELSE IF @pDBOP = 'U'
		BEGIN

			SET @strLog = 		  	' Seq : ' 	+ CONVERT( VARCHAR, @pSeq		)

			SET @strLog = @strLog + 	' Old Keep Name : '	+ @strOldName
			SET @strLog = @strLog + 	' New Keep Name : '	+ @pKeepName


			EXEC  TGLOBAL.DBO.OPTool_ManagerLog  @pIP, @pGMID, 'KEEPNAME UPDATE', @strLog
		END		
		ELSE IF @pDBOP = 'D'
		BEGIN

			SET @strLog = 		  	' Seq : ' 	+ CONVERT( VARCHAR, @pSeq		)
			SET @strLog = @strLog + 	' Old Keep Name : '	+ @strOldName

			EXEC  TGLOBAL.DBO.OPTool_ManagerLog  @pIP, @pGMID, 'KEEPNAME DELETE', @strLog
		END
	
	END

