

/* ROUTE PROCESS

========================================================
PARAMETER
========================================================
@bGroupID		TINYINT
@bServerID		TINYINT
@bType		TINYINT
@szIPAddr		VARCHAR(50)	OUTPUT
@wPort			SMALLINT	OUTPUT

========================================================
RETURN VALUE
========================================================
0	: SUCCESS
1	: NO SERVER

========================================================
PROCESS
========================================================
1. Find Machine
2. Set Route ID
3. Update Route ID

*/


CREATE PROCEDURE [dbo].[TRoute]
	@bGroupID		TINYINT,
	@bServerID		TINYINT,
	@bType		TINYINT,
	@szIPAddr		VARCHAR(50)	OUTPUT,
	@wPort			SMALLINT	OUTPUT
AS
	SET @szIPAddr = ''
	SET @wPort = 0

	DECLARE @nResult	INT
	EXEC @nResult = TGLOBAL_GSP.dbo.TRoute @bGroupID, @bServerID, @bType, @szIPAddr OUTPUT, @wPort OUTPUT

	RETURN @nResult



