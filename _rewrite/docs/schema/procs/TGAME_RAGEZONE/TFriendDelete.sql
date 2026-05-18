

/* FRIEND DELETE PROCESS

========================================================
PARAMETER
========================================================
@dwCharID		INT
@dwFriendID		INT

========================================================
RETURN VALUE
========================================================

========================================================
PROCESS
========================================================
1. Delete Friend Table

*/
CREATE PROCEDURE [dbo].[TFriendDelete]
	@dwCharID 		INT,
	@dwFriendID 		INT
AS

DELETE FROM TFRIENDTABLE WHERE (dwCharID=@dwCharID AND dwFriendID=@dwFriendID)



