
CREATE PROCEDURE [dbo].[TTournamentPayback]
@dwPostID INT OUTPUT,
@dwCharID INT,
@dwGold INT,
@dwSilver INT,
@dwCooper INT
AS

DECLARE @szName VARCHAR(50)
DECLARE @dwRecvID INT
DECLARE @szSender VARCHAR(50)
DECLARE @szTitle VARCHAR(50)
DECLARE @szMessage VARCHAR(1024)
DECLARE @date SMALLDATETIME
DECLARE @wLenTitle	BINARY(4)
DECLARE @wLenMessage BINARY(4)
DECLARE @szT VARCHAR(8)
DECLARE @szM VARCHAR(8)

SET @date = GetDate()

SELECT @szName = szNAME FROM TCHARTABLE WHERE dwCharID=@dwCharID
IF(@@ROWCOUNT = 0 OR @szName IS NULL)
BEGIN
	DELETE TTOURNAMENTPLAYERTABLE WHERE dwCharID=@dwCharID
	RETURN
END

SELECT @szTitle = szMessage FROM TSVRMSGCHART WHERE dwID = 38
SELECT @szMessage = szMessage FROM TSVRMSGCHART WHERE dwID = 39
SELECT @szSender = szMessage FROM TSVRMSGCHART WHERE dwID = 6

SET @wLenTitle = DATALENGTH(@szTitle)
SET @wLenMessage = DATALENGTH(@szMessage)
SET @szT = RIGHT(master.dbo.fn_sqlvarbasetostr(@wLenTitle), 8)
SET @szTitle  = @szT + @szTitle
SET @szM = RIGHT(master.dbo.fn_sqlvarbasetostr(@wLenMessage), 8)
SET @szMessage = @szM + @szMessage

EXEC TSavePost @dwPostID OUTPUT, @dwRecvID OUTPUT, 0, @dwCharID, @szName, @szSender, @szTitle, @szMessage, 0, 1, @dwGold, @dwSilver, @dwCooper, @date

DELETE TTOURNAMENTPLAYERTABLE WHERE dwCharID=@dwCharID


