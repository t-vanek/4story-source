




CREATE PROCEDURE [dbo].[TSaveLocalOccupy]
@wLocalID SMALLINT,
@bType TINYINT,
@dwGuildID INT,
@dwCharID INT,
@bCountry TINYINT OUTPUT,
@dwResultGuild INT OUTPUT
AS

DECLARE @dateOccupy SMALLDATETIME
DECLARE @dateDefend SMALLDATETIME
DECLARE @bCount TINYINT
DECLARE @dwHour INT
DECLARE @bLocalCountry TINYINT
DECLARE @dwChiefID INT

SET @dateOccupy = GetDate()
SET @dateDefend = 0
SET @bCountry = 3
SET @bLocalCountry = 3

SELECT @dwHour = dwBattleStart FROM TBATTLETIMECHART WHERE bType=0
SET @dateDefend=dateadd(year,datepart(year,@dateOccupy)-1900, @dateDefend)
SET @dateDefend=dateadd(month,datepart(month,@dateOccupy)-1, @dateDefend)
SET @dateDefend=dateadd(day,datepart(day,@dateOccupy), @dateDefend)
SET @dateDefend=dateadd(second,@dwHour, @dateDefend)

IF(@bType = 0)
BEGIN
	SELECT @bCountry=bCountry, @dwGuildID=dwGuild FROM TLOCALTABLE WHERE wLocalID = @wLocalID
	UPDATE TLOCALTABLE SET dateDefend = @dateDefend WHERE wLocalID = @wLocalID
	UPDATE TLOCALOCCUPYTABLE SET dwGuildID = @dwGuildID, bType=@bType WHERE wLocalID = @wLocalID AND bDay = DATEPART(weekday, @dateOccupy)
	IF(@@ROWCOUNT = 0)
		INSERT INTO TLOCALOCCUPYTABLE(wLocalID, bDay, dwGuildID, bType) VALUES(@wLocalID, DATEPART(weekday, @dateOccupy), @dwGuildID, @bType)
END
ELSE IF(@bType = 1)
BEGIN
	IF (@dwGuildID <> 0)
	BEGIN
		SELECT @dwChiefID = dwCharID, @bCountry=bCountry  FROM TCHARTABLE WHERE dwCharID = ( SELECT dwCharID FROM TGUILDMEMBERTABLE WHERE dwGuildID = @dwGuildID AND bDuty=2)
		IF(@bCountry > 1)
		BEGIN
			SET @dwGuildID = 0
			SELECT @bCountry = bCountry FROM TAIDTABLE WHERE dwCharID = @dwCharID
			IF(@@ROWCOUNT = 0)
			BEGIN
				SELECT @bLocalCountry=bCountry FROM TLOCALTABLE WHERE wLocalID = @wLocalID
				IF(@bLocalCountry = 0)
					SET @bCountry = 1
				ELSE IF(@bLocalCountry = 1)
					SET @bCountry = 0
			END
		END
	END
	ELSE
	BEGIN
		SELECT @bCountry=bCountry FROM TCHARTABLE WHERE dwCharID = @dwCharID
		IF(@@ROWCOUNT = 0)
			SELECT @bCountry=bCountry, @dwGuildID=dwGuild FROM TLOCALTABLE WHERE wLocalID = @wLocalID

		IF(@bCountry > 1)
		BEGIN
			SELECT @bCountry = bCountry FROM TAIDTABLE WHERE dwCharID = @dwCharID
			IF(@@ROWCOUNT = 0)
			BEGIN
				SELECT @bLocalCountry=bCountry FROM TLOCALTABLE WHERE wLocalID = @wLocalID
				IF(@bLocalCountry = 0)
					SET @bCountry = 1
				ELSE IF(@bLocalCountry = 1)
					SET @bCountry = 0
			END
		END
	END
/*
	BEGIN
		SELECT @bCount = count(wLocalID) FROM TLOCALTABLE WHERE dwGuild = @dwGuildID
		IF(@bCount = 3 )
			SET @dwGuildID = 0
	END
*/
	UPDATE TLOCALTABLE SET 
		bCountry = @bCountry,
		dwGuild = @dwGuildID, 
		dateOccupy = @dateOccupy, 
		dateDefend = @dateDefend,
		szHero = '',
		dateHero = 0 WHERE wLocalID = @wLocalID

	UPDATE TLOCALOCCUPYTABLE SET dwGuildID = @dwGuildID, bType=@bType  WHERE wLocalID = @wLocalID AND bDay = DATEPART(weekday, @dateOccupy)
	IF(@@ROWCOUNT = 0)
		INSERT INTO TLOCALOCCUPYTABLE(wLocalID, bDay, dwGuildID, bType) VALUES(@wLocalID, DATEPART(weekday, @dateOccupy), @dwGuildID, @bType)
END

SET @dwResultGuild = @dwGuildID

RETURN @@ERROR


