
CREATE PROCEDURE [dbo].[TSeventhdayEventPost]
@dwCharID INT
AS

DECLARE @szName VARCHAR(50)

SELECT @szName = szName  FROM TCHARTABLE WHERE dwCharID = @dwCharID
IF(@@ROWCOUNT = 0 OR @szName IS NULL)
	RETURN

IF NOT EXISTS(SELECT dwSeq FROM TRESERVEDPOST WHERE dwRecverID=@dwCharID AND wItemID = 9400)
BEGIN
	EXEC TEventItemGive 
		@szName,
		9400,
		1,
		'The Altair and Vega Event has begun!',
'<Colored Papers> will be dropped from the monsters of Iveria during the event period. The colored papers can be exchanged to paper decorations which imbues various buff effects, from "Altar of Wishes" located in Valley of Bicera of Thebekut.
Please help "Vega" and "Altair" complete their gifts fro each other from Eden Square of Thebekut. If you find <Silk Threads>, <Pearl Beads> they had lost to help them to completed their gifts, you will be able to receive <Mysterious Paper of Wish> which can be exchanged to <Heaven''s Gift> through [Unidentified]"Mushroom" after the event ends.
Enjoy the event and have a great time with 4Story Online! Thank you.'
END


