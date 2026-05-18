
CREATE PROCEDURE [dbo].[TGetPostInfo]
@wTotalCount  SMALLINT=0 OUTPUT,
@wReadCount  SMALLINT=0 OUTPUT,
@dwBeginID  INT=0 OUTPUT,
@dwCharID INT,
@wPage SMALLINT
AS

DECLARE @wCount SMALLINT

SELECT @wTotalCount = COUNT(*) FROM TPOSTTABLE WHERE dwCharID=@dwCharID
SELECT @wReadCount = COUNT(*) FROM TPOSTTABLE WHERE dwCharID=@dwCharID AND bRead=0

SET @wCount = 1
SET @dwBeginID=0
SELECT TOP 1 @dwBeginID = dwPostID FROM TPOSTTABLE WHERE dwCharID=@dwCharID AND dwPostID > @dwBeginID  ORDER BY dwPostID DESC

WHILE ( @wPage > @wCount)
BEGIN
	SELECT TOP 7 @dwBeginID = dwPostID FROM TPOSTTABLE WHERE dwCharID=@dwCharID AND dwPostID < @dwBeginID  ORDER BY dwPostID DESC

	SET @wCount = @wCount+1
END



