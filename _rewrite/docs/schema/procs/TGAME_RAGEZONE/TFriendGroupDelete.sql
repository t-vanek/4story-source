

/* FRIEND GROUP DELETE PROCESS

========================================================
PARAMETER
========================================================
@dwCharID		INT
@bGroup		TINYINT

========================================================
RETURN VALUE
========================================================

========================================================
PROCESS
========================================================
1. Check Group
2. Delete group data

*/
CREATE PROCEDURE [dbo].[TFriendGroupDelete]
	@dwCharID		INT,
	@bGroup		TINYINT
AS

DELETE FROM TFRIENDGROUPTABLE WHERE (dwCharID = @dwCharID) and (bGroup = @bGroup)



