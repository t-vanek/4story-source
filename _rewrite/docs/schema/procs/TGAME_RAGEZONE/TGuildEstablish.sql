

CREATE PROCEDURE [dbo].[TGuildEstablish]
	@dwGuildID	 INT 	OUTPUT,
	@szName 	VARCHAR(50),
	@dwChief	 INT,
	@timeEstablish 	SMALLDATETIME
AS

SET @dwGuildID = 0
BEGIN TRAN GUILD_ESTABLISH
SELECT * FROM TGUILDTABLE WHERE szName = @szName
IF(@@ROWCOUNT > 0)
BEGIN
	COMMIT TRAN GUILD_ESTABLISH
	RETURN 1
END

INSERT INTO 
	TGUILDTABLE 
	(	
		szName,
		dwChief, 
		bLevel, 
		dwGI, 
		dwExp, 
		bGPoint, 
		bStatus, 
		bDisorg, 
		dwTime, 
		timeEstablish
	)
 VALUES
	(
		@szName,	/* szName */
		@dwChief, 	/* dwChief */
		1,		/* bLevel */
		0,		/* dwGI */
		0,		/* dwEXP */
		0,		/* bGPoint */
		0,		/* bStatus */
		0,		/* bDisorg */
		0,		/* dwTime */
		@timeEstablish	/* timeEstablish */
	)

SET @dwGuildID = @@IDENTITY
COMMIT TRAN GUILD_ESTABLISH

RETURN 0



