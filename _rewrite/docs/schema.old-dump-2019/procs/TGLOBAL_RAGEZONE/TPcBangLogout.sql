CREATE PROCEDURE [dbo].[TPcBangLogout]
@dwUserID INT,
@dwPlayTime INT OUTPUT
AS

DECLARE @dwPcBangID INT
DECLARE @dEnterDate DATETIME

SET @dwPlayTime = 0

SELECT @dwPcBangID = dwPcBangID, @dEnterDate = dEnterDate FROM TCURRENTUSER WHERE dwUserID = @dwUserID
IF(@@ROWCOUNT <> 1)
	RETURN 1

SELECT @dwPlayTime = DATEDIFF(SECOND, @dEnterDate, GETDATE())
IF(@dwPlayTime < 0)
	SET @dwPlayTime = 0

IF(@dwPcBangID = 0)
	RETURN 0

--EXEC [4SDB6].TPCBANG.DBO.TSavePcBangPlay @dwPcBangID, @dwUserID, @dwPlayTime

