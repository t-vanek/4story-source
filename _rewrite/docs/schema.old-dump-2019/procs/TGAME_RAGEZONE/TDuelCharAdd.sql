

/* 
========================================================
RETURN VALUE
========================================================
0	: SUCCESS
1	: INVALID CHARACTER

*/


CREATE PROCEDURE [dbo].[TDuelCharAdd]
	@dwCharID		INT,
	@szName		VARCHAR(50),
	@bClass		TINYINT,
	@bLevel		TINYINT,
	@bWin			TINYINT,			
	@dwPoint		INT,
	@dTime		DATETIME
	
AS
	SELECT dwCharID FROM TCHARTABLE WHERE dwCharID = @dwCharID
	IF(@@ROWCOUNT = 0 )
		RETURN 1
	
	IF(@szName ='')
	BEGIN
		DELETE TDUELCHARTABLE WHERE dwCharID = @dwCharID
		RETURN 0
	END

	SELECT dwCharID FROM TCHARTABLE WHERE szName = @szName
	IF(@@ROWCOUNT = 0 )
		RETURN 1
	
	INSERT INTO TDUELCHARTABLE(
		dwCharID,
		szName,
		bClass,
		bLevel,
		bWin,			
		dwPoint,
		dTime) 
	VALUES(
		@dwCharID,
		@szName,
		@bClass,
		@bLevel,
		@bWin,			
		@dwPoint,
		@dTime) 
	
	RETURN 0



