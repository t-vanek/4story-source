
CREATE PROCEDURE [dbo].[TSetMountSaddle_copy]
	@dwUserID 	INT,
	@wItemID	SMALLINT,
	@dEndTime	SMALLDATETIME,
  @bType INT
AS

SELECT wItemID FROM TITEMCHART WHERE wItemID = @wItemID
IF(@@ROWCOUNT = 0)
	RETURN 0

SELECT dwUserID FROM TMOUNTITEMTABLE WHERE dwUserID = @dwUserID
IF(@@ROWCOUNT = 0)
	INSERT TMOUNTITEMTABLE (dwUserID, wItemID, bType, dEndTime) VALUES (@dwUserID, @wItemID, @bType, @dEndTime)
ELSE
	UPDATE TMOUNTITEMTABLE SET wItemID = @wItemID, dEndTime = @dEndTime, bType = @bType WHERE dwUserID = @dwUserID


