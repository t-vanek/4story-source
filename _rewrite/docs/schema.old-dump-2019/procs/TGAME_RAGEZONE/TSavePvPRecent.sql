

CREATE PROCEDURE [dbo].[TSavePvPRecent]
@dwCharID INT,
@szName VARCHAR(50),
@bClass TINYINT,
@bLevel TINYINT,
@bWin TINYINT,
@dwPoint INT,
@dlDate SMALLDATETIME
AS

IF(@szName = '')
	DELETE TPVPRECENTTABLE WHERE dwCharID=@dwCharID
ELSE
	INSERT INTO TPVPRECENTTABLE (dwCharID, szName, bClass, bLevel, bWin, dwPoint, dlDate) VALUES(@dwCharID, @szName, @bClass, @bLevel, @bWin, @dwPoint, @dlDate)
