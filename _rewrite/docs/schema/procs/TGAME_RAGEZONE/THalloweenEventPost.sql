
CREATE PROCEDURE [dbo].[THalloweenEventPost]
@dwCharID INT
AS

DECLARE @szName VARCHAR(50)

SELECT @szName = szName  FROM TCHARTABLE_PW WHERE dwCharID = @dwCharID
IF(@@ROWCOUNT = 0 OR @szName IS NULL)
	RETURN

IF NOT EXISTS(SELECT dwSeq FROM TRESERVEDPOST WHERE dwRecverID=@dwCharID AND wItemID = 9400)
BEGIN
	EXEC TEventItemGive 
		@szName,
		9400,
		1,
		'Halloween Event has begun!',
'<Halloween Invitation Letters> will be dropped from monsters of entire Iveria continent during the festival. When you collect the invitation letters and bring them to [Halloween]"Party Animal" in Eden Square of Thebekut, you can either exchange them to <Halloween Candies> or <Burning Pumpkins>.
[Burning Pumpkin]"Expert" in Eden Square will lend skill <Throwing Pumpkin> during the event, so you''ll be able to throw the pumpkins.
Have a great Halloween with the hot Halloween event in 4Story world. Thank you.'
END


