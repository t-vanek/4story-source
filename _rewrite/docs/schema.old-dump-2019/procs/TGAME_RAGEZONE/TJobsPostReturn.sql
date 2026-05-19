

CREATE PROCEDURE [dbo].[TJobsPostReturn]
AS

DECLARE @szSender VARCHAR(50)
DECLARE @szRecver VARCHAR(50)
DECLARE @dwSender INT
DECLARE @dwRecver INT
DECLARE @dwPostID INT
DECLARE POST_RETURN CURSOR FOR
SELECT dwCharID, szRecvName, dwPostID, dwSendID, szSender FROM TPOSTTABLE WHERE
	dwCharID NOT IN(SELECT dwCharID FROM TGLOBAL_GSP.DBO.TCURRENTUSER) AND bRead = 0 AND bType =2 AND timeRecv + 3 <= GetDate()

OPEN POST_RETURN
FETCH NEXT FROM POST_RETURN INTO @dwRecver, @szRecver, @dwPostID, @dwSender, @szSender
WHILE @@FETCH_STATUS = 0
BEGIN

UPDATE TPOSTTABLE SET dwCharID = @dwSender, szRecvName=@szSender, szSender = @szRecver, bType=3, dwGold=0, dwSilver=0, dwCooper=0, timeRecv = GetDate() WHERE dwPostID = @dwPostID
UPDATE TPOSTITEMTABLE SET dwCharID = @dwSender FROM TPOSTITEMTABLE WHERE dwPostID = @dwPostID
FETCH NEXT FROM POST_RETURN INTO @dwRecver, @szRecver, @dwPostID, @dwSender, @szSender

END

CLOSE POST_RETURN
DEALLOCATE POST_RETURN





