




CREATE PROCEDURE [dbo].[TSaveCharBase]
@dwCharID INT,
@bType TINYINT,
@bValue TINYINT,
@szNAME VARCHAR(50)
AS

DECLARE @szCurName VARCHAR(50)
DECLARE @bCountry TINYINT
DECLARE @bLevel TINYINT
DECLARE @dwUserID INT

SELECT @dwUserID=dwUserID, @szCurName = szName, @bCountry=bCountry, @bLevel=bLevel FROM TCHARTABLE WHERE dwCharID = @dwCharID

IF(@bType = 45)
	UPDATE TCHARTABLE SET bFace = @bValue WHERE dwCharID=@dwCharID
ELSE IF(@bType = 46)
	UPDATE TCHARTABLE SET bHair = @bValue WHERE dwCharID = @dwCharID
ELSE IF(@bType = 47)
	UPDATE TCHARTABLE SET bRace = @bValue WHERE dwCharID = @dwCharID
ELSE IF(@bType = 48)
BEGIN
	UPDATE TLOCALTABLE SET szHero = '' WHERE szHero = @szCurName
	UPDATE TCASTLETABLE SET szHero = '' WHERE szHero = @szCurName

	UPDATE TCHARTABLE SET szNAME = @szNAME WHERE dwCharID = @dwCharID
END
ELSE IF(@bType = 49)
	UPDATE TCHARTABLE SET bSex = @bValue  WHERE dwCharID = @dwCharID
ELSE IF(@bType = 96)
BEGIN
	UPDATE TCHARTABLE SET bCountry = @bValue WHERE dwCharID = @dwCharID
	DELETE TACTIVECHARTABLE WHERE dwCharID = @dwCharID
	IF(@bValue < 2 )
	BEGIN
		IF( @bLevel >= 80)
			INSERT INTO TACTIVECHARTABLE(dwCharID, dateEnter) VALUES(@dwCharID, GetDate())

		IF(@bCountry = 4)
		BEGIN
			DECLARE @fPosX FLOAT
			DECLARE @fPosY FLOAT
			DECLARE @fPosZ FLOAT
			DECLARE @wDir SMALLINT
			DECLARE @dwRegion INT
			DECLARE @wSpawnID SMALLINT
			IF(@bValue = 0)
			BEGIN
				SET @fPosX = 1540.33
				SET @fPosY = 112.09
				SET @fPosZ = 4303.17
				SET @wDir = 0
				SET @wSpawnID = 15001
				SET @dwRegion = 9474
			END
			ELSE
			BEGIN
				SET @fPosX = 6827.72
				SET @fPosY = 210.48
				SET @fPosZ = 4958.57
				SET @wDir = 208
				SET @wSpawnID = 15002
				SET @dwRegion = 9473
			END

			UPDATE TCHARTABLE SET bCountry=@bValue, bOriCountry=@bValue, fPosX=@fPosX, fPosY=@fPosY, fPosZ=@fPosZ, wDir=@wDir, wSpawnID=@wSpawnID, dwRegion=@dwRegion WHERE dwUserID = @dwUserID
			DECLARE @dwExp INT
			SELECT @dwExp = dwExp FROM TLEVELCHART WHERE bLevel=8
			--UPDATE TCHARTABLE SET bLevel=9, dwExp=@dwExp WHERE dwUserID=@dwUserID AND bLevel<9
			UPDATE TCHARTABLE SET bLevel=9, dwExp=@dwExp, wSkillPoint=CAST((9-bLevel)/2 AS SMALLINT)*2 WHERE dwUserID=@dwUserID AND bLevel<9

		END
	END
	ELSE
		DELETE TACTIVECHARTABLE WHERE dwCharID = @dwCharID
END
ELSE IF(@bType = 97)
BEGIN
	DELETE TAIDTABLE WHERE dwCharID = @dwCharID

	IF(@bValue < 2)
		INSERT INTO TAIDTABLE (dwCharID, bCountry, dDate) VALUES(@dwCharID, @bValue, GetDate())
END

EXEC TGLOBAL_GSP.DBO.TSaveCharBase @szCurName, @bType, @bValue, @szNAME


