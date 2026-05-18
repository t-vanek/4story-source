
CREATE PROCEDURE [dbo].[TPostView]
@bItemCount TINYINT=0 OUTPUT,
@dwSendID INT=0 OUTPUT, 
@bType TINYINT=0 OUTPUT,
@bRead TINYINT=0 OUTPUT,
@dwGold INT=0 OUTPUT,
@dwSilver INT=0 OUTPUT,
@dwCooper INT=0 OUTPUT,
@timeRecv SMALLDATETIME=0 OUTPUT,
@szSender VARCHAR(50)='' OUTPUT,
@szTitle VARCHAR(256)='' OUTPUT,
@szMessage VARCHAR(2048)='' OUTPUT,
@bContain TINYINT OUTPUT,
@dwCharID INT,
@dwPostID INT
AS

SELECT @bContain=bContain,@dwSendID=dwSendID,@bType=bType,@bRead=bRead, @dwGold=dwGold,@dwSilver=dwSilver,@dwCooper=dwCooper,@timeRecv=timeRecv,@szSender=szSender,@szTitle=szTitle,@szMessage=szMessage
	FROM TPOSTTABLE  WHERE dwCharID=@dwCharID AND dwPostID=@dwPostID

IF(@@ROWCOUNT <> 1)
	RETURN 1

SELECT @bItemCount = COUNT(*) FROM TITEMTABLE WHERE dwOwnerID=@dwCharID AND bOwnerType=0 AND bStorageType=2 AND dwStorageID=@dwPostID

IF(@bType <> 2 AND @bRead=0)
BEGIN
	UPDATE TPOSTTABLE SET bRead=1 WHERE  dwPostID = @dwPostID
	SET @bRead = 1
END


