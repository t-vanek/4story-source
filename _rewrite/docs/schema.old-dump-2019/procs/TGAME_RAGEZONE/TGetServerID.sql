

/* FIND SERVER ID PROCESS

========================================================
PARAMETER
========================================================
@wMapID		SMALLINT
@wUnitID		SMALLINT
@bChannel		TINYINT
@bServerID		TINYINT	OUTPUT

========================================================
RETURN VALUE
========================================================
0	: SUCCESS
1	: NO SERVER

========================================================
PROCESS
========================================================
1. Find Server ID

*/


CREATE PROCEDURE [dbo].[TGetServerID]
	@wMapID		SMALLINT,
	@wUnitID		SMALLINT,
	@bChannel		TINYINT,
	@bServerID		TINYINT	OUTPUT
AS
	SET @bServerID = 0xFF

	SELECT TOP 1 @bServerID = bServerID FROM TSVRCHART INNER JOIN TCHANNELCHART ON
		TSVRCHART.bGroup = TCHANNELCHART.bGroupID AND
		TSVRCHART.wMapID = TCHANNELCHART.wMapID AND
		TSVRCHART.wUnitID = TCHANNELCHART.wUnitID AND
		TSVRCHART.bChannel = TCHANNELCHART.bPhyChannel
	WHERE TSVRCHART.wUnitID = @wUnitID AND TSVRCHART.wMapID = @wMapID AND TCHANNELCHART.bLogChannel = @bChannel

	IF @@ROWCOUNT = 0
	BEGIN
		/* Not found server */
		RETURN 1
	END

	RETURN 0



