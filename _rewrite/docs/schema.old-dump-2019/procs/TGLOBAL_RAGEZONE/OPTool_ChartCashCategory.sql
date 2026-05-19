--
--	OPTool_ChartCashCategory
--
CREATE PROCEDURE [dbo].[OPTool_ChartCashCategory]
	@pIP		VARCHAR(10),
	@pDBOP	VARCHAR(10),

	@pSeq		INT,
	@pName	VARCHAR(20),

	@pOrder	INT,

	@pGMID	VARCHAR(20)
AS

	DECLARE @strLog 		VARCHAR(512)

	IF @pDBOP = 'I'
	BEGIN

		INSERT INTO	TCASHCATEGORYCHART
		(
			bID,
			szName,
			wOrder
		)
		VALUES
		(
			@pSeq,
			@pName,
			@pOrder
		)	

	END
	ELSE 
	IF  @pDBOP = 'U'
	BEGIN
	

		UPDATE  TCASHCATEGORYCHART
		SET	    szName = @pName,
			    wOrder  = @pOrder
		WHERE   bID = @pSeq


	END
	ELSE IF  @pDBOP = 'D'
	BEGIN

		SELECT   @pName = szName,
			    @pOrder = wOrder
		FROM	TCASHCATEGORYCHART
		WHERE bID = @pSeq

		DELETE	TCASHCATEGORYCHART
		WHERE	bID = @pSeq

	END
		

	---------------------------------------------------------------------------------------------------------------------------------------------------
	-- LOG
	---------------------------------------------------------------------------------------------------------------------------------------------------
	IF @@ROWCOUNT <> 0
	BEGIN
		

		IF @pDBOP = 'I'
		BEGIN		

			SET @strLog =  			' ID : '		+  CONVERT( VARCHAR, @pSeq )
			SET @strLog = @strLog + 	' Name : '	+ @pName
			SET @strLog = @strLog + 	' Order : '	+ CONVERT( VARCHAR,@pOrder)

			EXEC  TGLOBAL.DBO.OPTool_ManagerLog  @pIP, @pGMID, 'CASH CATEGORY INSERT', @strLog
		END
		ELSE IF @pDBOP = 'U'
		BEGIN

			SET @strLog =  			' ID : '		+  CONVERT( VARCHAR, @pSeq )
			SET @strLog = @strLog + 	' Name : '	+ @pName
			SET @strLog = @strLog + 	' Order : '	+ CONVERT( VARCHAR,@pOrder)

			EXEC  TGLOBAL.DBO.OPTool_ManagerLog  @pIP, @pGMID, 'CASH CATEGORY UPDATE', @strLog
		END		
		ELSE IF @pDBOP = 'D'
		BEGIN

			SET @strLog =  			' ID : '		+  CONVERT( VARCHAR, @pSeq )
			SET @strLog = @strLog + 	' Name : '	+ @pName
			SET @strLog = @strLog + 	' Order : '	+ CONVERT( VARCHAR,@pOrder)

			EXEC  TGLOBAL.DBO.OPTool_ManagerLog  @pIP, @pGMID, 'CASH CATEGORY DELETE', @strLog
		END
	
	END

