


CREATE PROCEDURE [dbo].[TSaveCastleOccupy]
@bCountry TINYINT OUTPUT,
@dwGuild INT,
@wCastle SMALLINT,
@bType TINYINT
AS

SET @bCountry = 3

IF(@bType = 0)
BEGIN
	SELECT @bCountry=bCountry FROM TCASTLETABLE WHERE wCastle = @wCastle
	UPDATE TCASTLETABLE SET dateWarTime = DATEADD(DAY,7,dateWarTime)  WHERE wCastle = @wCastle
END
ELSE IF(@bType = 1)
BEGIN
	IF (@dwGuild <> 0)
	BEGIN
		SELECT @bCountry=bCountry  FROM TCHARTABLE WHERE dwCharID = ( SELECT dwChief FROM TGUILDTABLE WHERE dwID = @dwGuild)
		UPDATE TCASTLETABLE SET 
			dwGuildID = @dwGuild, 
			bCountry = @bCountry,
			dateWarTime = DATEADD(DAY,7,dateWarTime),
			szHero = '',
			dateHero = 0  WHERE wCastle = @wCastle
	END
	ELSE
		RETURN 1
END

DELETE TLOCALOCCUPYTABLE WHERE wLocalID IN(SELECT wID FROM TBATTLEZONECHART WHERE wCastle = @wCastle)
DELETE TCASTLEAPPLICANTTABLE WHERE wCastleID = @wCastle


