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
	DECLARE @bMachineID	TINYINT
	DECLARE @bRouteID		TINYINT
	DECLARE @bCount		TINYINT

	SET @szIPAddr = ''
	SET @wPort = 0

	SELECT TOP 1 @bMachineID = bMachineID, @wPort = wPort FROM TSERVER WHERE bGroupID = @bGroupID AND bServerID = @bServerID AND bType = @bType
	IF @@ROWCOUNT = 0
	BEGIN
		/* Not found server */
		RETURN 1
	END

	BEGIN TRAN TROUTE
	SELECT @bCount = COUNT(bMachineID) FROM TIPADDR WHERE bMachineID = @bMachineID AND bActive = 1
	IF @bCount = 0
	BEGIN
		/* Not found server */
		COMMIT TRAN TROUTE
		RETURN 1
	END

	SELECT TOP 1 @bRouteID = bRouteID FROM TMACHINE WHERE bMachineID = @bMachineID
	SET @bRouteID = @bRouteID % @bCount + 1

	DECLARE CURSOR_ROUTE SCROLL CURSOR FOR
	SELECT szIPAddr FROM TIPADDR WHERE bMachineID = @bMachineID AND bActive = 1

	OPEN CURSOR_ROUTE
	FETCH ABSOLUTE @bRouteID FROM CURSOR_ROUTE INTO @szIPAddr

	CLOSE CURSOR_ROUTE
	DEALLOCATE CURSOR_ROUTE

	UPDATE TMACHINE SET bRouteID = @bRouteID WHERE bMachineID = @bMachineID
	COMMIT TRAN TROUTE

	RETURN 0


