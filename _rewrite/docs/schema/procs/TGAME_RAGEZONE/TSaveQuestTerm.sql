

CREATE PROCEDURE [dbo].[TSaveQuestTerm]
@dwCharID int,
@dwQuestID int,
@dwTermID int,
@bTermType tinyint,
@bCount tinyint
AS

update TQUESTTERMTABLE set bCount=@bCount where dwCharID=@dwCharID and dwQuestID=@dwQuestID and dwTermID=@dwTermID
if(@@rowcount = 0)
	insert into TQUESTTERMTABLE values(@dwCharID, @dwQuestID, @dwTermID, @bTermType, @bCount)



