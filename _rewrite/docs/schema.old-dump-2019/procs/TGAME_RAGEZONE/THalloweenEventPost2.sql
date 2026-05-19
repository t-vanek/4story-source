


CREATE PROCEDURE [dbo].[THalloweenEventPost2]
@dwCharID INT
AS

DECLARE @szName VARCHAR(50)

SELECT @szName = szName  FROM TCHARTABLE WHERE dwCharID = @dwCharID
IF(@@ROWCOUNT = 0 OR @szName IS NULL)
	RETURN

IF NOT EXISTS(SELECT dwSeq FROM TRESERVEDPOST WHERE dwRecverID=@dwCharID AND wItemID = 18143)
BEGIN
	EXEC TEventItemGive 
		@szName,
		18143,
		3,
		'The Halloween Event has been commenced!',
		'A <Halloween Invitation Letter> will be dropped from the all the monsters in Iveria. Please collect the letters and bring to [Halloween] “Party Animal” and you can exchange to <Pumpkim Helm> or <Halloween Candy> or <Burning Pumpkin>. 
		You can borrow the skill of <Throwing Pumpkin> from [Burning Pumpkin]”Expert” in Start Village from 27th of Oct (Event Commencing Day) to 3rd of Nov. 
		We wish you have a happy halloween with the Events from 4Story. Thank you.'
END


