
CREATE PROCEDURE [dbo].[TRPSGameRecord]
@bRecord TINYINT,
@dwCharID INT,
@bType TINYINT,
@bWinCount TINYINT,
@dWinDate SMALLDATETIME
AS

IF(@bRecord = 0)
	DELETE FROM TRPSGAMERECORDTABLE WHERE bType=@bType AND bWinCount=@bWinCount AND dWinDate <= @dWinDate
ELSE
	INSERT INTO TRPSGAMERECORDTABLE(bType, bWinCount, dWinDate, dwCharID) VALUES(@bType, @bWinCount, @dWinDate, @dwCharID)


