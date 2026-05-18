



CREATE PROCEDURE [dbo].[TSaveWarlordSay]
	@bType TINYINT,
	@bRankMonth	TINYINT,
	@dwCharID	INT,
	@szSay		VARCHAR(256)
 AS
	DECLARE @dwWarlordID	INT

	IF(@bType = 0)
	BEGIN
		SELECT @dwWarlordID = dwCharID FROM TMONTHRANKTABLE WHERE bMonth = @bRankMonth and bMonthRank = 0
		IF(@@ROWCOUNT = 0 OR @dwWarlordID <> @dwCharID)
			RETURN 1
	
		UPDATE TMONTHRANKTABLE SET szSay = @szSay WHERE bMonth = @bRankMonth AND bMonthRank = 0
	END
	ELSE IF(@bType < 3)
	BEGIN
		SELECT @dwWarlordID = dwCharID FROM THEROTABLE WHERE bMonth = @bRankMonth and bOrder = 0
		IF(@@ROWCOUNT = 0 OR @dwWarlordID <> @dwCharID)
			RETURN 1
	
		UPDATE THEROTABLE SET szSay = @szSay WHERE bMonth = @bRankMonth AND bOrder = 0
	END

	RETURN 0


