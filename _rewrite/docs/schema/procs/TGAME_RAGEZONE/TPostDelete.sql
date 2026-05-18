

CREATE PROCEDURE [dbo].[TPostDelete]
@dwCharID int,
@dwPostID int
AS

SELECT dwPostID FROM TPOSTTABLE WHERE dwCharID=@dwCharID AND dwPostID=@dwPostID AND (dwGold>0  OR dwSilver>0 OR dwCooper>0)
IF (@@ROWCOUNT = 1)
	RETURN 1

SELECT dlID FROM TITEMTABLE WHERE dwOwnerID=@dwCharID AND bOwnerType=0 AND bStorageType=2 AND dwStorageID=@dwPostID
IF(@@ROWCOUNT = 1)
	RETURN 2

DELETE TPOSTTABLE WHERE dwCharID = @dwCharID AND dwPostID = @dwPostID


