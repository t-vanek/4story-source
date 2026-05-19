/*
 *
 *
 */
CREATE PROCEDURE [dbo].[OPTool_ServerOnOff]
	@pIP		VARCHAR(15),
	@pGMID	VARCHAR(10),	
	@pDBOP	VARCHAR(10),
	@pWorld	INT,
	@pChannel	INT,
	@pStatus	INT
AS

	DECLARE	@strLog 	VARCHAR(100)


	IF @pDBOP = 'WORLD' 
	BEGIN

		IF @pWorld = 0
		BEGIN

			UPDATE   TGROUP	SET bStatus = @pStatus
			WHERE    bStatus <> 0	

			SET	@strLog = 'All Status:' + CONVERT( VARCHAR, @pStatus )
			EXEC  	OPTool_ManagerLog  @pIP, @pGMID, 'SERVER STATUS', @strLog	

		END
		ELSE
		BEGIN
	
			UPDATE  TGROUP  SET bStatus = @pStatus
			WHERE bGroupID =  @pWorld
			AND	 bStatus <> 0


			SET	@strLog = 'World:'+ CONVERT( VARCHAR,  @pWorld ) +' Status:' +  CONVERT( VARCHAR, @pStatus )
			EXEC  	OPTool_ManagerLog  @pIP, @pGMID, 'SERVER STATUS', @strLog

		END

	END
	ELSE
	BEGIN

		IF @pChannel = 0
		BEGIN


			UPDATE   TCHANNEL	
			SET	    bStatus = @pStatus
			WHERE bGroupID =  @pWorld
			AND	 bStatus <> 0



			SET	@strLog = 'GroupID:'+ CONVERT( VARCHAR, @pWorld ) +' All Status:' + CONVERT( VARCHAR, @pStatus )
			EXEC  	OPTool_ManagerLog  @pIP, @pGMID, 'CHANNEL STATUS', @strLog
	
		END
		ELSE
		BEGIN

			UPDATE   TCHANNEL	
			SET	    bStatus =   @pStatus
			WHERE    bGroupID =  @pWorld
			AND	    bChannel = @pChannel

			SET	@strLog = 'GroupID:'+ CONVERT( VARCHAR, @pWorld ) +' Channel:' + CONVERT( VARCHAR, @pChannel ) + ' Status:' + CONVERT( VARCHAR, @pStatus )
			EXEC  	OPTool_ManagerLog  @pIP, @pGMID, 'CHANNEL STATUS', @strLog

		END

	END

