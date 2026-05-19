CREATE PROCEDURE [dbo].[TSetPcBangData] 
@dwUserID INT,
@dwPcBangPlayTime INT,
@bPcBangItemCnt TINYINT
AS

BEGIN TRAN SETPCBANGDATA

DECLARE @dCurDate SMALLDATETIME
DECLARE @dwPassTime INT

SET @dCurDate = GETDATE()

SET @dwPassTime = DATEPART(hour, @dCurDate)*60*60+DATEPART(minute, @dCurDate)*60+DATEPART(second,@dCurDate)
IF(@dwPassTime < @dwPcBangPlayTime)
	SET @dwPcBangPlayTime = @dwPassTime
 
IF EXISTS( SELECT dwUserID FROM TPCBANGPLAYTABLE WHERE  dwUserID = @dwUserID AND dwPlayDate = YEAR(@dCurDate)*10000 + MONTH(@dCurDate)*100 + DAY(@dCurDate) )
	UPDATE TPCBANGPLAYTABLE SET dwPlayTime = @dwPcBangPlayTime, bItemCnt = @bPcBangItemCnt  WHERE dwUserID = @dwUserID AND dwPlayDate = YEAR(@dCurDate)*10000 + MONTH(@dCurDate)*100 + DAY(@dCurDate) 
ELSE
	INSERT INTO TPCBANGPLAYTABLE(dwUserID, dwPlayDate, dwPlayTime, bItemCnt) VALUES(@dwUserID, YEAR(@dCurDate)*10000 + MONTH(@dCurDate)*100 + DAY(@dCurDate) , @dwPcBangPlayTime, @bPcBangItemCnt)

COMMIT TRAN SETPCBANGDATA

