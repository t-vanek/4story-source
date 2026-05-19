/*

*/
CREATE PROCEDURE [dbo].[OPTool_ChartCashItem]
	@pIP		VARCHAR(10),

	@pDBOP	VARCHAR(10),

	@pID		INT,
	
	@pCategory	INT,

	@pOrder	INT,
	@pName	VARCHAR(50),

	@pMoney	INT,
	@pCount	INT,

	@pKind		TINYINT,
	@pCanSell	INT,

	@pGMID	VARCHAR(20)
AS

	DECLARE @strLog 	VARCHAR(100)

	
	IF @pDBOP = 'U'
	BEGIN


		UPDATE	TCASHSHOPITEMCHART
		SET		szName		=	@pName,
				bCategory 	=	@pCategory,
				wOrder		=	@pOrder,
				dwMoney	=	@pMoney,
				bCount		=	@pCount,
				bKind		=	@pKind,
				bCanSell	=	@pCanSell
		WHERE	wID = @pID

		--
		--	Log
		--
		IF @@ROWCOUNT <>  0
		BEGIN
			
			SET @strLog = 		  	' ID:'    	     	+ CONVERT( VARCHAR, @pID 		)
		
			SET @strLog = @strLog + 	' Category :'    	+ CONVERT( VARCHAR, @pCategory 	)

			SET @strLog = @strLog + 	' Order :'    	+ CONVERT( VARCHAR, @pOrder 	)
			SET @strLog = @strLog + 	' Name :'    	+ @pName
			SET @strLog = @strLog + 	' Money:'    	+ CONVERT( VARCHAR, @pMoney 	)

			SET @strLog = @strLog + 	' Count:'    	+ CONVERT( VARCHAR, @pCount 	)

			SET @strLog = @strLog + 	' Kind:'    	+ CONVERT( VARCHAR, @pKind 	)
			SET @strLog = @strLog + 	' CanSell: '  	+ CONVERT( VARCHAR, @pCanSell 	)
	
			EXEC  TGLOBAL.DBO.OPTool_ManagerLog  @pIP, @pGMID, 'CASH UPDATE', @strLog
		
		END
	END

