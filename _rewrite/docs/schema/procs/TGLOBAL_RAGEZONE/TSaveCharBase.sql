
CREATE PROCEDURE [dbo].[TSaveCharBase]
@szCurName VARCHAR(50),
@bType TINYINT,
@bValue TINYINT,
@szNAME VARCHAR(50)
AS

IF(@bType = 45)
	UPDATE TALLCHARTABLE SET bFace = @bValue WHERE szName=@szCurName
ELSE IF(@bType = 46)
	UPDATE TALLCHARTABLE SET bHair = @bValue WHERE szName = @szCurName
ELSE IF(@bType = 47)
	UPDATE TALLCHARTABLE SET bRace = @bValue WHERE szName = @szCurName
ELSE IF(@bType = 48)
BEGIN
	UPDATE TALLCHARTABLE SET szNAME = @szNAME WHERE szName = @szCurName
	DELETE TRESERVEDNAME WHERE szNAME = @szNAME
END
ELSE IF(@bType = 49)
	UPDATE TALLCHARTABLE SET bSex = @bValue  WHERE szName = @szCurName
ELSE IF(@bType = 96)
	UPDATE TALLCHARTABLE SET bCountry = @bValue WHERE szName = @szCurName


