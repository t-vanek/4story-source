

CREATE PROCEDURE [dbo].[TGetItemName]
	@wItemID1	SMALLINT,
	@wItemID2	SMALLINT,
	@wItemID3	SMALLINT,
	@wItemID4	SMALLINT,
	@wItemID5	SMALLINT,
	@szName1	VARCHAR(50)	OUTPUT,
	@szName2	VARCHAR(50)	OUTPUT,
	@szName3	VARCHAR(50)	OUTPUT,
	@szName4	VARCHAR(50)	OUTPUT,
	@szName5	VARCHAR(50)	OUTPUT
 AS

	SELECT @szName1 = szName FROM TITEMCHART WHERE wItemID = @wItemID1
	IF(@@ROWCOUNT = 0)
		SET @szName1 = ''

	SELECT @szName2 = szName FROM TITEMCHART WHERE wItemID = @wItemID2
	IF(@@ROWCOUNT = 0 )
		SET @szName2 = ''

	SELECT @szName3 = szName FROM TITEMCHART WHERE wItemID = @wItemID3
	IF(@@ROWCOUNT = 0)
		SET @szName3 =''

	SELECT @szName4 = szName FROM TITEMCHART WHERE wItemID = @wItemID4
	IF(@@ROWCOUNT = 0)
		SET @szName4 =''

	SELECT @szName5 = szName FROM TITEMCHART WHERE wItemID = @wItemID5
	IF(@@ROWCOUNT = 0)
		SET @szName5 = ''

	RETURN 0



