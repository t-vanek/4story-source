

CREATE PROCEDURE [dbo].[TPostRead]
@dwCharID INT,
@dwPostID INT,
@bRead TINYINT
AS

DECLARE @bType TINYINT
SET @bType = 0
SELECT @bType = bType  FROM TPOSTTABLE WHERE dwPostID = @dwPostID
IF(@bType = 2 AND @bRead = 1)
	UPDATE TPOSTTABLE SET bRead= @bRead, dwGold=0, dwSilver=0, dwCooper=0 WHERE dwPostID = @dwPostID
ELSE
	UPDATE TPOSTTABLE SET bRead = @bRead WHERE dwCharID = @dwCharID AND dwPostID = @dwPostID


