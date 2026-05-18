

CREATE PROCEDURE [dbo].[TPostBillsUpdate]
@dwNewID INT=0 OUTPUT,
@szSender VARCHAR(50) ='' OUTPUT,
@szRecver VARCHAR(50)='' OUTPUT,
@szTitle VARCHAR(256)='' OUTPUT,
@dwPostID INT,
@szCharName VARCHAR(50),
@bType TINYINT
AS

DECLARE @dwGold INT
DECLARE @dwSilver INT
DECLARE @dwCooper INT
DECLARE @dwCharID INT
DECLARE @dwSendID INT
DECLARE @szSendName VARCHAR(50)
DECLARE @szMessage VARCHAR(2048)
DECLARE @timeCurrent SMALLDATETIME

SET @timeCurrent = GetDate()

SELECT @dwCharID = dwCharID, @dwSendID=dwSendID, @dwGold=dwGold, @dwSilver=dwSilver, @dwCooper=dwCooper, @szSender=szSender, @szRecver=szRecvName, @szTitle=szTitle, @szMessage=szMessage FROM TPOSTTABLE WHERE dwPostID=@dwPostID AND bRead=0
IF (@@ROWCOUNT <> 1)
	RETURN 8

IF(@szCharName <> '' AND @szRecver <> @szCharName)
	RETURN 8

IF(@bType = 3)
BEGIN
	UPDATE TPOSTTABLE SET dwCharID=@dwSendID, dwGold=0, dwSilver=0, dwCooper=0, szRecvName=@szSender, bType=@bType, bRead=0, timeRecv=@timeCurrent, dwSendID=@dwCharID, szSender=@szRecver WHERE dwPostID = @dwPostID
	UPDATE TITEMTABLE SET dwOwnerID = @dwSendID WHERE dwOwnerID=@dwCharID AND bOwnerType=0 AND bStorageType=2 AND dwStorageID=@dwPostID
	SET @dwNewID = @dwPostID
END
ELSE
BEGIN
	EXEC TSavePost @dwNewID OUTPUT, @dwCharID OUTPUT, @dwCharID, @dwSendID, @szSender, @szRecver, @szTitle, @szMessage, 0, @bType, @dwGold, @dwSilver, @dwCooper, @timeCurrent
	UPDATE TPOSTTABLE SET dwGold=0, dwSilver=0, dwCooper=0, bRead=1 WHERE dwPostID=@dwPostID
END

SET @szSendName = @szSender
SET @szSender = @szRecver
SET @szRecver = @szSendName

