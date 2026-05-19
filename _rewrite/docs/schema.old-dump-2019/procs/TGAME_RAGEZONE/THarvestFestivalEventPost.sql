
CREATE PROCEDURE [dbo].[THarvestFestivalEventPost]
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
		'Thanksgiving Event has begun!',
'<Corns> will drop from monsters all over the Iveria continent during the event. When you collect these materials and bring them to "Master of Rice Cake" in Eden Square of Thebekut to exchange them to <Roast Turkey> which heals life point.
Happy Thanksgiving holiday with 4Story and enjoy the event. Thank you.'
END


