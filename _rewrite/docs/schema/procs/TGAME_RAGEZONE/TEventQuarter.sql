


CREATE PROCEDURE [dbo].[TEventQuarter]
@szName varchar(50),
@bDay tinyint,
@bHour tinyint,
@bMinute tinyint
AS

DECLARE @wItemID1 SMALLINT
DECLARE @wItemID2 SMALLINT
DECLARE @wItemID3 SMALLINT
DECLARE @wItemID4 SMALLINT
DECLARE @wItemID5 SMALLINT
DECLARE @bCount TINYINT
DECLARE @szTitle VARCHAR(50)
DECLARE @szMessage VARCHAR(500)

SET @bCount = 0
SET @wItemID1 = 0
SET @wItemID2 = 0
SET @wItemID3 = 0
SET @wItemID4 = 0
SET @wItemID5 = 0

SELECT @wItemID1 = wItemID1,  @wItemID2 = wItemID2,  @wItemID3 = wItemID3,  @wItemID4 = wItemID4,  @wItemID5 = wItemID5, @bCount = bCount, @szTitle=szTitle, @szMessage=szMessage 
	FROM TEVENTQUARTERCHART WHERE bDay=@bDay AND bHour=@bHour AND bMinute=@bMinute

IF(@bCount <> 0)
BEGIN
	INSERT INTO TEVENTQUARTERGIVETABLE(bDay, bHour, bMinute, szName) VALUES(@bDay, @bHour, @bMinute, @szName)

	IF( @wItemID1 <> 0)
		EXEC TEventItemGive @szName, @witemID1, @bCount, @szTitle, @szMessage
	IF( @wItemID2 <> 0)
		EXEC TEventItemGive @szName, @witemID2, @bCount, @szTitle, @szMessage
	IF( @wItemID3 <> 0)
		EXEC TEventItemGive @szName, @witemID3, @bCount, @szTitle, @szMessage
	IF( @wItemID4 <> 0)
		EXEC TEventItemGive @szName, @witemID4, @bCount, @szTitle, @szMessage
	IF( @wItemID5 <> 0)
		EXEC TEventItemGive @szName, @witemID5, @bCount, @szTitle, @szMessage
END



