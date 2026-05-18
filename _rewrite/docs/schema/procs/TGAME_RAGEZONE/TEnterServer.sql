

/* ENTER SERVER PROCESS

========================================================
PARAMETER
========================================================
@dwKEY		INT
@dwUserID		INT
@dwCharID		INT
@bGroupID		TINYINT
@bChannel		TINYINT
@szIPAddr		VARCHAR(50)
@wPort			SMALLINT

========================================================
RETURN VALUE
========================================================
0	: SUCCESS
1	: FAILED

========================================================
PROCESS
========================================================

*/


CREATE PROCEDURE [dbo].[TEnterServer]
	@dwKEY		INT,
	@dwUserID		INT,
	@dwCharID		INT,
	@bGroupID		TINYINT,
	@bChannel		TINYINT,
	@szIPAddr		VARCHAR(50),
	@wPort			SMALLINT
AS
	DECLARE @nResult	INT
	EXEC @nResult = TGLOBAL_GSP.dbo.TEnterServer @dwKEY, @dwUserID, @dwCharID, @bGroupID, @bChannel, @szIPAddr, @wPort

	RETURN @nResult



