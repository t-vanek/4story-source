

CREATE PROCEDURE [dbo].[TSaveQuestComplete]
@dwCharID int,
@dwQuestID int
AS

SELECT dwCharID FROM TQUESTCOMPLETE WHERE dwCharID=@dwCharID and dwQuestID=@dwQuestID
IF(@@ROWCOUNT = 0)
	insert into TQUESTCOMPLETE(dwCharID, dwQuestID) values(@dwCharID, @dwQuestID)



