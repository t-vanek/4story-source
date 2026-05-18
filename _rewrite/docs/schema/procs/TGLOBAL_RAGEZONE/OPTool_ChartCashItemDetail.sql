--
--
--
CREATE PROCEDURE [dbo].[OPTool_ChartCashItemDetail]
	@pIP		VARCHAR(10),

	@pDBOP	VARCHAR(10),

	@pCashItemID	INT,	
	@pItemID	INT,

	@pCount	INT,

	@pGMID	VARCHAR(20)

AS

	DECLARE @strLog 	VARCHAR(100)

	
	IF @pDBOP = 'U'
	BEGIN


		UPDATE	TCASHBONUSITEMCHART
		SET		bBonusItemCount = @pCount
		WHERE	wCashItemID = @pCashItemID
		AND		wBonusItem   = @pItemID

		--
		--	Log
		--
		IF @@ROWCOUNT <>  0
		BEGIN
			
			SET @strLog = 		  	' Cash ID:' 	+ CONVERT( VARCHAR, @pCashItemID )		
			SET @strLog = @strLog + 	' Bonus Item :'    	+ CONVERT( VARCHAR, @pCashItemID )
			SET @strLog = @strLog + 	' Count :'    	+ CONVERT( VARCHAR, @pCount 	)
	
			EXEC  TGLOBAL.DBO.OPTool_ManagerLog  @pIP, @pGMID, 'CASH DETAIL UPDATE', @strLog
		
		END

	END

