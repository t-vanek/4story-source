


CREATE PROCEDURE [dbo].[TSaveMonthPvPoint] 
	@dwCharID	INT,
	@dwPoint	INT,
	@wWin		SMALLINT,
	@wLose	SMALLINT,
	@szSay		VARCHAR(256),
	@bCountry	TINYINT
AS
	IF NOT EXISTS( SELECT dwPoint FROM TMONTHPVPOINTTABLE WHERE dwCharID = @dwCharID)
	BEGIN
		INSERT INTO TMONTHPVPOINTTABLE(
		dwCharID,
		dwPoint,
		wWin,
		wLose,
		szSay,
		bCountry) VALUES (
		@dwCharID,
		@dwPoint,
		@wWin,
		@wLose,
		@szSay,
		@bCountry)

	END
	ELSE
	BEGIN
		UPDATE TMONTHPVPOINTTABLE SET dwPoint = @dwPoint, wWin = @wWin, wLose = @wLose, szSay = @szSay ,bCountry = @bCountry WHERE dwCharID = @dwCharID
	END
	
	
	RETURN 0




