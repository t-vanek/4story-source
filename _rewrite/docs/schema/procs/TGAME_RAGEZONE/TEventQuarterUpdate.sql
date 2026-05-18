

CREATE PROCEDURE [dbo].[TEventQuarterUpdate]  
	@bType	TINYINT,
	@wID		SMALLINT,
	@bDay		TINYINT,
	@bHour	TINYINT,
	@bMinute	TINYINT,
	@wItemID1	SMALLINT,
	@wItemID2 	SMALLINT,
	@wItemID3	SMALLINT,
	@wItemID4	SMALLINT,
	@wItemID5	SMALLINT,
	@bCount	TINYINT,
	@szPresent	VARCHAR(50),
	@szAnnounce	VARCHAR(1024),
	@szTitle	VARCHAR(50),
	@szMessage	VARCHAR(500),
	@wOutID	SMALLINT	OUTPUT,
	@szName1	VARCHAR(50)	OUTPUT,
	@szName2	VARCHAR(50)	OUTPUT,
	@szName3	VARCHAR(50)	OUTPUT,
	@szName4	VARCHAR(50)	OUTPUT,
	@szName5	VARCHAR(50)	OUTPUT

AS
	-- Delete
	IF(@bType = 0 )  
	BEGIN
		IF NOT EXISTS ( SELECT TOP 1 wID FROM TEVENTQUARTERCHART WHERE wID = @wID)
			RETURN 1

		DELETE TEVENTQUARTERCHART WHERE wID = @wID

		RETURN 0
	END	
	
	-- Add
	IF(@bType = 1)
	BEGIN
		SET @wOutID = 0
		IF EXISTS ( SELECT wItemID1 FROM TEVENTQUARTERCHART WHERE bDay = @bDay AND bHour = @bHour AND bMinute = @bMinute)
			RETURN 1

		SELECT @wOutID =  MAX(wID+1) FROM TEVENTQUARTERCHART
		IF @wOutID IS NULL
			SET @wOutID = 1

		INSERT INTO TEVENTQUARTERCHART(wID, bDay, bHour, bMinute, wItemID1, wItemID2, wItemID3, wItemID4, wItemID5, bCount, szPresent, szAnnounce, szTitle, szMessage)
		VALUES ( @wOutID, @bDay, @bHour, @bMinute, @wItemID1, @wItemID2, @wItemID3, @wItemID4, @wItemID5, @bCount, @szPresent, @szAnnounce, @szTitle, @szMessage)

		EXEC TGetItemName @wItemID1,@wItemID2,@wItemID3,@wItemID4,@wItemID5,@szName1 OUTPUT,@szName2 OUTPUT, @szName3 OUTPUT, @szName4 OUTPUT, @szName5 OUTPUT
		RETURN 0
	END

	-- Update
	IF(@bType = 2 )
	BEGIN

		IF NOT EXISTS ( SELECT TOP 1 wID FROM TEVENTQUARTERCHART WHERE wID = @wID)
			RETURN 1

		UPDATE TEVENTQUARTERCHART SET bDay = @bDay, bHour = @bHour, bMinute = @bMinute, wItemID1 = @wItemID1, wItemID2 = @wItemID2, wItemID3 = @wItemID3, wItemID4 = @wItemID4, wItemID5 = @wItemID5 , bCount = @bCount,
			 szPresent = @szPresent, szAnnounce = @szAnnounce, szTitle = @szTitle, szMessage = @szMessage
		WHERE wID = @wID

		EXEC TGetItemName @wItemID1,@wItemID2,@wItemID3,@wItemID4,@wItemID5,@szName1 OUTPUT,@szName2 OUTPUT, @szName3 OUTPUT, @szName4 OUTPUT, @szName5 OUTPUT

		RETURN 0
	END

	RETURN 1

