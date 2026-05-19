
CREATE PROCEDURE [dbo].[TCharLevelSet]
@szName VARCHAR(50),
@bLevel TINYINT
AS

DECLARE @dwExp INT
SET @dwExp = 0

IF EXISTS (SELECT dwCharID FROM TCHARTABLE WHERE szName = @szName)
BEGIN
	SELECT @dwExp = dwExp FROM TLEVELCHART WHERE bLevel = @bLevel-1
	UPDATE TCHARTABLE SET bLevel = @bLevel, dwExp = @dwExp, wSkillPoint = @bLevel, dwGold = 200 WHERE szName = @szName
END
ELSE
	SELECT '그런 캐릭터 없소'

