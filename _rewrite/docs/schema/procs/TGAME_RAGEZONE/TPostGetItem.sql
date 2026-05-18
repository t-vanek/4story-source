

CREATE PROCEDURE [dbo].[TPostGetItem]
@dwCharID int,
@dwPostID int
AS

UPDATE TPOSTTABLE SET dwGold=0, dwSilver=0, dwCooper=0 WHERE dwCharID = @dwCharID AND dwPostID = @dwPostID
EXEC TItemDelete @dwCharID, 0, 2, @dwPostID


