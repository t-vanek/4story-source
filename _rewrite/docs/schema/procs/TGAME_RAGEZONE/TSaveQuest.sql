

CREATE PROCEDURE [dbo].[TSaveQuest]
@dwCharID int,
@dwQuestID int,
@dwTick int,
@bCompleteCount tinyint,
@bTriggerCount tinyint
AS

update TQUESTTABLE set dwTick=@dwTick, bCompleteCount=@bCompleteCount, bTriggerCount =@bTriggerCount where @dwCharID=dwCharID and @dwQuestID=dwQuestID
if(@@rowcount = 0)
	insert into TQUESTTABLE(dwCharID, dwQuestID, dwTick, bCompleteCount, bTriggerCount) values(@dwCharID, @dwQuestID, @dwTick, @bCompleteCount, @bTriggerCount)



