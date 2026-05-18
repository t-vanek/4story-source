
CREATE PROCEDURE [dbo].[TChristmasEventPost]
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
		'Christmas Festival has begun!',
		'Special event <lollipop>s and <snowball>s will be dropped from monsters of Iverian continent. You will be able to rent skill <Throwing Snowball> from beautiful event NPC, and obtain <Rudolph Chocolate> which gives you special buffs from the Christmas Tree.
When you click the event NPC and click <Shop>, a window will pop up with <Throwing Snowball> skill in it. Click and drag the skill to your quick-slot, then select/click a target, and use the hot-key to throw the snowball at the target.
Have a great time with the Christmas Festival in 4Story world. Thank you.'

END


