
CREATE PROCEDURE [dbo].[TTournamentResult]
@bStep TINYINT,
@bRet TINYINT,
@dwWin INT,
@dwLose INT
AS

DECLARE @bResult TINYINT

IF(@bRet = 1)
	UPDATE TTOURNAMENTPLAYERTABLE SET bStep=@bStep, bResult=1 WHERE dwChiefID = @dwWin
ELSE
	UPDATE TTOURNAMENTPLAYERTABLE SET bStep=@bStep, bResult=2 WHERE dwChiefID = @dwWin

UPDATE TTOURNAMENTPLAYERTABLE SET bStep=@bStep, bResult=2 WHERE dwChiefID = @dwLose


