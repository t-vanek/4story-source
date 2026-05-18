
CREATE PROCEDURE [dbo].[TAllSkillReward]
AS

DECLARE @dwPrevChar INT
DECLARE @dwMoney INT
DECLARE @dwCharID INT
DECLARE @wSkillID SMALLINT
DECLARE @bLevel TINYINT
SET @dwPrevChar = 0
SET @dwMoney = 0
DECLARE CUR_CR CURSOR FOR
SELECT dwCharID, wSkillID, bLevel FROM TSKILLTABLE ORDER BY dwCharID
OPEN CUR_CR
FETCH NEXT FROM CUR_CR INTO @dwCharID, @wSkillID, @bLevel
WHILE(@@FETCH_STATUS = 0)
BEGIN
	
	IF(@dwPrevChar = @dwCharID)
		SELECT @dwMoney = @dwMoney + dwMoney FROM TSKILLREWARD WHERE wID=@wSkillID AND bLevel=@bLevel
	ELSE
	BEGIN
		INSERT INTO TSKILLREWARDMONEY VALUES(@dwPrevChar, @dwMoney)
		UPDATE TCHARTABLE SET dwGold=dwGold+@dwMoney/1000/1000, dwSilver=dwSilver+(@dwMoney/1000)%1000, dwCooper=dwCooper+@dwMoney%1000 WHERE dwCharID=@dwPrevChar
		SET @dwPrevChar = @dwCharID
		SELECT @dwMoney = dwMoney FROM TSKILLREWARD WHERE wID=@wSkillID AND bLevel=@bLevel
	END

FETCH NEXT FROM CUR_CR INTO @dwCharID, @wSkillID, @bLevel
END
CLOSE CUR_CR
DEALLOCATE CUR_CR



