
CREATE PROCEDURE [dbo].[TServerRewardPost]
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
		'Title',
'Msg'
END

