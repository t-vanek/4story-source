
CREATE PROCEDURE [dbo].[TSkillInit] 
@dwCharID INT
AS

DECLARE @dwMoney INT
DECLARE @bClass TINYINT
DECLARE @bLevel TINYINT
DECLARE @bCountry TINYINT
DECLARE @wSkillPoint SMALLINT

SELECT @dwMoney=SUM(dwPayBack) FROM TSKILLPOINTCHART AS SR INNER JOIN TSKILLTABLE AS ST ON 
SR.wID=ST.wSkillID AND SR.bLevel=ST.bLevel
WHERE ST.dwCharID = @dwCharID
IF(@dwMoney>0)
BEGIN
	SELECT @dwMoney=@dwMoney+dwGold*1000*1000+dwSilver*1000+dwCooper FROM TCHARTABLE WHERE dwCharID=@dwCharID
	UPDATE TCHARTABLE SET dwGold=@dwMoney/1000/1000, dwSilver=(@dwMoney/1000)%1000, dwCooper=@dwMoney%1000 WHERE dwCharID=@dwCharID
END

SELECT @bClass = bClass, @bLevel=bLevel , @bCountry=bCountry FROM TCHARTABLE WHERE dwCharID=@dwCharID
SELECT @wSkillPoint = SUM(bSkillPoint) FROM TLEVELCHART WHERE bLevel<=@bLevel
UPDATE TCHARTABLE SET wSkillPoint = @wSkillPoint WHERE dwCharID = @dwCharID

DELETE TSKILLTABLE WHERE dwCharID = @dwCharID AND wSkillID >= 100
INSERT INTO TSKILLTABLE SELECT @dwCharID, wSkillID, 1, 0 FROM TSTARTSKILL WHERE bClassID=@bClass AND wSkillID >= 100

DELETE THOTKEYTABLE WHERE dwCharID=@dwCharID
INSERT INTO THOTKEYTABLE 
	SELECT @dwCharID, bInvenID, bType1, wID1, bType2, wID2, bType3, wID3, bType4, wID4, bType5, wID5,
		  bType6, wID6, bType7, wID7, bType8, wID8, bType9, wID9, bType10, wID10, bType11, wID11, bType12, wID12 
	FROM TSTARTHOTKEY WHERE bClassID=@bClass


