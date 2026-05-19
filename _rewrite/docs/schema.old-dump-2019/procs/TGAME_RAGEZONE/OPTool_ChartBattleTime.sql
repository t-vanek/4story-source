
/*
	OPTool_ChartBattleTime
*/
CREATE PROCEDURE OPTool_ChartBattleTime
	@pIP			VARCHAR(10),

	@pType		INT,

	@pBattleDur		INT,
	@pBattleStart		INT,
	@pAlarmStart		INT,
	@pAlarmEnd		INT,
	@pPeaceDur		INT,

	@pGMID		VARCHAR(20)
AS

	DECLARE @strLog 	VARCHAR(100)

	UPDATE	TBATTLETIMECHART
	SET		dwBattleDur	= @pBattleDur,
			dwBattleStart	= @pBattleStart,
			dwAlarmStart	= @pAlarmStart,
			dwAlarmEnd	= @pAlarmEnd,
			dwPeaceDur	= @pPeaceDur
	WHERE	bType		= @pType

	--
	--	Log
	--
	IF @@ROWCOUNT <>  0
	BEGIN
		
		SET @strLog = 		  	' Type:'    	+ CONVERT( VARCHAR,  @pType )
		SET @strLog = @strLog +	' attleDur : ' 	+ CONVERT( VARCHAR,  @pBattleDur  )
		SET @strLog = @strLog + 	' BattleStart : '    	+ CONVERT( VARCHAR,  @pBattleStart )	
		SET @strLog = @strLog + 	' AlarmStart : '	+ CONVERT( VARCHAR, @pAlarmStart )
		SET @strLog = @strLog + 	' AlarmEnd : '	+ CONVERT( VARCHAR, @pAlarmEnd )
		SET @strLog = @strLog + 	' PeaceDur : '	+ CONVERT( VARCHAR, @pPeaceDur )

		EXEC  TGLOBAL_GSP.DBO.OPTool_ManagerLog  @pIP, @pGMID, 'CHART BATTLETIME', @strLog
	
	END

