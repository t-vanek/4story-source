
CREATE PROCEDURE [dbo].[TNewYearEventPost]
@dwCharID INT
AS

DECLARE @szName VARCHAR(50)
DECLARE @bCountry TINYINT

SELECT @szName = szName, @bCountry=bCountry FROM TCHARTABLE WHERE dwCharID = @dwCharID
IF(@@ROWCOUNT = 0 OR @szName IS NULL)
	RETURN

IF NOT EXISTS(SELECT dwSeq FROM TRESERVEDPOST WHERE dwRecverID=@dwCharID AND wItemID = 9400)
BEGIN
	EXEC TEventItemGive
		@szName,
		9400,
		1,
		'Happy New Year Event of 4Story!',
'We wish you a happy new year with the events prepared in 4Story world!
1. During the event, the monsters of Iveria continent will drop <Firecracker>s that shoots beautiful sparkling flames in to the sky, and material item to make <Rice Cake Soup> which heals 20% of health point when used. <Rice Cake Soup> can be manufactured through [Novice Craft NPC]"Basil" in Eden Square of Thebekut.
2. <Fortune Pouch> can be obtained from hunting the monsters during the event. You will be able to get special items when you unseal the <Fortune Pouch> through "Elf Sorcerer" in town.
Have a great New Year with 4Story Online. Thank you.'
END


