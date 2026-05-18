
CREATE PROCEDURE [dbo].[TGetPostInfo_old]
@wTotalCount  SMALLINT=0 OUTPUT,
@wReadCount  SMALLINT=0 OUTPUT,
@dwBeginID  INT=0 OUTPUT,
@dwCharID INT,
@wPage SMALLINT
AS

/*SELECT @wTotalCount = COUNT(*) FROM TPOSTTABLE WHERE dwCharID=@dwCharID
SELECT @wReadCount = COUNT(*) FROM TPOSTTABLE WHERE dwCharID=@dwCharID AND bRead=0
SELECT @dwBeginID = dwPostID FROM TPOSTTABLE WHERE dwCharID=@dwCharID*/

SET @dwBeginID = 7
SET @wTotalCount = 7
SET @wReadCount = 7
/* AND dwPostID > @dwBeginID */

