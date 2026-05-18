
CREATE PROCEDURE [dbo].[TAddQuset]
@szNAME varchar(50),
@dwQuestID INT
AS

INSERT TQUESTTABLE (dwCharID, dwQuestID, dwTick, bCompleteCount, bTriggerCount) SELECT dwCharID, @dwQuestID, 0, 0, 1 FROM TCHARTABLE WHERE szName = @szNAME

