

CREATE PROCEDURE [dbo].[TGetServerInfo]
@bServerID		TINYINT OUTPUT,
@szIPAddr		VARCHAR(50)	OUTPUT,
@wPort			SMALLINT	OUTPUT,
@bGroupID		TINYINT,
@bChannel		TINYINT,
@bType		TINYINT,
@wMapID		SMALLINT,
@wPosX		SMALLINT,
@wPosY		SMALLINT,
@wPosZ		SMALLINT
AS

DECLARE @wUnitID	SMALLINT

SET @szIPAddr = ''
SET @wPort = 0

SET @wUnitID = CAST(@wPosZ / 1024 AS SMALLINT) * 256 + CAST(@wPosX / 1024 AS SMALLINT)

SELECT TOP 1 @bServerID = bServerID FROM TSVRCHART WHERE bChannel = @bChannel AND wMapID = @wMapID AND wUnitID = @wUnitID
IF @@ROWCOUNT = 0
BEGIN
	/* Not found server */
	RETURN 1
END

DECLARE @nResult	INT
EXEC @nResult = TRoute @bGroupID, @bServerID, @bType, @szIPAddr OUTPUT, @wPort OUTPUT
RETURN @nResult



