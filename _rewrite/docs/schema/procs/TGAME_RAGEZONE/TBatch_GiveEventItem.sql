

CREATE PROCEDURE [dbo].[TBatch_GiveEventItem]
AS

DECLARE @dwCharID INT
DECLARE @bCompleteCount TINYINT
DECLARE @bItemCount TINYINT
DECLARE EVENT_ITEM CURSOR FOR 
SELECT dwCharID, bCompleteCount FROM TQUESTTABLE WHERE (dwQuestID = 5559 OR dwQuestID = 5560) and bCompleteCount > 0
OPEN EVENT_ITEM
FETCH NEXT FROM EVENT_ITEM INTO @dwCharID, @bCompleteCount

WHILE (@@FETCH_STATUS  =  0)
BEGIN
	IF EXISTS(SELECT dwCharID FROM TEVENTITEMTABLE WHERE dwCharID = @dwCharID)
	BEGIN
		SELECT @bItemCount = @bCompleteCount - bGiveItemCount FROM TEVENTITEMTABLE WHERE dwCharID = @dwCharID
		IF(@bItemCount <> 0)
		BEGIN
			EXEC TEventMagicItemGive @dwCharID, @bItemCount
			UPDATE TEVENTITEMTABLEf SET bGiveItemCount = @bCompleteCount WHERE dwCharID = @dwCharID
		END
	END
	ELSE
	BEGIN
		EXEC TEventMagicItemGive @dwCharID, @bCompleteCount
		INSERT INTO TEVENTITEMTABLE VALUES(@dwCharID, @bCompleteCount)
	END
FETCH NEXT FROM EVENT_ITEM INTO @dwCharID, @bCompleteCount
END

CLOSE EVENT_ITEM
DEALLOCATE EVENT_ITEM


