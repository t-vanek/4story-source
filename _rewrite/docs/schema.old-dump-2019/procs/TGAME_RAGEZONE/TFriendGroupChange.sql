

/* FRIEND GROUP CHANGE PROCESS

========================================================
PARAMETER
========================================================
@dwCharID		INT
@bGroup		TINYINT
@dwFriend		INT

========================================================
RETURN VALUE
========================================================

========================================================
PROCESS
========================================================
1. Check Group
2. Change friend group data

*/
CREATE PROCEDURE [dbo].[TFriendGroupChange]
	@dwCharID		INT,
	@bGroup		TINYINT,
	@dwFriendID		INT
AS

UPDATE TFRIENDTABLE SET bGroup = @bGroup WHERE (dwCharID = @dwCharID) and (dwFriendID = @dwFriendID)



