
CREATE PROCEDURE [dbo].[TEditQuest]
@szName VARCHAR(50),
@dwQuestID INT,
@bEdit TINYINT	--0:삭제,1:진행중, 2:완료
AS

DECLARE @dwCharID INT
SELECT @dwCharID = dwCharID FROM TCHARTABLE WHERE szName = @szName
IF(@@ROWCOUNT = 0)
BEGIN
	SELECT '이런 이름을 가진 캐릭터가 없군요. 정신 똑바로 차리세요'
	RETURN
END

IF(@bEdit = 0)
BEGIN
	DELETE TQUESTTABLE WHERE dwCharID = @dwCharID AND dwQuestID = @dwQuestID
	DELETE TQUESTTERMTABLE WHERE dwCharID = @dwCharID AND dwQuestID = @dwQuestID
END
ELSE IF(@bEdit = 1)
BEGIN
	IF EXISTS(SELECT dwQuestID FROM TQUESTTABLE WHERE dwCharID=@dwCharID AND dwQuestID=@dwQuestID)
		UPDATE TQUESTTABLE SET bCompleteCount = 0, bTriggerCount=1 WHERE dwCharID=@dwCharID AND dwQuestID=@dwQuestID
	ELSE
		INSERT INTO TQUESTTABLE (dwCharID, dwQuestID, dwTick, bCompleteCount, bTriggerCount) VALUES(@dwCharID, @dwQuestID, 0, 0, 1)
END
ELSE IF(@bEdit = 2)
BEGIN
	IF EXISTS(SELECT dwQuestID FROM TQUESTTABLE WHERE dwCharID=@dwCharID AND dwQuestID=@dwQuestID)
		UPDATE TQUESTTABLE SET bCompleteCount = 1, bTriggerCount=1 WHERE dwCharID=@dwCharID AND dwQuestID=@dwQuestID
	ELSE
		INSERT INTO TQUESTTABLE (dwCharID, dwQuestID, dwTick, bCompleteCount, bTriggerCount) VALUES(@dwCharID, @dwQuestID, 0, 1, 1)
END




